/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2013 Apple Inc.  All rights reserved.
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
#include "DecodingOptions.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GraphicsTypes.h"
#include "ImageOrientation.h"
#include "ImagePaintingOptions.h"
#include "ImageTypes.h"
#include "NativeImage.h"
#include "Timer.h"
#include <wtf/EnumTraits.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

#if USE(APPKIT)
OBJC_CLASS NSImage;
#endif

#if USE(CG)
struct CGContext;
#endif

#if PLATFORM(WIN)
typedef struct tagSIZE SIZE;
typedef SIZE* LPSIZE;
typedef struct HBITMAP__ *HBITMAP;
#endif

#if PLATFORM(GTK)
typedef struct _GdkPixbuf GdkPixbuf;
#if USE(GTK4)
typedef struct _GdkTexture GdkTexture;
#endif
#endif

namespace WebCore {

class AffineTransform;
class FloatPoint;
class FloatSize;
class GraphicsContext;
class SharedBuffer;
struct Length;

// This class gets notified when an image creates or destroys decoded frames and when it advances animation frames.
class ImageObserver;

class Image : public RefCounted<Image> {
    friend class GraphicsContext;
public:
    virtual ~Image();
    
    WEBCORE_EXPORT static Ref<Image> loadPlatformResource(const char* name);
    WEBCORE_EXPORT static RefPtr<Image> create(ImageObserver&);
    WEBCORE_EXPORT static bool supportsType(const String&);
    static bool isPDFResource(const String& mimeType, const URL&);
    static bool isPostScriptResource(const String& mimeType, const URL&);

    virtual bool isBitmapImage() const { return false; }
    virtual bool isGeneratedImage() const { return false; }
    virtual bool isCrossfadeGeneratedImage() const { return false; }
    virtual bool isNamedImageGeneratedImage() const { return false; }
    virtual bool isGradientImage() const { return false; }
    virtual bool isSVGImage() const { return false; }
    virtual bool isSVGImageForContainer() const { return false; }
    virtual bool isPDFDocumentImage() const { return false; }
    virtual bool isCustomPaintImage() const { return false; }

    bool drawsSVGImage() const { return isSVGImage() || isSVGImageForContainer(); }

    virtual bool currentFrameKnownToBeOpaque() const = 0;
    virtual bool isAnimated() const { return false; }

    // Derived classes should override this if they can assure that 
    // the image contains only resources from its own security origin.
    virtual bool hasSingleSecurityOrigin() const { return false; }

    WEBCORE_EXPORT static Image& nullImage();
    bool isNull() const { return size().isEmpty(); }

    virtual void setContainerSize(const FloatSize&) { }
    virtual bool usesContainerSize() const { return false; }
    virtual bool hasRelativeWidth() const { return false; }
    virtual bool hasRelativeHeight() const { return false; }
    virtual void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio);

    virtual FloatSize size(ImageOrientation = ImageOrientation::FromImage) const = 0;
    virtual FloatSize sourceSize(ImageOrientation orientation = ImageOrientation::FromImage) const { return size(orientation); }
    virtual bool hasDensityCorrectedSize() const { return false; }
    FloatRect rect() const { return FloatRect(FloatPoint(), size()); }
    float width() const { return size().width(); }
    float height() const { return size().height(); }
    virtual std::optional<IntPoint> hotSpot() const { return std::nullopt; }
    virtual ImageOrientation orientation() const { return ImageOrientation::FromImage; }

    WEBCORE_EXPORT EncodedDataStatus setData(RefPtr<SharedBuffer>&& data, bool allDataReceived);
    virtual EncodedDataStatus dataChanged(bool /*allDataReceived*/) { return EncodedDataStatus::Unknown; }

    virtual String uti() const { return String(); } // null string if unknown
    virtual String filenameExtension() const { return String(); } // null string if unknown
    virtual String accessibilityDescription() const { return String(); } // null string if unknown

    virtual void destroyDecodedData(bool destroyAll = true) = 0;

    SharedBuffer* data() { return m_encodedImageData.get(); }
    const SharedBuffer* data() const { return m_encodedImageData.get(); }

    // Animation begins whenever someone draws the image, so startAnimation() is not normally called.
    // It will automatically pause once all observers no longer want to render the image anywhere.
    virtual void startAnimation() { }
    void startAnimationAsynchronously();
    virtual void stopAnimation() {}
    virtual void resetAnimation() {}
    virtual bool isAnimating() const { return false; }
    bool animationPending() const { return m_animationStartTimer && m_animationStartTimer->isActive(); }

    // Typically the CachedImage that owns us.
    ImageObserver* imageObserver() const { return m_imageObserver; }
    void setImageObserver(ImageObserver* observer) { m_imageObserver = observer; }
    URL sourceURL() const;
    String mimeType() const;
    long long expectedContentLength() const;

    enum TileRule { StretchTile, RoundTile, SpaceTile, RepeatTile };

    virtual RefPtr<NativeImage> nativeImage(const GraphicsContext* = nullptr) { return nullptr; }
    virtual RefPtr<NativeImage> nativeImageForCurrentFrame(const GraphicsContext* = nullptr) { return nullptr; }
    virtual RefPtr<NativeImage> preTransformedNativeImageForCurrentFrame(bool = true, const GraphicsContext* = nullptr) { return nullptr; }
    virtual RefPtr<NativeImage> nativeImageOfSize(const IntSize&, const GraphicsContext* = nullptr) { return nullptr; }

    // Accessors for native image formats.

#if USE(APPKIT)
    virtual NSImage *nsImage() { return nullptr; }
    virtual RetainPtr<NSImage> snapshotNSImage() { return nullptr; }
#endif

#if PLATFORM(COCOA)
    virtual CFDataRef tiffRepresentation() { return nullptr; }
#endif

#if PLATFORM(WIN)
    virtual bool getHBITMAP(HBITMAP) { return false; }
    virtual bool getHBITMAPOfSize(HBITMAP, const IntSize*) { return false; }
#endif

#if PLATFORM(GTK)
    virtual GdkPixbuf* getGdkPixbuf() { return nullptr; }
#if USE(GTK4)
    virtual GdkTexture* gdkTexture() { return nullptr; }
#endif
#endif

    virtual void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { });

#if ASSERT_ENABLED
    virtual bool notSolidColor() { return true; }
#endif

    virtual void dump(WTF::TextStream&) const;

protected:
    WEBCORE_EXPORT Image(ImageObserver* = nullptr);

    static void fillWithSolidColor(GraphicsContext&, const FloatRect& dstRect, const Color&, CompositeOperator);

#if PLATFORM(WIN)
    virtual void drawFrameMatchingSourceSize(GraphicsContext&, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator) { }
#endif
    virtual ImageDrawResult draw(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& = { }) = 0;
    ImageDrawResult drawTiled(GraphicsContext&, const FloatRect& dstRect, const FloatPoint& srcPoint, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& = { });
    ImageDrawResult drawTiled(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, const FloatSize& tileScaleFactor, TileRule hRule, TileRule vRule, const ImagePaintingOptions& = { });

    // Supporting tiled drawing
    virtual Color singlePixelSolidColor() const { return Color(); }

private:
    RefPtr<SharedBuffer> m_encodedImageData;
    ImageObserver* m_imageObserver;
    std::unique_ptr<Timer> m_animationStartTimer;
};

WTF::TextStream& operator<<(WTF::TextStream&, const Image&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_IMAGE(ToClassName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::Image& image) { return image.is##ToClassName(); } \
SPECIALIZE_TYPE_TRAITS_END()


namespace WTF {

template<> struct EnumTraits<WebCore::Image::TileRule> {
    using values = EnumValues<
        WebCore::Image::TileRule,
        WebCore::Image::TileRule::StretchTile,
        WebCore::Image::TileRule::RoundTile,
        WebCore::Image::TileRule::SpaceTile,
        WebCore::Image::TileRule::RepeatTile
    >;
};

} // namespace WTF
