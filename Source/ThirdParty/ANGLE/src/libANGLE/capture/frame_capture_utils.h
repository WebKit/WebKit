//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_utils.h:
//   ANGLE frame capture utils interface.
//
#ifndef FRAME_CAPTURE_UTILS_H_
#define FRAME_CAPTURE_UTILS_H_

#include "libANGLE/Error.h"

namespace gl
{
class Context;
}  // namespace gl

namespace angle
{
Result SerializeContextToString(const gl::Context *context, std::string *stringOut);
}  // namespace angle
#endif  // FRAME_CAPTURE_UTILS_H_
