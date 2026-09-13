//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// trace.h: Wrappers for ANGLE trace event functions.
//

#ifndef LIBANGLE_TRACE_H_
#define LIBANGLE_TRACE_H_

#if defined(ANGLE_USE_PERFETTO)

#    include "common/base/anglebase/trace_event/trace_categories.h"
#    include "perfetto/tracing/string_helpers.h"  // nogncheck
#    include "perfetto/tracing/track_event.h"     // nogncheck

#    define ANGLE_TRACE_EVENT_BEGIN(category, ...) TRACE_EVENT_BEGIN(category, ##__VA_ARGS__)
#    define ANGLE_TRACE_EVENT_END(category, ...) TRACE_EVENT_END(category, ##__VA_ARGS__)
#    define ANGLE_TRACE_EVENT_INSTANT(category, ...) TRACE_EVENT_INSTANT(category, ##__VA_ARGS__)
#    define ANGLE_TRACE_EVENT(category, ...) TRACE_EVENT(category, ##__VA_ARGS__)
#    define ANGLE_TRACE_COUNTER(category, ...) TRACE_COUNTER(category, ##__VA_ARGS__)

#else  // !defined(ANGLE_USE_PERFETTO)

#    define ANGLE_TRACE_EVENT_BEGIN(CATEGORY, ...) ((void)0)
#    define ANGLE_TRACE_EVENT_END(CATEGORY, ...) ((void)0)
#    define ANGLE_TRACE_EVENT_INSTANT(CATEGORY, ...) ((void)0)
#    define ANGLE_TRACE_EVENT(CATEGORY, ...) ((void)0)
#    define ANGLE_TRACE_COUNTER(CATEGORY, ...) ((void)0)

#endif  // !defined(ANGLE_USE_PERFETTO)

// Deprecated, use ANGLE_TRACE_EVENT
#define ANGLE_TRACE_EVENT0(CATEGORY, EVENT) ANGLE_TRACE_EVENT(CATEGORY, EVENT)
// Deprecated, use ANGLE_TRACE_EVENT
#define ANGLE_TRACE_EVENT1(CATEGORY, EVENT, NAME, VAL) ANGLE_TRACE_EVENT(CATEGORY, EVENT, NAME, VAL)

#endif  // LIBANGLE_TRACE_H_
