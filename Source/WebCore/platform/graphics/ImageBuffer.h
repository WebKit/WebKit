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

#include "DisplayListFlushIdentifier.h"
#include "ImageBufferBackend.h"
#include "RenderingMode.h"
#include "RenderingResourceIdentifier.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace DisplayList {
class DisplayList;
class DrawingContext;
struct ItemBufferHandle;
}

class ImageBuffer : public ThreadSafeRefCounted<ImageBuffer>, public CanMakeWeakPtr<ImageBuffer> {
public:
    // Will return a null pointer on allocation failure.
    WEBCORE_EXPORT static RefPtr<ImageBuffer> create(const FloatSize&, RenderingMode, ShouldUseDisplayList, RenderingPurpose, float resolutionScale, const DestinationColorSpace&, PixelFormat, const HostWindow* = nullptr);
    WEBCORE_EXPORT static RefPtr<ImageBuffer> create(const FloatSize&, RenderingMode, float resolutionScale, const DestinationColorSpace&, PixelFormat, const HostWindow* = nullptr);

    // Create an image buffer compatible with the context, with suitable resolution for drawing into the buffer and then into this context.
    static RefPtr<ImageBuffer> createCompatibleBuffer(const FloatSize&, const GraphicsContext&);
    static RefPtr<ImageBuffer> createCompatibleBuffer(const FloatSize&, const DestinationColorSpace&, const GraphicsContext&);
    static RefPtr<ImageBuffer> createCompatibleBuffer(const FloatSize&, float resolutionScale, const DestinationColorSpace&, const GraphicsContext&);

    // These functions are used when clamping the ImageBuffer which is created for filter, masker or clipper.
    static bool sizeNeedsClamping(const FloatSize&);
    static bool sizeNeedsClamping(const FloatSize&, FloatSize& scale);
    static FloatSize clampedSize(const FloatSize&);
    static FloatSize clampedSize(const FloatSize&, FloatSize& scale);
    static FloatRect clampedRect(const FloatRect&);

    static IntSize compatibleBufferSize(const FloatSize&, const GraphicsContext&);
    
    WEBCORE_EXPORT virtual ~ImageBuffer() = default;

    virtual void setBackend(std::unique_ptr<ImageBufferBackend>&&) = 0;
    virtual void clearBackend() = 0;
    virtual ImageBufferBackend* backend() const = 0;
    virtual ImageBufferBackend* ensureBackendCreated() const = 0;

    virtual RenderingMode renderingMode() const = 0;
    virtual bool canMapBackingStore() const = 0;
    virtual RenderingResourceIdentifier renderingResourceIdentifier() const { return { }; }

    virtual GraphicsContext& context() const = 0;
    virtual void flushContext() = 0;

    virtual DisplayList::DrawingContext* drawingContext() { return nullptr; }
    virtual bool prefersPreparationForDisplay() { return false; }
    virtual void flushDrawingContext() { }
    virtual void flushDrawingContextAsync() { }
    virtual void didFlush(DisplayList::FlushIdentifier) { }

    virtual void changeDestinationImageBuffer(RenderingResourceIdentifier) { }
    virtual void prepareToAppendDisplayListItems(DisplayList::ItemBufferHandle&&) { }

    virtual IntSize logicalSize() const = 0;
    virtual float resolutionScale() const = 0;
    virtual DestinationColorSpace colorSpace() const = 0;
    virtual PixelFormat pixelFormat() const = 0;
    virtual const ImageBufferBackend::Parameters& parameters() const = 0;

    virtual IntSize backendSize() const = 0;
    virtual AffineTransform baseTransform() const = 0;

    virtual size_t memoryCost() const = 0;
    virtual size_t externalMemoryCost() const = 0;

    virtual bool isInUse() const = 0;
    virtual void releaseGraphicsContext() = 0;
    virtual VolatilityState setVolatile(bool) = 0;
    virtual void releaseBufferToPool() = 0;

    virtual std::unique_ptr<ThreadSafeImageBufferFlusher> createFlusher() = 0;

    virtual RefPtr<NativeImage> copyNativeImage(BackingStoreCopy = CopyBackingStore) const = 0;
    virtual RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, PreserveResolution = PreserveResolution::No) const = 0;

    // Create an image buffer compatible with the context and copy rect from this buffer into this new one.
    RefPtr<ImageBuffer> copyRectToBuffer(const FloatRect&, const DestinationColorSpace&, const GraphicsContext&);

    virtual void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), const ImagePaintingOptions& = { }) = 0;
    virtual void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) = 0;

    static RefPtr<NativeImage> sinkIntoNativeImage(RefPtr<ImageBuffer>);

    WEBCORE_EXPORT static RefPtr<Image> sinkIntoImage(RefPtr<ImageBuffer>, PreserveResolution = PreserveResolution::No);
    static void drawConsuming(RefPtr<ImageBuffer>, GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), const ImagePaintingOptions& = { });
    
    virtual void clipToMask(GraphicsContext&, const FloatRect& destRect) = 0;

    virtual void convertToLuminanceMask() = 0;
    virtual void transformToColorSpace(const DestinationColorSpace&) = 0;

    virtual String toDataURL(const String& mimeType, std::optional<double> quality = std::nullopt, PreserveResolution = PreserveResolution::No) const = 0;
    virtual Vector<uint8_t> toData(const String& mimeType, std::optional<double> quality = std::nullopt) const = 0;

    virtual std::optional<PixelBuffer> getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect) const = 0;
    virtual void putPixelBuffer(const PixelBuffer&, const IntRect& srcRect, const IntPoint& destPoint = { }, AlphaPremultiplication destFormat = AlphaPremultiplication::Premultiplied) = 0;

    // FIXME: current implementations of this method have the restriction that they only work
    // with textures that are RGB or RGBA format, and UNSIGNED_BYTE type.
    virtual bool copyToPlatformTexture(GraphicsContextGL&, GCGLenum, PlatformGLObject, GCGLenum, bool, bool) const = 0;
    virtual PlatformLayer* platformLayer() const = 0;

protected:
    ImageBuffer() = default;

    virtual RefPtr<NativeImage> sinkIntoNativeImage() = 0;
    virtual RefPtr<Image> sinkIntoImage(PreserveResolution = PreserveResolution::No) = 0;
    virtual void drawConsuming(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) = 0;
};

using ImageBufferHashMap = HashMap<RenderingResourceIdentifier, Ref<ImageBuffer>>;

} // namespace WebCore
