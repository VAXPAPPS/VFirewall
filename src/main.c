#include <gtk/gtk.h>
#include <adwaita.h>
#include <libnotify/notify.h>
#include "vaxp_window.h"

static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, 
        "window, .background, window.background { background-color: rgba(0, 0, 0, 0.392); }\n"
        "textview.view, scrolledwindow { background-color: transparent; }\n"
        "actionbar > revealer > box { background-color: transparent; border: none; }\n"
        "actionbar { background-color: transparent; border: none; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), 
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    load_css();
    VaxpWindow *win = vaxp_window_new(app);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char *argv[]) {
    g_autoptr(AdwApplication) app = NULL;
    int status;

    notify_init("VAXP Firewall");

    app = adw_application_new("com.vaxp.firewall", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    notify_uninit();
    return status;
}
