#ifndef VAXP_POLKIT_HELPER_H
#define VAXP_POLKIT_HELPER_H

#include <glib.h>

G_BEGIN_DECLS

gboolean vaxp_polkit_execute_command(const gchar *command, GError **error);

G_END_DECLS

#endif /* VAXP_POLKIT_HELPER_H */
