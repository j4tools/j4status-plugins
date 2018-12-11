/*
 * j4status-plugins - A collection of plugins for j4status
 *
 * Copyright © 2014-2017 Quentin "Sardem FF7" Glidic
 *
 * This file is part of j4status-plugins.
 *
 * j4status-plugins is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * j4status-plugins is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with j4status-plugins. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>

#include <j4status-plugin-input.h>

#include <i3ipc-glib/i3ipc-glib.h>

typedef struct {
    J4statusPluginContext *context;
    J4statusSection *section;
    gchar *name;
    gulong id;
    gsize i;
} J4statusI3focusSection;

struct _J4statusPluginContext {
    J4statusCoreInterface *core;
    i3ipcConnection *connection;
    gboolean focus_only;
    gboolean tabs_mode;
    guint64 max_width;
    guint64 max_total_width;
    GQueue *sections;
    J4statusI3focusSection *focus;
};


static void
_j4status_i3focus_section_free(gpointer data)
{
    J4statusI3focusSection *section = data;

    j4status_section_free(section->section);

    if ( section == section->context->focus )
        section->context->focus = NULL;

    g_slice_free(J4statusI3focusSection, section);
}

static void
_j4status_i3focus_section_callback(G_GNUC_UNUSED J4statusSection *section_, G_GNUC_UNUSED const gchar *event_id, gpointer user_data)
{
    J4statusI3focusSection *section = user_data;
    gchar command[60];
    g_snprintf(command, sizeof(command), "[con_id=\"%lu\"] focus", section->id);
    i3ipc_connection_command(section->context->connection, command, NULL);
}

static gint
_j4status_i3focus_section_search(gconstpointer a, gconstpointer b)
{
    const J4statusI3focusSection *section = a;
    i3ipcCon *con = (i3ipcCon *) b;
    gulong id;
    g_object_get(con, "id", &id, NULL);

    return ( section->id == id ) ? 0 : -1;
}


static void
_j4status_i3focus_section_set_colour(J4statusI3focusSection *section)
{
    guint8 value;
    if ( section == section->context->focus )
        value = 0xff;
    else if ( ( section->i % 2 ) == 0 )
        value = 0x88;
    else
        value = 0xaa;

    J4statusColour colour = {
        .set = TRUE,
        .red = value,
        .green = value,
        .blue = value,
        .alpha = 0xff
    };

    if ( section->context->tabs_mode )
    {
        colour.red = colour.green = colour.blue = 0xff - value;
        j4status_section_set_background_colour(section->section, colour);
    }
    else
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
_j4status_i3focus_section_set_value(gpointer data, gconstpointer user_data)
{
    J4statusI3focusSection *section = data;
    guint64 max_width = section->context->max_width;
    const gchar *new_name = user_data;
    if ( new_name != NULL )
    {
        g_free(section->name);

        const gchar *e;
        if ( g_utf8_validate(new_name, -1, &e) )
            section->name = g_strdup(new_name);
        else
            section->name = g_strndup(new_name, e - new_name);
    }
    j4status_section_set_value(section->section, ( max_width != 0 ) ? g_utf8_substring(section->name, 0, max_width) : g_strdup(section->name));
}

static void
_j4status_i3focus_section_new(J4statusPluginContext *context, i3ipcCon *window, gsize i)
{
    gulong id;
    gchar id_str[20];

    g_object_get(window, "id", &id, NULL);
    g_snprintf(id_str, sizeof(id_str), "%lu", id);

    J4statusI3focusSection *section = g_slice_new0(J4statusI3focusSection);
    section->context = context;
    section->id = id;
    section->i = i;

    section->section = j4status_section_new(context->core);
    j4status_section_set_name(section->section, "i3focus");
    j4status_section_set_instance(section->section, id_str);
    j4status_section_set_action_callback(section->section, _j4status_i3focus_section_callback, section);
    if ( ! j4status_section_insert(section->section) )
        _j4status_i3focus_section_free(section);
    else
    {
        section->name = g_strdup("");
        _j4status_i3focus_section_set_value(section, i3ipc_con_get_name(window));
        _j4status_i3focus_section_set_colour(section);

        g_queue_push_head(context->sections, section);
    }
}

static void
_j4status_i3focus_window_callback(G_GNUC_UNUSED GObject *object, i3ipcWindowEvent *event, gpointer user_data)
{
    J4statusPluginContext *context = user_data;
    GList *section_;

    if ( g_strcmp0(event->change, "focus") == 0 )
    {
        if ( context->focus_only )
        {
            if ( context->focus != NULL )
            {
                g_queue_pop_head(context->sections);
                _j4status_i3focus_section_free(context->focus);
                context->focus = NULL;
            }
            _j4status_i3focus_section_new(context, event->container, 1);
        }
        section_ = g_queue_find_custom(context->sections, event->container, _j4status_i3focus_section_search);
        if ( section_ == NULL )
            return;
        _j4status_i3focus_section_set_focus(section_->data);
    }
    else if ( context->focus_only )
        return;
    else if ( g_strcmp0(event->change, "title") == 0 )
    {
        section_ = g_queue_find_custom(context->sections, event->container, _j4status_i3focus_section_search);
        if ( section_ == NULL )
            return;
        J4statusI3focusSection *section = section_->data;
        _j4status_i3focus_section_set_value(section, i3ipc_con_get_name(event->container));
    }
    else if ( g_strcmp0(event->change, "new") == 0 )
    {
        gsize length = g_queue_get_length(context->sections);
        if ( context->max_total_width > 0 )
            context->max_width = context->max_total_width / ( length + 1 );
        g_queue_foreach(context->sections, (GFunc) _j4status_i3focus_section_set_value, NULL);
        _j4status_i3focus_section_new(context, event->container, g_queue_get_length(context->sections));
    }
    else if ( g_strcmp0(event->change, "close") == 0 )
    {
        section_ = g_queue_find_custom(context->sections, event->container, _j4status_i3focus_section_search);
        if ( section_ == NULL )
            return;
        J4statusI3focusSection *section = section_->data;
        g_queue_delete_link(context->sections, section_);
        _j4status_i3focus_section_free(section);
        if ( ( context->max_total_width > 0 ) && ( ! g_queue_is_empty(context->sections) ) )
        {
            context->max_width = context->max_total_width / g_queue_get_length(context->sections);
            g_queue_foreach(context->sections, (GFunc) _j4status_i3focus_section_set_value, NULL);
        }
    }
}


static void
_j4status_i3focus_workspace_callback(G_GNUC_UNUSED GObject *object, i3ipcWorkspaceEvent *event, gpointer user_data)
{
    J4statusPluginContext *context = user_data;

    if ( g_strcmp0(event->change, "focus") == 0 )
    {
        GList *windows = i3ipc_con_leaves(event->current), *window_;

        g_queue_foreach(context->sections, (GFunc) _j4status_i3focus_section_free, NULL);
        g_queue_clear(context->sections);
        context->focus = NULL;

        if ( ( context->max_total_width > 0 ) && ( windows != NULL ) )
            context->max_width = context->max_total_width / g_list_length(windows);
        gsize i = 0;
        for ( window_ = g_list_last(windows) ; window_ != NULL ; window_ = g_list_previous(window_) )
            _j4status_i3focus_section_new(context, window_->data, i++);
        g_list_free(windows);
    }
}

static void _j4status_i3focus_uninit(J4statusPluginContext *context);

static J4statusPluginContext *
_j4status_i3focus_init(J4statusCoreInterface *core)
{
    GKeyFile *key_file;
    gboolean focus_only = TRUE;
    i3ipcConnection *connection;
    i3ipcCommandReply *reply;
    GError *error = NULL;

    key_file = j4status_config_get_key_file("i3focus");
    if ( key_file != NULL )
    {
        gboolean b;
        b = g_key_file_get_boolean(key_file, "i3focus", "FocusOnly", &error);
        if ( error == NULL )
            focus_only = b;
        g_clear_error(&error);
    }

    connection = i3ipc_connection_new(g_getenv("I3SOCK"), &error);
    if ( connection == NULL )
    {
        g_warning("Couldn't connection to i3: %s", error->message);
        g_clear_error(&error);
        return NULL;
    }

    i3ipcEvent events = I3IPC_EVENT_WINDOW;
    if ( ! focus_only )
        events |= I3IPC_EVENT_WORKSPACE;

    reply = i3ipc_connection_subscribe(connection, events, &error);
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
    context->focus_only = focus_only;

    context->sections = g_queue_new();

    g_signal_connect(context->connection, "window", G_CALLBACK(_j4status_i3focus_window_callback), context);
    if ( ! context->focus_only )
        g_signal_connect(context->connection, "workspace", G_CALLBACK(_j4status_i3focus_workspace_callback), context);

    if ( key_file != NULL )
    {
        context->tabs_mode = g_key_file_get_boolean(key_file, "i3focus", "TabsMode", NULL);
        context->max_total_width = g_key_file_get_uint64(key_file, "i3focus", "MaxTotalWidth", NULL);
        if ( context->max_total_width == 0 )
            context->max_width = g_key_file_get_uint64(key_file, "i3focus", "MaxWidth", NULL);
    }

    return context;
}

static void
_j4status_i3focus_uninit(J4statusPluginContext *context)
{
    g_queue_free_full(context->sections, _j4status_i3focus_section_free);

    g_object_unref(context->connection);

    g_free(context);
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_i3focus_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_i3focus_uninit);
}
