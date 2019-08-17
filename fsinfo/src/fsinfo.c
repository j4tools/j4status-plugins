/*
 * fsinfo - j4status plugin for displaying filesystem statistics
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

#include <blkid.h> // blkid_evaluate_tag(), blkid_cache
#include <unistd.h> // access()
#include <mntent.h> // setmntent(), struct mntent, getmntent(), endmntent()
#include <sys/statvfs.h> // statvfs(), struct statvfs

/// implementation of J4statusPluginContext
struct _J4statusPluginContext
{
    GSList *sections;
    gboolean started;
    guint period;
    gchar *not_found;
    gchar *unmounted;
    const gchar *mtab;
    blkid_cache cache;
};

/// derived from J4statusSection
typedef struct
{
    J4statusSection *section;
    enum
      {
        ID_UUID,
        ID_LABEL,
        ID_DEVICE,
        ID_MOUNTPOINT
      } id_t;
    gchar *id;
    gchar *device;
    gulong fsid;
    gchar *path;
    J4statusFormatString *format;
    guint64 used_tokens;
} J4statusFSInfoSection;

/// possible mtab file location
const gchar MTAB[] = "/etc/mtab";
/// alternative mtab file location
const gchar PROC_MOUNTS[] = "/proc/mounts";
//TODO: organize these things into an array if they keep growing

/// indices for _j4status_fsinfo_tokens[]
enum J4statusFSInfoToken
{
    TOKEN_AVAILABLE,
    TOKEN_FREE,
    TOKEN_USED,
    TOKEN_TOTAL,
    TOKEN_AVAILABLE_RATIO,
    TOKEN_FREE_RATIO,
    TOKEN_USED_RATIO,

    TOTAL_TOKEN_COUNT
};

/// used in j4status_format_string_parse()
static const gchar *const _j4status_fsinfo_tokens[] =
{
    [TOKEN_AVAILABLE]       = "avail",
    [TOKEN_FREE]            = "free",
    [TOKEN_USED]            = "used",
    [TOKEN_TOTAL]           = "total", // actually avail + used
    [TOKEN_AVAILABLE_RATIO] = "p_avail",
    [TOKEN_FREE_RATIO]      = "p_free", // ratio to "true" total
    [TOKEN_USED_RATIO]      = "p_used",
};



/**
 * Tries to find device by provided credentials
 * Returns successfulness of the attempt
 * Called only at section_update
 */
static gboolean
_j4status_fsinfo_section_locate_device(J4statusFSInfoSection *section,
                                       blkid_cache cache)
{
    g_free(section->device);
    section->device = NULL;
    switch (section->id_t)
      {
        case ID_UUID:
            section->device = blkid_evaluate_tag("UUID", section->id, &cache);
            break;
        case ID_LABEL:
            section->device = blkid_evaluate_tag("LABEL", section->id, &cache);
            break;
        case ID_DEVICE:
            if (access(section->id, F_OK) >= 0)
                section->device = g_strdup(section->id);
            break;
        case ID_MOUNTPOINT:
            section->path = g_strdup(section->id);
            return TRUE;
        default:
            // not supposed to happen!
            return FALSE;
      }
    return section->device != NULL;
}

/**
 * J4statusFormatStringReplaceCallback instance
 * Strings are in user_data[]
 */
static GVariant *
_j4status_fsinfo_format_callback(const gchar *token, guint64 value,
                                 gconstpointer user_data)
{
    GVariant *const *fdata = user_data;
    if (value < TOTAL_TOKEN_COUNT && fdata[value])
        return g_variant_ref(fdata[value]);
    else
        return NULL;
}

/**
 * A part of section_update that gets re-used several times
 * Reports an error during update and returns
 */
#define UPDATE_ERROR(error) do                                                \
{                                                                             \
    j4status_section_set_state(section->section, J4STATUS_STATE_BAD);         \
    j4status_section_set_value(section->section, g_strdup("Error"));          \
    g_warning(error);                                                         \
    return;                                                                   \
} while (0)

/**
 * GFunc instance
 * Called every "period" seconds
 * Locates the device if necessary
 * then calculates token values as needed
 * Unmounted/unplugged devices are also reported
 * (Referenced above as section_update)
 */
static void
_j4status_fsinfo_section_update(gpointer data, gpointer user_data)
{
    const gchar ESTATVFS[] = "Could not get filesystem stats";

    J4statusFSInfoSection *section = data;
    J4statusPluginContext *context = user_data;
    if ((!section->device || access(section->device, F_OK) < 0)
       && !_j4status_fsinfo_section_locate_device(section, context->cache))
      {
        j4status_section_set_state(section->section,
                                   J4STATUS_STATE_UNAVAILABLE);
        j4status_section_set_value(section->section,
                                   g_strdup(context->not_found));
        return;
      }

    struct statvfs stats;
    if (section->path)
      {
        if (statvfs(section->path, &stats) < 0)
          {
            //TODO: account for the possibility that device was unmounted
            // & mount point deleted since last check
            UPDATE_ERROR(ESTATVFS);
          }
        if (stats.f_fsid != section->fsid)
          {
            if (section->id_t == ID_MOUNTPOINT)
              {
                section->fsid = stats.f_fsid;
              }
            else
              {
                g_free(section->path);
                section->path = NULL;
              }
          }
      }
    if (!section->path)
      {
        FILE *mtab = setmntent(context->mtab, "r");
        if (!mtab)
            UPDATE_ERROR("Could not open mtab");
        struct mntent *mnt;
        while ((mnt = getmntent(mtab)) != NULL)
            if (g_strcmp0(section->device, mnt->mnt_fsname) == 0)
              {
                section->path = g_strdup(mnt->mnt_dir);
                break;
              }
        endmntent(mtab);
        if (section->path)
          {
            if (statvfs(section->path, &stats) < 0)
                UPDATE_ERROR(ESTATVFS);
            section->fsid = stats.f_fsid;
          }
      }
    if (!section->path)
      {
        j4status_section_set_state(section->section, J4STATUS_STATE_NO_STATE);
        j4status_section_set_value(section->section,
                                   g_strdup(context->unmounted));
        return;
      }

    GVariant *fdata[TOTAL_TOKEN_COUNT] = { NULL };
    fsblkcnt_t used = stats.f_blocks - stats.f_bfree;
    // we don't include reserved blocks in total count
    // because (a) it wouldn't make sense to display static parameter
    // in dynamic statusline
    // and (b) all the other cool guys do it (like df)
    // although p_free does use real total in calculation,
    // since free includes privilegied blocks itself
    fsblkcnt_t adjusted_total = used + stats.f_bavail;
    if (section->used_tokens & 1 << TOKEN_AVAILABLE)
        fdata[TOKEN_AVAILABLE] = g_variant_new_uint64(stats.f_bavail * stats.f_bsize);
    if (section->used_tokens & 1 << TOKEN_FREE)
        fdata[TOKEN_FREE] = g_variant_new_uint64(stats.f_bfree * stats.f_bsize);
    if (section->used_tokens & 1 << TOKEN_USED)
        fdata[TOKEN_USED] = g_variant_new_uint64(used * stats.f_bsize);
    if (section->used_tokens & 1 << TOKEN_TOTAL)
        fdata[TOKEN_TOTAL] = g_variant_new_uint64(adjusted_total * stats.f_bsize);
    if (section->used_tokens & 1 << TOKEN_AVAILABLE_RATIO)
        fdata[TOKEN_AVAILABLE_RATIO] = g_variant_new_double(100.0 * stats.f_bavail / adjusted_total);
    if (section->used_tokens & 1 << TOKEN_FREE_RATIO)
        fdata[TOKEN_FREE_RATIO] = g_variant_new_double(100.0 * stats.f_bfree / stats.f_blocks);
    if (section->used_tokens & 1 << TOKEN_USED_RATIO)
        fdata[TOKEN_USED_RATIO] = g_variant_new_double(100.0 * used / adjusted_total);

    //TODO: configurable coeff?
    j4status_section_set_state(section->section, used < adjusted_total * 3/4 ?
                                 J4STATUS_STATE_GOOD : J4STATUS_STATE_AVERAGE);
    j4status_section_set_value(section->section,
                               j4status_format_string_replace(section->format,
                                   &_j4status_fsinfo_format_callback, &fdata));
    for (guint idx = 0; idx < TOTAL_TOKEN_COUNT; idx++)
      {
        if (fdata[idx])
            g_variant_unref(fdata[idx]);
      }
}

/**
 * GSourceFunc instance
 * Called on plugin start
 * and every "period" seconds thereafter
 */
static gboolean
_j4status_fsinfo_update(gpointer user_data)
{
    J4statusPluginContext *context = user_data;
    if (!context->started) return G_SOURCE_REMOVE;
    g_slist_foreach(context->sections, &_j4status_fsinfo_section_update,
                    context);
    return G_SOURCE_CONTINUE;
}

/**
 * GDestroyNotify instance
 * Called on freeing section list
 * or if failed to insert the section
 */
static void
_j4status_fsinfo_section_free(gpointer data)
{
    J4statusFSInfoSection *section = data;
    j4status_section_free(section->section);
    j4status_format_string_unref(section->format);
    g_free(section->id);
    g_free(section->device);
    g_free(section->path);
    g_free(section);
}



/**
 * J4statusPluginInitFunc instance
 * Checks for mtab, reads config, parses formats, inits sections
 * Device existence is NOT checked here
 */
static J4statusPluginContext *
_j4status_fsinfo_init(J4statusCoreInterface *core)
{
    const gchar FILESYSTEM[] = "Filesystem";
    const gchar FORMAT_DEFAULT[] = "${avail(b.1)}b (${p_avail(f04.1)}%)";

    GKeyFile *key_file = j4status_config_get_key_file(FILESYSTEM);
    if (!key_file) return NULL;
    const gchar *mtab;
    if (access(PROC_MOUNTS, R_OK) >= 0)
        mtab = PROC_MOUNTS;
    else if (access(MTAB, R_OK) >= 0)
        mtab = MTAB;
    else
      {
        g_critical("Could not find mtab; aborting");
        return NULL;
      }
    gchar **names = g_key_file_get_string_list(key_file, FILESYSTEM, "Names",
                                               NULL, NULL);
    if (!names)
      {
        g_key_file_free(key_file);
        return NULL;
      }
    gint period = g_key_file_get_integer(key_file, FILESYSTEM, // positive only
                                         "Frequency", NULL);
    gchar *not_found = g_key_file_get_locale_string(key_file, FILESYSTEM,
                                                    "NotFound", NULL, NULL);
    gchar *unmounted = g_key_file_get_locale_string(key_file, FILESYSTEM,
                                                    "Unmounted", NULL, NULL);
    blkid_cache cache;
    gboolean blkid = blkid_get_cache(&cache, NULL) >= 0;
    if (!blkid)
        g_warning("Could not open blkid cache; UUID/label resolving disabled");

    GSList *sections = NULL;
    for (guint idx = 0; names[idx]; idx++)
      {
        gchar *group = g_strjoin(" ", FILESYSTEM, names[idx], NULL);
        if (!g_key_file_has_group(key_file, group))
          {
            g_free(group);
            continue;
          }

        J4statusFSInfoSection *section = g_new(J4statusFSInfoSection, 1);
        if (blkid)
          {
            section->id = g_key_file_get_string(key_file, group, "UUID", NULL);
            if (section->id)
                section->id_t = ID_UUID;
            else
              {
                section->id = g_key_file_get_string(key_file, group, "Label",
                                                    NULL);
                if (section->id)
                    section->id_t = ID_LABEL;
              }
          }
        if (!blkid || !section->id)
          {
            section->id = g_key_file_get_string(key_file, group, "Device",
                                                NULL);
            if (section->id)
                section->id_t = ID_DEVICE;
            else
              {
                section->id = g_key_file_get_string(key_file, group, "Mountpoint",
                                                    NULL);
                if (section->id)
                  {
                    section->id_t = ID_MOUNTPOINT;
                  }
              }
          }
        if (!section->id)
          {
            g_warning("Could not locate device %s", names[idx]);
            g_free(section);
            g_free(group);
            continue;
          }

        gchar *format = g_key_file_get_locale_string(key_file, group,
                                                     "Format", NULL, NULL);
        g_free(group);

        section->device = NULL;
        section->path = NULL;
        section->format = j4status_format_string_parse(format,
                                    _j4status_fsinfo_tokens, TOTAL_TOKEN_COUNT,
                                        FORMAT_DEFAULT, &section->used_tokens);
        section->section = j4status_section_new(core);
        j4status_section_set_name(section->section, "fsinfo");
        j4status_section_set_instance(section->section, names[idx]);
        //TODO: actions
        if (!j4status_section_insert(section->section))
            _j4status_fsinfo_section_free(section);
        else
            sections = g_slist_prepend(sections, section);
      }
    g_strfreev(names);
    if (!sections) return NULL;

    J4statusPluginContext *context = g_new(J4statusPluginContext, 1);
    context->sections = sections;
    context->started = FALSE;
    context->period = period > 0 ? period : 10;
    context->not_found = not_found ? not_found : g_strdup("Not found");
    context->unmounted = unmounted ? unmounted : g_strdup("Unmounted");
    context->cache = blkid ? cache : NULL;
    context->mtab = mtab;
    return context;
  }

/**
 * J4statusPluginSimpleFunc instance
 * Starts the update timer
 */
static void
_j4status_fsinfo_start(J4statusPluginContext *context)
{
    if (context->started) return;
    context->started = TRUE;
    _j4status_fsinfo_update(context);
    g_timeout_add_seconds(context->period, &_j4status_fsinfo_update, context);
}

/**
 * J4statusPluginSimpleFunc instance
 * Stops the plugin
 */
static void
_j4status_fsinfo_stop(J4statusPluginContext *context)
{
    context->started = FALSE;
}

/**
 * J4statusPluginSimpleFunc instance
 * Cleans stuff up
 */
static void
_j4status_fsinfo_uninit(J4statusPluginContext *context)
{
    if (context->started) _j4status_fsinfo_stop(context);
    g_slist_free_full(context->sections, &_j4status_fsinfo_section_free);
    //TODO: drop cache dynamically
    blkid_put_cache(context->cache);
    g_free(context->unmounted);
    g_free(context->not_found);
    g_free(context);
}

/**
 * The exported function
 * Inserts callbacks
 */
void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface,
                                                        _j4status_fsinfo_init);
    libj4status_input_plugin_interface_add_start_callback(interface,
                                                       _j4status_fsinfo_start);
    libj4status_input_plugin_interface_add_stop_callback(interface,
                                                        _j4status_fsinfo_stop);
    libj4status_input_plugin_interface_add_uninit_callback(interface,
                                                      _j4status_fsinfo_uninit);
}
