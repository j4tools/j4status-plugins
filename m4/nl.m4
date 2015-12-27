AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_NL], [
    J4SP_ADD_INPUT_PLUGIN(nl, [Netlink], [yes], [
        GW_CHECK_NL([libnl-route-3.0], [sys/socket.h linux/if_arp.h])
    ])
])
