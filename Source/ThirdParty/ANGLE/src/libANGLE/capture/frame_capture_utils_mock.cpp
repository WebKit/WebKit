//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_utils_mock.cpp:
//   ANGLE frame capture util stub implementation.
//

#include "libANGLE/capture/frame_capture_utils.h"

namespace angle
{
Result SerializeContextToString(const gl::Context *context, std::string *stringOut)
{
    *stringOut = "SerializationNotAvailable";
    return angle::Result::Continue;
}
}  // namespace angle
