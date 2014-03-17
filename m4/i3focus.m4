AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_I3FOCUS], [
    J4SP_ADD_INPUT_PLUGIN(i3focus, [i3 focus event], [no], [
        PKG_CHECK_MODULES([I3FOCUS_PLUGIN], [i3ipc-glib-1.0 gobject-2.0 glib-2.0])
    ])
])
