//
// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#ifndef WEBRTC_SYSTEM_WRAPPERS_INCLUDE_METRICS_H_
#define WEBRTC_SYSTEM_WRAPPERS_INCLUDE_METRICS_H_

#include <string>

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/include/logging.h"

// Macros for allowing WebRTC clients (e.g. Chrome) to gather and aggregate
// statistics.
//
// Histogram for counters.
// RTC_HISTOGRAM_COUNTS(name, sample, min, max, bucket_count);
//
// Histogram for enumerators.
// The boundary should be above the max enumerator sample.
// RTC_HISTOGRAM_ENUMERATION(name, sample, boundary);
//
//
// The macros use the methods HistogramFactoryGetCounts,
// HistogramFactoryGetEnumeration and HistogramAdd.
//
// Therefore, WebRTC clients must either:
//
// - provide implementations of
//   Histogram* webrtc::metrics::HistogramFactoryGetCounts(
//       const std::string& name, int sample, int min, int max,
//       int bucket_count);
//   Histogram* webrtc::metrics::HistogramFactoryGetEnumeration(
//       const std::string& name, int sample, int boundary);
//   void webrtc::metrics::HistogramAdd(
//       Histogram* histogram_pointer, const std::string& name, int sample);
//
// - or link with the default implementations (i.e.
//   system_wrappers:metrics_default).
//
//
// Example usage:
//
// RTC_HISTOGRAM_COUNTS("WebRTC.Video.NacksSent", nacks_sent, 1, 100000, 100);
//
// enum Types {
//   kTypeX,
//   kTypeY,
//   kBoundary,
// };
//
// RTC_HISTOGRAM_ENUMERATION("WebRTC.Types", kTypeX, kBoundary);
//
// NOTE: It is recommended to do the Chromium review for modifications to
// histograms.xml before new metrics are committed to WebRTC.


// Macros for adding samples to a named histogram.

// Histogram for counters (exponentially spaced buckets).
#define RTC_HISTOGRAM_COUNTS_100(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 100, 50)

#define RTC_HISTOGRAM_COUNTS_200(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 200, 50)

#define RTC_HISTOGRAM_COUNTS_500(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 500, 50)

#define RTC_HISTOGRAM_COUNTS_1000(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 1000, 50)

#define RTC_HISTOGRAM_COUNTS_10000(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 10000, 50)

#define RTC_HISTOGRAM_COUNTS_100000(name, sample) \
  RTC_HISTOGRAM_COUNTS(name, sample, 1, 100000, 50)

#define RTC_HISTOGRAM_COUNTS(name, sample, min, max, bucket_count) \
  RTC_HISTOGRAM_COMMON_BLOCK(name, sample, \
      webrtc::metrics::HistogramFactoryGetCounts(name, min, max, bucket_count))

#define RTC_HISTOGRAM_COUNTS_LINEAR(name, sample, min, max, bucket_count)      \
  RTC_HISTOGRAM_COMMON_BLOCK(name, sample,                                     \
                             webrtc::metrics::HistogramFactoryGetCountsLinear( \
                                 name, min, max, bucket_count))

// Deprecated.
// TODO(asapersson): Remove.
#define RTC_HISTOGRAM_COUNTS_SPARSE_100(name, sample) \
  RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, 1, 100, 50)

#define RTC_HISTOGRAM_COUNTS_SPARSE(name, sample, min, max, bucket_count) \
  RTC_HISTOGRAM_COMMON_BLOCK_SLOW(name, sample, \
      webrtc::metrics::HistogramFactoryGetCounts(name, min, max, bucket_count))

// Histogram for percentage (evenly spaced buckets).
#define RTC_HISTOGRAM_PERCENTAGE(name, sample) \
  RTC_HISTOGRAM_ENUMERATION(name, sample, 101)

// Histogram for booleans.
#define RTC_HISTOGRAM_BOOLEAN(name, sample) \
  RTC_HISTOGRAM_ENUMERATION(name, sample, 2)

// Histogram for enumerators (evenly spaced buckets).
// |boundary| should be above the max enumerator sample.
#define RTC_HISTOGRAM_ENUMERATION(name, sample, boundary) \
  RTC_HISTOGRAM_COMMON_BLOCK(name, sample, \
      webrtc::metrics::HistogramFactoryGetEnumeration(name, boundary))

// The name of the histogram should not vary.
// TODO(asapersson): Consider changing string to const char*.
#define RTC_HISTOGRAM_COMMON_BLOCK(constant_name, sample,                  \
                                   factory_get_invocation)                 \
  do {                                                                     \
    static webrtc::metrics::Histogram* atomic_histogram_pointer = nullptr; \
    webrtc::metrics::Histogram* histogram_pointer =                        \
        rtc::AtomicOps::AcquireLoadPtr(&atomic_histogram_pointer);         \
    if (!histogram_pointer) {                                              \
      histogram_pointer = factory_get_invocation;                          \
      webrtc::metrics::Histogram* prev_pointer =                           \
          rtc::AtomicOps::CompareAndSwapPtr(                               \
              &atomic_histogram_pointer,                                   \
              static_cast<webrtc::metrics::Histogram*>(nullptr),           \
              histogram_pointer);                                          \
      RTC_DCHECK(prev_pointer == nullptr ||                                \
                 prev_pointer == histogram_pointer);                       \
    }                                                                      \
    if (histogram_pointer) {                                               \
      RTC_DCHECK_EQ(constant_name,                                         \
                    webrtc::metrics::GetHistogramName(histogram_pointer))  \
          << "The name should not vary.";                                  \
      webrtc::metrics::HistogramAdd(histogram_pointer, sample);            \
    }                                                                      \
  } while (0)

// Deprecated.
// The histogram is constructed/found for each call.
// May be used for histograms with infrequent updates.`
#define RTC_HISTOGRAM_COMMON_BLOCK_SLOW(name, sample, factory_get_invocation) \
  do {                                                                        \
    webrtc::metrics::Histogram* histogram_pointer = factory_get_invocation;   \
    if (histogram_pointer) {                                                  \
      webrtc::metrics::HistogramAdd(histogram_pointer, sample);               \
    }                                                                         \
  } while (0)

// Helper macros.
// Macros for calling a histogram with varying name (e.g. when using a metric
// in different modes such as real-time vs screenshare).
#define RTC_HISTOGRAMS_COUNTS_100(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_COUNTS(name, sample, 1, 100, 50))

#define RTC_HISTOGRAMS_COUNTS_200(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_COUNTS(name, sample, 1, 200, 50))

#define RTC_HISTOGRAMS_COUNTS_500(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_COUNTS(name, sample, 1, 500, 50))

#define RTC_HISTOGRAMS_COUNTS_1000(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_COUNTS(name, sample, 1, 1000, 50))

#define RTC_HISTOGRAMS_COUNTS_10000(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_COUNTS(name, sample, 1, 10000, 50))

#define RTC_HISTOGRAMS_COUNTS_100000(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_COUNTS(name, sample, 1, 100000, 50))

#define RTC_HISTOGRAMS_ENUMERATION(index, name, sample, boundary) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_ENUMERATION(name, sample, boundary))

#define RTC_HISTOGRAMS_PERCENTAGE(index, name, sample) \
  RTC_HISTOGRAMS_COMMON(index, name, sample, \
      RTC_HISTOGRAM_PERCENTAGE(name, sample))

#define RTC_HISTOGRAMS_COMMON(index, name, sample, macro_invocation) \
  do { \
    switch (index) { \
      case 0: \
        macro_invocation; \
        break; \
      case 1: \
        macro_invocation; \
        break; \
      case 2: \
        macro_invocation; \
        break; \
      default: \
        RTC_NOTREACHED(); \
    } \
  } while (0)


namespace webrtc {
namespace metrics {

// Time that should have elapsed for stats that are gathered once per call.
enum { kMinRunTimeInSeconds = 10 };

class Histogram;

// Functions for getting pointer to histogram (constructs or finds the named
// histogram).

// Get histogram for counters.
Histogram* HistogramFactoryGetCounts(
    const std::string& name, int min, int max, int bucket_count);

// Get histogram for counters with linear bucket spacing.
Histogram* HistogramFactoryGetCountsLinear(const std::string& name,
                                           int min,
                                           int max,
                                           int bucket_count);

// Get histogram for enumerators.
// |boundary| should be above the max enumerator sample.
Histogram* HistogramFactoryGetEnumeration(
    const std::string& name, int boundary);

// Returns name of the histogram.
const std::string& GetHistogramName(Histogram* histogram_pointer);

// Function for adding a |sample| to a histogram.
void HistogramAdd(Histogram* histogram_pointer, int sample);

}  // namespace metrics
}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INCLUDE_METRICS_H_
