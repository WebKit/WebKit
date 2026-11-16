/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TRACE_CATEGORIES_H_
#define RTC_BASE_TRACE_CATEGORIES_H_

#if defined(RTC_USE_PERFETTO)

#define PERFETTO_ENABLE_LEGACY_TRACE_EVENTS 1

#include "rtc_base/system/rtc_export.h"
// IWYU pragma: begin_exports
#include "third_party/perfetto/include/perfetto/tracing/track_event.h"  // nogncheck
// IWYU pragma: end_exports
#include "third_party/perfetto/include/perfetto/tracing/track_event_category_registry.h"  // nogncheck
// IWYU pragma: begin_exports
#include "third_party/perfetto/include/perfetto/tracing/track_event_legacy.h"  // nogncheck
// IWYU pragma: end_exports
PERFETTO_DEFINE_TEST_CATEGORY_PREFIXES("webrtc-test");

PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS(
    webrtc,
    RTC_EXPORT,
    perfetto::Category("webrtc"),
    perfetto::Category("webrtc_stats"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("webrtc")).SetTags("slow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("webrtc_stats"))
        .SetTags("slow"));

PERFETTO_USE_CATEGORIES_FROM_NAMESPACE(webrtc);

#endif  // RTC_USE_PERFETTO

#endif  // RTC_BASE_TRACE_CATEGORIES_H_
