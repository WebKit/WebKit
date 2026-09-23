//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// scoped_capture_exclude.h:
//   Scoped wrapper that brackets a series of GL/EGL calls with glDebugMessageInsert
//   markers so ANGLE frame-capture can skip recording them during retraces/trace upgrades
//

#ifndef UTIL_CAPTURE_SCOPED_CAPTURE_EXCLUDE_H_
#define UTIL_CAPTURE_SCOPED_CAPTURE_EXCLUDE_H_

#include "common/frame_capture_shared.h"

namespace angle
{
// For perf, this runs the detection only the first time and preserves the result.
inline bool RetraceModeActive()
{
    static const bool retraceModeActive = angle::IsCaptureConfiguredFromEnv();
    return retraceModeActive;
}

// This class uses KHR debug calls to mark the beginning and end of a call sequence to be
// skipped for capture during a retrace or trace upgrade
class ScopedCaptureExclude
{
  public:
    ScopedCaptureExclude()
    {
        if (RetraceModeActive())
        {
            glDebugMessageInsertKHR(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER,
                                    angle::kFixtureInjectedCommandsBeginId,
                                    GL_DEBUG_SEVERITY_NOTIFICATION, -1, "");
        }
    }
    ~ScopedCaptureExclude()
    {
        if (RetraceModeActive())
        {
            glDebugMessageInsertKHR(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER,
                                    angle::kFixtureInjectedCommandsEndId,
                                    GL_DEBUG_SEVERITY_NOTIFICATION, -1, "");
        }
    }
};
}  // namespace angle

#endif  // UTIL_CAPTURE_SCOPED_CAPTURE_EXCLUDE_H_
