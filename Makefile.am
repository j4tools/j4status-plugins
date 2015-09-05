 comma := ,
 empty :=
 space := $(empty) $(empty)

 AM_DISTCHECK_CONFIGURE_FLAGS = \
	--with-pluginsdir="$$dc_install_base/$(pluginsdir)" \
	--disable-debug

ACLOCAL_AMFLAGS = -I m4 -I src/libgwater ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

noinst_LTLIBRARIES =
plugins_LTLIBRARIES =

man1_MANS =
man5_MANS =

BUILT_SOURCES =
EXTRA_DIST =
CLEANFILES =

XSLTPROC_CONDITIONS = \
	installation

AM_CPPFLAGS += \
	-D LOCALEDIR=\"$(localedir)\" \
	-I $(srcdir)/src/ \
	-I $(builddir)/src/

AM_CFLAGS += \
	$(J4STATUS_PLUGIN_CFLAGS)

AM_XSLTPROCFLAGS += \
	--path "$(srcdir)/src/" \
	--path "$(builddir)/src/"

#
# Plugins
#
if ENABLE_ALSA_INPUT
include alsa/alsa.mk
endif

if ENABLE_I3FOCUS_INPUT
include i3focus/i3focus.mk
endif

if ENABLE_MPRIS_INPUT
include mpris/mpris.mk
endif

if ENABLE_IW_INPUT
include iw/iw.mk
endif

if ENABLE_FSINFO_INPUT
include fsinfo/fsinfo.mk
endif

if ENABLE_CPU_INPUT
include cpu/cpu.mk
endif

if ENABLE_MEM_INPUT
include mem/mem.mk
endif

if ENABLE_BACKLIGHT_INPUT
include backlight/backlight.mk
endif

include src/man.mk

-include local-rules.mk

#
# Hooks
#
 ifneq ($(plugins_LTLIBRARIES),)
install-data-hook: la-files-install-hook
uninstall-hook: la-files-uninstall-hook

# *.la files cleanup
la-files-install-hook:
	cd $(DESTDIR)$(pluginsdir) && \
		rm $(notdir $(plugins_LTLIBRARIES))

# Remove shared object files at uninstall since
# we remove *.la files at install
la-files-uninstall-hook:
	cd $(DESTDIR)$(pluginsdir) && \
		rm $(notdir $(plugins_LTLIBRARIES:.la=$(shrext_cmds)))
	rmdir $(DESTDIR)$(pluginsdir)
 endif
