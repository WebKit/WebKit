//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Workarounds.h: Workarounds for driver bugs and other behaviors seen
// on all platforms.

#ifndef LIBANGLE_WORKAROUNDS_H_
#define LIBANGLE_WORKAROUNDS_H_

namespace gl
{

struct Workarounds
{
    // Force the context to be lost (via KHR_robustness) if a GL_OUT_OF_MEMORY error occurs. The
    // driver may be in an inconsistent state if this happens, and some users of ANGLE rely on this
    // notification to prevent further execution.
    bool loseContextOnOutOfMemory = false;
};
}  // namespace gl

#endif  // LIBANGLE_WORKAROUNDS_H_
