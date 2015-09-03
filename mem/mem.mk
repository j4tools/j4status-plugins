plugins_LTLIBRARIES += \
	mem/mem.la

man5_MANS += \
	mem/man/j4status-mem.conf.5

mem_mem_la_SOURCES = \
	mem/src/mem.c

mem_mem_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(MEM_PLUGIN_CFLAGS)

mem_mem_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input

mem_mem_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(MEM_PLUGIN_LIBS)
