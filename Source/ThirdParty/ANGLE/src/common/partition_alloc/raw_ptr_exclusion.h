// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMMON_PARTITION_ALLOC_RAW_PTR_EXCLUSION_H_
#define COMMON_PARTITION_ALLOC_RAW_PTR_EXCLUSION_H_

#if defined(ANGLE_USE_PARTITION_ALLOC)
#    include "partition_alloc/pointers/raw_ptr_exclusion.h"
#else
#    include "common/partition_alloc/noop/raw_ptr_exclusion.h"
#endif

#endif  // COMMON_PARTITION_ALLOC_RAW_PTR_EXCLUSION_H_
