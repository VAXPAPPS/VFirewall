#include "vaxp_ufw_backend.h"
#include "vaxp_polkit_helper.h"
#include <gio/gio.h>
#include <string.h>
#include <stdio.h>

VaxpUfwStatus vaxp_ufw_get_status(void) {
    g_autofree gchar *standard_output = NULL;
    g_autofree gchar *standard_error = NULL;
    gint exit_status = 0;
    GError *error = NULL;

    const gchar *argv[] = {"pkexec", "ufw", "status", NULL};

    gboolean res = g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                &standard_output, &standard_error, &exit_status, &error);

    if (!res || error != NULL) {
        if (error) g_error_free(error);
        return VAXP_UFW_STATUS_UNKNOWN;
    }

    if (g_strrstr(standard_output, "Status: active")) {
        return VAXP_UFW_STATUS_ACTIVE;
    } else if (g_strrstr(standard_output, "Status: inactive")) {
        return VAXP_UFW_STATUS_INACTIVE;
    }
    
    return VAXP_UFW_STATUS_UNKNOWN;
}

gchar *vaxp_ufw_get_policies(GError **error) {
    g_autofree gchar *standard_output = NULL;
    gint exit_status = 0;
    
    const gchar *argv[] = {"pkexec", "ufw", "status", "verbose", NULL};

    gboolean res = g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                &standard_output, NULL, &exit_status, error);

    if (!res) {
        return NULL;
    }

    // Default: deny (incoming), allow (outgoing), disabled (routed)
    gchar *default_str = g_strrstr(standard_output, "Default:");
    if (default_str) {
        gchar *end = strchr(default_str, '\n');
        if (end) {
            return g_strndup(default_str, end - default_str);
        }
        return g_strdup(default_str);
    }
    
    return g_strdup("Default: unknown");
}

gboolean vaxp_ufw_set_policy(const gchar *direction, const gchar *policy, GError **error) {
    gchar *dir_lower = g_utf8_strdown(direction, -1);
    gchar *pol_lower = g_utf8_strdown(policy, -1);
    
    gchar *cmd = g_strdup_printf("ufw default %s %s", pol_lower, dir_lower);
    gboolean res = vaxp_polkit_execute_command(cmd, error);
    
    g_free(cmd);
    g_free(dir_lower);
    g_free(pol_lower);
    
    return res;
}

gboolean vaxp_ufw_enable(GError **error) {
    return vaxp_polkit_execute_command("ufw enable", error);
}

gboolean vaxp_ufw_disable(GError **error) {
    return vaxp_polkit_execute_command("ufw disable", error);
}

void vaxp_ufw_rule_free(VaxpUfwRule *rule) {
    if (!rule) return;
    g_free(rule->to);
    g_free(rule->action);
    g_free(rule->from);
    g_free(rule);
}

GList *vaxp_ufw_get_rules(GError **error) {
    g_autofree gchar *standard_output = NULL;
    gint exit_status = 0;
    
    // We must run ufw status numbered as root to see full details sometimes,
    // but typically `ufw status numbered` works if root.
    const gchar *argv[] = {"pkexec", "ufw", "status", "numbered", NULL};

    gboolean res = g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                &standard_output, NULL, &exit_status, error);

    if (!res || *error != NULL) {
        return NULL;
    }

    GList *rules_list = NULL;
    gchar **lines = g_strsplit(standard_output, "\n", -1);
    
    for (int i = 0; lines[i] != NULL; i++) {
        gchar *line = lines[i];
        
        // Skip empty lines or header lines
        if (strlen(line) < 5 || line[0] != '[') continue;
        
        // Look for the closing bracket
        gchar *bracket_end = strchr(line, ']');
        if (!bracket_end) continue;
        
        gchar num_str[10] = {0};
        strncpy(num_str, line + 1, bracket_end - (line + 1));
        gint number = atoi(num_str);
        
        // The columns are separated by multiple spaces.
        // Format: [ 1] 22/tcp                     ALLOW IN    Anywhere
        gchar *rest = bracket_end + 1;
        gchar **tokens = g_strsplit(g_strstrip(rest), "  ", -1);
        
        // Filter out empty tokens
        GPtrArray *valid_tokens = g_ptr_array_new();
        for (int j = 0; tokens[j] != NULL; j++) {
            gchar *t = g_strstrip(tokens[j]);
            if (strlen(t) > 0) {
                g_ptr_array_add(valid_tokens, t);
            }
        }
        
        if (valid_tokens->len >= 3) {
            VaxpUfwRule *rule = g_new0(VaxpUfwRule, 1);
            rule->number = number;
            // The first token is 'To'
            rule->to = g_strdup((gchar *)g_ptr_array_index(valid_tokens, 0));
            // The second token is 'Action'
            rule->action = g_strdup((gchar *)g_ptr_array_index(valid_tokens, 1));
            // The third token is 'From'
            rule->from = g_strdup((gchar *)g_ptr_array_index(valid_tokens, 2));
            
            // Reattach any trailing tokens to 'From' if it got split wrongly due to weird spacing
            for (guint k = 3; k < valid_tokens->len; k++) {
                gchar *temp = rule->from;
                rule->from = g_strdup_printf("%s %s", temp, (gchar *)g_ptr_array_index(valid_tokens, k));
                g_free(temp);
            }
            
            rules_list = g_list_append(rules_list, rule);
        }
        
        g_ptr_array_free(valid_tokens, TRUE);
        g_strfreev(tokens);
    }
    
    g_strfreev(lines);
    return rules_list;
}

gboolean vaxp_ufw_delete_rule(gint number, GError **error) {
    gchar *cmd = g_strdup_printf("ufw --force delete %d", number);
    gboolean res = vaxp_polkit_execute_command(cmd, error);
    g_free(cmd);
    return res;
}

gchar *vaxp_ufw_get_logs(GError **error) {
    g_autofree gchar *standard_output = NULL;
    gint exit_status = 0;
    
    // We can grep ufw logs from /var/log/syslog or /var/log/messages
    // As a simple proxy, `pkexec grep UFW /var/log/syslog | ...`
    const gchar *argv[] = {"pkexec", "sh", "-c", "grep UFW /var/log/syslog 2>/dev/null | tail -n 100 || grep UFW /var/log/messages 2>/dev/null | tail -n 100", NULL};

    gboolean res = g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                &standard_output, NULL, &exit_status, error);

    if (!res) {
        return NULL;
    }

    if (standard_output == NULL || strlen(standard_output) == 0) {
        return g_strdup("No recent UFW logs found.");
    }
    
    // Ownership is transferred to caller
    return g_steal_pointer(&standard_output);
}

gboolean vaxp_ufw_add_rule(const gchar *port, const gchar *protocol, const gchar *action, GError **error) {
    gchar *cmd = NULL;
    
    // Convert action to lowercase for UFW command
    gchar *action_lower = g_utf8_strdown(action, -1);
    
    if (protocol && strlen(protocol) > 0 && g_strcmp0(protocol, "Any") != 0) {
        gchar *proto_lower = g_utf8_strdown(protocol, -1);
        cmd = g_strdup_printf("ufw %s %s/%s", action_lower, port, proto_lower);
        g_free(proto_lower);
    } else {
        cmd = g_strdup_printf("ufw %s %s", action_lower, port);
    }
    
    gboolean res = vaxp_polkit_execute_command(cmd, error);
    
    g_free(action_lower);
    g_free(cmd);
    
    return res;
}

GList *vaxp_ufw_get_apps(GError **error) {
    g_autofree gchar *standard_output = NULL;
    gint exit_status = 0;
    
    const gchar *argv[] = {"pkexec", "ufw", "app", "list", NULL};

    gboolean res = g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                &standard_output, NULL, &exit_status, error);

    if (!res || *error != NULL) {
        return NULL;
    }

    GList *apps_list = NULL;
    gchar **lines = g_strsplit(standard_output, "\n", -1);
    
    // Output looks like:
    // Available applications:
    //   CUPS
    //   Nginx Full
    //   OpenSSH
    for (int i = 0; lines[i] != NULL; i++) {
        gchar *line = lines[i];
        if (g_str_has_prefix(line, "Available applications:")) continue;
        
        gchar *app_name = g_strstrip(g_strdup(line));
        if (strlen(app_name) > 0) {
            apps_list = g_list_append(apps_list, app_name);
        } else {
            g_free(app_name);
        }
    }
    
    g_strfreev(lines);
    return apps_list;
}

gboolean vaxp_ufw_add_app_rule(const gchar *app_name, const gchar *action, GError **error) {
    gchar *action_lower = g_utf8_strdown(action, -1);
    
    // We need to quote the app name in case it has spaces (like "Nginx Full")
    gchar *cmd = g_strdup_printf("ufw %s '%s'", action_lower, app_name);
    gboolean res = vaxp_polkit_execute_command(cmd, error);
    
    g_free(action_lower);
    g_free(cmd);
    
    return res;
}

void vaxp_network_get_stats(guint64 *rx_bytes, guint64 *tx_bytes) {
    if (rx_bytes) *rx_bytes = 0;
    if (tx_bytes) *tx_bytes = 0;

    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return;

    gchar line[256];
    // skip first 2 header lines
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }

    guint64 total_rx = 0;
    guint64 total_tx = 0;

    while (fgets(line, sizeof(line), f)) {
        gchar *colon = strchr(line, ':');
        if (!colon) continue;

        // format: face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
        guint64 rx = 0, tx = 0, dummy;
        int parsed = sscanf(colon + 1, 
            "%" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT,
            &rx, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &tx);
            
        if (parsed == 9) {
            total_rx += rx;
            total_tx += tx;
        }
    }
    
    fclose(f);
    
    if (rx_bytes) *rx_bytes = total_rx;
    if (tx_bytes) *tx_bytes = total_tx;
}
