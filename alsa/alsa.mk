include src/libgwater/alsa-mixer.mk

plugins_LTLIBRARIES += \
	alsa/alsa.la

man5_MANS += \
	alsa/man/j4status-alsa.conf.5


alsa_alsa_la_SOURCES = \
	alsa/src/alsa.c

alsa_alsa_la_CPPFLAGS = \
	-D G_LOG_DOMAIN=\"j4status-plugins-alsa\" \
	$(null)

alsa_alsa_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ALSA_PLUGIN_CFLAGS)

alsa_alsa_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input

alsa_alsa_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(ALSA_PLUGIN_LIBS) \
	-lm
