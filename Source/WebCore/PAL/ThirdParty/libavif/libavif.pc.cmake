prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=@PC_LIBDIR@
includedir=@PC_INCLUDEDIR@

Name: @PROJECT_NAME@
Description: Library for encoding and decoding .avif files
Version: @PROJECT_VERSION@
Libs: -L${libdir} -lavif
Libs.private:@AVIF_PKG_CONFIG_EXTRA_LIBS_PRIVATE@
Cflags: -I${includedir}@AVIF_PKG_CONFIG_EXTRA_CFLAGS@
Cflags.private: -UAVIF_DLL
Requires.private:@AVIF_PKG_CONFIG_EXTRA_REQUIRES_PRIVATE@
