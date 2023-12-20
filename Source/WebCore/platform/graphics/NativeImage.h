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
#include <wtf/UniqueRef.h>

namespace WebCore {

class GraphicsContext;

class NativeImageBackend;

class NativeImage final : public RenderingResource {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());
    // Creates a NativeImage that is intended to be drawn once or only few times. Signals the platform to avoid generating any caches for the image.
    static WEBCORE_EXPORT RefPtr<NativeImage> createTransient(PlatformImagePtr&&, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());

    WEBCORE_EXPORT const PlatformImagePtr& platformImage() const;

    WEBCORE_EXPORT IntSize size() const;
    bool hasAlpha() const;
    Color singlePixelSolidColor() const;
    WEBCORE_EXPORT DestinationColorSpace colorSpace() const;

    void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions);
    void clearSubimages();

    WEBCORE_EXPORT void replaceContents(PlatformImagePtr);
protected:
    NativeImage(UniqueRef<NativeImageBackend>, RenderingResourceIdentifier);

    bool isNativeImage() const final { return true; }

    UniqueRef<NativeImageBackend> m_backend;
};

class NativeImageBackend {
public:
    WEBCORE_EXPORT virtual ~NativeImageBackend();
    virtual const PlatformImagePtr& platformImage() const = 0;
    virtual IntSize size() const = 0;
    virtual bool hasAlpha() const = 0;
    virtual DestinationColorSpace colorSpace() const = 0;
};

class PlatformImageNativeImageBackend final : public NativeImageBackend {
public:
    WEBCORE_EXPORT ~PlatformImageNativeImageBackend() final;
    WEBCORE_EXPORT const PlatformImagePtr& platformImage() const final;
    WEBCORE_EXPORT IntSize size() const final;
    WEBCORE_EXPORT bool hasAlpha() const final;
    WEBCORE_EXPORT DestinationColorSpace colorSpace() const final;
private:
    PlatformImageNativeImageBackend(PlatformImagePtr);
    PlatformImagePtr m_platformImage;
    friend class NativeImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::NativeImage)
    static bool isType(const WebCore::RenderingResource& renderingResource) { return renderingResource.isNativeImage(); }
SPECIALIZE_TYPE_TRAITS_END()
