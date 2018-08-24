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

#include <netdb.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <libnm/NetworkManager.h>

#include <j4status-plugin-input.h>

typedef enum {
    ADDRESSES_IPV4,
    ADDRESSES_IPV6,
    ADDRESSES_ALL,
} J4statusNmAddresses;

typedef enum {
    TOKEN_UP_ETH_ADDRESSES,
    TOKEN_UP_ETH_SPEED,
} J4statusNmFormatUpEthToken;

typedef enum {
    TOKEN_UP_WIFI_ADDRESSES,
    TOKEN_UP_WIFI_STRENGTH,
    TOKEN_UP_WIFI_SSID,
    TOKEN_UP_WIFI_BITRATE,
} J4statusNmFormatUpWiFiToken;

typedef enum {
    TOKEN_DOWN_WIFI_APS,
} J4statusNmFormatDownWiFiToken;

typedef enum {
    TOKEN_UP_OTHER_ADDRESSES,
} J4statusNmFormatUpOtherToken;

static const gchar * const _j4status_nm_addresses[] = {
    [ADDRESSES_IPV4] = "ipv4",
    [ADDRESSES_IPV6] = "ipv6",
    [ADDRESSES_ALL]  = "all",
};

static const gchar * const _j4status_nm_format_up_eth_tokens[] = {
    [TOKEN_UP_ETH_ADDRESSES] = "addresses",
    [TOKEN_UP_ETH_SPEED]     = "speed",
};

static const gchar * const _j4status_nm_format_up_wifi_tokens[] = {
    [TOKEN_UP_WIFI_ADDRESSES] = "addresses",
    [TOKEN_UP_WIFI_STRENGTH]  = "strength",
    [TOKEN_UP_WIFI_SSID]      = "ssid",
    [TOKEN_UP_WIFI_BITRATE]   = "bitrate",
};

static const gchar * const _j4status_nm_format_down_wifi_tokens[] = {
    [TOKEN_DOWN_WIFI_APS] = "aps",
};

static const gchar * const _j4status_nm_format_up_other_tokens[] = {
    [TOKEN_UP_OTHER_ADDRESSES] = "addresses",
};

typedef enum {
    TOKEN_FLAG_UP_ETH_ADDRESSES = (1 << TOKEN_UP_ETH_ADDRESSES),
    TOKEN_FLAG_UP_ETH_SPEED     = (1 << TOKEN_UP_ETH_SPEED),
} J4statusNmFormatUpEthTokenFlag;

typedef enum {
    TOKEN_FLAG_UP_WIFI_ADDRESSES = (1 << TOKEN_UP_WIFI_ADDRESSES),
    TOKEN_FLAG_UP_WIFI_STRENGTH  = (1 << TOKEN_UP_WIFI_STRENGTH),
    TOKEN_FLAG_UP_WIFI_SSID      = (1 << TOKEN_UP_WIFI_SSID),
    TOKEN_FLAG_UP_WIFI_BITRATE   = (1 << TOKEN_UP_WIFI_BITRATE),
} J4statusNmFormatUpWiFiTokenFlag;

typedef enum {
    TOKEN_FLAG_DOWN_WIFI_APS = (1 << TOKEN_DOWN_WIFI_APS),
} J4statusNmFormatDownWiFiTokenFlag;

typedef enum {
    TOKEN_FLAG_UP_OTHER_ADDRESSES = (1 << TOKEN_UP_OTHER_ADDRESSES),
} J4statusNmFormatUpOtherTokenFlag;

#define J4STATUS_NM_FORMAT_UP_ETH_SPEED_SIZE (10+1+1)
typedef struct {
    GVariant *addresses;
    gint64 speed;
} J4statusNmFormatUpEthData;

#define J4STATUS_NM_FORMAT_UP_WIFI_STRENGTH_SIZE (3+1)
#define J4STATUS_NM_FORMAT_UP_WIFI_BITRATE_SIZE (10+1+1)
typedef struct {
    GVariant *addresses;
    gint8 strength;
    GVariant *ssid;
    gint64 bitrate;
} J4statusNmFormatUpWiFiData;

#define J4STATUS_NM_FORMAT_APS_SIZE (10+1)


#define J4STATUS_NM_DEFAULT_FORMAT_UP_ETH "${addresses}${speed:+(${speed(p)}b/s)}"
#define J4STATUS_NM_DEFAULT_FORMAT_UP_WIFI "${addresses} (${strength}${strength:+% }${ssid/^.+$/at \\0, }${bitrate(p.0)}b/s)"
#define J4STATUS_NM_DEFAULT_FORMAT_DOWN_WIFI "Down${aps/^.+$/ (\\0 APs)}"
#define J4STATUS_NM_DEFAULT_FORMAT_UP_OTHER "${addresses}"
#define J4STATUS_NM_DEFAULT_FORMAT_DOWN_OTHER "Down"

struct _J4statusPluginContext {
    J4statusCoreInterface *core;
    GHashTable *sections;

    /* Generic configuration */
    gboolean show_unknown;
    gboolean show_unmanaged;
    gboolean hide_unavailable;
    J4statusNmAddresses addresses;

    /* Formats */
    struct {
        guint64 up_eth_tokens;
        J4statusFormatString *up_eth;
        guint64 up_wifi_tokens;
        J4statusFormatString *up_wifi;
        guint64 down_wifi_tokens;
        J4statusFormatString *down_wifi;
        guint64 up_other_tokens;
        J4statusFormatString *up_other;
        guint64 down_other_tokens;
        J4statusFormatString *down_other;
    } formats;

    NMClient *nm_client;
    gboolean started;
};

typedef struct {
    J4statusPluginContext *context;
    const gchar *interface;
    NMDevice *device;
    J4statusSection *section;

    guint64 used_tokens;

    gulong state_changed_handler;

    /* For WiFi devices */
    NMAccessPoint *ap;

    gulong bitrate_handler;
    gulong active_access_point_handler;
    gulong strength_handler;
} J4statusNmSection;

static GVariant *
_j4status_nm_format_up_eth_callback(G_GNUC_UNUSED const gchar *token, guint64 value, gconstpointer user_data)
{
    const J4statusNmFormatUpEthData *data = user_data;
    switch ( value )
    {
    case TOKEN_UP_ETH_ADDRESSES:
        if ( data->addresses == NULL )
            return NULL;
        return g_variant_ref(data->addresses);
    break;
    case TOKEN_UP_ETH_SPEED:
        return g_variant_new_int64(data->speed);
    break;
    default:
        g_assert_not_reached();
    }
    return NULL;
}

static GVariant *
_j4status_nm_format_up_wifi_callback(G_GNUC_UNUSED const gchar *token, guint64 value, gconstpointer user_data)
{
    const J4statusNmFormatUpWiFiData *data = user_data;
    switch ( value )
    {
    case TOKEN_UP_WIFI_ADDRESSES:
        if ( data->addresses == NULL )
            return NULL;
        return g_variant_ref(data->addresses);
    case TOKEN_UP_WIFI_STRENGTH:
        if ( data->strength < 0 )
            return NULL;
        return g_variant_new_byte(data->strength);
    case TOKEN_UP_WIFI_SSID:
        return g_variant_ref(data->ssid);
    case TOKEN_UP_WIFI_BITRATE:
        return g_variant_new_int64(data->bitrate);
    default:
        g_assert_not_reached();
    }
    return NULL;
}

static GVariant *
_j4status_nm_format_down_wifi_callback(G_GNUC_UNUSED const gchar *token, guint64 value, gconstpointer user_data)
{
    const gint64 *aps_number = user_data;
    switch ( value )
    {
    case TOKEN_DOWN_WIFI_APS:
        if ( *aps_number < 0 )
            return NULL;
        return g_variant_new_int64(*aps_number);
    break;
    default:
        g_assert_not_reached();
    }
    return NULL;
}

static GVariant *
_j4status_nm_format_up_other_callback(G_GNUC_UNUSED const gchar *token, guint64 value, gconstpointer user_data)
{
    switch ( value )
    {
    case TOKEN_UP_OTHER_ADDRESSES:
        if ( user_data == NULL )
            return NULL;
        return g_variant_ref((gpointer) user_data);
    break;
    default:
        g_assert_not_reached();
    }
    return NULL;
}

static GVariant *
_j4status_nm_format_down_other_callback(G_GNUC_UNUSED const gchar *token,
                                        G_GNUC_UNUSED guint64 value,
                                        G_GNUC_UNUSED gconstpointer user_data)
{
    return NULL;
}

static void
_j4status_nm_device_get_addresses_ipv4(G_GNUC_UNUSED J4statusPluginContext *context, NMDevice *device, GVariantBuilder *builder)
{
    NMIPConfig *ip4_config;

    ip4_config = nm_device_get_ip4_config(device);
    if ( ip4_config == NULL )
        return;

    const GPtrArray *addresses = nm_ip_config_get_addresses(ip4_config);
    guint i;
    for ( i = 0 ; i < addresses->len ; ++i )
    {
        NMIPAddress *addr = g_ptr_array_index(addresses, i);
        g_variant_builder_add_value(builder, g_variant_new_string(nm_ip_address_get_address(addr)));
    }
}

static void
_j4status_nm_device_get_addresses_ipv6(G_GNUC_UNUSED J4statusPluginContext *context, NMDevice *device, GVariantBuilder *builder)
{
    NMIPConfig *ip6_config;

    ip6_config = nm_device_get_ip6_config(device);
    if ( ip6_config == NULL )
        return;

    const GPtrArray *addresses = nm_ip_config_get_addresses(ip6_config);
    guint i;
    for ( i = 0 ; i < addresses->len ; ++i )
    {
        NMIPAddress *addr = g_ptr_array_index (addresses, i);
        g_variant_builder_add_value(builder, g_variant_new_string(nm_ip_address_get_address(addr)));
    }
}

static GVariant *
_j4status_nm_device_get_addresses(J4statusPluginContext *context, NMDevice *device)
{
    GVariantBuilder builder;
    GVariant *var;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_STRING_ARRAY);
    if ( context->addresses != ADDRESSES_IPV6 )
        _j4status_nm_device_get_addresses_ipv4(context, device, &builder);
    if ( context->addresses != ADDRESSES_IPV4 )
        _j4status_nm_device_get_addresses_ipv6(context, device, &builder);

    var = g_variant_builder_end(&builder);
    if ( g_variant_n_children(var) < 1)
    {
        g_variant_unref(var);
        return NULL;
    }
    return var;
}

static void
_j4status_nm_device_update(J4statusPluginContext *context, J4statusNmSection *section, NMDevice *device)
{

    J4statusState state = J4STATUS_STATE_NO_STATE;
    gchar *value = NULL;
    switch ( nm_device_get_state(device) )
    {
    case NM_DEVICE_STATE_UNKNOWN:
        value = context->show_unknown ? g_strdup("Unknown") : NULL;
    break;
    case NM_DEVICE_STATE_UNMANAGED:
        value = context->show_unmanaged ? g_strdup("Unmanaged") : NULL;
    break;
    case NM_DEVICE_STATE_UNAVAILABLE:
        state = J4STATUS_STATE_UNAVAILABLE;
        value = context->hide_unavailable ? NULL : g_strdup("Unavailable");
    break;
    case NM_DEVICE_STATE_DISCONNECTED:
        state = J4STATUS_STATE_BAD;
        switch ( nm_device_get_device_type(device) )
        {
        case NM_DEVICE_TYPE_WIFI:
        {
            gint64 aps_number = -1;
            if ( context->formats.down_wifi_tokens & TOKEN_FLAG_DOWN_WIFI_APS )
            {
                const GPtrArray *aps;
                aps = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(device));
                if ( aps != NULL )
                    aps_number = aps->len;
            }

            value = j4status_format_string_replace(context->formats.down_wifi, _j4status_nm_format_down_wifi_callback, &aps_number);
        }
        break;
        default:
            value = j4status_format_string_replace(context->formats.down_other, _j4status_nm_format_down_other_callback, NULL);
        break;
        }
    break;
    case NM_DEVICE_STATE_PREPARE:
        state = J4STATUS_STATE_AVERAGE;
        value = g_strdup("Prepare");
    break;
    case NM_DEVICE_STATE_CONFIG:
        state = J4STATUS_STATE_AVERAGE;
        value = g_strdup("Config");
    break;
    case NM_DEVICE_STATE_NEED_AUTH:
        state = J4STATUS_STATE_AVERAGE;
        value = g_strdup("Need auth");
    break;
    case NM_DEVICE_STATE_IP_CONFIG:
        state = J4STATUS_STATE_GOOD;
        value = g_strdup("IP configuration");
    break;
    case NM_DEVICE_STATE_IP_CHECK:
        state = J4STATUS_STATE_GOOD;
        value = g_strdup("IP check");
    break;
    case NM_DEVICE_STATE_SECONDARIES:
        state = J4STATUS_STATE_AVERAGE;
        value = g_strdup("Secondaries");
    break;
    case NM_DEVICE_STATE_ACTIVATED:
    {
        gchar *addresses = NULL;

        switch ( nm_device_get_device_type(device) )
        {
        case NM_DEVICE_TYPE_WIFI:
        {
            J4statusNmFormatUpWiFiData data = {NULL};

            if ( context->formats.up_eth_tokens & TOKEN_FLAG_UP_WIFI_ADDRESSES )
                data.addresses = _j4status_nm_device_get_addresses(context, device);

            state = J4STATUS_STATE_AVERAGE;
            if ( section->ap != NULL )
            {
                data.strength =  nm_access_point_get_strength(section->ap);
                if ( data.strength > 75 )
                    state = J4STATUS_STATE_GOOD;
                else if ( data.strength < 25 )
                    state = J4STATUS_STATE_BAD;

                if ( context->formats.up_wifi_tokens & TOKEN_FLAG_UP_WIFI_SSID )
                {
                    GBytes *raw_ssid = nm_access_point_get_ssid(section->ap);
                    if (raw_ssid) {
                        char *active_ssid_str = nm_utils_ssid_to_utf8(g_bytes_get_data (raw_ssid, NULL),
                                                                      g_bytes_get_size (raw_ssid));
                        data.ssid = g_variant_new_take_string(active_ssid_str);
                    }
                }
            }

            data.bitrate = nm_device_wifi_get_bitrate(NM_DEVICE_WIFI(device)) * 1000;

            value = j4status_format_string_replace(context->formats.up_wifi, _j4status_nm_format_up_wifi_callback, &data);

            if ( data.ssid != NULL ) {
                g_variant_unref(data.ssid);
                data.ssid = NULL;
            }
            if ( data.addresses != NULL )
                g_variant_unref(data.addresses);
        }
        break;
        case NM_DEVICE_TYPE_ETHERNET:
        {
            J4statusNmFormatUpEthData data = {NULL};

            if ( context->formats.up_eth_tokens & TOKEN_FLAG_UP_ETH_ADDRESSES )
                data.addresses = _j4status_nm_device_get_addresses(context, device);

            data.speed = nm_device_ethernet_get_speed(NM_DEVICE_ETHERNET(device)) * 1000000;

            state = J4STATUS_STATE_GOOD;
            value = j4status_format_string_replace(context->formats.up_eth, _j4status_nm_format_up_eth_callback, &data);

            if ( data.addresses != NULL )
                g_variant_unref(data.addresses);
        }
        break;
        default:
        {
            GVariant *addresses = NULL;
            if ( context->formats.up_other_tokens & TOKEN_FLAG_UP_OTHER_ADDRESSES )
                addresses = _j4status_nm_device_get_addresses(context, device);

            state = J4STATUS_STATE_GOOD;
            value = j4status_format_string_replace(context->formats.up_other, _j4status_nm_format_up_other_callback, addresses);

            if ( addresses != NULL )
                g_variant_unref(addresses);

        }
        break;
        }
        g_free(addresses);
    }
    break;
    case NM_DEVICE_STATE_DEACTIVATING:
        state = J4STATUS_STATE_BAD;
        value = g_strdup("Disconnecting");
    break;
    case NM_DEVICE_STATE_FAILED:
        state = J4STATUS_STATE_BAD;
        value = g_strdup("Failed");
    break;
    }

    j4status_section_set_state(section->section, state);
    j4status_section_set_value(section->section, value);
}

static void
_j4status_nm_access_point_property_changed(G_GNUC_UNUSED NMAccessPoint *device,
                                           G_GNUC_UNUSED GParamSpec *pspec, gpointer user_data)
{
    J4statusNmSection *section = user_data;
    _j4status_nm_device_update(section->context, section, section->device);
}

static void
_j4status_nm_device_property_changed(NMDevice *device, GParamSpec *pspec, gpointer user_data)
{
    J4statusNmSection *section = user_data;
    if ( g_strcmp0("active-access-point", pspec->name) == 0 )
    {
        if ( section->ap != NULL )
        {
            g_signal_handler_disconnect(section->ap, section->strength_handler);
            g_object_unref(section->ap);
        }
        section->ap = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(device));
        if ( section->ap != NULL )
            section->strength_handler = g_signal_connect(g_object_ref(section->ap), "notify::strength", G_CALLBACK(_j4status_nm_access_point_property_changed), section);
    }
    _j4status_nm_device_update(section->context, section, device);
}

static void
_j4status_nm_device_state_changed(NMDevice *device,
                                  G_GNUC_UNUSED guint state,
                                  G_GNUC_UNUSED guint arg2,
                                  G_GNUC_UNUSED guint arg3, gpointer user_data)
{
    J4statusNmSection *section = user_data;
    _j4status_nm_device_update(section->context, section, device);
}

static void
_j4status_nm_device_monitor(G_GNUC_UNUSED gpointer key, gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    J4statusNmSection *section = data;

    if ( section->device == NULL )
        return;

    switch ( nm_device_get_device_type(section->device) )
    {
    case NM_DEVICE_TYPE_WIFI:
        section->bitrate_handler = g_signal_connect(section->device, "notify::bitrate", G_CALLBACK(_j4status_nm_device_property_changed), section);
        section->active_access_point_handler = g_signal_connect(section->device, "notify::active-access-point", G_CALLBACK(_j4status_nm_device_property_changed), section);
        if ( section->ap != NULL )
            section->strength_handler = g_signal_connect(section->ap, "notify::strength", G_CALLBACK(_j4status_nm_access_point_property_changed), section);
    break;
    default:
    break;
    }
    section->state_changed_handler = g_signal_connect(section->device, "state-changed", G_CALLBACK(_j4status_nm_device_state_changed), section);
    _j4status_nm_device_update(section->context, section, section->device);
}

static void
_j4status_nm_device_unmonitor(G_GNUC_UNUSED gpointer key, gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    J4statusNmSection *section = data;

    if ( section->device == NULL )
        return;

    switch ( nm_device_get_device_type(section->device) )
    {
    case NM_DEVICE_TYPE_WIFI:
        g_signal_handler_disconnect(section->device, section->bitrate_handler);
        g_signal_handler_disconnect(section->device, section->active_access_point_handler);
        if ( section->ap != NULL )
            g_signal_handler_disconnect(section->ap, section->strength_handler);
    break;
    default:
    break;
    }
    g_signal_handler_disconnect(section->device, section->state_changed_handler);
}

static void
_j4status_nm_section_attach_device(J4statusPluginContext *context, J4statusNmSection *section, NMDevice *device)
{

    const gchar *name;
    const gchar *label;
    switch ( nm_device_get_device_type(device) )
    {
    case NM_DEVICE_TYPE_UNKNOWN:
    case NM_DEVICE_TYPE_UNUSED1:
    case NM_DEVICE_TYPE_UNUSED2:
        name = "nm-unknown";
        label = "Unknownw";
    break;
    case NM_DEVICE_TYPE_ETHERNET:
        name = "nm-ethernet";
        label = "E";
    break;
    case NM_DEVICE_TYPE_WIFI:
        name = "nm-wifi";
        label = "W";
        section->ap = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(device));
        if ( section->ap != NULL )
            g_object_ref(section->ap);
    break;
    case NM_DEVICE_TYPE_BT:
        name = "nm-bluetooth";
        label = "B";
    break;
    case NM_DEVICE_TYPE_OLPC_MESH:
        name = "nm-olpc-mesh";
        label = "OM";
    break;
    case NM_DEVICE_TYPE_WIMAX:
        name = "nm-wimax";
        label = "Wx";
    break;
    case NM_DEVICE_TYPE_MODEM:
        name = "nm-modem";
        label = "M";
    break;
    case NM_DEVICE_TYPE_INFINIBAND:
        name = "nm-infiniband";
        label = "I";
    break;
    case NM_DEVICE_TYPE_BOND:
        name = "nm-bond";
        label = "Bo";
    break;
    case NM_DEVICE_TYPE_VLAN:
        name = "nm-vlan";
        label = "V";
    break;
#if NM_CHECK_VERSION(0,9,5)
    case NM_DEVICE_TYPE_ADSL:
        name = "nm-adsl";
        label = "A";
    break;
#endif /* NM_CHECK_VERSION(0.9.5) */
    default:
        g_warning("Unsupported device type for interface '%s'", section->interface);
        return;
    }
    section->section = j4status_section_new(context->core);
    j4status_section_set_name(section->section, name);
    j4status_section_set_instance(section->section, section->interface);
    j4status_section_set_label(section->section, label);

    if ( j4status_section_insert(section->section) )
    {
        section->device = g_object_ref(device);
        if ( context->started )
            _j4status_nm_device_monitor(NULL, section, NULL);
    }
    else
    {
        j4status_section_free(section->section);
        section->section = NULL;
    }
}

static void
_j4status_nm_section_detach_device(J4statusNmSection *section)
{
    g_object_unref(section->device);
    section->device = NULL;
    j4status_section_free(section->section);
    section->section = NULL;
}

static void
_j4status_nm_client_device_added(G_GNUC_UNUSED NMClient *client, NMDevice *device, gpointer user_data)
{
    J4statusPluginContext *context = user_data;
    J4statusNmSection *section;
    section = g_hash_table_lookup(context->sections, nm_device_get_iface(device));
    if ( section != NULL )
        _j4status_nm_section_attach_device(context, section, device);
}

static void
_j4status_nm_client_device_removed(G_GNUC_UNUSED NMClient *client, NMDevice *device, gpointer user_data)
{
    J4statusPluginContext *context = user_data;
    J4statusNmSection *section;
    section = g_hash_table_lookup(context->sections, nm_device_get_iface(device));
    if ( section != NULL )
        _j4status_nm_section_detach_device(section);
}

static void
_j4status_nm_section_new(J4statusPluginContext *context, gchar *interface)
{
    J4statusNmSection *section;
    section = g_new0(J4statusNmSection, 1);
    section->context = context;
    section->interface = interface;

    g_hash_table_insert(context->sections, interface, section);

    NMDevice *device;
    device = nm_client_get_device_by_iface(context->nm_client, interface);
    if ( device != NULL )
        _j4status_nm_section_attach_device(context, section, device);
}

static void
_j4status_nm_section_free(gpointer data)
{
    J4statusNmSection *section = data;

    if ( section->device != NULL )
    {
        g_object_unref(section->device);
        j4status_section_free(section->section);
    }

    g_free(section);
}

static J4statusPluginContext *
_j4status_nm_init(J4statusCoreInterface *core)
{
    GKeyFile *key_file;
    key_file = j4status_config_get_key_file("NetworkManager");
    if ( key_file == NULL )
    {
        g_message("Missing configuration: No section, aborting");
        return NULL;
    }

    gchar **interfaces;
    interfaces = g_key_file_get_string_list(key_file, "NetworkManager", "Interfaces", NULL, NULL);
    if ( interfaces == NULL )
    {
        g_message("Missing configuration: Empty list of interfaces to monitor, aborting");
        g_key_file_free(key_file);
        return NULL;
    }

    J4statusPluginContext *context;
    context = g_new0(J4statusPluginContext, 1);
    context->core = core;

    context->nm_client = nm_client_new(NULL, NULL);
    if ( context->nm_client == NULL )
    {
        g_message("Could not connect to network manager; aborting");
        g_key_file_free(key_file);
        g_free(context);
        return NULL;
    }

    context->sections = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _j4status_nm_section_free);

    context->show_unknown = g_key_file_get_boolean(key_file, "NetworkManager", "ShowUnknown", NULL);
    context->show_unmanaged = g_key_file_get_boolean(key_file, "NetworkManager", "ShowUnmanaged", NULL);
    context->hide_unavailable = g_key_file_get_boolean(key_file, "NetworkManager", "HideUnavailable", NULL);

    g_key_file_free(key_file);


    guint64 addresses = ADDRESSES_ALL;
    gchar *format_up_eth = NULL;
    gchar *format_up_wifi = NULL;
    gchar *format_down_wifi = NULL;
    gchar *format_up_other = NULL;
    gchar *format_down_other = NULL;

    key_file = j4status_config_get_key_file("NetworkManager Formats");
    if ( key_file != NULL )
    {
        j4status_config_key_file_get_enum(key_file, "NetworkManager Formats", "Addresses", _j4status_nm_addresses, G_N_ELEMENTS(_j4status_nm_addresses), &addresses);
        format_up_eth     = g_key_file_get_string(key_file, "NetworkManager Formats", "UpEthernet", NULL);
        format_up_wifi    = g_key_file_get_string(key_file, "NetworkManager Formats", "UpWiFi", NULL);
        format_down_wifi  = g_key_file_get_string(key_file, "NetworkManager Formats", "DownWiFi", NULL);
        format_up_other   = g_key_file_get_string(key_file, "NetworkManager Formats", "UpOther", NULL);
        format_down_other = g_key_file_get_string(key_file, "NetworkManager Formats", "DownOther", NULL);
        g_key_file_free(key_file);
    }

    context->addresses = addresses;
    context->formats.up_eth     = j4status_format_string_parse(format_up_eth,     _j4status_nm_format_up_eth_tokens,     G_N_ELEMENTS(_j4status_nm_format_up_eth_tokens),     J4STATUS_NM_DEFAULT_FORMAT_UP_ETH,     &context->formats.up_eth_tokens);
    context->formats.up_wifi    = j4status_format_string_parse(format_up_wifi,    _j4status_nm_format_up_wifi_tokens,    G_N_ELEMENTS(_j4status_nm_format_up_wifi_tokens),    J4STATUS_NM_DEFAULT_FORMAT_UP_WIFI,    &context->formats.up_wifi_tokens);
    context->formats.down_wifi  = j4status_format_string_parse(format_down_wifi,  _j4status_nm_format_down_wifi_tokens,  G_N_ELEMENTS(_j4status_nm_format_down_wifi_tokens),  J4STATUS_NM_DEFAULT_FORMAT_DOWN_WIFI,  &context->formats.down_wifi_tokens);
    context->formats.up_other   = j4status_format_string_parse(format_up_other,   _j4status_nm_format_up_other_tokens,   G_N_ELEMENTS(_j4status_nm_format_up_other_tokens),   J4STATUS_NM_DEFAULT_FORMAT_UP_OTHER,   &context->formats.up_other_tokens);
    context->formats.down_other = j4status_format_string_parse(format_down_other, NULL,                                  0,                                                   J4STATUS_NM_DEFAULT_FORMAT_DOWN_OTHER, &context->formats.down_other_tokens);

    gchar **interface;
    for ( interface = interfaces ; *interface != NULL ; ++interface )
        _j4status_nm_section_new(context, *interface);
    g_free(interfaces);

    g_signal_connect(context->nm_client, "device-added", G_CALLBACK(_j4status_nm_client_device_added), context);
    g_signal_connect(context->nm_client, "device-removed", G_CALLBACK(_j4status_nm_client_device_removed), context);

    return context;
}

static void
_j4status_nm_uninit(J4statusPluginContext *context)
{
    g_hash_table_unref(context->sections);

    g_object_unref(context->nm_client);

    g_free(context);
}

static void
_j4status_nm_start(J4statusPluginContext *context)
{
    g_hash_table_foreach(context->sections, _j4status_nm_device_monitor, NULL);
    context->started = TRUE;
}

static void
_j4status_nm_stop(J4statusPluginContext *context)
{
    context->started = FALSE;
    g_hash_table_foreach(context->sections, _j4status_nm_device_unmonitor, NULL);
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_nm_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_nm_uninit);

    libj4status_input_plugin_interface_add_start_callback(interface, _j4status_nm_start);
    libj4status_input_plugin_interface_add_stop_callback(interface, _j4status_nm_stop);
}
