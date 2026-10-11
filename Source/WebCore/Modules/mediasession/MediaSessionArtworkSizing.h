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

// https://w3c.github.io/mediasession/#mediametadata-artwork-images
//
//   Specified size:      none
//   Default object size: the ideal artwork size
//   Algorithm:           the default sizing algorithm
//
// FIXME: The spec does not currently say how to size artwork without dimensions. This is being tracked via https://github.com/w3c/mediasession/issues/379.
class MediaSessionArtworkSizing final : public ImageSizingContext {
public:
    static constexpr FloatSize minimumSize { 128, 128 };
    static constexpr FloatSize idealSize { 512, 512 };
    static_assert(minimumSize.maxDimension() < idealSize.maxDimension());

private:
    ObjectSizeNegotiation::SpecifiedSize specifiedSize() const final { return ObjectSizeNegotiation::SpecifiedSize::none(); }
    FloatSize defaultObjectSize() const final { return idealSize; }
};

} // namespace WebCore
