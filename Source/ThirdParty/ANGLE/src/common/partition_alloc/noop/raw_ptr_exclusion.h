// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMMON_PARTITION_ALLOC_NOOP_RAW_PTR_EXCLUSION_H_
#define COMMON_PARTITION_ALLOC_NOOP_RAW_PTR_EXCLUSION_H_

// Marks a field as excluded from the `raw_ptr<T>` usage enforcement via
// Chromium Clang plugin.
//
// This file is adapted from Skia's src/partition_alloc/noop/raw_ptr_exclusion.h:
// https://skia.googlesource.com/skia/+/refs/heads/main/src/partition_alloc/noop/raw_ptr_exclusion.h
// and Chromium's PartitionAlloc pointers implementation:
// https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_exclusion.h

#define RAW_PTR_EXCLUSION

#endif  // COMMON_PARTITION_ALLOC_NOOP_RAW_PTR_EXCLUSION_H_
