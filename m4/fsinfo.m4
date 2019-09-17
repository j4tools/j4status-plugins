AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_FSINFO], [
    J4SP_ADD_INPUT_PLUGIN(fsinfo, [Disk usage], [yes], [
        PKG_CHECK_MODULES([FSINFO_PLUGIN], [glib-2.0 blkid])
        AC_CHECK_HEADERS([unistd.h mntent.h sys/statvfs.h], [], [
            AC_MSG_ERROR([unistd.h, mntent.h, and sys/statvfs.h are required for the fsinfo plugin])
        ])
    ])
])
