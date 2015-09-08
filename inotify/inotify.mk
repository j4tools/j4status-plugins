plugins_LTLIBRARIES += \
	inotify/inotify.la

man5_MANS += \
	inotify/man/j4status-inotify.conf.5

inotify_inotify_la_SOURCES = \
	inotify/src/inotify.c

inotify_inotify_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(INOTIFY_PLUGIN_CFLAGS)

inotify_inotify_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input -pthread

inotify_inotify_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(INOTIFY_PLUGIN_LIBS)
