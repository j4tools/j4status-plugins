AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_CPU], [
    J4SP_ADD_INPUT_PLUGIN(cpu, [CPU usage], [yes], [
        PKG_CHECK_MODULES([CPU_PLUGIN], [glib-2.0])
        AC_CHECK_HEADERS([stdlib.h string.h unistd.h], [], [
            AC_MSG_ERROR([stdlib.h, string.h, and unistd.h are required for the cpu plugin])
        ])
    ])
])
