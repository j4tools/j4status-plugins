#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <glib.h>
#include <glib/gprintf.h>
#include <sys/sysinfo.h>

#include <j4status-plugin-input.h>

#define TIME_SIZE 4095

struct _J4statusPluginContext {
    J4statusSection *section;
    guint period;
    guint timeout_id;
    guint good_threshold;
    guint bad_threshold;
};

static gboolean
_j4status_mem_update(gpointer user_data)
{
    J4statusPluginContext *context = user_data;

    double mem_percent = -1;

    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        unsigned long val, total = 0, available = 0;
        char key[21];
        while (fscanf(f, "%20[^:]: %lu%*[^\n]\n", key, &val) >= 2) {
            if (strcmp(key, "MemTotal") == 0)
                total = val;
            else if (strcmp(key, "MemAvailable") == 0) {
                available = val;
                mem_percent = 100.0 * (total - available) / total;
                break;
            }
        }
        fclose(f);
    }

    if (mem_percent == -1) {
        unsigned long used_ram;
        struct sysinfo meminfo;
        sysinfo(&meminfo);

        used_ram = meminfo.totalram - meminfo.freeram -
            meminfo.sharedram - meminfo.bufferram;
        mem_percent = 100.0 * used_ram / meminfo.totalram;
    }

    j4status_section_set_state(context->section,
        mem_percent < context->good_threshold ? J4STATUS_STATE_GOOD :
        mem_percent > context->bad_threshold ? J4STATUS_STATE_BAD :
            J4STATUS_STATE_AVERAGE);
    j4status_section_set_value(context->section,
        g_strdup_printf("%04.1f%%", mem_percent));

    return G_SOURCE_CONTINUE;
}

// static void _j4status_mem_uninit(J4statusPluginContext *context);

static J4statusPluginContext *
_j4status_mem_init(J4statusCoreInterface *core)
{
    const gchar MEM[]= "Memory";
    GKeyFile *key_file = j4status_config_get_key_file(MEM);
    guint period = 0;
    guint good_threshold = 0;
    guint bad_threshold = 0;
    if (key_file) {
        period = g_key_file_get_integer(key_file, MEM, "Frequency", NULL);
        good_threshold = g_key_file_get_integer(
            key_file, MEM, "GoodThreshold", NULL);
        bad_threshold = g_key_file_get_integer(
            key_file, MEM, "BadThreshold", NULL);
        g_key_file_free(key_file);
    }

    J4statusSection *section = j4status_section_new(core);
    j4status_section_set_name(section, "mem");
    if (!j4status_section_insert(section)) {
        j4status_section_free(section);
        return NULL;
    }

    J4statusPluginContext *context = g_new(J4statusPluginContext, 1);
    context->section = section;
    context->good_threshold = good_threshold > 0 ? good_threshold : 50;
    context->bad_threshold = bad_threshold > context->good_threshold ?
        bad_threshold : 90;
    context->period = MAX(period, 1);
    return context;
}

static void
_j4status_mem_uninit(J4statusPluginContext *context)
{
    g_free(context->section);
    g_free(context);
}

static void
_j4status_mem_start(J4statusPluginContext *context)
{
    _j4status_mem_update(context);
    context->timeout_id = g_timeout_add_seconds(
        context->period, _j4status_mem_update, context);
}

static void
_j4status_mem_stop(J4statusPluginContext *context)
{
    g_source_remove(context->timeout_id);
    context->timeout_id = 0;
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_mem_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_mem_uninit);

    libj4status_input_plugin_interface_add_start_callback(interface, _j4status_mem_start);
    libj4status_input_plugin_interface_add_stop_callback(interface, _j4status_mem_stop);
}
