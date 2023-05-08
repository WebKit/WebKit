/*
 * Copyright (C) 2004-2023 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 * Copyright (C) 2012 Company 100 Inc.
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

#include "Color.h"
#include "ImagePaintingOptions.h"
#include "IntSize.h"
#include "PlatformImage.h"
#include "RenderingResource.h"

#if USE(CAIRO)
#include "PixelBuffer.h"
#endif

namespace WebCore {

class GraphicsContext;

class NativeImage final : public RenderingResource {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());
#if USE(CAIRO)
    static RefPtr<NativeImage> create(Ref<PixelBuffer>&&, bool premultipliedAlpha);
#endif

    WEBCORE_EXPORT void setPlatformImage(PlatformImagePtr&&);
    const PlatformImagePtr& platformImage() const { return m_platformImage; }

    WEBCORE_EXPORT IntSize size() const;
    bool hasAlpha() const;
    Color singlePixelSolidColor() const;
    WEBCORE_EXPORT DestinationColorSpace colorSpace() const;

    void draw(GraphicsContext&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&);
    void clearSubimages();

private:
    NativeImage(PlatformImagePtr&&, RenderingResourceIdentifier);
#if USE(CAIRO)
    NativeImage(PlatformImagePtr&&, RenderingResourceIdentifier, Ref<PixelBuffer>&&);
#endif

    bool isNativeImage() const final { return true; }

    PlatformImagePtr m_platformImage;
#if USE(CAIRO)
    RefPtr<PixelBuffer> m_pixelBuffer;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::NativeImage)
    static bool isType(const WebCore::RenderingResource& renderingResource) { return renderingResource.isNativeImage(); }
SPECIALIZE_TYPE_TRAITS_END()
