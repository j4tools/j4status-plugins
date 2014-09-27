plugins_LTLIBRARIES += \
	mpris/mpris.la \
	$(null)

man5_MANS += \
	mpris/man/j4status-mpris.conf.5 \
	$(null)

BUILT_SOURCES += \
	mpris/src/mpris-generated.c \
	mpris/src/mpris-generated.h \
	$(null)

CLEANFILES += \
	mpris/src/mpris-generated.c \
	mpris/src/mpris-generated.h \
	$(null)

EXTRA_DIST += \
	mpris/src/mpris-interface.xml \
	$(null)


mpris_mpris_la_SOURCES = \
	mpris/src/mpris-generated.c \
	mpris/src/mpris.c \
	mpris/src/mpris-generated.h \
	$(null)

mpris_mpris_la_CPPFLAGS = \
	-D G_LOG_DOMAIN=\"j4status-plugins-mpris\" \
	$(null)

mpris_mpris_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(MPRIS_PLUGIN_CFLAGS) \
	$(null)

mpris_mpris_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input \
	$(null)

mpris_mpris_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(MPRIS_PLUGIN_LIBS) \
	$(null)


# Ordering rule
mpris/mpris.la mpris/src/mpris_mpris_la-mpris.lo: mpris/src/mpris-generated.h

mpris/src/mpris-generated.h mpris/src/mpris-generated.c:
	$(AM_V_GEN)$(MPRIS_GDBUS_CODEGEN) --generate-c-code mpris/src/mpris-generated mpris/src/mpris-interface.xml
