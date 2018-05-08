AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_NM], [
    libnm_min_version=1.10
    J4SP_ADD_INPUT_PLUGIN(nm, [NetworkManager], [yes], [
        AC_CHECK_HEADERS([netdb.h], [], [AC_MSG_ERROR([*** netdb.h is require for the NetworkManager input plugin])])
        PKG_CHECK_MODULES(LIBNM, [libnm >= $libnm_min_version glib-2.0])
    ])
])
