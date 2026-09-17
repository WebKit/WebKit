/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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

#include <WebCore/ColorSpace.h>
#include <WebCore/DecodingOptions.h>
#include <WebCore/FloatRect.h>
#include <WebCore/ImageAdapter.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/ImagePaintingOptions.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/NaturalDimensions.h>
#include <WebCore/ObjectSizeNegotiation.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AffineTransform;
class Document;
class FloatPoint;
class FloatSize;
class FragmentedSharedBuffer;
class GraphicsContext;
class ImageDrawingExtras;
class NativeImage;
class ShareableBitmap;
class Timer;

enum class CompositeOperator : uint8_t;

// This class gets notified when an image creates or destroys decoded frames and when it advances animation frames.
class ImageObserver;

class Image : public RefCountedAndCanMakeWeakPtr<Image> {
    friend class CachedSubimage;
    friend class GraphicsContext;
public:
    virtual ~Image();
    
    WEBCORE_EXPORT static RefPtr<Image> create(ImageObserver&);
    WEBCORE_EXPORT static std::optional<Ref<Image>> create(RefPtr<ShareableBitmap>&&);
    WEBCORE_EXPORT static bool supportsType(const String&);

    virtual bool isBitmapImage() const { return false; }
    virtual bool isGeneratedImage() const { return false; }
    virtual bool isCrossfadeGeneratedImage() const { return false; }
    virtual bool isNamedImageGeneratedImage() const { return false; }
    virtual bool isGradientImage() const { return false; }
    virtual bool NODELETE isSVGImage() const { return false; }
    virtual bool isSVGResourceImage() const { return false; }
    virtual bool isPDFDocumentImage() const { return false; }
    virtual bool isCustomPaintImage() const { return false; }

    virtual void subresourcesAreFinished(Document*, CompletionHandler<void()>&&);

    bool NODELETE drawsSVGImage() const { return isSVGImage(); }

    virtual unsigned frameCount() const { return 1; }

    virtual bool currentFrameKnownToBeOpaque() const = 0;
    virtual bool currentFrameIsComplete() const = 0;
    virtual bool isAnimated() const { return false; }

    // Derived classes should override this if their rendering could leak
    // cross-origin data (outside of the resource itself, which undergoes
    // a CORS cross-origin check).
    virtual bool renderingTaintsOrigin() const { return false; }

    WEBCORE_EXPORT static Image& nullImage();


    // https://drafts.csswg.org/css-images-3/#natural-dimensions
    NaturalDimensions naturalDimensions(ImageOrientation requestedOrientation = ImageOrientation::Orientation::FromImage) const
    {
        auto orientation = requestedOrientation.orientation() == ImageOrientation::Orientation::FromImage ? this->orientation() : requestedOrientation;
        return unorientedNaturalDimensions().oriented(orientation);
    }

    virtual bool hasDensityCorrectedSize() const { return false; }
    virtual std::optional<IntPoint> hotSpot() const { return std::nullopt; }
    virtual ImageOrientation orientation() const { return ImageOrientation::Orientation::None; }

protected:
    virtual NaturalDimensions unorientedNaturalDimensions() const = 0;

public:

    WEBCORE_EXPORT EncodedDataStatus setData(RefPtr<FragmentedSharedBuffer>&& data, bool allDataReceived);
    virtual EncodedDataStatus dataChanged(bool /* allDataReceived */) { return EncodedDataStatus::Unknown; }

    // Can be called after final setData() to replace encoded data with an identical copy of the
    // data (e.g. to replace a dirty copy of the data with a clean copy of the data).
    bool tryReplaceData(Ref<FragmentedSharedBuffer>&& data);

    virtual String uti() const { return String(); } // null string if unknown
    virtual String filenameExtension() const { return String(); } // null string if unknown
    virtual String accessibilityDescription() const { return String(); } // null string if unknown

    virtual void destroyDecodedData(bool /*destroyAll*/ = true) { }

    FragmentedSharedBuffer* data() { return m_encodedImageData.get(); }
    const FragmentedSharedBuffer* data() const { return m_encodedImageData.get(); }

    virtual ColorSpace colorSpace();
    virtual bool hasHDRContent() const { return false; }

    // Animation begins whenever someone draws the image, so startAnimation() is not normally called.
    // It will automatically pause once all observers no longer want to render the image anywhere.
    virtual void startAnimation() { }
    void startAnimationAsynchronously();
    virtual void stopAnimation() {}
    virtual void resetAnimation() {}
    virtual bool isAnimating() const { return false; }
    WEBCORE_EXPORT bool NODELETE animationPending() const;
    std::optional<bool> allowsAnimation() const { return m_allowsAnimation; }
    void setAllowsAnimation(std::optional<bool> allowsAnimation) { m_allowsAnimation = allowsAnimation; }
    static bool systemAllowsAnimationControls() { return gSystemAllowsAnimationControls; }
    WEBCORE_EXPORT static void NODELETE setSystemAllowsAnimationControls(bool allowsControls);

    // Typically the CachedImage that owns us.
    RefPtr<ImageObserver> NODELETE imageObserver() const;
    void NODELETE setImageObserver(RefPtr<ImageObserver>&&);

    WEBCORE_EXPORT ImageAdapter& adapter();
    void invalidateAdapter();

    URL sourceURL() const;
    WEBCORE_EXPORT String mimeType() const;
    long long expectedContentLength() const;

    enum TileRule { StretchTile, RoundTile, SpaceTile, RepeatTile };

    virtual RefPtr<NativeImage> nativeImageAtIndex(unsigned, ConcreteObjectSize);
    virtual RefPtr<NativeImage> currentNativeImage(ConcreteObjectSize);
    virtual RefPtr<NativeImage> currentPreTransformedNativeImage(ConcreteObjectSize, ImageOrientation = ImageOrientation::Orientation::FromImage);

    virtual void drawPattern(GraphicsContext&, ConcreteObjectSize, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr);

#if ASSERT_ENABLED
    virtual bool hasSolidColor() { return false; }
#endif
#if ENABLE(QUICKLOOK_FULLSCREEN)
    virtual bool isPanorama() const { return false; }
#endif

#if ENABLE(SPATIAL_IMAGE_DETECTION)
    virtual bool isSpatial() const { return false; }
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
    virtual bool isMaybePanoramic() const { return false; }
#endif

    virtual void dump(WTF::TextStream&) const;

    WEBCORE_EXPORT RefPtr<ShareableBitmap> toShareableBitmap() const;

protected:
    WEBCORE_EXPORT Image(ImageObserver* = nullptr);

    static void fillWithSolidColor(GraphicsContext&, const FloatRect& dstRect, const Color&, CompositeOperator);

    virtual bool shouldDrawFromCachedSubimage(GraphicsContext&) const { return false; }
    virtual bool mustDrawFromCachedSubimage(GraphicsContext&) const { return false; }
    virtual ImageDrawResult draw(GraphicsContext&, ConcreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr) = 0;
    ImageDrawResult drawTiled(GraphicsContext&, ConcreteObjectSize, const FloatRect& dstRect, const FloatPoint& srcPoint, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr);
    ImageDrawResult drawTiled(GraphicsContext&, ConcreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, const FloatSize& tileScaleFactor, TileRule hRule, TileRule vRule, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr);

    // Supporting tiled drawing
    virtual std::optional<Color> singlePixelSolidColor() const;

    virtual bool canReplaceData() const { return false; }
    virtual void dataReplaced() { }

private:
    RefPtr<FragmentedSharedBuffer> m_encodedImageData;
    WeakPtr<ImageObserver> m_imageObserver;
    std::unique_ptr<ImageAdapter> m_adapter;

    // A value of true or false will override the default Page::imageAnimationEnabled state.
    std::optional<bool> m_allowsAnimation { std::nullopt };
    std::unique_ptr<Timer> m_animationStartTimer;
    WEBCORE_EXPORT static bool gSystemAllowsAnimationControls;
};

// FIXME: Make these better.
WEBCORE_EXPORT ConcreteObjectSize sourceConcreteSize(const Image&);
WEBCORE_EXPORT std::optional<FloatSize> sizeSampledAt(const Image&, ImageOrientation = ImageOrientation::Orientation::FromImage);
WEBCORE_EXPORT FloatSize selfReportedSize(const Image&, ImageOrientation = ImageOrientation::Orientation::FromImage);
WEBCORE_EXPORT bool hasNothingToDraw(const Image&);

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Image&);

class ImageObserverDisableScope {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONCOPYABLE(ImageObserverDisableScope);
public:
    WEBCORE_EXPORT explicit ImageObserverDisableScope(Image&, bool disable = true);
    WEBCORE_EXPORT ~ImageObserverDisableScope();

private:
    const Ref<Image> m_image;
    RefPtr<ImageObserver> m_observer;
    bool m_disable;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_IMAGE(ToClassName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::Image& image) { return image.is##ToClassName(); } \
SPECIALIZE_TYPE_TRAITS_END()
