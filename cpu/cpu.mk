plugins_LTLIBRARIES += \
	cpu/cpu.la

cpu_cpu_la_SOURCES = \
	cpu/src/cpu.c

cpu_cpu_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(CPU_PLUGIN_CFLAGS)

cpu_cpu_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input

cpu_cpu_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(CPU_PLUGIN_LIBS)
