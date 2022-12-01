#
# Copyright (c) 2018, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
list(APPEND AOM_INSTALL_INCS "${AOM_ROOT}/aom/aom.h"
            "${AOM_ROOT}/aom/aom_codec.h" "${AOM_ROOT}/aom/aom_frame_buffer.h"
            "${AOM_ROOT}/aom/aom_image.h" "${AOM_ROOT}/aom/aom_integer.h")

if(CONFIG_AV1_DECODER)
  list(APPEND AOM_INSTALL_INCS "${AOM_ROOT}/aom/aom_decoder.h"
              "${AOM_ROOT}/aom/aomdx.h")
endif()

if(CONFIG_AV1_ENCODER)
  list(APPEND AOM_INSTALL_INCS "${AOM_ROOT}/aom/aomcx.h"
              "${AOM_ROOT}/aom/aom_encoder.h"
              "${AOM_ROOT}/aom/aom_external_partition.h")
endif()

# Generate aom.pc and setup dependencies to ensure it is created when necessary.
# Note: aom.pc generation uses GNUInstallDirs:
# https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html
macro(setup_aom_install_targets)
  if(NOT XCODE)
    include("GNUInstallDirs")
    set(AOM_PKG_CONFIG_FILE "${AOM_CONFIG_DIR}/aom.pc")

    # Create a dummy library target for creating aom.pc.
    create_dummy_source_file(aom_pc c AOM_PKG_CONFIG_SOURCES)
    add_library(aom_pc ${AOM_PKG_CONFIG_SOURCES})

    # Setup a rule to generate aom.pc.
    add_custom_command(
      OUTPUT "${AOM_PKG_CONFIG_FILE}"
      COMMAND ${CMAKE_COMMAND} ARGS
              -DAOM_CONFIG_DIR=${AOM_CONFIG_DIR}
              -DAOM_ROOT=${AOM_ROOT}
              -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
              -DCMAKE_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
              -DCMAKE_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}
              -DCMAKE_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
              -DCMAKE_PROJECT_NAME=${CMAKE_PROJECT_NAME}
              -DCONFIG_MULTITHREAD=${CONFIG_MULTITHREAD}
              -DCONFIG_TUNE_VMAF=${CONFIG_TUNE_VMAF}
              -DCONFIG_TUNE_BUTTERAUGLI=${CONFIG_TUNE_BUTTERAUGLI}
              -DCONFIG_TFLITE=${CONFIG_TFLITE}
              -DHAVE_PTHREAD_H=${HAVE_PTHREAD_H}
              -P
              "${AOM_ROOT}/build/cmake/pkg_config.cmake"
      COMMENT "Writing aom.pc"
      VERBATIM)

    # Explicitly add a dependency on the pkg-config file to ensure it's built.
    get_property(aom_pc_sources TARGET aom_pc PROPERTY SOURCES)
    set_source_files_properties(${aom_pc_sources} OBJECT_DEPENDS
                                "${AOM_PKG_CONFIG_FILE}")

    # Our pkg-config file carries version information: add a dependency on the
    # version rule.
    add_dependencies(aom_pc aom_version)

    if(CONFIG_AV1_DECODER)
      if(ENABLE_EXAMPLES)
        list(APPEND AOM_INSTALL_BINS aomdec)
      endif()
    endif()

    if(CONFIG_AV1_ENCODER)
      if(ENABLE_EXAMPLES)
        list(APPEND AOM_INSTALL_BINS aomenc)
      endif()
    endif()

    if(BUILD_SHARED_LIBS)
      set(AOM_INSTALL_LIBS aom aom_static)
    else()
      set(AOM_INSTALL_LIBS aom)
    endif()

    # Setup the install rules. install() will automatically prepend
    # CMAKE_INSTALL_PREFIX to relative paths
    install(FILES ${AOM_INSTALL_INCS}
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/aom")
    install(FILES "${AOM_PKG_CONFIG_FILE}"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    install(TARGETS ${AOM_INSTALL_LIBS};${AOM_INSTALL_BINS}
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
  endif()
endmacro()
