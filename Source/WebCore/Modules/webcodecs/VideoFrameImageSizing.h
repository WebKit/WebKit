/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ImageSizingContext.h"

namespace WebCore {

// https://w3c.github.io/webcodecs/#dom-videoframe-videoframe
//
//   Specified size:      none
//   Default object size: none -- an image with no natural dimensions is rejected first
//   Algorithm:           the default sizing algorithm
//
// The density applies to the default display size, which is the image's natural size in CSS
// pixels, and not to the coded size, which is the size of the resource in image pixels.
class VideoFrameImageSizing final : public ImageSizingContext {
public:
    explicit VideoFrameImageSizing(float density = 1)
        : m_density(density)
    {
    }

private:
    ObjectSizeNegotiation::SpecifiedSize specifiedSize() const final { return ObjectSizeNegotiation::SpecifiedSize::none(); }
    FloatSize defaultObjectSize() const final { return ObjectSizeNegotiation::defaultObjectSize; }
    float density() const final { return m_density; }

    float m_density { 1 };
};

} // namespace WebCore
