#include "vaxp_window.h"
#include "vaxp_ufw_backend.h"
#include <libnotify/notify.h>

struct _VaxpWindow {
    GtkApplicationWindow parent_instance;
    
    GtkHeaderBar *headerbar;
    
    GtkSwitch *ufw_switch;
    GtkLabel *status_label;
    
    GtkDropDown *incoming_policy_drop;
    GtkDropDown *outgoing_policy_drop;
    
    GtkListBox *rules_group;
    GtkButton *add_rule_btn;
    GtkButton *refresh_rules_btn;
    GtkTextView *logs_view;
    
    GtkDrawingArea *network_graph;
    guint64 last_rx;
    guint64 last_tx;
    double rx_history[60];
    double tx_history[60];
    int history_index;
    guint timer_id;
};

G_DEFINE_TYPE(VaxpWindow, vaxp_window, GTK_TYPE_APPLICATION_WINDOW)

static void populate_rules_list(VaxpWindow *self);
static void refresh_logs(VaxpWindow *self);

static void refresh_ufw_status(VaxpWindow *self);
static gboolean on_ufw_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data);

static void refresh_ufw_status(VaxpWindow *self) {
    VaxpUfwStatus status = vaxp_ufw_get_status();
    gboolean active;
    const gchar *state_str;
    GError *error = NULL;
    
    // Temporarily block signal to avoid infinite loops when setting state programmatically
    g_signal_handlers_block_by_func(self->ufw_switch, (gpointer)on_ufw_switch_state_set, self);
    
    if (status == VAXP_UFW_STATUS_ACTIVE) {
        active = TRUE;
        state_str = "Firewall is Active";
    } else if (status == VAXP_UFW_STATUS_INACTIVE) {
        active = FALSE;
        state_str = "Firewall is Inactive";
    } else {
        active = FALSE; // Default to inactive if status is unknown
        state_str = "Status Unknown (Need Root?)";
    }
    
    gtk_label_set_text(self->status_label, state_str);
    gtk_switch_set_active(self->ufw_switch, active);
    
    // Update Policy dropdowns
    gchar *policies_str = vaxp_ufw_get_policies(&error);
    if (policies_str) {
        // expected format "Default: deny (incoming), allow (outgoing), disabled (routed)"
        if (g_strrstr(policies_str, "deny (incoming)")) gtk_drop_down_set_selected(self->incoming_policy_drop, 0);
        else if (g_strrstr(policies_str, "allow (incoming)")) gtk_drop_down_set_selected(self->incoming_policy_drop, 1);
        else if (g_strrstr(policies_str, "reject (incoming)")) gtk_drop_down_set_selected(self->incoming_policy_drop, 2);
        
        if (g_strrstr(policies_str, "deny (outgoing)")) gtk_drop_down_set_selected(self->outgoing_policy_drop, 0);
        else if (g_strrstr(policies_str, "allow (outgoing)")) gtk_drop_down_set_selected(self->outgoing_policy_drop, 1);
        else if (g_strrstr(policies_str, "reject (outgoing)")) gtk_drop_down_set_selected(self->outgoing_policy_drop, 2);
        
        g_free(policies_str);
    }
    
    if (error) {
        g_error_free(error);
        error = NULL; // Clear error after handling
    }
    
    g_signal_handlers_unblock_by_func(self->ufw_switch, (gpointer)on_ufw_switch_state_set, self);
}

static void on_policy_changed(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    VaxpWindow *self = VAXP_WINDOW(user_data);
    GtkDropDown *drop_down = GTK_DROP_DOWN(gobject);
    const gchar *direction = (drop_down == self->incoming_policy_drop) ? "incoming" : "outgoing";
    
    GtkStringObject *selected = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(drop_down));
    if (!selected) return;
    
    const gchar *policy = gtk_string_object_get_string(selected);
    
    if (gtk_switch_get_active(self->ufw_switch)) {
        GError *error = NULL;
        if (!vaxp_ufw_set_policy(direction, policy, &error)) {
            g_printerr("Failed to set %s policy to %s: %s\n", direction, policy, error ? error->message : "Unknown");
            if (error) g_error_free(error);
        } else {
            // Success, verify status text
            refresh_ufw_status(self);
            
            // Send Desktop Notification
            gchar *msg = g_strdup_printf("Default %s policy changed to %s.", direction, policy);
            NotifyNotification *n = notify_notification_new("VAXP Firewall", msg, "security-high-symbolic");
            notify_notification_show(n, NULL);
            g_object_unref(G_OBJECT(n));
            g_free(msg);
        }
    }
}

static gboolean on_ufw_switch_state_set(GtkSwitch *widget, gboolean state, gpointer user_data) {
    VaxpWindow *self = VAXP_WINDOW(user_data);
    GError *error = NULL;
    gboolean success = FALSE;

    if (state) {
        success = vaxp_ufw_enable(&error);
    } else {
        success = vaxp_ufw_disable(&error);
    }

    if (!success) {
        g_printerr("Failed to change UFW state: %s\n", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
    } else {
        gchar *msg = g_strdup_printf("Firewall %s successfully.", state ? "Enabled" : "Disabled");
        NotifyNotification *n = notify_notification_new("VAXP Firewall", msg, state ? "security-high-symbolic" : "security-medium-symbolic");
        notify_notification_show(n, NULL);
        g_object_unref(G_OBJECT(n));
        g_free(msg);
    }

    refresh_ufw_status(self);
    return TRUE; // Handled
}

static void on_delete_rule_clicked(GtkButton *btn, gpointer user_data) {
    // The user_data is a GINT_TO_POINTER of the rule number.
    gint rule_number = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "rule_number"));
    VaxpWindow *self = VAXP_WINDOW(user_data);

    GError *error = NULL;
    if (!vaxp_ufw_delete_rule(rule_number, &error)) {
        g_printerr("Failed to delete rule %d: %s\n", rule_number, error ? error->message : "Unknown error");
        if (error) g_error_free(error);
    } else {
        gchar *msg = g_strdup_printf("Deleted Rule #%d successfully.", rule_number);
        NotifyNotification *n = notify_notification_new("VAXP Firewall", msg, "edit-delete-symbolic");
        notify_notification_show(n, NULL);
        g_object_unref(G_OBJECT(n));
        g_free(msg);
    }
    
    populate_rules_list(self);
}

static void on_add_rule_response(GtkDialog *dialog, gint response, gpointer user_data) {
    VaxpWindow *self = VAXP_WINDOW(user_data);
    
    if (response == GTK_RESPONSE_ACCEPT) {
        GtkNotebook *notebook = GTK_NOTEBOOK(g_object_get_data(G_OBJECT(dialog), "notebook"));
        gint page = gtk_notebook_get_current_page(notebook);
        GError *error = NULL;
        
        if (page == 0) {
            // Custom Rule
            GtkEntry *port_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "port_entry"));
            GtkDropDown *action_drop = GTK_DROP_DOWN(g_object_get_data(G_OBJECT(dialog), "action_drop_custom"));
            GtkDropDown *proto_drop = GTK_DROP_DOWN(g_object_get_data(G_OBJECT(dialog), "proto_drop"));
            
            const gchar *port = gtk_editable_get_text(GTK_EDITABLE(port_entry));
            GtkStringObject *action_item = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(action_drop));
            const gchar *action = gtk_string_object_get_string(action_item);
            GtkStringObject *proto_item = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(proto_drop));
            const gchar *proto = gtk_string_object_get_string(proto_item);
            
            if (strlen(port) > 0) {
                if (!vaxp_ufw_add_rule(port, proto, action, &error)) {
                    g_printerr("Failed to add rule: %s\n", error ? error->message : "Unknown error");
                    if (error) g_error_free(error);
                } else {
                    gchar *msg = g_strdup_printf("Added Custom Rule (Port: %s, Proto: %s, Action: %s)", port, proto, action);
                    NotifyNotification *n = notify_notification_new("VAXP Firewall", msg, "list-add-symbolic");
                    notify_notification_show(n, NULL);
                    g_object_unref(G_OBJECT(n));
                    g_free(msg);
                }
            }
        } else {
            // App Rule
            GtkDropDown *app_drop = GTK_DROP_DOWN(g_object_get_data(G_OBJECT(dialog), "app_drop"));
            GtkDropDown *action_drop = GTK_DROP_DOWN(g_object_get_data(G_OBJECT(dialog), "action_drop_app"));
            
            GtkStringObject *app_item = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(app_drop));
            GtkStringObject *action_item = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(action_drop));
            
            if (app_item) {
                const gchar *app_name = gtk_string_object_get_string(app_item);
                const gchar *action = gtk_string_object_get_string(action_item);
                
                if (!vaxp_ufw_add_app_rule(app_name, action, &error)) {
                    g_printerr("Failed to add app rule: %s\n", error ? error->message : "Unknown error");
                    if (error) g_error_free(error);
                } else {
                    gchar *msg = g_strdup_printf("Added App Rule (App: %s, Action: %s)", app_name, action);
                    NotifyNotification *n = notify_notification_new("VAXP Firewall", msg, "list-add-symbolic");
                    notify_notification_show(n, NULL);
                    g_object_unref(G_OBJECT(n));
                    g_free(msg);
                }
            }
        }
        
        populate_rules_list(self);
        refresh_logs(self);
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_add_rule_clicked(GtkButton *btn, gpointer user_data) {
    VaxpWindow *self = VAXP_WINDOW(user_data);
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Add Rule",
                                                    GTK_WINDOW(self),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Add", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_top(content_area, 12);
    gtk_widget_set_margin_bottom(content_area, 12);
    gtk_widget_set_margin_start(content_area, 12);
    gtk_widget_set_margin_end(content_area, 12);
    
    GtkWidget *notebook = gtk_notebook_new();
    
    // Page 1: Custom Port
    GtkWidget *custom_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(custom_box, 12);
    
    GtkWidget *port_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(port_entry), "Port (e.g., 22, 80)");
    gtk_box_append(GTK_BOX(custom_box), port_entry);
    
    const char *actions[] = {"Allow", "Deny", "Reject", NULL};
    GtkWidget *action_drop_custom = gtk_drop_down_new_from_strings(actions);
    gtk_box_append(GTK_BOX(custom_box), action_drop_custom);
    
    const char *protocols[] = {"Any", "TCP", "UDP", NULL};
    GtkWidget *proto_drop = gtk_drop_down_new_from_strings(protocols);
    gtk_box_append(GTK_BOX(custom_box), proto_drop);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), custom_box, gtk_label_new("Custom Port"));
    
    // Page 2: Application Profile
    GtkWidget *app_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(app_box, 12);
    
    GError *error = NULL;
    GList *apps = vaxp_ufw_get_apps(&error);
    GtkStringList *app_strings = gtk_string_list_new(NULL);
    for (GList *l = apps; l != NULL; l = l->next) {
        gtk_string_list_append(app_strings, (const gchar *)l->data);
        g_free(l->data);
    }
    g_list_free(apps);
    
    GtkWidget *app_drop = gtk_drop_down_new(G_LIST_MODEL(app_strings), NULL);
    gtk_box_append(GTK_BOX(app_box), app_drop);
    
    GtkWidget *action_drop_app = gtk_drop_down_new_from_strings(actions);
    gtk_box_append(GTK_BOX(app_box), action_drop_app);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), app_box, gtk_label_new("Application"));
    
    gtk_box_append(GTK_BOX(content_area), notebook);
    
    g_object_set_data(G_OBJECT(dialog), "notebook", notebook);
    g_object_set_data(G_OBJECT(dialog), "port_entry", port_entry);
    g_object_set_data(G_OBJECT(dialog), "action_drop_custom", action_drop_custom);
    g_object_set_data(G_OBJECT(dialog), "proto_drop", proto_drop);
    g_object_set_data(G_OBJECT(dialog), "app_drop", app_drop);
    g_object_set_data(G_OBJECT(dialog), "action_drop_app", action_drop_app);
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_add_rule_response), self);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void refresh_logs(VaxpWindow *self) {
    GError *error = NULL;
    gchar *logs = vaxp_ufw_get_logs(&error);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(self->logs_view);
    
    if (logs == NULL) {
        gtk_text_buffer_set_text(buffer, error ? error->message : "Could not fetch logs.", -1);
        if (error) g_error_free(error);
    } else {
        gtk_text_buffer_set_text(buffer, logs, -1);
        g_free(logs);
    }
}

static void populate_rules_list(VaxpWindow *self) {
    // Clear existing rules rows
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(self->rules_group));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        if (GTK_IS_LIST_BOX_ROW(child)) {
            gtk_list_box_remove(self->rules_group, child);
        }
        child = next;
    }

    GError *error = NULL;
    GList *rules = vaxp_ufw_get_rules(&error);
    
    if (rules == NULL) {
        if (error) {
            g_printerr("Failed to get rules: %s\n", error->message);
            g_error_free(error);
        }
        
        GtkWidget *empty_row = gtk_list_box_row_new();
        gtk_widget_set_focusable(empty_row, FALSE);
        GtkWidget *empty_label = gtk_label_new("No active rules found. Verify UFW is enabled.");
        gtk_widget_set_margin_top(empty_label, 12);
        gtk_widget_set_margin_bottom(empty_label, 12);
        gtk_list_box_append(self->rules_group, empty_row);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(empty_row), empty_label);
        return;
    }

    for (GList *iter = rules; iter != NULL; iter = iter->next) {
        VaxpUfwRule *rule = (VaxpUfwRule *)iter->data;
        
        GtkWidget *row = gtk_list_box_row_new();
        gtk_widget_set_focusable(row, FALSE);
        
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_start(hbox, 12);
        gtk_widget_set_margin_end(hbox, 12);
        gtk_widget_set_margin_top(hbox, 8);
        gtk_widget_set_margin_bottom(hbox, 8);
        
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_hexpand(vbox, TRUE);
        gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

        gchar *title_str = g_strdup_printf("[%d] %s", rule->number, rule->to);
        gchar *subtitle_str = g_strdup_printf("Action: %s | From: %s", rule->action, rule->from);

        GtkWidget *title = gtk_label_new(title_str);
        gtk_widget_set_halign(title, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
        
        GtkWidget *subtitle = gtk_label_new(subtitle_str);
        gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
        gtk_widget_add_css_class(subtitle, "dim-label");
        gtk_label_set_ellipsize(GTK_LABEL(subtitle), PANGO_ELLIPSIZE_END);
        
        gtk_box_append(GTK_BOX(vbox), title);
        gtk_box_append(GTK_BOX(vbox), subtitle);
        
        gtk_box_append(GTK_BOX(hbox), vbox);
        
        // Add a delete button
        GtkWidget *del_btn = gtk_button_new_from_icon_name("user-trash-symbolic");
        gtk_widget_set_valign(del_btn, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class(del_btn, "destructive-action");
        g_object_set_data(G_OBJECT(del_btn), "rule_number", GINT_TO_POINTER(rule->number));
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_rule_clicked), self);
        
        gtk_box_append(GTK_BOX(hbox), del_btn);
        
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
        gtk_list_box_append(self->rules_group, row);
        
        g_free(title_str);
        g_free(subtitle_str);
        vaxp_ufw_rule_free(rule);
    }
    g_list_free(rules);
}

static void on_refresh_rules_clicked(GtkButton *btn, gpointer user_data) {
    VaxpWindow *self = VAXP_WINDOW(user_data);
    populate_rules_list(self);
    refresh_logs(self);
}

static void draw_network_graph(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    VaxpWindow *self = VAXP_WINDOW(user_data);

    // Fill background
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.5);
    cairo_paint(cr);

    // Find max value for scaling
    double max_val = 1024.0; // minimum 1KB scale
    for (int i = 0; i < 60; i++) {
        if (self->rx_history[i] > max_val) max_val = self->rx_history[i];
        if (self->tx_history[i] > max_val) max_val = self->tx_history[i];
    }

    // Draw RX (Download) - Green
    cairo_set_source_rgba(cr, 0.2, 0.8, 0.2, 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, 0, height);
    for (int i = 0; i < 60; i++) {
        int idx = (self->history_index + i) % 60;
        double x = (width / 59.0) * i;
        double y = height - (self->rx_history[idx] / max_val * height);
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    // Draw TX (Upload) - Blue
    cairo_set_source_rgba(cr, 0.2, 0.5, 1.0, 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, 0, height);
    for (int i = 0; i < 60; i++) {
        int idx = (self->history_index + i) % 60;
        double x = (width / 59.0) * i;
        double y = height - (self->tx_history[idx] / max_val * height);
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
    
    // Draw Text overlay for current speeds
    double current_rx = self->rx_history[(self->history_index + 59) % 60] / 1024.0;
    double current_tx = self->tx_history[(self->history_index + 59) % 60] / 1024.0;
    
    gchar *text = g_strdup_printf("⬇ %.1f KB/s   ⬆ %.1f KB/s", current_rx, current_tx);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, 10, 20);
    cairo_show_text(cr, text);
    g_free(text);
}

static gboolean on_network_timer(gpointer user_data) {
    VaxpWindow *self = VAXP_WINDOW(user_data);
    
    guint64 rx, tx;
    vaxp_network_get_stats(&rx, &tx);
    
    if (self->last_rx > 0 && self->last_tx > 0) {
        // Calculate delta bytes per second
        double rx_speed = (double)(rx - self->last_rx);
        double tx_speed = (double)(tx - self->last_tx);
        
        self->rx_history[self->history_index] = rx_speed;
        self->tx_history[self->history_index] = tx_speed;
        self->history_index = (self->history_index + 1) % 60;
        
        gtk_widget_queue_draw(GTK_WIDGET(self->network_graph));
    }
    
    self->last_rx = rx;
    self->last_tx = tx;
    
    return G_SOURCE_CONTINUE;
}

static void vaxp_window_dispose(GObject *object) {
    VaxpWindow *self = VAXP_WINDOW(object);
    if (self->timer_id > 0) {
        g_source_remove(self->timer_id);
        self->timer_id = 0;
    }
    G_OBJECT_CLASS(vaxp_window_parent_class)->dispose(object);
}

static void vaxp_window_class_init(VaxpWindowClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
    
    object_class->dispose = vaxp_window_dispose;
    
    gtk_widget_class_set_template_from_resource(widget_class, "/com/vaxp/firewall/ui/vaxp_window.ui");
    
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, headerbar);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, ufw_switch);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, status_label);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, incoming_policy_drop);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, outgoing_policy_drop);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, rules_group);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, add_rule_btn);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, refresh_rules_btn);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, logs_view);
    gtk_widget_class_bind_template_child(widget_class, VaxpWindow, network_graph);
}

static void vaxp_window_init(VaxpWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
    
    // Set up policy dropdowns
    const char *policies_list[] = {"Deny", "Allow", "Reject", NULL};
    GtkStringList *string_list = gtk_string_list_new(policies_list);
    gtk_drop_down_set_model(self->incoming_policy_drop, G_LIST_MODEL(string_list));
    
    string_list = gtk_string_list_new(policies_list);
    gtk_drop_down_set_model(self->outgoing_policy_drop, G_LIST_MODEL(string_list));
    
    g_signal_connect(self->incoming_policy_drop, "notify::selected", G_CALLBACK(on_policy_changed), self);
    g_signal_connect(self->outgoing_policy_drop, "notify::selected", G_CALLBACK(on_policy_changed), self);
    
    g_signal_connect(self->ufw_switch, "state-set", G_CALLBACK(on_ufw_switch_state_set), self);
    g_signal_connect(self->refresh_rules_btn, "clicked", G_CALLBACK(on_refresh_rules_clicked), self);
    g_signal_connect(self->add_rule_btn, "clicked", G_CALLBACK(on_add_rule_clicked), self);
    
    gtk_drawing_area_set_draw_func(self->network_graph, draw_network_graph, self, NULL);
    
    // Initialize network stats
    self->last_rx = 0;
    self->last_tx = 0;
    self->history_index = 0;
    for(int i=0; i<60; i++) { self->rx_history[i] = 0; self->tx_history[i] = 0; }
    
    self->timer_id = g_timeout_add(1000, on_network_timer, self);
    
    refresh_ufw_status(self);
    populate_rules_list(self);
    refresh_logs(self);
}

VaxpWindow* vaxp_window_new(GtkApplication *app) {
    return g_object_new(VAXP_TYPE_WINDOW, "application", app, NULL);
}
