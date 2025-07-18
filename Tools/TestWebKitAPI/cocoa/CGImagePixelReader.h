/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreGraphics/CoreGraphics.h>
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

namespace WebCore {
class Color;
}

namespace TestWebKitAPI {

// FIXME: We can unify most of this helper class with the logic in `TestPDFPage::colorAtPoint`, and deploy this
// helper class in several other tests that read pixel data from CGImages.
class CGImagePixelReader {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CGImagePixelReader); WTF_MAKE_NONCOPYABLE(CGImagePixelReader);
public:
    CGImagePixelReader(CGImageRef);

    bool isTransparentBlack(unsigned x, unsigned y) const;
    WebCore::Color at(unsigned x, unsigned y) const;

    unsigned width() const { return m_width; }
    unsigned height() const { return m_height; }

    static constexpr auto defaultWebViewSamplingInterval { 100 };

private:
    unsigned m_width { 0 };
    unsigned m_height { 0 };
    RetainPtr<CGContextRef> m_context;
};

} // namespace TestWebKitAPI
