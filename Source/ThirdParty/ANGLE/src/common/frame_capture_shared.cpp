//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_shared.cpp:
//   Common code between the ANGLE frame-capture runtime and the replay trace fixture.
//

#include "common/frame_capture_shared.h"

namespace angle
{
bool IsCaptureEnabledFromEnv()
{
    // Capture is enabled unless explicitly turned off
    return GetEnvironmentVarOrUnCachedAndroidProperty(kEnabledVarName, kAndroidEnabled) != "0";
}

bool IsCaptureConfiguredFromEnv()
{
    if (!IsCaptureEnabledFromEnv())
    {
        return false;
    }
    // Capture runs if a start frame or a trigger is configured
    bool frameStartSet =
        !GetEnvironmentVarOrUnCachedAndroidProperty(kFrameStartVarName, kAndroidFrameStart).empty();
    bool triggerSet =
        !GetEnvironmentVarOrUnCachedAndroidProperty(kTriggerVarName, kAndroidTrigger).empty();
    return frameStartSet || triggerSet;
}
}  // namespace angle
