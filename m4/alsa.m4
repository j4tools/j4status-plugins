AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_ALSA], [
    J4SP_ADD_INPUT_PLUGIN(alsa, [ALSA sound support], [yes], [
        GW_CHECK_ALSA_MIXER([glib-2.0], [math.h])
    ])
])
