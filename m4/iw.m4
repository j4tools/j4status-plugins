AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_IW], [
    J4SP_ADD_INPUT_PLUGIN(iw, [Wireless Extensons interface], [no], [
        PKG_CHECK_MODULES([IW_PLUGIN], [glib-2.0])
        AC_CHECK_HEADERS([string.h net/if.h netinet/in.h], [], [
            AC_MSG_ERROR([string.h, net/if.h, and netinet/in.h are required for the iw plugin])
        ])
        AC_SEARCH_LIBS([iw_sockets_open], [iw], [
            AC_SUBST(LIBIW_CFLAGS, ["-I/usr/include"])
            AC_SUBST(LIBIW_LIBS, ["-liw"])
        ], [
            AC_MSG_ERROR([libiw not found - required by iw plugin])
        ])
    ])
])
