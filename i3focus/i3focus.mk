plugins_LTLIBRARIES += \
	i3focus/i3focus.la

man5_MANS += \
	i3focus/man/j4status-i3focus.conf.5


i3focus_i3focus_la_SOURCES = \
	i3focus/src/i3focus.c

i3focus_i3focus_la_CPPFLAGS = \
	-D G_LOG_DOMAIN=\"j4status-plugins-mpris\" \
	$(null)

i3focus_i3focus_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(I3FOCUS_PLUGIN_CFLAGS)

i3focus_i3focus_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input

i3focus_i3focus_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(I3FOCUS_PLUGIN_LIBS)
