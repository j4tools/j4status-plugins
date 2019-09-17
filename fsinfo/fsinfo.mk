plugins_LTLIBRARIES += \
	fsinfo/fsinfo.la

man5_MANS += \
	fsinfo/man/j4status-fsinfo.conf.5

fsinfo_fsinfo_la_SOURCES = \
	fsinfo/src/fsinfo.c

fsinfo_fsinfo_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(FSINFO_PLUGIN_CFLAGS)

fsinfo_fsinfo_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input

fsinfo_fsinfo_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(FSINFO_PLUGIN_LIBS)
