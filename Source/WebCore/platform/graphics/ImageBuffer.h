/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include "ImmutableImageBuffer.h"
#include "ProcessIdentity.h"

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#include "DynamicContentScalingResourceCache.h"
#endif

namespace WebCore {

class DynamicContentScalingDisplayList;
class GraphicsClient;
#if HAVE(IOSURFACE)
class IOSurfacePool;
#endif

class SerializedImageBuffer;

struct ImageBufferCreationContext {
#if HAVE(IOSURFACE)
    IOSurfacePool* surfacePool { nullptr };
    PlatformDisplayID displayID { 0 };
#endif
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    DynamicContentScalingResourceCache dynamicContentScalingResourceCache;
#endif
    ProcessIdentity resourceOwner;

    ImageBufferCreationContext() = default;
};

class ImageBuffer : public ImmutableImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ImageBuffer, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT static RefPtr<ImageBuffer> create(const FloatSize&, RenderingMode, RenderingPurpose, float resolutionScale, const DestinationColorSpace&, ImageBufferPixelFormat, GraphicsClient* = nullptr);

    template<typename BackendType, typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat, RenderingPurpose purpose, const ImageBufferCreationContext& creationContext, Arguments&&... arguments)
    {
        Parameters parameters { size, resolutionScale, colorSpace, pixelFormat, purpose };
        auto backendParameters = ImageBuffer::backendParameters(parameters);
        auto backend = BackendType::create(backendParameters, creationContext);
        if (!backend)
            return nullptr;
        auto backendInfo = populateBackendInfo<BackendType>(backendParameters);
        return create<ImageBufferType>(parameters, backendInfo, creationContext, WTFMove(backend), std::forward<Arguments>(arguments)...);
    }

    template<typename ImageBufferType = ImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(Parameters parameters, const ImageBufferBackend::Info& backendInfo, const WebCore::ImageBufferCreationContext& creationContext, std::unique_ptr<ImageBufferBackend>&& backend, Arguments&&... arguments)
    {
        return adoptRef(new ImageBufferType(parameters, backendInfo, creationContext, WTFMove(backend), std::forward<Arguments>(arguments)...));
    }

    template<typename BackendType>
    static ImageBufferBackend::Info populateBackendInfo(const ImageBufferBackend::Parameters& parameters)
    {
        return {
            BackendType::renderingMode,
            ImageBufferBackend::calculateBaseTransform(parameters),
            BackendType::calculateMemoryCost(parameters),
        };
    }

    WEBCORE_EXPORT virtual ~ImageBuffer();

    using ImmutableImageBuffer::context;

    WEBCORE_EXPORT virtual void flushDrawingContext();
    WEBCORE_EXPORT virtual bool flushDrawingContextAsync();

    WEBCORE_EXPORT virtual void convertToLuminanceMask();
    WEBCORE_EXPORT virtual void transformToColorSpace(const DestinationColorSpace& newColorSpace);

    WEBCORE_EXPORT virtual void putPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint = { }, AlphaPremultiplication destFormat = AlphaPremultiplication::Premultiplied);

    WEBCORE_EXPORT static RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread(RefPtr<ImageBuffer>);

    static std::unique_ptr<SerializedImageBuffer> sinkIntoSerializedImageBuffer(RefPtr<ImageBuffer>&&);

protected:
    WEBCORE_EXPORT ImageBuffer(ImageBufferParameters, const ImageBufferBackend::Info&, const WebCore::ImageBufferCreationContext&, std::unique_ptr<ImageBufferBackend>&& = nullptr, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());

    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> sinkIntoBufferForDifferentThread();
    WEBCORE_EXPORT virtual std::unique_ptr<SerializedImageBuffer> sinkIntoSerializedImageBuffer();
};

class SerializedImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(SerializedImageBuffer, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(SerializedImageBuffer);
public:

    SerializedImageBuffer() = default;
    virtual ~SerializedImageBuffer() = default;

    virtual size_t memoryCost() const = 0;

    WEBCORE_EXPORT static RefPtr<ImageBuffer> sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer>, GraphicsClient* = nullptr);

    virtual bool isRemoteSerializedImageBufferProxy() const { return false; }

protected:
    virtual RefPtr<ImageBuffer> sinkIntoImageBuffer() = 0;
};

} // namespace WebCore
