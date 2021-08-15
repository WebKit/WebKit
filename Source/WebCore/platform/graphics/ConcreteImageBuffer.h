/*
 * Copyright (C) 2020-2021 Apple Inc.  All rights reserved.
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

#include "ImageBuffer.h"
#include "PixelBuffer.h"

namespace WebCore {

template<typename BackendType>
class ConcreteImageBuffer : public ImageBuffer {
public:
    template<typename ImageBufferType = ConcreteImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, const HostWindow* hostWindow, Arguments&&... arguments)
    {
        auto parameters = ImageBufferBackend::Parameters { size, resolutionScale, colorSpace, pixelFormat };
        auto backend = BackendType::create(parameters, hostWindow);
        if (!backend)
            return nullptr;
        return adoptRef(new ImageBufferType(parameters, WTFMove(backend), std::forward<Arguments>(arguments)...));
    }

    template<typename ImageBufferType = ConcreteImageBuffer, typename... Arguments>
    static RefPtr<ImageBufferType> create(const FloatSize& size, const GraphicsContext& context, Arguments&&... arguments)
    {
        auto parameters = ImageBufferBackend::Parameters { size, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8 };
        auto backend = BackendType::create(parameters, context);
        if (!backend)
            return nullptr;
        return adoptRef(new ImageBufferType(parameters, WTFMove(backend), std::forward<Arguments>(arguments)...));
    }

    RenderingMode renderingMode() const override { return BackendType::renderingMode; }
    bool canMapBackingStore() const override { return BackendType::canMapBackingStore; }

protected:
    ConcreteImageBuffer(const ImageBufferBackend::Parameters& parameters, std::unique_ptr<BackendType>&& backend = nullptr, RenderingResourceIdentifier renderingResourceIdentifier = RenderingResourceIdentifier::generate())
        : m_parameters(parameters)
        , m_backend(WTFMove(backend))
        , m_renderingResourceIdentifier(renderingResourceIdentifier)
    {
    }

    void setBackend(std::unique_ptr<ImageBufferBackend>&& backend) override
    {
        ASSERT(!m_backend);
        m_backend = std::unique_ptr<BackendType> { static_cast<BackendType*>(backend.release()) };
    }

    void clearBackend() override { m_backend = nullptr; }
    ImageBufferBackend* backend() const override { return m_backend.get(); }
    ImageBufferBackend* ensureBackendCreated() const override { return m_backend.get(); }

    RenderingResourceIdentifier renderingResourceIdentifier() const override { return m_renderingResourceIdentifier; }

    GraphicsContext& context() const override
    {
        ASSERT(m_backend);
        return m_backend->context();
    }

    void flushContext() override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            backend->flushContext();
        }
    }

    IntSize logicalSize() const override { return IntSize(m_parameters.logicalSize); }
    float resolutionScale() const override { return m_parameters.resolutionScale; }
    DestinationColorSpace colorSpace() const override { return m_parameters.colorSpace; }
    PixelFormat pixelFormat() const override { return m_parameters.pixelFormat; }
    const ImageBufferBackend::Parameters& parameters() const override { return m_parameters; }

    IntSize backendSize() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->backendSize();
        return { };
    }

    AffineTransform baseTransform() const override
    {
        if (BackendType::isOriginAtBottomLeftCorner)
            return AffineTransform(1, 0, 0, -1, 0, logicalSize().height());
        return { };
    }

    size_t memoryCost() const override
    {
        return BackendType::calculateMemoryCost(m_parameters);
    }

    size_t externalMemoryCost() const override
    {
        return BackendType::calculateExternalMemoryCost(m_parameters);
    }

    RefPtr<NativeImage> copyNativeImage(BackingStoreCopy copyBehavior = CopyBackingStore) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushDrawingContext();
            return backend->copyNativeImage(copyBehavior);
        }
        return nullptr;
    }

    RefPtr<Image> copyImage(BackingStoreCopy copyBehavior = CopyBackingStore, PreserveResolution preserveResolution = PreserveResolution::No) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushDrawingContext();
            return backend->copyImage(copyBehavior, preserveResolution);
        }
        return nullptr;
    }

    void draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            backend->draw(destContext, destRect, srcRect, options);
        }
    }

    void drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            backend->drawPattern(destContext, destRect, srcRect, patternTransform, phase, spacing, options);
        }
    }

    RefPtr<NativeImage> sinkIntoNativeImage() override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            return backend->sinkIntoNativeImage();
        }
        return nullptr;
    }

    RefPtr<Image> sinkIntoImage(PreserveResolution preserveResolution = PreserveResolution::No) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            return backend->sinkIntoImage(preserveResolution);
        }
        return nullptr;
    }

    void drawConsuming(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            backend->drawConsuming(destContext, destRect, srcRect, options);
        }
    }
    
    void clipToMask(GraphicsContext& destContext, const FloatRect& destRect) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushContext();
            backend->clipToMask(destContext, destRect);
        }
    }

    void convertToLuminanceMask() override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushContext();
            backend->convertToLuminanceMask();
        }
    }

    void transformToColorSpace(const DestinationColorSpace& newColorSpace) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            backend->transformToColorSpace(newColorSpace);
            m_parameters.colorSpace = newColorSpace;
        }
    }

    String toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushContext();
            return backend->toDataURL(mimeType, quality, preserveResolution);
        }
        return String();
    }

    Vector<uint8_t> toData(const String& mimeType, std::optional<double> quality = std::nullopt) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushContext();
            return backend->toData(mimeType, quality);
        }
        return { };
    }

    std::optional<PixelBuffer> getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushContext();
            return backend->getPixelBuffer(outputFormat, srcRect);
        }
        return std::nullopt;
    }

    void putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint = { }, AlphaPremultiplication destFormat = AlphaPremultiplication::Premultiplied) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushContext();
            backend->putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
        }
    }

    PlatformLayer* platformLayer() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->platformLayer();
        return nullptr;
    }

    bool copyToPlatformTexture(GraphicsContextGL& context, GCGLenum target, PlatformGLObject destinationTexture, GCGLenum internalformat, bool premultiplyAlpha, bool flipY) const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->copyToPlatformTexture(context, target, destinationTexture, internalformat, premultiplyAlpha, flipY);
        return false;
    }

    bool isInUse() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->isInUse();
        return false;
    }

    void releaseGraphicsContext() override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->releaseGraphicsContext();
    }

    VolatilityState setVolatile(bool isVolatile) override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->setVolatile(isVolatile);
        return VolatilityState::Valid;
    }

    std::unique_ptr<ThreadSafeImageBufferFlusher> createFlusher() override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->createFlusher();
        return nullptr;
    }

    void releaseBufferToPool() override
    {
        if (auto* backend = ensureBackendCreated())
            backend->releaseBufferToPool();
    }

    ImageBufferBackend::Parameters m_parameters;
    std::unique_ptr<BackendType> m_backend;
    RenderingResourceIdentifier m_renderingResourceIdentifier;
};

} // namespace WebCore
