#ifndef VAXP_HEADERBAR_H
#define VAXP_HEADERBAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VAXP_TYPE_HEADERBAR (vaxp_headerbar_get_type ())
G_DECLARE_FINAL_TYPE (VaxpHeaderBar, vaxp_headerbar, VAXP, HEADERBAR, GtkBox)

VaxpHeaderBar *vaxp_headerbar_new            (GtkWindow *window);
void           vaxp_headerbar_set_window     (VaxpHeaderBar *self, GtkWindow *window);
void           vaxp_headerbar_set_title      (VaxpHeaderBar *self, const char *title);
void           vaxp_headerbar_set_subtitle   (VaxpHeaderBar *self, const char *subtitle);
GtkWidget     *vaxp_headerbar_get_start_box  (VaxpHeaderBar *self);
GtkWidget     *vaxp_headerbar_get_end_box    (VaxpHeaderBar *self);

G_END_DECLS

#endif /* VAXP_HEADERBAR_H */
