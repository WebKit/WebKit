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
if(AOM_BUILD_CMAKE_TOOLCHAINS_MIPS64_LINUX_GCC_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_MIPS64_LINUX_GCC_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_MIPS64_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

if("${CROSS}" STREQUAL "")

  # TODO(tomfinegan): Make it possible to turn this off. The $CROSS prefix won't
  # be desired on a mips host.
  #
  # Default the cross compiler prefix to something known to work.
  set(CROSS mips-img-linux-gnu-)
endif()

if(ENABLE_MSA)
  set(HAVE_MSA 1 CACHE BOOL "" FORCE)
  set(MIPS_CFLAGS "-mmsa")
  set(MIPS_CXXFLAGS "-mmsa")
endif()

if("${MIPS_CPU}" STREQUAL "i6400" OR "${MIPS_CPU}" STREQUAL "p6600")
  set(MIPS_CPU_FLAGS "-mips64r6 -mabi=64 -mload-store-pairs -msched-weight")
  set(MIPS_CPU_FLAGS "${MIPS_CPU_FLAGS} -mhard-float -mfp64")
  set(MIPS_CFLAGS "${MIPS_CFLAGS} ${MIPS_CPU_FLAGS}")
  set(MIPS_CXXFLAGS "${MIPS_CXXFLAGS} ${MIPS_CPU_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS
      "-mips64r6 -mabi64 -mfp64 ${CMAKE_EXE_LINKER_FLAGS}")
endif()

set(CMAKE_C_COMPILER ${CROSS}gcc)
set(CMAKE_CXX_COMPILER ${CROSS}g++)
set(AS_EXECUTABLE ${CROSS}as)
set(CMAKE_C_FLAGS_INIT "-EL ${MIPS_CFLAGS}")
set(CMAKE_CXX_FLAGS_INIT "-EL ${MIPS_CXXFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-EL ${MIPS_CXXFLAGS}")
set(CMAKE_SYSTEM_PROCESSOR "mips64")

# No runtime cpu detect for mips64-linux-gcc.
if(CONFIG_RUNTIME_CPU_DETECT)
  message("--- CONFIG_RUNTIME_CPU_DETECT not supported for mips64 targets.")
endif()

set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE STRING "" FORCE)
