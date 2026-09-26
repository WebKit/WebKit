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

#include <WebCore/ImageSizingContext.h>

namespace WebCore {

// https://html.spec.whatwg.org/multipage/canvas.html#dom-context-2d-drawimage
//
//   Specified size:      none
//   Default object size: the size of the output bitmap
//   Algorithm:           the default sizing algorithm
//
// The density applies to the destination rectangle, which is in CSS pixels, and not to the
// source rectangle, which is in image pixels.
class CanvasDrawImageSizing final : public ImageSizingContext {
public:
    explicit CanvasDrawImageSizing(FloatSize outputBitmapSize, float density = 1)
        : m_outputBitmapSize(outputBitmapSize)
        , m_density(density)
    {
    }

private:
    ObjectSizeNegotiation::SpecifiedSize specifiedSize() const final { return ObjectSizeNegotiation::SpecifiedSize::none(); }
    FloatSize defaultObjectSize() const final { return m_outputBitmapSize; }
    float density() const final { return m_density; }

    FloatSize m_outputBitmapSize;
    float m_density { 1 };
};

} // namespace WebCore
