#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_AOM_MEM_AOM_MEM_CMAKE_)
  return()
endif() # AOM_AOM_MEM_AOM_MEM_CMAKE_
set(AOM_AOM_MEM_AOM_MEM_CMAKE_ 1)

list(APPEND AOM_MEM_SOURCES "${AOM_ROOT}/aom_mem/aom_mem.c"
            "${AOM_ROOT}/aom_mem/aom_mem.h"
            "${AOM_ROOT}/aom_mem/include/aom_mem_intrnl.h")

# Creates the aom_mem build target and makes libaom depend on it. The libaom
# target must exist before this function is called.
function(setup_aom_mem_targets)
  add_library(aom_mem OBJECT ${AOM_MEM_SOURCES})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_mem PARENT_SCOPE)
  target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_mem>)
  if(BUILD_SHARED_LIBS)
    target_sources(aom_static PRIVATE $<TARGET_OBJECTS:aom_mem>)
  endif()
endfunction()
