AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_INOTIFY], [
    J4SP_ADD_INPUT_PLUGIN(inotify, [Inotify], [yes], [
        PKG_CHECK_MODULES([INOTIFY_PLUGIN], [gobject-2.0 glib-2.0 gio-unix-2.0])
        AC_CHECK_HEADERS([sys/inotify.h pthread.h], [], [AC_MSG_ERROR([sys/inotify.h and pthread.h required for plugin inotify])])
    ])
])
