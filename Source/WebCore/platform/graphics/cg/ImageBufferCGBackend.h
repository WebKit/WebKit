/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#if USE(CG)

#include "ImageBufferBackend.h"
#include <wtf/Forward.h>

typedef struct CGImage* CGImageRef;

namespace WebCore {

class WEBCORE_EXPORT ImageBufferCGBackend : public ImageBufferBackend {
public:
    static unsigned calculateBytesPerRow(const IntSize& backendSize);

    RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, PreserveResolution = PreserveResolution::No) const override;
    RefPtr<Image> sinkIntoImage(PreserveResolution) override;

    void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) override;
    void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) override;
    
    void clipToMask(GraphicsContext&, const FloatRect& destRect) override;

    String toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution) const override;
    Vector<uint8_t> toData(const String& mimeType, std::optional<double> quality) const override;

    std::unique_ptr<ThreadSafeImageBufferFlusher> createFlusher() override;

    static constexpr bool isOriginAtBottomLeftCorner = true;

protected:
    using ImageBufferBackend::ImageBufferBackend;

    static RetainPtr<CGColorSpaceRef> contextColorSpace(const GraphicsContext&);
    void setupContext() const;

    virtual void prepareToDrawIntoContext(GraphicsContext&);

    virtual RetainPtr<CGImageRef> copyCGImageForEncoding(CFStringRef destinationUTI, PreserveResolution) const;
};

} // namespace WebCore

#endif // USE(CG)
