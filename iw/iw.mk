plugins_LTLIBRARIES += \
	iw/iw.la

man5_MANS += \
	iw/man/j4status-iw.conf.5

iw_iw_la_SOURCES = \
	iw/src/iw.c

iw_iw_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(IW_PLUGIN_CFLAGS)

iw_iw_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input

iw_iw_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(IW_PLUGIN_LIBS)
