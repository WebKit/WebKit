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

    // Program binaries don't contain transform feedback varyings on Qualcomm GPUs.
    // Work around this by disabling the program cache for programs with transform feedback.
    bool disableProgramCachingForTransformFeedback = false;

    // On Windows Intel OpenGL drivers TexImage sometimes seems to interact with the Framebuffer.
    // Flaky crashes can occur unless we sync the Framebuffer bindings. The workaround is to add
    // Framebuffer binding dirty bits to TexImage updates. See http://anglebug.com/2906
    bool syncFramebufferBindingsOnTexImage = false;
};
}  // namespace gl

#endif  // LIBANGLE_WORKAROUNDS_H_
