#ifndef VAXP_WINDOW_H
#define VAXP_WINDOW_H

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define VAXP_TYPE_WINDOW (vaxp_window_get_type())
G_DECLARE_FINAL_TYPE(VaxpWindow, vaxp_window, VAXP, WINDOW, AdwApplicationWindow)

VaxpWindow* vaxp_window_new(GtkApplication *app);

G_END_DECLS

#endif /* VAXP_WINDOW_H */
