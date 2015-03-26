
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define _GNU_SOURCE
#ifdef HAVE_MATH_H
#include <math.h>
#endif /* HAVE_MATH_H */

#include <glib.h>
#include <glib/gprintf.h>

#include <j4status-plugin-input.h>

#include <asoundlib.h>
#include <libgwater-alsa-mixer.h>

struct _J4statusPluginContext {
    GList *sections;
};

typedef struct {
    gchar *card;
    GWaterAlsaMixerSource *source;
    snd_mixer_t *mixer;
    J4statusSection *section;
    gboolean use_dB;
    glong min;
    glong max;
} J4statusAlsaSection;

#define J4STATUS_ALSA_VOLUME_TO_PERCENT(min, volume, max) ((((gdouble)(volume - min) / (gdouble)(max - min)) * 100.))

/*
 * Formulas and algorithm from alsamixer volume_mapping.c
 * which is licenced under MIT license
 * http://opensource.org/licenses/MIT
 *
 * Copyright (c) 2010 Clemens Ladisch <clemens@ladisch.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef __UCLIBC__
/* 10^x = 10^(log e^x) = (e^x)^log10 = e^(x * log 10) */
#define exp10(x) (exp((x) * log(10)))
#endif /* __UCLIBC__ */
#define MAX_LINEAR_DB_SCALE	24
static inline guint8
_j4status_alsa_get_normalized_volume(glong value, glong min, glong max, gboolean use_dB)
{
    gdouble volume;
    if ( ( use_dB ) && ( max - min > MAX_LINEAR_DB_SCALE * 100 ) )
    {
        volume = exp10((value - max) / 6000.0);
        if ( min != SND_CTL_TLV_DB_GAIN_MUTE )
        {
            gdouble min_norm;
            min_norm = exp10((min - max) / 6000.0);
            volume = (volume - min_norm) / (1 - min_norm);
        }
    }
    else
        volume = (gdouble)(value - min) / (gdouble)(max - min);
    if ( ( volume < 0 ) || ( volume > 1 ) )
        return 0;
    return rint(volume * 100);
}

static void
_j4status_alsa_section_update(J4statusAlsaSection *section, snd_mixer_elem_t *elem)
{
    if ( ( ! snd_mixer_selem_has_playback_volume(elem) )
         || ( ! snd_mixer_selem_has_playback_switch(elem) )
         || ( ! snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_FRONT_LEFT) ) )
        return;

    int error;
    gboolean pswitch = TRUE;
    error = snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &pswitch);
    if ( error < 0 )
        g_warning("Couldn't get muted status: %s", snd_strerror(error));
    else
    {
        if ( pswitch )
            j4status_section_set_state(section->section, J4STATUS_STATE_GOOD);
        else
            j4status_section_set_state(section->section, J4STATUS_STATE_BAD);
    }

    glong volume = 0;
    if ( section->use_dB )
        error = snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);
    else
        error = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);
    if ( error < 0 )
        g_warning("Couldn't get volume: %s", snd_strerror(error));
    else
    {
        guint8 vol;
        gchar *v;
        vol = _j4status_alsa_get_normalized_volume(volume, section->min, section->max, section->use_dB);
        v = g_strdup_printf("%hu%%", vol);
        j4status_section_set_value(section->section, v);
    }
}

static gint
_j4status_alsa_section_elem_callback(snd_mixer_elem_t *elem, guint mask)
{
    J4statusAlsaSection *section = snd_mixer_elem_get_callback_private(elem);
    if ( mask == SND_CTL_EVENT_MASK_REMOVE )
        return 0;

    if ( mask & SND_CTL_EVENT_MASK_INFO )
    {
        glong min, max;
        int error;
        error = snd_mixer_selem_get_playback_dB_range(elem, &min, &max);
        if ( ( error == 0 ) && ( min < max ) )
            section->use_dB = TRUE;
        else
        {
            section->use_dB = FALSE;
            snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        }
        section->min = min;
        section->max = max;
    }

    if ( mask & SND_CTL_EVENT_MASK_VALUE )
        _j4status_alsa_section_update(section, elem);

    return 0;
}

static gint
_j4status_alsa_section_mixer_callback(snd_mixer_t *mixer, guint mask, snd_mixer_elem_t *elem)
{
    J4statusAlsaSection *section = snd_mixer_get_callback_private(mixer);
    if ( mask & SND_CTL_EVENT_MASK_ADD )
    {
        if ( ( snd_mixer_selem_get_index(elem) == 0 )
             && ( g_strcmp0(snd_mixer_selem_get_name(elem), "Master") == 0 )
             && snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_FRONT_LEFT)
             && ( snd_mixer_elem_get_callback_private(elem) == NULL )
            )
        {
            snd_mixer_elem_set_callback(elem, _j4status_alsa_section_elem_callback);
            snd_mixer_elem_set_callback_private(elem, section);
        }
    }
    return 0;
}


static void
_j4status_alsa_section_free(gpointer data)
{
    J4statusAlsaSection *section = data;

    j4status_section_free(section->section);

    snd_mixer_free(section->mixer);
    g_water_alsa_mixer_source_unref(section->source);

    g_free(section->card);

    g_free(section);
}

static J4statusAlsaSection *
_j4status_alsa_section_new(J4statusCoreInterface *core, gchar *card)
{
    J4statusAlsaSection *section;

    section = g_new0(J4statusAlsaSection, 1);
    section->card = card;
    int error;

    section->source = g_water_alsa_mixer_source_new(NULL, card, _j4status_alsa_section_mixer_callback, section, NULL, &error);
    if ( section->source == NULL )
    {
        g_warning("Couldn't create ALSA mixer source for card %s: %s", card, snd_strerror(error));
        g_free(section->card);
        g_free(section);
        return NULL;
    }
    section->mixer = g_water_alsa_mixer_source_get_mixer(section->source);

    error = snd_mixer_selem_register(section->mixer, NULL, NULL);
    if ( error < 0 )
    {
        g_warning("Couldn't register ALSA mixer to card %s: %s", card, snd_strerror(error));
        g_water_alsa_mixer_source_unref(section->source);
        g_free(section->card);
        g_free(section);
        return NULL;
    }

    section->section = j4status_section_new(core);
    j4status_section_set_name(section->section, "alsa");
    j4status_section_set_instance(section->section, card);

    if ( ! j4status_section_insert(section->section) )
    {
        _j4status_alsa_section_free(section);
        return NULL;
    }

    error = snd_mixer_load(section->mixer);
    if ( error < 0 )
    {
        g_warning("Couldn't load ALSA mixer for card %s: %s", section->card, snd_strerror(error));
        _j4status_alsa_section_free(section);
        return NULL;
    }

    return section;
}

static void _j4status_alsa_uninit(J4statusPluginContext *context);

static J4statusPluginContext *
_j4status_alsa_init(J4statusCoreInterface *core)
{
    GKeyFile *key_file;
    key_file = j4status_config_get_key_file("ALSA");
    if ( key_file == NULL )
        return NULL;

    gchar **cards;

    cards = g_key_file_get_string_list(key_file, "ALSA", "Cards", NULL, NULL);

    g_key_file_free(key_file);

    if ( cards == NULL )
        return NULL;

    GList *sections = NULL;
    gchar **card;
    for ( card = cards ; *card != NULL ; ++card )
    {
        J4statusAlsaSection *section;
        section = _j4status_alsa_section_new(core, *card);
        if ( section != NULL )
            sections = g_list_prepend(sections, section);
    }
    g_free(cards);
    if ( sections == NULL )
        return NULL;

    J4statusPluginContext *context;

    context = g_new0(J4statusPluginContext, 1);
    context->sections = sections;

    return context;
}

static void
_j4status_alsa_uninit(J4statusPluginContext *context)
{
    g_list_free_full(context->sections, _j4status_alsa_section_free);

    g_free(context);
}

void
j4status_input_plugin(J4statusInputPluginInterface *interface)
{
    libj4status_input_plugin_interface_add_init_callback(interface, _j4status_alsa_init);
    libj4status_input_plugin_interface_add_uninit_callback(interface, _j4status_alsa_uninit);
}
