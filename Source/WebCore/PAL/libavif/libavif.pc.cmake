prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@CMAKE_INSTALL_PREFIX@
libdir=@CMAKE_INSTALL_FULL_LIBDIR@
includedir=@CMAKE_INSTALL_FULL_INCLUDEDIR@

Name: @PROJECT_NAME@
Description: Library for encoding and decoding .avif files
Version: @PROJECT_VERSION@
Libs: -L${libdir} -lavif
Cflags: -I${includedir}@AVIF_PKG_CONFIG_EXTRA_CFLAGS@
