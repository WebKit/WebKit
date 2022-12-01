#
# Copyright (c) 2021, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_COMMON_IVF_DEC_CMAKE_)
  return()
endif() # AOM_COMMON_AOM_COMMON_CMAKE_
set(AOM_COMMON_IVF_DEC_CMAKE_ 1)

list(APPEND IVF_DEC_SOURCES "${AOM_ROOT}/common/ivfdec.c"
            "${AOM_ROOT}/common/ivfdec.h")

# Creates the aom_common build target and makes libaom depend on it. The libaom
# target must exist before this function is called.
function(setup_ivf_dec_targets)
  add_library(ivf_dec OBJECT ${IVF_DEC_SOURCES})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} ivf_dec PARENT_SCOPE)
  target_sources(aom PRIVATE $<TARGET_OBJECTS:ivf_dec>)
  if(BUILD_SHARED_LIBS)
    target_sources(aom_static PRIVATE $<TARGET_OBJECTS:ivf_dec>)
  endif()
endfunction()
