//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// histogram_macros.h:
//   Helpers for making histograms, to keep consistency with Chromium's
//   histogram_macros.h.

#ifndef LIBANGLE_HISTOGRAM_MACROS_H_
#define LIBANGLE_HISTOGRAM_MACROS_H_

#include <platform/Platform.h>

#define ANGLE_HISTOGRAM_TIMES(name, sample) ANGLE_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, 1, 10000, 50)

#define ANGLE_HISTOGRAM_MEDIUM_TIMES(name, sample) ANGLE_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, 10, 180000, 50)

// Use this macro when times can routinely be much longer than 10 seconds.
#define ANGLE_HISTOGRAM_LONG_TIMES(name, sample) ANGLE_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, 1, 3600000, 50)

// Use this macro when times can routinely be much longer than 10 seconds and
// you want 100 buckets.
#define ANGLE_HISTOGRAM_LONG_TIMES_100(name, sample) ANGLE_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, 1, 3600000, 100)

// For folks that need real specific times, use this to select a precise range
// of times you want plotted, and the number of buckets you want used.
#define ANGLE_HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    ANGLE_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count)

#define ANGLE_HISTOGRAM_COUNTS(name, sample) ANGLE_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000000, 50)

#define ANGLE_HISTOGRAM_COUNTS_100(name, sample) \
    ANGLE_HISTOGRAM_CUSTOM_COUNTS(name, sample, 1, 100, 50)

#define ANGLE_HISTOGRAM_COUNTS_10000(name, sample) \
    ANGLE_HISTOGRAM_CUSTOM_COUNTS(name, sample, 1, 10000, 50)

#define ANGLE_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    ANGLEPlatformCurrent()->histogramCustomCounts(\
      name, sample, min, max, bucket_count)

#define ANGLE_HISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    ANGLE_HISTOGRAM_ENUMERATION(name, under_one_hundred, 101)

#define ANGLE_HISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    ANGLEPlatformCurrent()->histogramEnumeration(name, sample, boundary_value)

#define ANGLE_HISTOGRAM_MEMORY_KB(name, sample) ANGLE_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1000, 500000, 50)

#define ANGLE_HISTOGRAM_MEMORY_MB(name, sample) ANGLE_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000, 50)

#endif  // BASE_METRICS_HISTOGRAM_MACROS_H_
