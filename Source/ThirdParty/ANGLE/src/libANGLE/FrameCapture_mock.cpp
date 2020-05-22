//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameCapture_mock.cpp:
//   ANGLE mock Frame capture implementation.
//

#include "libANGLE/FrameCapture.h"

#if ANGLE_CAPTURE_ENABLED
#    error Frame capture must be disabled to include this file.
#endif  // ANGLE_CAPTURE_ENABLED

namespace angle
{
CallCapture::~CallCapture() {}
ParamBuffer::~ParamBuffer() {}
ParamCapture::~ParamCapture() {}
ResourceTracker::ResourceTracker() {}
ResourceTracker::~ResourceTracker() {}

FrameCapture::FrameCapture() {}
FrameCapture::~FrameCapture() {}
void FrameCapture::onEndFrame(const gl::Context *context) {}
void FrameCapture::replay(gl::Context *context) {}
}  // namespace angle
