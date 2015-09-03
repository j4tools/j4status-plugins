AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_MEM], [
    J4SP_ADD_INPUT_PLUGIN(mem, [Memory info], [yes], [
        PKG_CHECK_MODULES([MEM_PLUGIN], [gobject-2.0 glib-2.0])
        AC_CHECK_HEADERS([sys/sysinfo.h])
    ])
])
