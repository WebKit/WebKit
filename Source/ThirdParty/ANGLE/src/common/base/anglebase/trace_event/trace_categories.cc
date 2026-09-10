// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common/base/anglebase/trace_event/trace_categories.h"

#if defined(ANGLE_USE_PERFETTO)
PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE_WITH_ATTRS(angle_tracing, ANGLEBASE_EXPORT);

#endif
