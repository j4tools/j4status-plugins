/*
 * cpu - j4status plugin for CPU usage fetching
 *
 * Copyright 2014 Mihail "Tomsod" Sh. <tomsod-m@ya.ru>
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
#endif // HAVE_CONFIG_H

#include <glib.h>
#include <j4status-plugin-input.h>

#include <stdlib.h> // strtoul()
#include <string.h> // memcmp(), memset()
#include <unistd.h> // access()

/// Number of (used) entries for "cpu" line in PROC_STAT
#define NUM_ENTRIES 8

/// implementation of J4statusPluginContext
struct _J4statusPluginContext
{
    J4statusSection *section;
    gboolean started;
    guint period;
    gulong time1[NUM_ENTRIES], time2[NUM_ENTRIES];
    gulong *time;
    union
      {
        gulong *old_time;
        gulong *new_time;
      };
};

/// /proc/stat "cpu" line entries
enum J4statusCPUStatEntry
{
    ENTRY_USER,
    ENTRY_NICE,
    ENTRY_SYSTEM,
    ENTRY_IDLE, // this is what we need
    ENTRY_IOWAIT, // also counted towards idle
    ENTRY_IRQ,
    ENTRY_SOFTIRQ,
    ENTRY_STEAL,
    ENTRY_GUEST, // unused
    ENTRY_GUEST_NICE, // not used either
};

/// the file itself
#define PROC_STAT_STR "/proc/stat"
const gchar PROC_STAT[] = PROC_STAT_STR;



/**
 * Gathers CPU load metrics from /proc/stat
 * Returns FALSE on error
 * Called on start to gather initial numbers
 * and each update for comparsion
 */
static gboolean
_j4status_cpu_parse_load(gulong time[])
{
    GIOChannel *pstat = g_io_channel_new_file(PROC_STAT, "r", NULL);
    if (!pstat)
      {
        g_warning("Could not open " PROC_STAT_STR " file");
        return FALSE;
      }
    gchar *line;
    GIOStatus read = g_io_channel_read_line(pstat, &line, NULL, NULL, NULL);
    g_io_channel_unref(pstat);
    if (read != G_IO_STATUS_NORMAL)
      {
        g_warning("Error reading " PROC_STAT_STR);
        g_free(line);
        return FALSE;
      }

    // I don't think it's in the specifications, but it's pretty much standard
    // to start the file with "cpu" entry
    if (memcmp(line, "cpu ", 4) != 0)
      {
        g_critical("cpu is not in the first line!");
        g_free(line);
        return FALSE;
      }
    gchar *marker = line + 4;
    for (guint idx = 0; idx < NUM_ENTRIES && *marker; idx++)
        time[idx] = strtoul(marker, &marker, 0);
    g_free(line);
    return TRUE;
}

/**
 * GSourceFunc instance
 * Called every "period" seconds
 * Load percentage is calculated from diff between new & old metrics
 */
static gboolean
_j4status_cpu_update(gpointer user_data)
{
    J4statusPluginContext *context = user_data;
    if (!context->started) return G_SOURCE_REMOVE;
    if (!_j4status_cpu_parse_load(context->new_time))
      {
        j4status_section_set_state(context->section, J4STATUS_STATE_BAD);
        return G_SOURCE_CONTINUE;
      }
  {
    register gulong *swap = context->new_time;
    context->old_time = context->time;
    context->time = swap;
  }
    gulong busy = 0, idle = 0;
    // overflows are okay here
    for (guint idx = 0; idx < NUM_ENTRIES; idx++)
        switch (idx)
          {
            case ENTRY_USER:
            case ENTRY_NICE:
            case ENTRY_SYSTEM:
            case ENTRY_IRQ:
            case ENTRY_SOFTIRQ:
            case ENTRY_STEAL:
                busy += context->time[idx] - context->old_time[idx];
                break;
            case ENTRY_IDLE:
            case ENTRY_IOWAIT:
                idle += context->time[idx] - context->old_time[idx];
                break;
            default:
                continue;
          }
    gdouble load = 100.0 * busy / MAX(idle + busy, 1);
    j4status_section_set_state(context->section,
                    load < 50.0 ? J4STATUS_STATE_NO_STATE :
                    load > 90.0 ? J4STATUS_STATE_BAD : J4STATUS_STATE_AVERAGE);
    j4status_section_set_value(context->section, g_strdup_printf("%04.1f%%",
                                                                 load));
    return G_SOURCE_CONTINUE;
}



/**
 * J4statusPluginInitFunc instance
 * Should be called on startup
 * Does preliminary checks, initialization, config etc.
 */
static J4statusPluginContext *
_j4status_cpu_init(J4statusCoreInterface *core)
{
    const gchar CPU_LOAD[] = "CPULoad";

    if (access(PROC_STAT, R_OK) < 0)
      {
        g_critical("Could not find " PROC_STAT_STR "; aborting");
        return NULL;
      }
    GKeyFile *key_file = j4status_config_get_key_file(CPU_LOAD);
    gint period = 0;
    if (key_file)
      {
        // Lower update frequency also smoothes load metric diffs,
        // so it may be desirable
        period = g_key_file_get_integer(key_file, CPU_LOAD, "Frequency", NULL);
        g_key_file_free(key_file);
      }

    J4statusSection *section = j4status_section_new(core);
    j4status_section_set_name(section, "cpu");
    if (!j4status_section_insert(section))
      {
        j4status_section_free(section);
        return NULL;
      }

    J4statusPluginContext *context = g_new(J4statusPluginContext, 1);
    context->section = section;
    context->started = FALSE;
    context->period = MAX(period, 1);
    return context;
}

/**
 * J4statusPluginSimpleFunc instance
 * Inits the metrics arrays and launches update timer
 */
static void
_j4status_cpu_start(J4statusPluginContext *context)
{
    if (context->started) return;
    context->time = context->time1;
    context->old_time = context->time2;
    // Blanking is only for the case when "cpu" line has too few entries
    memset(&context->time1, 0, NUM_ENTRIES);
    memset(&context->time2, 0, NUM_ENTRIES);
    if (!_j4status_cpu_parse_load(context->time)) return;
    context->started = TRUE;
    j4status_section_set_state(context->section, J4STATUS_STATE_UNAVAILABLE);
    j4status_section_set_value(context->section, g_strdup("....."));
    g_timeout_add_seconds(context->period, &_j4status_cpu_update, context);
}

/**
 * J4statusPluginSimpleFunc instance
 * The evil twin of start()
 */
static void
_j4status_cpu_stop(J4statusPluginContext *context)
{
    context->started = FALSE;
}

/**
 * J4statusPluginSimpleFunc instance
 * Called on exit, frees memory
 */
static void
_j4status_cpu_uninit(J4statusPluginContext *context)
{
    if (context->started) _j4status_cpu_stop(context);
    j4status_section_free(context->section);
    g_free(context);
}

/**
 * The exported function
 * Registers plugin callbacks
 */
void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface,
                                                         _j4status_cpu_init);
    libj4status_input_plugin_interface_add_start_callback(interface,
                                                          _j4status_cpu_start);
    libj4status_input_plugin_interface_add_stop_callback(interface,
                                                         _j4status_cpu_stop);
    libj4status_input_plugin_interface_add_uninit_callback(interface,
                                                         _j4status_cpu_uninit);
}
