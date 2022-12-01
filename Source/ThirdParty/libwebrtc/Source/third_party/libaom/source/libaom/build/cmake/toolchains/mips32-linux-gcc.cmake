#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_TOOLCHAINS_MIPS32_LINUX_GCC_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_MIPS32_LINUX_GCC_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_MIPS32_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

if(ENABLE_DSPR2 AND ENABLE_MSA)
  message(FATAL_ERROR "ENABLE_DSPR2 and ENABLE_MSA cannot be combined.")
endif()

if(ENABLE_DSPR2)
  set(HAVE_DSPR2 1 CACHE BOOL "" FORCE)

  if("${CROSS}" STREQUAL "")

    # Default the cross compiler prefix to something known to work.
    set(CROSS mips-linux-gnu-)
  endif()

  set(MIPS_CFLAGS "-mdspr2")
  set(MIPS_CXXFLAGS "-mdspr2")
elseif(ENABLE_MSA)
  set(HAVE_MSA 1 CACHE BOOL "" FORCE)

  if("${CROSS}" STREQUAL "")

    # Default the cross compiler prefix to something known to work.
    set(CROSS mips-mti-linux-gnu-)
  endif()

  set(MIPS_CFLAGS "-mmsa")
  set(MIPS_CXXFLAGS "-mmsa")
endif()

if("${CROSS}" STREQUAL "")

  # TODO(tomfinegan): Make it possible to turn this off. The $CROSS prefix won't
  # be desired on a mips host.  Default cross compiler prefix to something that
  # might work for an  unoptimized build.
  set(CROSS mips-linux-gnu-)
endif()

if("${MIPS_CPU}" STREQUAL "")
  set(MIPS_CFLAGS "${MIPS_CFLAGS} -mips32r2")
  set(MIPS_CXXFLAGS "${MIPS_CXXFLAGS} -mips32r2")
elseif("${MIPS_CPU}" STREQUAL "p5600")
  set(P56_FLAGS
      "-mips32r5 -mload-store-pairs -msched-weight -mhard-float -mfp64")
  set(MIPS_CFLAGS "${MIPS_CFLAGS} ${P56_FLAGS}")
  set(MIPS_CXXFLAGS "${MIPS_CXXFLAGS} ${P56_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "-mfp64 ${CMAKE_EXE_LINKER_FLAGS}")
endif()

set(CMAKE_C_COMPILER ${CROSS}gcc)
set(CMAKE_CXX_COMPILER ${CROSS}g++)
set(AS_EXECUTABLE ${CROSS}as)
set(CMAKE_C_FLAGS_INIT "-EL ${MIPS_CFLAGS}")
set(CMAKE_CXX_FLAGS_INIT "-EL ${MIPS_CXXFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-EL ${MIPS_CXXFLAGS}")
set(CMAKE_SYSTEM_PROCESSOR "mips32")

# No runtime cpu detect for mips32-linux-gcc.
if(CONFIG_RUNTIME_CPU_DETECT)
  message("--- CONFIG_RUNTIME_CPU_DETECT not supported for mips32 targets.")
endif()

set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE STRING "" FORCE)
