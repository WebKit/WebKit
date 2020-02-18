/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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

#include "AffineTransform.h"
#include "AlphaPremultiplication.h"
#include "ColorSpace.h"
#include "GraphicsTypes.h"
#include "GraphicsTypesGL.h"
#include "ImageBufferData.h"
#include "ImagePaintingOptions.h"
#include "IntSize.h"
#include "PlatformLayer.h"
#include "RenderingMode.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/IsoMalloc.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;
class GraphicsContextGLOpenGL;
class Image;
class ImageData;
class IntPoint;
class IntRect;
class HostWindow;

enum BackingStoreCopy {
    CopyBackingStore, // Guarantee subsequent draws don't affect the copy.
    DontCopyBackingStore // Subsequent draws may affect the copy.
};

enum class PreserveResolution {
    No,
    Yes,
};

class ImageBuffer {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(ImageBuffer, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(ImageBuffer);
    friend class IOSurface;
public:
    // Will return a null pointer on allocation failure.
    WEBCORE_EXPORT static std::unique_ptr<ImageBuffer> create(const FloatSize&, RenderingMode, float resolutionScale = 1, ColorSpace = ColorSpace::SRGB, const HostWindow* = nullptr);
#if USE(DIRECT2D)
    WEBCORE_EXPORT static std::unique_ptr<ImageBuffer> create(const FloatSize&, RenderingMode, const GraphicsContext*, float resolutionScale = 1, ColorSpace = ColorSpace::SRGB, const HostWindow* = nullptr);
#endif

    // Create an image buffer compatible with the context and copy rect from this buffer into this new one.
    std::unique_ptr<ImageBuffer> copyRectToBuffer(const FloatRect&, ColorSpace, const GraphicsContext&);

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
    
    WEBCORE_EXPORT ~ImageBuffer();

    WEBCORE_EXPORT GraphicsContext& context() const;
#if USE(CG) || USE(DIRECT2D)
    void flushContext() const;
#endif

    // The actual resolution of the backing store
    const IntSize& internalSize() const { return m_size; }
    const IntSize& logicalSize() const { return m_logicalSize; }
    float resolutionScale() const { return m_resolutionScale; }
    
    size_t memoryCost() const;
    size_t externalMemoryCost() const;

#if USE(CG) || USE(DIRECT2D)
    NativeImagePtr copyNativeImage(BackingStoreCopy = CopyBackingStore) const;
#elif USE(CAIRO)
    NativeImagePtr nativeImage() const;
#endif

    WEBCORE_EXPORT RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, PreserveResolution = PreserveResolution::No) const;
    
    void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), const ImagePaintingOptions& = { });
    void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { });

#if USE(CG) || USE(DIRECT2D)
    static NativeImagePtr sinkIntoNativeImage(std::unique_ptr<ImageBuffer>);
#endif
    WEBCORE_EXPORT static RefPtr<Image> sinkIntoImage(std::unique_ptr<ImageBuffer>, PreserveResolution = PreserveResolution::No);
    static void drawConsuming(std::unique_ptr<ImageBuffer>, GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), const ImagePaintingOptions& = { });

    void convertToLuminanceMask();
#if !USE(CG)
    AffineTransform baseTransform() const { return AffineTransform(); }
    void transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace);
    void platformTransformColorSpace(const std::array<uint8_t, 256>&);
#else
    AffineTransform baseTransform() const { return AffineTransform(1, 0, 0, -1, 0, m_data.backingStoreSize.height()); }
#endif

    String toDataURL(const String& mimeType, Optional<double> quality = WTF::nullopt, PreserveResolution = PreserveResolution::No) const;
    Vector<uint8_t> toData(const String& mimeType, Optional<double> quality = WTF::nullopt) const;
    Vector<uint8_t> toBGRAData() const;

    RefPtr<ImageData> getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect) const;
    void putImageData(AlphaPremultiplication inputFormat, const ImageData&, const IntRect& srcRect, const IntPoint& destPoint = { });
    
    PlatformLayer* platformLayer() const;

    // FIXME: current implementations of this method have the restriction that they only work
    // with textures that are RGB or RGBA format, and UNSIGNED_BYTE type.
    bool copyToPlatformTexture(GraphicsContextGLOpenGL&, GCGLenum, PlatformGLObject, GCGLenum, bool, bool);

private:
    ImageBufferData m_data;
    IntSize m_size;
    IntSize m_logicalSize;
    float m_resolutionScale;

    // This constructor will place its success into the given out-variable
    // so that create() knows when it should return failure.
    WEBCORE_EXPORT ImageBuffer(const FloatSize&, float resolutionScale, ColorSpace, RenderingMode, const HostWindow*, bool& success);
#if USE(CG)
    ImageBuffer(const FloatSize&, float resolutionScale, CGColorSpaceRef, RenderingMode, const HostWindow*, bool& success);
    RetainPtr<CFDataRef> toCFData(const String& mimeType, Optional<double> quality, PreserveResolution) const;
#elif USE(DIRECT2D)
    ImageBuffer(const FloatSize&, float resolutionScale, ColorSpace, RenderingMode, const HostWindow*, const GraphicsContext*, bool& success);
#endif
};

} // namespace WebCore
