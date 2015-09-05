plugins_LTLIBRARIES += \
	backlight/backlight.la

man5_MANS += \
	backlight/man/j4status-backlight.conf.5

backlight_backlight_la_SOURCES = \
	backlight/src/backlight.c

backlight_backlight_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(BACKLIGHT_PLUGIN_CFLAGS)

backlight_backlight_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input -pthread

backlight_backlight_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(BACKLIGHT_PLUGIN_LIBS)
