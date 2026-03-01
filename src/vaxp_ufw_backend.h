#ifndef VAXP_UFW_BACKEND_H
#define VAXP_UFW_BACKEND_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    VAXP_UFW_STATUS_ACTIVE,
    VAXP_UFW_STATUS_INACTIVE,
    VAXP_UFW_STATUS_UNKNOWN
} VaxpUfwStatus;

typedef struct {
    gint number;
    gchar *to;
    gchar *action;
    gchar *from;
} VaxpUfwRule;

VaxpUfwStatus vaxp_ufw_get_status(void);
gboolean vaxp_ufw_enable(GError **error);
gboolean vaxp_ufw_disable(GError **error);

// Retrieves the default policies. Output string should be freed.
// Expects strings like "deny (incoming), allow (outgoing)"
gchar *vaxp_ufw_get_policies(GError **error);
gboolean vaxp_ufw_set_policy(const gchar *direction, const gchar *policy, GError **error);

// Returns a GList of VaxpUfwRule*. Caller must free each rule and the list.
GList *vaxp_ufw_get_rules(GError **error);
void vaxp_ufw_rule_free(VaxpUfwRule *rule);

gboolean vaxp_ufw_delete_rule(gint number, GError **error);
gboolean vaxp_ufw_add_rule(const gchar *port, const gchar *protocol, const gchar *action, GError **error);
gchar *vaxp_ufw_get_logs(GError **error);

// Returns a GList of gchar* (app names). Caller must free each string and the list.
GList *vaxp_ufw_get_apps(GError **error);
gboolean vaxp_ufw_add_app_rule(const gchar *app_name, const gchar *action, GError **error);

// Gets the total current RX and TX bytes across all interfaces
void vaxp_network_get_stats(guint64 *rx_bytes, guint64 *tx_bytes);

G_END_DECLS

#endif /* VAXP_UFW_BACKEND_H */
