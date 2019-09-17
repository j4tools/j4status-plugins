AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_IW], [
    J4SP_ADD_INPUT_PLUGIN(iw, [Wireless Extensons interface], [no], [
        PKG_CHECK_MODULES([IW_PLUGIN], [glib-2.0])
        AC_CHECK_HEADERS([iwlib.h string.h net/if.h netinet/in.h], [], [
            AC_MSG_ERROR([iwlib.h, string.h, net/if.h, and netinet/in.h are required for the iw plugin])
        ])
        AC_CHECK_LIB([iw], [iw_sockets_open], [
            AC_SUBST([IW_PLUGIN_LIBS], ["$IW_PLUGIN_LIBS -liw"])
        ], [
            AC_MSG_ERROR([libiw not found - required by iw plugin])
        ])
    ])
])
