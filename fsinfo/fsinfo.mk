plugins_LTLIBRARIES += \
	fsinfo/fsinfo.la

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
