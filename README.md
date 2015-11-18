j4status-plugins
================

j4status-plugins is a collection of plugins to go further with j4status.
It contains plugins that are not part of the “core” set of plugins (maintained by j4status developper).
However, plugins in this collection have been reviewed and tested by j4status developper.


Build from Git
--------------

To build j4status-plugins from Git, you will need some additional dependencies:
- autoconf 2.65 (or newer)
- automake 1.11 (or newer)
- libtool
- pkg-config 0.25 (or newer) or pkgconf 0.2 (or newer)

You also need to make sure Autotools and pkg-config will find the j4status files.
Therefore, before running `./autogen.sh`, you will probably need to export these two variables:
```
export ACLOCAL_FLAGS='-I /usr/local/share/aclocal'
export PKG_CONFIG_PATH='/usr/local/lib/pkgconfig'
```
