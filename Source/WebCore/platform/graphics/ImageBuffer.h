/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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

#include "ImageBufferBackend.h"
#include "RenderingMode.h"

namespace WebCore {

namespace DisplayList {
class DrawingContext;
}

class ImageBuffer {
public:
    // Will return a null pointer on allocation failure.
    WEBCORE_EXPORT static std::unique_ptr<ImageBuffer> create(const FloatSize&, ShouldAccelerate, ShouldUseDisplayList, RenderingPurpose, float resolutionScale = 1, ColorSpace = ColorSpace::SRGB, const HostWindow* = nullptr);
    WEBCORE_EXPORT static std::unique_ptr<ImageBuffer> create(const FloatSize&, RenderingMode, float resolutionScale = 1, ColorSpace = ColorSpace::SRGB, const HostWindow* = nullptr);
    static std::unique_ptr<ImageBuffer> create(const FloatSize&, const GraphicsContext&);

    // Create an image buffer compatible with the context, with suitable resolution for drawing into the buffer and then into this context.
    static std::unique_ptr<ImageBuffer> createCompatibleBuffer(const FloatSize&, const GraphicsContext&);
    static std::unique_ptr<ImageBuffer> createCompatibleBuffer(const FloatSize&, ColorSpace, const GraphicsContext&);
    static std::unique_ptr<ImageBuffer> createCompatibleBuffer(const FloatSize&, float resolutionScale, ColorSpace, const GraphicsContext&);

    // These functions are used when clamping the ImageBuffer which is created for filter, masker or clipper.
    static bool sizeNeedsClamping(const FloatSize&);
    static bool sizeNeedsClamping(const FloatSize&, FloatSize& scale);
    static FloatSize clampedSize(const FloatSize&);
    static FloatSize clampedSize(const FloatSize&, FloatSize& scale);
    static FloatRect clampedRect(const FloatRect&);

    static IntSize compatibleBufferSize(const FloatSize&, const GraphicsContext&);
    
    WEBCORE_EXPORT virtual ~ImageBuffer() = default;

    virtual GraphicsContext& context() const = 0;
    virtual void flushContext() = 0;

    virtual DisplayList::DrawingContext* drawingContext() { return nullptr; }
    virtual void flushDrawingContext() { }

    virtual AffineTransform baseTransform() const = 0;
    virtual IntSize logicalSize() const = 0;
    virtual IntSize backendSize() const = 0;
    virtual float resolutionScale() const = 0;

    virtual size_t memoryCost() const = 0;
    virtual size_t externalMemoryCost() const = 0;

    virtual NativeImagePtr copyNativeImage(BackingStoreCopy = CopyBackingStore) const = 0;
    virtual RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, PreserveResolution = PreserveResolution::No) const = 0;

    // Create an image buffer compatible with the context and copy rect from this buffer into this new one.
    std::unique_ptr<ImageBuffer> copyRectToBuffer(const FloatRect&, ColorSpace, const GraphicsContext&);

    virtual void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), const ImagePaintingOptions& = { }) = 0;
    virtual void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) = 0;

    static NativeImagePtr sinkIntoNativeImage(std::unique_ptr<ImageBuffer>);

    WEBCORE_EXPORT static RefPtr<Image> sinkIntoImage(std::unique_ptr<ImageBuffer>, PreserveResolution = PreserveResolution::No);
    static void drawConsuming(std::unique_ptr<ImageBuffer>, GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), const ImagePaintingOptions& = { });

    virtual void convertToLuminanceMask() = 0;
    virtual void transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace) = 0;

    virtual String toDataURL(const String& mimeType, Optional<double> quality = WTF::nullopt, PreserveResolution = PreserveResolution::No) const = 0;
    virtual Vector<uint8_t> toData(const String& mimeType, Optional<double> quality = WTF::nullopt) const = 0;
    virtual Vector<uint8_t> toBGRAData() const = 0;

    virtual RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const = 0;
    virtual void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint = { }, AlphaPremultiplication destFormat = AlphaPremultiplication::Premultiplied) = 0;

    // FIXME: current implementations of this method have the restriction that they only work
    // with textures that are RGB or RGBA format, and UNSIGNED_BYTE type.
    virtual bool copyToPlatformTexture(GraphicsContextGLOpenGL&, GCGLenum, PlatformGLObject, GCGLenum, bool, bool) const = 0;
    virtual PlatformLayer* platformLayer() const = 0;
    
    virtual bool isAccelerated() const = 0;

protected:
    ImageBuffer() = default;

    virtual NativeImagePtr sinkIntoNativeImage() = 0;
    virtual RefPtr<Image> sinkIntoImage(PreserveResolution = PreserveResolution::No) = 0;
    virtual void drawConsuming(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) = 0;
};

} // namespace WebCore
