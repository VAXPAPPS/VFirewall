#include "headerbar.h"

struct _VaxpHeaderBar {
    GtkBox       parent_instance;

    GtkWindow   *window;
    GtkWidget   *title_label;
    GtkWidget   *subtitle_label;
    GtkWidget   *start_box;
    GtkWidget   *end_box;
    GtkWidget   *btn_box;

    /* Drag state */
    double       drag_start_x;
    double       drag_start_y;
};

G_DEFINE_TYPE (VaxpHeaderBar, vaxp_headerbar, GTK_TYPE_BOX)

static void
on_close_clicked (GtkButton *btn, gpointer user_data)
{
    (void)btn;
    VaxpHeaderBar *self = VAXP_HEADERBAR (user_data);
    gtk_window_close (self->window);
}

static void
on_minimize_clicked (GtkButton *btn, gpointer user_data)
{
    (void)btn;
    VaxpHeaderBar *self = VAXP_HEADERBAR (user_data);
    gtk_window_minimize (self->window);
}

static void
on_maximize_clicked (GtkButton *btn, gpointer user_data)
{
    (void)btn;
    VaxpHeaderBar *self = VAXP_HEADERBAR (user_data);
    if (!self->window) return;
    if (gtk_window_is_maximized (self->window))
        gtk_window_unmaximize (self->window);
    else
        gtk_window_maximize (self->window);
}

static void
on_drag_begin (GtkGestureDrag *gesture, double x, double y, gpointer user_data)
{
    (void)gesture;
    VaxpHeaderBar *self = VAXP_HEADERBAR (user_data);
    self->drag_start_x = x;
    self->drag_start_y = y;
}

static void
on_drag_update (GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data)
{
    (void)offset_x; (void)offset_y;
    VaxpHeaderBar *self = VAXP_HEADERBAR (user_data);
    
    if (!self->window) return;

    GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self->window));
    if (surface && GDK_IS_TOPLEVEL (surface)) {
        gdk_toplevel_begin_move (GDK_TOPLEVEL (surface),
                                 gtk_gesture_get_device (GTK_GESTURE (gesture)),
                                 gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)),
                                 self->drag_start_x,
                                 self->drag_start_y,
                                 gtk_event_controller_get_current_event_time (GTK_EVENT_CONTROLLER (gesture)));
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static GtkWidget *
create_window_button (const char *css_class, const char *tooltip, GCallback callback, gpointer user_data)
{
    GtkWidget *btn = gtk_button_new ();
    gtk_widget_add_css_class (btn, "var-window-btn");
    gtk_widget_add_css_class (btn, css_class);
    gtk_widget_set_focusable (btn, FALSE);
    gtk_widget_set_tooltip_text (btn, tooltip);
    g_signal_connect (btn, "clicked", callback, user_data);
    return btn;
}

static void
vaxp_headerbar_class_init (VaxpHeaderBarClass *klass)
{
    (void)klass;
}

static void
vaxp_headerbar_init (VaxpHeaderBar *self)
{
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class (GTK_WIDGET (self), "var-headerbar");
    
    self->window = NULL;

    /* Left side: Window control buttons (VAXP style) */
    self->btn_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start (self->btn_box, 8);
    gtk_widget_set_margin_end (self->btn_box, 8);

    GtkWidget *close_btn    = create_window_button ("var-btn-close",    "Close",    G_CALLBACK (on_close_clicked), self);
    GtkWidget *minimize_btn = create_window_button ("var-btn-minimize", "Minimize", G_CALLBACK (on_minimize_clicked), self);
    GtkWidget *maximize_btn = create_window_button ("var-btn-maximize", "Maximize", G_CALLBACK (on_maximize_clicked), self);

    /* VAXP order when on right: Minimize, Maximize, Close */
    gtk_box_append (GTK_BOX (self->btn_box), minimize_btn);
    gtk_box_append (GTK_BOX (self->btn_box), maximize_btn);
    gtk_box_append (GTK_BOX (self->btn_box), close_btn);
    gtk_widget_set_valign (self->btn_box, GTK_ALIGN_CENTER);

    /* Custom start box for toolbar buttons */
    self->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign (self->start_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (self->start_box, 8);

    /* Center context: Title and Subtitle */
    GtkWidget *center_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign (center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (center_box, TRUE);
    gtk_widget_set_halign (center_box, GTK_ALIGN_START);
    gtk_widget_set_margin_start (center_box, 12);
    gtk_widget_set_margin_top(center_box, 12);
    gtk_widget_set_margin_bottom(center_box, 12);

    self->title_label = gtk_label_new ("VAXP Color Picker");
    gtk_widget_add_css_class (self->title_label, "title");
    gtk_label_set_ellipsize (GTK_LABEL (self->title_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_valign(self->title_label, GTK_ALIGN_CENTER);

    self->subtitle_label = gtk_label_new ("");
    gtk_widget_add_css_class (self->subtitle_label, "subtitle");
    gtk_label_set_ellipsize (GTK_LABEL (self->subtitle_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_valign(self->subtitle_label, GTK_ALIGN_CENTER);

    gtk_box_append (GTK_BOX (center_box), self->title_label);
    gtk_box_append (GTK_BOX (center_box), self->subtitle_label);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);

    /* End box for toolbar buttons */
    self->end_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign (self->end_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end (self->end_box, 16);

    /* Assemble */
    gtk_box_append (GTK_BOX (self), self->start_box);
    gtk_box_append (GTK_BOX (self), center_box);
    gtk_box_append (GTK_BOX (self), self->end_box);
    gtk_box_append (GTK_BOX (self), self->btn_box);

    /* Add window drag behavior */
    GtkGesture *drag = gtk_gesture_drag_new ();
    g_signal_connect (drag, "drag-begin", G_CALLBACK (on_drag_begin), self);
    g_signal_connect (drag, "drag-update", G_CALLBACK (on_drag_update), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag));
}

VaxpHeaderBar *
vaxp_headerbar_new (GtkWindow *window)
{
    VaxpHeaderBar *self = g_object_new (VAXP_TYPE_HEADERBAR, NULL);
    vaxp_headerbar_set_window(self, window);
    return self;
}

void vaxp_headerbar_set_window(VaxpHeaderBar *self, GtkWindow *window) {
    g_return_if_fail (VAXP_IS_HEADERBAR (self));
    self->window = window;
}

void
vaxp_headerbar_set_title (VaxpHeaderBar *self, const char *title)
{
    g_return_if_fail (VAXP_IS_HEADERBAR (self));
    gtk_label_set_text (GTK_LABEL (self->title_label), title ? title : "VAXP Color Picker");
}

void
vaxp_headerbar_set_subtitle (VaxpHeaderBar *self, const char *subtitle)
{
    g_return_if_fail (VAXP_IS_HEADERBAR (self));
    gtk_label_set_text (GTK_LABEL (self->subtitle_label), subtitle ? subtitle : "");
    gtk_widget_set_visible (self->subtitle_label, subtitle && subtitle[0]);
}

GtkWidget *
vaxp_headerbar_get_start_box (VaxpHeaderBar *self)
{
    g_return_val_if_fail (VAXP_IS_HEADERBAR (self), NULL);
    return self->start_box;
}

GtkWidget *
vaxp_headerbar_get_end_box (VaxpHeaderBar *self)
{
    g_return_val_if_fail (VAXP_IS_HEADERBAR (self), NULL);
    return self->end_box;
}
