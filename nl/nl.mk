include src/libgwater/nl.mk

plugins_LTLIBRARIES += \
	nl/nl.la \
	$(null)

man5_MANS += \
	nl/man/j4status-nl.conf.5 \
	$(null)


nl_nl_la_SOURCES = \
	nl/src/nl.c \
	$(null)

nl_nl_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D G_LOG_DOMAIN=\"j4status-nl\" \
	$(null)

nl_nl_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GW_NL_CFLAGS) \
	$(null)

nl_nl_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input \
	$(null)

nl_nl_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(GW_NL_LIBS) \
	$(null)
