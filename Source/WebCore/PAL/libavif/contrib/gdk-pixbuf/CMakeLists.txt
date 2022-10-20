# Copyright 2020 Emmanuel Gil Peyrot. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause

option(AVIF_BUILD_GDK_PIXBUF "Build a gdk-pixbuf loader" OFF)
if(AVIF_BUILD_GDK_PIXBUF)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_search_module(GDK_PIXBUF gdk-pixbuf-2.0)
        if(GDK_PIXBUF_FOUND)
            set(GDK_PIXBUF_SRCS loader.c)
            add_library(pixbufloader-avif MODULE ${GDK_PIXBUF_SRCS})

            target_link_libraries(pixbufloader-avif PUBLIC ${GDK_PIXBUF_LIBRARIES} avif)
            target_include_directories(pixbufloader-avif PUBLIC ${GDK_PIXBUF_INCLUDE_DIRS})

            pkg_get_variable(GDK_PIXBUF_MODULEDIR gdk-pixbuf-2.0 gdk_pixbuf_moduledir)
            string(REPLACE ${GDK_PIXBUF_PREFIX} ${CMAKE_INSTALL_PREFIX} GDK_PIXBUF_MODULEDIR ${GDK_PIXBUF_MODULEDIR})
            install(TARGETS pixbufloader-avif DESTINATION ${GDK_PIXBUF_MODULEDIR})

            configure_file(avif.thumbnailer.in ${CMAKE_CURRENT_BINARY_DIR}/avif.thumbnailer @ONLY)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/avif.thumbnailer DESTINATION ${CMAKE_INSTALL_DATADIR}/thumbnailers)
        else()
            message(WARNING "gdk-pixbuf loader: disabled due to missing gdk-pixbuf-2.0")
        endif()
    else()
        message(WARNING "gdk-pixbuf loader: disabled due to missing pkg-config")
    endif()
endif()
