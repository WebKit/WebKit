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
if(AOM_BUILD_CMAKE_EXPORTS_SOURCES_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_EXPORTS_SOURCES_CMAKE_
set(AOM_BUILD_CMAKE_EXPORTS_SOURCES_CMAKE_ 1)

list(APPEND AOM_EXPORTS_SOURCES "${AOM_ROOT}/aom/exports_com"
            "${AOM_ROOT}/av1/exports_com")

if(CONFIG_AV1_DECODER)
  list(APPEND AOM_EXPORTS_SOURCES "${AOM_ROOT}/aom/exports_dec"
              "${AOM_ROOT}/av1/exports_dec")
  if(CONFIG_INSPECTION)
    list(APPEND AOM_EXPORTS_SOURCES "${AOM_ROOT}/av1/exports_ident")
  endif()
endif()

if(CONFIG_AV1_ENCODER)
  list(APPEND AOM_EXPORTS_SOURCES "${AOM_ROOT}/aom/exports_enc"
              "${AOM_ROOT}/av1/exports_enc")
endif()

if(ENABLE_TESTS)
  list(APPEND AOM_EXPORTS_SOURCES "${AOM_ROOT}/aom/exports_test"
              "${AOM_ROOT}/av1/exports_test")
endif()
