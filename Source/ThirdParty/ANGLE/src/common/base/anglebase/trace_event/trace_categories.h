// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANGLEBASE_TRACE_CATEGORIES_H_
#define ANGLEBASE_TRACE_CATEGORIES_H_

#if defined(ANGLE_USE_PERFETTO)

#    include "common/base/anglebase/base_export.h"
#    include "perfetto/tracing/track_event.h"

// List of categories used by built-in angle trace events.
PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS(angle_tracing,
                                                   ANGLEBASE_EXPORT,
                                                   perfetto::Category("gpu.angle"),
                                                   perfetto::Category("gpu.angle.gpu"),
                                                   perfetto::Category("gpu.angle.texture_metrics"));

PERFETTO_USE_CATEGORIES_FROM_NAMESPACE(angle_tracing);

#endif  // defined(ANGLE_USE_PERFETTO)

#endif  // ANGLEBASE_TRACE_CATEGORIES_H_
