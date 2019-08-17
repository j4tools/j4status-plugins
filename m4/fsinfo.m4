AC_DEFUN([J4STATUS_PLUGINS_PLUGIN_FSINFO], [
    J4SP_ADD_INPUT_PLUGIN(fsinfo, [Disk usage], [yes], [
        PKG_CHECK_MODULES([FSINFO_PLUGIN], [glib-2.0 blkid])
        AC_CHECK_HEADERS([unistd.h mntent.h sys/statvfs.h], [], [
            AC_MSG_ERROR([unistd.h, mntent.h, and sys/statvfs.h are required for the fsinfo plugin])
        ])
        AC_SEARCH_LIBS([blkid_evaluate_tag], [blkid], [
            AC_SUBST(LIBBLKID_CFLAGS, ["-I/usr/include/blkid"])
            AC_SUBST(LIBBLKID_LIBS, ["-lblkid"])
        ], [
            AC_MSG_ERROR([libblkid not found - required by the fsinfo plugin])
        ])
    ])
])
