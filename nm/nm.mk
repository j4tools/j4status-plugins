plugins_LTLIBRARIES += \
	nm/nm.la \
	$(null)

man5_MANS += \
	nm/man/j4status-nm.conf.5 \
	$(null)


nm_nm_la_SOURCES = \
	nm/src/nm.c \
	$(null)

nm_nm_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D G_LOG_DOMAIN=\"j4status-plugins-nm\" \
	$(null)

nm_nm_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(LIBNM_CFLAGS) \
	$(null)

nm_nm_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input \
	$(null)

nm_nm_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(LIBNM_LIBS) \
	$(null)
