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

#include "ImageBuffer.h"
#include "ImageData.h"

namespace WebCore {

template<typename BackendType>
class ConcreteImageBuffer : public ImageBuffer {
public:
    template<typename ImageBufferType = ConcreteImageBuffer, typename... Arguments>
    static std::unique_ptr<ImageBufferType> create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const HostWindow* hostWindow, Arguments&&... arguments)
    {
        auto backend = BackendType::create(size, resolutionScale, colorSpace, hostWindow);
        if (!backend)
            return nullptr;
        return std::unique_ptr<ImageBufferType>(new ImageBufferType(WTFMove(backend), std::forward<Arguments>(arguments)...));
    }

    template<typename ImageBufferType = ConcreteImageBuffer, typename... Arguments>
    static std::unique_ptr<ImageBufferType> create(const FloatSize& size, const GraphicsContext& context, Arguments&&... arguments)
    {
        auto backend = BackendType::create(size, context);
        if (!backend)
            return nullptr;
        return std::unique_ptr<ImageBufferType>(new ImageBufferType(WTFMove(backend), std::forward<Arguments>(arguments)...));
    }
    
    bool isAccelerated() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->isAccelerated();
        return false;
    }

protected:
    ConcreteImageBuffer(std::unique_ptr<BackendType>&& backend)
        : m_backend(WTFMove(backend))
    {
    }

    ConcreteImageBuffer() = default;

    virtual BackendType* ensureBackendCreated() const { return m_backend.get(); }

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

    AffineTransform baseTransform() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->baseTransform();
        return { };
    }

    IntSize logicalSize() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->logicalSize();
        return { };
    }

    IntSize backendSize() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->backendSize();
        return { };
    }

    float resolutionScale() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->resolutionScale();
        return 1;
    }

    size_t memoryCost() const override
    {
        return m_backend ? m_backend->memoryCost() : 0;
    }

    size_t externalMemoryCost() const override
    {
        return m_backend ? m_backend->externalMemoryCost() : 0;
    }

    NativeImagePtr copyNativeImage(BackingStoreCopy copyBehavior = CopyBackingStore) const override
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

    NativeImagePtr sinkIntoNativeImage() override
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

    void convertToLuminanceMask() override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushContext();
            backend->convertToLuminanceMask();
        }
    }

    void transformColorSpace(ColorSpace srcColorSpace, ColorSpace destColorSpace) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushDrawingContext();
            backend->transformColorSpace(srcColorSpace, destColorSpace);
        }
    }

    String toDataURL(const String& mimeType, Optional<double> quality, PreserveResolution preserveResolution) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushContext();
            return backend->toDataURL(mimeType, quality, preserveResolution);
        }
        return String();
    }

    Vector<uint8_t> toData(const String& mimeType, Optional<double> quality = WTF::nullopt) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushContext();
            return backend->toData(mimeType, quality);
        }
        return { };
    }

    Vector<uint8_t> toBGRAData() const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushContext();
            return backend->toBGRAData();
        }
        return { };
    }

    RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const override
    {
        if (auto* backend = ensureBackendCreated()) {
            const_cast<ConcreteImageBuffer&>(*this).flushContext();
            return backend->getImageData(outputFormat, srcRect);
        }
        return nullptr;
    }

    void putImageData(AlphaPremultiplication inputFormat, const ImageData& imageData, const IntRect& srcRect, const IntPoint& destPoint = { }, AlphaPremultiplication destFormat = AlphaPremultiplication::Premultiplied) override
    {
        if (auto* backend = ensureBackendCreated()) {
            flushContext();
            backend->putImageData(inputFormat, imageData, srcRect, destPoint, destFormat);
        }
    }

    PlatformLayer* platformLayer() const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->platformLayer();
        return nullptr;
    }

    bool copyToPlatformTexture(GraphicsContextGLOpenGL& context, GCGLenum target, PlatformGLObject destinationTexture, GCGLenum internalformat, bool premultiplyAlpha, bool flipY) const override
    {
        if (auto* backend = ensureBackendCreated())
            return backend->copyToPlatformTexture(context, target, destinationTexture, internalformat, premultiplyAlpha, flipY);
        return false;
    }

    std::unique_ptr<BackendType> m_backend;
};

} // namespace WebCore
