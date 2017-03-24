
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>

#include <j4status-plugin-input.h>

#include <i3ipc-glib/i3ipc-glib.h>

typedef struct {
    J4statusPluginContext *context;
    J4statusSection *section;
    i3ipcCon *con;
} J4statusI3focusSection;

struct _J4statusPluginContext {
    J4statusCoreInterface *core;
    i3ipcConnection *connection;
    GList *sections;
    J4statusI3focusSection *focus;
};


static void
_j4status_i3focus_section_free(gpointer data)
{
    J4statusI3focusSection *section = data;

    j4status_section_free(section->section);
    g_object_unref(section->con);

    g_slice_free(J4statusI3focusSection, section);
}

static void
_j4status_i3focus_section_callback(G_GNUC_UNUSED J4statusSection *section_, G_GNUC_UNUSED const gchar *event_id, gpointer user_data)
{
    J4statusI3focusSection *section = user_data;

    i3ipc_con_command(section->con, "focus", NULL);
}

static gint
_j4status_i3focus_section_search(gconstpointer a, gconstpointer b)
{
    const J4statusI3focusSection *section = a;
    i3ipcCon *con = (i3ipcCon *) b;
    gulong ida, idb;
    g_object_get(section->con, "id", &ida, NULL);
    g_object_get(con, "id", &idb, NULL);

    return ( ida == idb ) ? 0 : -1;
}


static void
_j4status_i3focus_section_set_colour(J4statusI3focusSection *section)
{
    gboolean focused = ( section == section->context->focus );
    J4statusColour colour = {
        .set = TRUE,
        .red = focused ? 0xff : 0x7f,
        .green = focused ? 0xff : 0x7f,
        .blue = focused ? 0xff : 0x7f,
        .alpha = 0xff
    };

    j4status_section_set_colour(section->section, colour);
}


static void
_j4status_i3focus_section_set_focus(J4statusI3focusSection *section)
{
    J4statusI3focusSection *old = section->context->focus;
    section->context->focus = section;
    if ( old != NULL )
        _j4status_i3focus_section_set_colour(old);
    _j4status_i3focus_section_set_colour(section);
}


static void
_j4status_i3focus_window_callback(G_GNUC_UNUSED GObject *object, i3ipcWindowEvent *event, gpointer user_data)
{
    J4statusPluginContext *context = user_data;
    GList *section;

    if ( g_strcmp0(event->change, "focus") != 0 )
        return;

    section = g_list_find_custom(context->sections, event->container, _j4status_i3focus_section_search);
    if ( section == NULL )
        return;
    _j4status_i3focus_section_set_focus(section->data);
}


static void
_j4status_i3focus_workspace_callback(G_GNUC_UNUSED GObject *object, i3ipcWorkspaceEvent *event, gpointer user_data)
{
    J4statusPluginContext *context = user_data;

    if ( g_strcmp0(event->change, "focus") == 0 )
    {
        GList *windows = i3ipc_con_leaves(event->current), *window_;

        g_list_free_full(context->sections, _j4status_i3focus_section_free);
        context->sections = NULL;
        context->focus = NULL;

        for ( window_ = g_list_last(windows) ; window_ != NULL ; window_ = g_list_previous(window_) )
        {
            i3ipcCon *window = window_->data;
            gulong id;
            gchar id_str[20];

            g_object_get(window, "id", &id, NULL);
            g_snprintf(id_str, sizeof(id_str), "%lu", id);

            J4statusI3focusSection *section = g_slice_new0(J4statusI3focusSection);
            section->context = context;
            section->con = g_object_ref(window);

            section->section = j4status_section_new(context->core);
            j4status_section_set_name(section->section, "i3focus");
            j4status_section_set_instance(section->section, id_str);
            j4status_section_set_action_callback(section->section, _j4status_i3focus_section_callback, section);
            if ( ! j4status_section_insert(section->section) )
                _j4status_i3focus_section_free(section);
            else
            {
                gboolean focused;
                g_object_get(section->con, "focused", &focused, NULL);
                j4status_section_set_value(section->section, g_strdup(i3ipc_con_get_name(window)));
                if ( focused )
                    _j4status_i3focus_section_set_focus(section);
                _j4status_i3focus_section_set_colour(section);

                context->sections = g_list_prepend(context->sections, section);
            }
        }
        g_list_free(windows);
    }
}

static void _j4status_i3focus_uninit(J4statusPluginContext *context);

static J4statusPluginContext *
_j4status_i3focus_init(J4statusCoreInterface *core)
{
    i3ipcConnection *connection;
    i3ipcCommandReply *reply;
    GError *error = NULL;

    connection = i3ipc_connection_new(NULL, &error);
    if ( connection == NULL )
    {
        g_warning("Couldn't connection to i3: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    reply = i3ipc_connection_subscribe(connection, I3IPC_EVENT_WINDOW|I3IPC_EVENT_WORKSPACE, &error);
    if ( reply == NULL )
    {
        g_warning("Couldn't subscribe to i3 events: %s", error->message);
        g_clear_error(&error);
        g_object_unref(connection);
        return NULL;
    }
    if ( ! reply->success )
    {
        g_warning("Couldn't subscribe to i3 events: %s", reply->error);
        i3ipc_command_reply_free(reply);
        g_object_unref(connection);
        return NULL;
    }
    i3ipc_command_reply_free(reply);

    J4statusPluginContext *context;

    context = g_new0(J4statusPluginContext, 1);
    context->core = core;
    context->connection = connection;

    g_signal_connect(context->connection, "window", G_CALLBACK(_j4status_i3focus_window_callback), context);
    g_signal_connect(context->connection, "workspace", G_CALLBACK(_j4status_i3focus_workspace_callback), context);

    return context;
}

static void
_j4status_i3focus_uninit(J4statusPluginContext *context)
{
    g_list_free_full(context->sections, _j4status_i3focus_section_free);

    g_object_unref(context->connection);

    g_free(context);
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_i3focus_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_i3focus_uninit);
}
