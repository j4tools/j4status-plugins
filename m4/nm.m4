AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_NM], [
    libnm_min_version=0.9
    J4SP_ADD_INPUT_PLUGIN(nm, [NetworkManager], [yes], [
        AC_CHECK_HEADERS([netdb.h], [], [AC_MSG_ERROR([*** netdb.h is require for the NetworkManager input plugin])])
        PKG_CHECK_MODULES(LIBNM, [libnm-util >= $libnm_min_version libnm-glib >= $libnm_min_version glib-2.0])
    ])
])
