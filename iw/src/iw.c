/*
 * iw - j4status plugin for displaying wireless connection data using
 * the Linux Wireless Extension API
 *
 * Copyright 2014-2019 Mihail "Tomsod" Sh. <tomsod-m@ya.ru>
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

#include <string.h> // memcpy()
#include <net/if.h> // struct ifreq, if_nametoindex()
#include <netinet/in.h> // struct in_addr
#include <iwlib.h> // various WE wrappers

/// implementation of J4statusPluginContext (w/o the _underscore)
struct _J4statusPluginContext
{
    GSList *sections;
    gboolean started;
    guint period;
    J4statusFormatString *format;
    guint64 used_tokens;
    char *unknown;
    gboolean hide_inactive;
};

/// derived from J4statusSection
typedef struct
{
    J4statusSection *section;
    gchar *interface;
} J4statusIWSection;

/// indices for _j4status_iw_tokens[]
enum J4statusIWToken
{
    TOKEN_IPV4,
    TOKEN_QUALITY,
    TOKEN_ESSID,
    TOKEN_BITRATE,

    TOTAL_TOKEN_COUNT
};

/// used in j4status_format_string_parse()
static const gchar * const _j4status_iw_tokens[] =
{
    [TOKEN_IPV4]    = "ip",
    [TOKEN_QUALITY] = "quality",
    [TOKEN_ESSID]   = "essid",
    [TOKEN_BITRATE] = "bitrate",
};

/// data for _j4status_iw_format_callback()
struct J4statusIWFormatData
{
    guint64 set_tokens;
    guint8 ipv4[4];
    gdouble quality;
    gchar *essid;
    guint32 bitrate;
};



/**
 * J4statusFormatStringReplaceCallback instance
 * Now returns various types of data instead of only strings
 */
static GVariant *
_j4status_iw_format_callback(G_GNUC_UNUSED const gchar *token, guint64 value,
                             gconstpointer user_data)
{
    const struct J4statusIWFormatData *fdata = user_data;

    if ((fdata->set_tokens & 1 << value) == 0)
        return NULL;

    switch (value)
      {
    case TOKEN_IPV4:
        return g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, fdata->ipv4, 4,
                                         sizeof(guint8));
    case TOKEN_QUALITY:
        return g_variant_new_double(fdata->quality);
    case TOKEN_ESSID:
        return g_variant_new_string(fdata->essid);
    case TOKEN_BITRATE:
        return g_variant_new_uint32(fdata->bitrate);
      }

    return NULL; // actually should not be reached
}

/**
 * GFunc instance
 * Called every "period" seconds for each section
 * Here data is gathered via WE (and more general) ioctl() calls
 * and put into "fdata" for further formatting
 */
static void
_j4status_iw_section_update(gpointer data, gpointer user_data)
{
    J4statusIWSection *section = data;
    J4statusPluginContext *context = user_data;
    int skfd = iw_sockets_open();
    if (skfd < 0)
      {
        g_warning("Could not open socket descriptor!");
        j4status_section_set_state(section->section,
                                   J4STATUS_STATE_BAD);
        j4status_section_set_value(section->section,
                                   g_strdup(context->unknown));
        return;
      }

    if (context->hide_inactive)
      {
        struct ifreq request;
        if (iw_get_ext(skfd, section->interface, SIOCGIFFLAGS, &request) >= 0
            && (request.ifr_flags & (IFF_UP | IFF_RUNNING))
               != (IFF_UP | IFF_RUNNING))
          {
            iw_sockets_close(skfd);
            j4status_section_set_value(section->section, NULL);
            return;
          }
      }

    struct J4statusIWFormatData fdata = {.set_tokens = 0};
    char essid[IW_ESSID_MAX_SIZE + 2];
  {
    iwrange range;
    iwstats stats;
    if (iw_get_range_info(skfd, section->interface, &range) < 0
       || iw_get_stats(skfd, section->interface, &stats, &range, TRUE)
       || stats.qual.updated & IW_QUAL_QUAL_INVALID)
      {
        j4status_section_set_state(section->section, J4STATUS_STATE_NO_STATE);
      }
    else
      {
        j4status_section_set_state(section->section,
                                   (stats.qual.qual > range.avg_qual.qual) ?
                                 J4STATUS_STATE_GOOD : J4STATUS_STATE_AVERAGE);
        if (context->used_tokens & 1 << TOKEN_QUALITY)
          {
            fdata.quality = 100.0 * stats.qual.qual / range.max_qual.qual;
            fdata.set_tokens |= 1 << TOKEN_QUALITY;
          }
      }
  }

    struct iwreq request;

    if (context->used_tokens & 1 << TOKEN_ESSID)
      {
        request.u.essid = (struct iw_point)
          {
            .pointer = essid,
            .length = IW_ESSID_MAX_SIZE + 2,
            .flags = 0,
          };
        if (iw_get_ext(skfd, section->interface, SIOCGIWESSID, &request) < 0
            || !request.u.essid.flags) // this should mean "no ESSID"
          {
            // will only raise alarm if ESSID was expected!
            j4status_section_set_state(section->section,
                                       J4STATUS_STATE_UNAVAILABLE);
            fdata.essid = context->unknown;
          }
        else
          {
            essid[request.u.essid.length] = '\0'; // just in case
            fdata.essid = essid;
          }
        fdata.set_tokens |= 1 << TOKEN_ESSID;
      }

    if (context->used_tokens & 1 << TOKEN_BITRATE
        && iw_get_ext(skfd, section->interface, SIOCGIWRATE, &request) >= 0)
      {
        fdata.bitrate = request.u.bitrate.value;
        fdata.set_tokens |= 1 << TOKEN_BITRATE;
      }

    if (context->used_tokens & 1 << TOKEN_IPV4)
      {
        request.u.ap_addr.sa_family = AF_INET;
        if (iw_get_ext(skfd, section->interface, SIOCGIFADDR, &request) < 0)
          {
            //TODO: IPv6
          }
        else
          {
            // IPv4 resides in bytes 2 to 5
            memcpy(fdata.ipv4, &request.u.ap_addr.sa_data[2], 4);
            fdata.set_tokens |= 1 << TOKEN_IPV4;
          }
      }
    iw_sockets_close(skfd);

    j4status_section_set_value(section->section,
                               j4status_format_string_replace(context->format,
                                       &_j4status_iw_format_callback, &fdata));
}

/**
 * GSourceFunc instance
 * Called every "period" seconds
 */
static gboolean
_j4status_iw_update(gpointer user_data)
{
    J4statusPluginContext *context = user_data;
    if (!context->started) return G_SOURCE_REMOVE;
    g_slist_foreach(context->sections, &_j4status_iw_section_update, context);
    return G_SOURCE_CONTINUE;
}



/**
 * GDestroyNotify instance
 * Called while freeing section list
 * and also when init goes awry
 */
static void
_j4status_iw_section_free(gpointer data)
{
    J4statusIWSection *section = data;
    j4status_section_free(section->section);
    g_free(section->interface);
    g_free(section);
}

/**
 * J4statusPluginInitFunc instance
 * Naturally, called on startup
 * Reads config and initializes data
 */
static J4statusPluginContext *
_j4status_iw_init(J4statusCoreInterface *core)
{
    const gchar WIRELESS[] = "Wireless";
    const gchar FORMAT_DEFAULT[] = "${ip[@.]}${ip:+ @ }${essid}${quality:+: }"
                                   "${quality(f.0)}${quality:+%}";

    GKeyFile *key_file = j4status_config_get_key_file(WIRELESS);
    if (!key_file)
      {
        g_warning("Configuration section not found, stopping");
        return NULL;
      }
    gchar **ifaces = g_key_file_get_string_list(key_file, WIRELESS,
                                                 "Interfaces", NULL, NULL);
    if (!ifaces)
      {
        g_warning("No interfaces specified, stopping");
        g_key_file_free(key_file);
        return NULL;
      }
    gchar *format = g_key_file_get_locale_string(key_file, WIRELESS,
                                                 "Format", NULL, NULL);
    gchar *unknown = g_key_file_get_locale_string(key_file, WIRELESS,
                                                 "Unknown", NULL, NULL);
    gint period = g_key_file_get_integer(key_file, WIRELESS, //must be positive
                                         "Frequency", NULL);
    gboolean hide_inactive = g_key_file_get_boolean(key_file, WIRELESS,
                                                    "HideInactive", NULL);
    g_key_file_free(key_file);

    GSList *sections = NULL;
    for (guint idx = 0; ifaces[idx]; idx++)
      {
        if (!if_nametoindex(ifaces[idx]))
          {
            g_warning("Interface %s seemingly not found, skipping",
                      ifaces[idx]);
            continue;
          }
        J4statusIWSection *section = g_new(J4statusIWSection, 1);
        section->interface = ifaces[idx];
        section->section = j4status_section_new(core);
        j4status_section_set_name(section->section, "iw");
        j4status_section_set_instance(section->section, section->interface);

        if (!j4status_section_insert(section->section))
          {
            g_warning("Failed to insert section for %s", ifaces[idx]);
            _j4status_iw_section_free(section);
          }
        else
            sections = g_slist_prepend(sections, section);
      }
    g_free(ifaces); // array's *values* are still used for a while, though
    if (!sections)
      {
        g_warning("Nothing to display, stopping");
        return NULL;
      }

    J4statusPluginContext *context = g_new(J4statusPluginContext, 1);
    context->sections = sections;
    context->started = FALSE;
    context->period = MAX(period, 1);
    context->format = j4status_format_string_parse(format, _j4status_iw_tokens,
                     TOTAL_TOKEN_COUNT, FORMAT_DEFAULT, &context->used_tokens);
    context->unknown = unknown ? unknown : g_strdup("unknown");
    context->hide_inactive = hide_inactive;
    return context;
}

/**
 * J4statusPluginSimpleFunc instance
 * Designed to be re-callable,
 * but I'm not sure if it makes any difference?
 * Probably just called after inits
 */
static void
_j4status_iw_start(J4statusPluginContext *context)
{
    if (context->started) return;
    context->started = TRUE;
    _j4status_iw_update(context);
    g_timeout_add_seconds(context->period, &_j4status_iw_update, context);
}

/**
 * J4statusPluginSimpleFunc instance
 * Called just before the next one, I think
 */
static void
_j4status_iw_stop(J4statusPluginContext *context)
{
    context->started = FALSE;
}

/**
 * J4statusPluginSimpleFunc instance
 * Called on exit, so is cleanup and such
 */
static void
_j4status_iw_uninit(J4statusPluginContext *context)
{
    if (context->started) _j4status_iw_stop(context);
    g_slist_free_full(context->sections, &_j4status_iw_section_free);
    j4status_format_string_unref(context->format);
    g_free(context->unknown);
    g_free(context);
}

/**
 * The "main", only exported func
 * Does callback bindings
 */
void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface,
                                                         _j4status_iw_init);
    libj4status_input_plugin_interface_add_start_callback(interface,
                                                          _j4status_iw_start);
    libj4status_input_plugin_interface_add_stop_callback(interface,
                                                          _j4status_iw_stop);
    libj4status_input_plugin_interface_add_uninit_callback(interface,
                                                          _j4status_iw_uninit);
}
