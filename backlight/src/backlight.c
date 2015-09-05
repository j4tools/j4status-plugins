#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <glib.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/inotify.h>

#include <j4status-plugin-input.h>

struct _J4statusPluginContext {
    J4statusSection *section;
    GIOChannel *inotify_channel;
    gchar* brightness_path;
    gint64 max_brightness;
    pthread_t thread;
    int last_value;
    int fd;
    int wd;
};

#define J4STATUS_BACKLIGHT_PATH "/sys/class/backlight"
#define J4STATUS_BACKLIGHT_BUFF_SIZE (sizeof(struct inotify_event) * 1024)

static gboolean _j4status_backlight_update(gpointer user_data);

static void
_j4status_backlight_listen(J4statusPluginContext* context)
{
    int fd = inotify_init();
    context->fd = fd;
    if (fd < 0) {
        g_warning("Error initializing inotify\n");
        return;
    }

    int wd = inotify_add_watch(fd, context->brightness_path, IN_MODIFY);
    context->wd = wd;
    if (wd < 0) {
        g_warning("Error initializing watch on target: %s\n",
            context->brightness_path);
        return;
    }

    gchar buff[J4STATUS_BACKLIGHT_BUFF_SIZE] = {0};
    while (1) {
        // blocks until there is an event
        read(fd, buff, J4STATUS_BACKLIGHT_BUFF_SIZE);

        g_idle_add(_j4status_backlight_update, context);
    }
}

static gint64
_j4status_backlight_get_brightness(gchar* filename)
{
    gchar* contents;
    if (!g_file_get_contents(filename, &contents, NULL, NULL)) {
        g_warning("Can't read brightness from file: %s\n", filename);
        return 0;
    }

    gint64 brightness = g_ascii_strtoll(contents, NULL, 10);
    g_free(contents);

    return brightness;
}

static gboolean
_j4status_backlight_update(gpointer user_data)
{
    J4statusPluginContext* context = user_data;

    gint64 brightness = _j4status_backlight_get_brightness(
        context->brightness_path);
    gint64 max_brightness = context->max_brightness;

    int backlight_percent = 100.0 * brightness / max_brightness + 0.5;

    if (context->last_value == backlight_percent) {
        return FALSE;
    }

    gchar* value = g_strdup_printf("%3d%%", backlight_percent);

    j4status_section_set_state(context->section, J4STATUS_STATE_GOOD);
    j4status_section_set_value(context->section, value);

    context->last_value = backlight_percent;

    return FALSE;
}

static J4statusPluginContext *
_j4status_backlight_init(J4statusCoreInterface *core)
{
    const gchar* BACKLIGHT = "Backlight";
    GKeyFile* key_file = j4status_config_get_key_file(BACKLIGHT);
    gchar* backend;
    if (key_file) {
        backend = g_key_file_get_string(key_file, BACKLIGHT, "Backend", NULL);
        g_key_file_free(key_file);
    }
    else {
        backend = g_strdup("intel_backlight");
    }

    J4statusSection *section = j4status_section_new(core);
    j4status_section_set_name(section, "backlight");
    if (!j4status_section_insert(section)) {
        j4status_section_free(section);
        return NULL;
    }

    J4statusPluginContext *context = g_new(J4statusPluginContext, 1);
    context->section = section;

    context->brightness_path = g_build_filename(
        J4STATUS_BACKLIGHT_PATH, backend, "brightness", NULL);
    gchar* max_brightness_path = g_build_filename(
        J4STATUS_BACKLIGHT_PATH, backend, "max_brightness", NULL);

    context->max_brightness = _j4status_backlight_get_brightness(
        max_brightness_path);

    g_free(max_brightness_path);

    return context;
}

static void
_j4status_backlight_uninit(J4statusPluginContext *context)
{
    g_free(context->section);
    g_free(context->brightness_path);
    g_free(context);
}

static void
_j4status_backlight_start(J4statusPluginContext *context)
{
    _j4status_backlight_update(context);

    pthread_create(&context->thread, NULL,
        (void*) &_j4status_backlight_listen, context);
}

static void
_j4status_backlight_stop(J4statusPluginContext *context)
{
    pthread_cancel(context->thread);
    pthread_join(context->thread, NULL);

    inotify_rm_watch(context->fd, context->wd);
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_backlight_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_backlight_uninit);

    libj4status_input_plugin_interface_add_start_callback(interface, _j4status_backlight_start);
    libj4status_input_plugin_interface_add_stop_callback(interface, _j4status_backlight_stop);
}
