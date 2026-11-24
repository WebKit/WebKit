/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/ColorSpace.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class Gradient;

// Looks up the CGGradients of Gradients. A CGGradient is immutable, so it can be shared across threads.
class GradientCacheCG {
public:
    // Returns the renderer color space for painting into a destination of the given color space.
    static std::optional<ColorSpace> colorSpaceForDestination(const ColorSpace&);

    // Returns the CGGradient of the gradient in the renderer color space. The CGGradients of non-transient
    // gradients, and of gradients that are created by sampling, are cached process-wide and shared by the
    // gradients with the same color interpolation method and color stops. The other CGGradients of transient
    // gradients are created on each call.
    static RetainPtr<CGGradientRef> cgGradient(const Gradient&, const std::optional<ColorSpace>&);
};

}
