mpris/src/mpris-generated.c:
	$(MPRIS_GDBUS_CODEGEN) --generate-c-code mpris/src/mpris-generated mpris/src/mpris-interface.xml

CLEANFILES+= \
	mpris/src/mpris-generated.c \
	mpris/src/mpris-generated.h \
	$(null)

plugins_LTLIBRARIES += \
	mpris.la \
	$(null)

# sources are generated with:
# gdbus-codegen --generate-c-code src/input/mpris/mpris-generated src/input/mpris/mpris-interface.xml
mpris_la_SOURCES = \
	mpris/src/mpris-generated.c \
	mpris/src/mpris.c \
	mpris/src/mpris-generated.h \
	$(null)

mpris_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(MPRIS_PLUGIN_CFLAGS) \
	$(null)

mpris_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module -avoid-version -export-symbols-regex j4status_input \
	$(null)

mpris_la_LIBADD = \
	$(J4STATUS_PLUGIN_LIBS) \
	$(MPRIS_PLUGIN_LIBS) \
	$(null)

man5_MANS += \
	mpris/j4status-mpris.conf.5 \
	$(null)
