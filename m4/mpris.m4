AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_MPRIS], [
    J4SP_ADD_INPUT_PLUGIN([mpris], [MPRIS D-Bus interface], [yes], [
        AC_ARG_VAR([MPRIS_GDBUS_CODEGEN], [The gdbus-codegen executable])
        PKG_CHECK_MODULES(MPRIS_PLUGIN, [gobject-2.0 gio-unix-2.0])
        AC_PATH_PROG([MPRIS_GDBUS_CODEGEN], [gdbus-codegen])
        if test -z "$MPRIS_GDBUS_CODEGEN"; then
            MPRIS_GDBUS_CODEGEN=`$PKG_CONFIG --variable=gdbus_codegen gio-2.0`
        fi
        if test -z "$MPRIS_GDBUS_CODEGEN"; then
            AC_MSG_ERROR([*** gdbus-codegen is required to build the MPRIS plugin])
        fi
    ])
])
