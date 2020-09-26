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

#include "AlphaPremultiplication.h"
#include "ColorSpace.h"
#include "FloatRect.h"
#include "GraphicsTypesGL.h"
#include "ImagePaintingOptions.h"
#include "IntRect.h"
#include "NativeImage.h"
#include "PlatformLayer.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class GraphicsContextGLOpenGL;
class HostWindow;
class Image;
class ImageData;

enum BackingStoreCopy {
    CopyBackingStore, // Guarantee subsequent draws don't affect the copy.
    DontCopyBackingStore // Subsequent draws may affect the copy.
};

enum class PreserveResolution : uint8_t {
    No,
    Yes,
};

enum class ColorFormat : uint8_t {
    RGBA,
    BGRA
};

class ImageBufferBackend {
public:
    WEBCORE_EXPORT virtual ~ImageBufferBackend() = default;

    WEBCORE_EXPORT static IntSize calculateBackendSize(const FloatSize&, float resolutionScale);

    virtual GraphicsContext& context() const = 0;
    virtual void flushContext() { }

    IntSize logicalSize() const { return m_logicalSize; }
    IntSize backendSize() const { return m_backendSize; }
    float resolutionScale() const { return m_resolutionScale; }
    ColorSpace colorSpace() const { return m_colorSpace; }

    virtual AffineTransform baseTransform() const { return AffineTransform(); }
    virtual size_t memoryCost() const { return 4 * m_backendSize.area().unsafeGet(); }
    virtual size_t externalMemoryCost() const { return 0; }

    virtual NativeImagePtr copyNativeImage(BackingStoreCopy) const = 0;
    virtual RefPtr<Image> copyImage(BackingStoreCopy, PreserveResolution) const = 0;

    WEBCORE_EXPORT virtual void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) = 0;
    WEBCORE_EXPORT virtual void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) = 0;

    WEBCORE_EXPORT virtual NativeImagePtr sinkIntoNativeImage();
    WEBCORE_EXPORT virtual RefPtr<Image> sinkIntoImage(PreserveResolution);
    WEBCORE_EXPORT virtual void drawConsuming(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&);

    WEBCORE_EXPORT void convertToLuminanceMask();
    virtual void transformColorSpace(ColorSpace, ColorSpace) { }

    virtual String toDataURL(const String& mimeType, Optional<double> quality, PreserveResolution) const = 0;
    virtual Vector<uint8_t> toData(const String& mimeType, Optional<double> quality) const = 0;
    virtual Vector<uint8_t> toBGRAData() const = 0;

    virtual RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect&) const = 0;
    virtual void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat) = 0;

    virtual PlatformLayer* platformLayer() const { return nullptr; }
    virtual bool copyToPlatformTexture(GraphicsContextGLOpenGL&, GCGLenum, PlatformGLObject, GCGLenum, bool, bool) const { return false; }
    
    static constexpr bool isOriginAtUpperLeftCorner = false;
    virtual bool isAccelerated() const { return false; }

protected:
    WEBCORE_EXPORT ImageBufferBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace);

    virtual unsigned bytesPerRow() const { return 4 * m_backendSize.width(); }
    virtual ColorFormat backendColorFormat() const { return ColorFormat::RGBA; }

    template<typename T>
    T toBackendCoordinates(T t) const
    {
        static_assert(std::is_same<T, IntPoint>::value || std::is_same<T, IntSize>::value || std::is_same<T, IntRect>::value);
        if (m_resolutionScale != 1)
            t.scale(m_resolutionScale);
        return t;
    }

    IntRect logicalRect() const { return IntRect(IntPoint::zero(), m_logicalSize); };
    IntRect backendRect() const { return IntRect(IntPoint::zero(), m_backendSize); };

    WEBCORE_EXPORT virtual void copyImagePixels(
        AlphaPremultiplication srcAlphaFormat, ColorFormat srcColorFormat, unsigned srcBytesPerRow, uint8_t* srcRows,
        AlphaPremultiplication destAlphaFormat, ColorFormat destColorFormat, unsigned destBytesPerRow, uint8_t* destRows, const IntSize&) const;

    WEBCORE_EXPORT Vector<uint8_t> toBGRAData(void* data) const;

    WEBCORE_EXPORT RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect, void* data) const;
    WEBCORE_EXPORT void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat, void* data);

    IntSize m_logicalSize;
    IntSize m_backendSize;
    float m_resolutionScale;
    ColorSpace m_colorSpace;
};

} // namespace WebCore
