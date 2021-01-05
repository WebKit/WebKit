/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#if USE(HAIKU)

#include "ImageBufferBackend.h"

#include "ImageBufferDataHaiku.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class ImageBufferHaikuSurfaceBackend : public ImageBufferBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferHaikuSurfaceBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferHaikuSurfaceBackend);
public:
    static std::unique_ptr<ImageBufferHaikuSurfaceBackend> create(const FloatSize&, float resolutionScale, ColorSpace, PixelFormat, const HostWindow*);
    static std::unique_ptr<ImageBufferHaikuSurfaceBackend> create(const FloatSize&, const GraphicsContext&);
    ~ImageBufferHaikuSurfaceBackend();

    GraphicsContext& context() const override;
    WTF::RefPtr<WebCore::NativeImage> copyNativeImage(BackingStoreCopy) const override;
    RefPtr<Image> copyImage(BackingStoreCopy, PreserveResolution) const override;
    void draw(GraphicsContext&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&) override;
    void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) override;
    String toDataURL(const String& mimeType, Optional<double> quality, PreserveResolution) const override;
    Vector<uint8_t> toData(const String& mimeType, Optional<double> quality) const override;
    Vector<uint8_t> toBGRAData() const override;
    RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect&) const override;
    void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) override;


private:
    ImageBufferHaikuSurfaceBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace, const HostWindow*);

    ImageBufferData m_data;
};

} // namespace WebCore

#endif // USE(HAIKU)

