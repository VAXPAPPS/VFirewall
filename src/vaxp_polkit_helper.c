#include "vaxp_polkit_helper.h"
#include <gio/gio.h>

gboolean vaxp_polkit_execute_command(const gchar *command, GError **error) {
    gint exit_status = 0;
    
    // Split the command string into an array
    gint argc;
    gchar **argv_orig;
    if (!g_shell_parse_argv(command, &argc, &argv_orig, error)) {
        return FALSE;
    }

    // Build pkexec command array
    gchar **argv = g_new0(gchar *, argc + 2);
    argv[0] = g_strdup("pkexec");
    for (int i = 0; i < argc; i++) {
        argv[i + 1] = g_strdup(argv_orig[i]);
    }
    argv[argc + 1] = NULL;

    gboolean res = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                NULL, NULL, &exit_status, error);

    g_strfreev(argv_orig);
    g_strfreev(argv);

    if (!res) {
        return FALSE;
    }

    if (exit_status != 0) {
        g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "Command failed with exit status %d", exit_status);
        return FALSE;
    }

    return TRUE;
}
