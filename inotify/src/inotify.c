/*
 * j4status - Status line generator
 *
 * Copyright © 2012-2013 Quentin "Sardem FF7" Glidic
 *
 * This file is part of j4status.
 *
 * j4status is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * j4status is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with j4status. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <glib.h>
#include <gio/gio.h>

#include <pthread.h>
#include <sys/inotify.h>

#include <j4status-plugin-input.h>

struct _J4statusPluginContext {
    J4statusCoreInterface *core;
    GHashTable *sections;
    pthread_t thread;
    gchar* dir;
    int fd;
    int wd;
};

typedef struct {
    J4statusPluginContext *context;
    GFile* file;
    gint length;
    J4statusSection *section;
} J4statusInotifySection;

#define J4_STATUS_INOTIFY_BUFF_SIZE (sizeof(struct inotify_event) * 1024)


static void _j4status_inotify_get_event(int, J4statusPluginContext*);
static gboolean _j4status_inotify_section_update(gpointer);

static void _j4status_inotify_listen(J4statusPluginContext* context) {
    while (1) {
        _j4status_inotify_get_event(context->fd, context);
    }
}

static void _j4status_inotify_get_event(
    int fd, J4statusPluginContext* context)
{
    ssize_t len, i = 0;
    gchar buff[J4_STATUS_INOTIFY_BUFF_SIZE] = {0};

    len = read(fd, buff, J4_STATUS_INOTIFY_BUFF_SIZE);

    while (i < len) {
        struct inotify_event *event = (struct inotify_event*) &buff[i];

        J4statusInotifySection* section = g_hash_table_lookup(
            context->sections, event->name);

        if (section != NULL) {
            g_idle_add(_j4status_inotify_section_update, section);
        }

        i += sizeof(struct inotify_event) + event->len;
    }
}

static char* _j4status_inotify_read_file(GFile* file) {
    GFileInputStream* stream = g_file_read(file, NULL, NULL);

    if (stream == NULL) {
        return NULL;
    }

    GDataInputStream *data_stream;
    data_stream = g_data_input_stream_new(G_INPUT_STREAM(stream));
    g_object_unref(stream);
    gsize length;

    return g_data_input_stream_read_upto(
        data_stream, "\r\n", -1, &length, NULL, NULL);
}

static gboolean _j4status_inotify_section_update(gpointer data) {
    J4statusInotifySection* section = data;

    gchar* value = _j4status_inotify_read_file(section->file);

    gchar* formatted;
    gint len = section->length;
    if (section->length > 0) {
        formatted = g_strdup_printf("%*.*s", len, len, value);
        g_free(value);
    } else {
        formatted = value != NULL ? value : g_strdup("N/A");
    }

    j4status_section_set_value(section->section, formatted);
    return FALSE;
}

static gboolean
_j4status_inotify_section_set_state(gpointer data)
{
    J4statusInotifySection* section = data;
    j4status_section_set_state(section->section, J4STATUS_STATE_GOOD);
    return FALSE;
}

static void
_j4status_inotify_section_free(gpointer data)
{
    J4statusInotifySection* section = data;

    j4status_section_free(section->section);
    g_object_unref(section->file);

    g_free(section);
}

static J4statusInotifySection*
_j4status_inotify_section_create(
    J4statusPluginContext* context, char* file, gint length)
{
    J4statusInotifySection *section;
    section = g_new0(J4statusInotifySection, 1);

    char* filepath = g_build_filename(context->dir, file, NULL);
    section->file = g_file_new_for_path(filepath);
    section->context = context;
    section->length = length;
    section->section = j4status_section_new(context->core);

    j4status_section_set_name(section->section, "inotify");
    j4status_section_set_instance(section->section, g_strdup(file));
    j4status_section_set_label(section->section, g_strdup(file));
    g_idle_add(_j4status_inotify_section_update, section);
    g_idle_add(_j4status_inotify_section_set_state, section);

    if (!j4status_section_insert(section->section)) {
        _j4status_inotify_section_free(section);
        return NULL;
    }

    g_hash_table_insert(context->sections, g_strdup(file), section);
    return section;
}

static J4statusPluginContext *
_j4status_inotify_init(J4statusCoreInterface *core)
{
    GKeyFile *key_file = NULL;
    gchar *dir = NULL;
    const gchar* section = "Inotify";

    key_file = j4status_config_get_key_file(section);
    if (key_file == NULL) {
        g_warning("inotify: No Inotify section in config file, aborting");
        goto fail;
    }

    dir = g_key_file_get_string(key_file, section, "Dir", NULL);
    if (dir == NULL) {
        dir = g_build_filename(g_get_user_runtime_dir(),
            PACKAGE_NAME G_DIR_SEPARATOR_S "inotify", NULL);
        if ((!g_file_test(dir, G_FILE_TEST_IS_DIR)) &&
            (g_mkdir_with_parents(dir, 0755) < 0))
        {
            g_warning(
                "inotify: couldn't create the directory to monitor '%s'",
                dir);
            goto fail;
        }
    }
    g_message("dir: %s", dir);

    gchar **files;
    files = g_key_file_get_string_list(key_file, section, "Files", NULL, NULL);
    if (files == NULL) {
        g_warning("inotify: No Files set to monitor, aborting");
        goto fail;
    }

    gint *lengths;
    lengths = g_key_file_get_integer_list(key_file, section, "Lengths", NULL, NULL);

    g_key_file_free(key_file);

    int fd = inotify_init();
    if (fd < 0) {
        g_warning("inotify: error initializing inotify");
        goto fail;
    }

    int wd = inotify_add_watch(fd, dir, IN_MODIFY | IN_DELETE);
    if (wd < 0) {
        g_warning("inotify: couldn't monitor directory: %s", dir);
        goto fail;
    }

    J4statusPluginContext *context;
    context = g_new0(J4statusPluginContext, 1);
    context->core = core;
    context->sections = g_hash_table_new_full(g_str_hash, g_str_equal,
        g_free, _j4status_inotify_section_free);
    context->dir = dir;
    context->fd = fd;
    context->wd = wd;

    gchar **file;
    gint *length = lengths;
    for (file = files; *file != NULL; ++file) {
        gint sec_len = length != NULL ? *length : 0;

        _j4status_inotify_section_create(context, *file, sec_len);

        if (length != NULL)
            ++length;
    }
    g_strfreev(files);

    if (context->sections == NULL) {
        g_free(context);
        return NULL;
    }

    return context;

fail:
    if (key_file != NULL) {
        g_key_file_free(key_file);
    }
    return NULL;
}

static void
_j4status_inotify_uninit(J4statusPluginContext *context)
{
    g_hash_table_remove_all(context->sections);
    g_hash_table_unref(context->sections);
    g_free(context->dir);
    inotify_rm_watch(context->fd, context->wd);

    g_free(context);
}

static void
_j4status_inotify_start(J4statusPluginContext* context)
{
    pthread_create(&context->thread, NULL,
        (void*) &_j4status_inotify_listen, context);
}

static void
_j4status_inotify_stop(J4statusPluginContext* context)
{
    pthread_cancel(context->thread);
    pthread_join(context->thread, NULL);
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_inotify_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_inotify_uninit);
    libj4status_input_plugin_interface_add_start_callback(interface, _j4status_inotify_start);
    libj4status_input_plugin_interface_add_stop_callback(interface, _j4status_inotify_stop);
}
