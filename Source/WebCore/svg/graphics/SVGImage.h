/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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

#include <WebCore/Image.h>
#include <WebCore/StyleLinkParameters.h>
#include <WebCore/Timer.h>
#include <wtf/URL.h>

namespace WebCore {

class Element;
class ImageBuffer;
class LocalFrameView;
class Page;
class RenderObject;
class RenderReplaced;
class SVGSVGElement;
class SVGImageChromeClient;
class Settings;

class SVGImage final : public Image {
public:
    static Ref<SVGImage> create(ImageObserver* observer) { return adoptRef(*new SVGImage(observer)); }
    WEBCORE_EXPORT static void tryCreateFromData(std::span<const uint8_t>, CompletionHandler<void(RefPtr<SVGImage>&&)>&&);
    WEBCORE_EXPORT static bool isDataDecodable(const Settings&, std::span<const uint8_t>);

    RenderReplaced* embeddedSVGRoot() const;
    LocalFrameView* NODELETE frameView() const;

    bool NODELETE isSVGImage() const final { return true; }

    void subresourcesAreFinished(Document*, CompletionHandler<void()>&&) final;

    FloatSize size(ImageOrientation = ImageOrientation::Orientation::FromImage) const final { return m_intrinsicSize; }

    bool renderingTaintsOrigin() const final;

    bool hasIntrinsicWidth() const final;
    bool hasIntrinsicHeight() const final;
    bool hasRelativeWidth() const final;
    bool hasRelativeHeight() const final;

    void startAnimation() final;
    void stopAnimation() final;
    void resetAnimation() final;
    bool isAnimating() const final;

    Page* internalPage() { return m_page.get(); }
    WEBCORE_EXPORT RefPtr<SVGSVGElement> rootElement() const;

    FloatSize resolvedIntrinsicSize(float density = 1.0f) const;

    RefPtr<NativeImage> nativeImage(const FloatSize&, const ColorSpace& = ColorSpace::SRGB(), const ImageDrawingExtras* = nullptr, ImagePaintingOptions = { });

private:
    friend class SVGImageChromeClient;

    virtual ~SVGImage();

    String filenameExtension() const final;

    void setContainerSize(const FloatSize&);
    IntSize containerSize() const;
    void computeIntrinsicDimensions(float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio) final;
    bool hasNaturalAspectRatio() const final;
    NaturalDimensions unorientedNaturalDimensions() const final;

    void reportApproximateMemoryCost() const;
    EncodedDataStatus dataChanged(bool allDataReceived) final;

    // FIXME: SVGImages will be unable to prune because destroyDecodedData() is not implemented yet.

    // FIXME: Implement this to be less conservative.
    bool currentFrameKnownToBeOpaque() const final { return false; }
    bool currentFrameIsComplete() const final { return !!m_page; }

    bool hasHDRContent() const final;

    RefPtr<NativeImage> nativeImage(ConcreteObjectSize, const ColorSpace& = ColorSpace::SRGB(), const ImageDrawingExtras* = nullptr) final;
    RefPtr<NativeImage> currentNativeImage(ConcreteObjectSize, const ImageDrawingExtras* = nullptr) final;
    RefPtr<NativeImage> currentPreTransformedNativeImage(ConcreteObjectSize, ImageOrientation = ImageOrientation::Orientation::FromImage, const ImageDrawingExtras* = nullptr) final;

    void startAnimationTimerFired();

    WEBCORE_EXPORT explicit SVGImage(ImageObserver*);
    ImageDrawResult draw(GraphicsContext&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr) final;
    void drawPattern(GraphicsContext&, ConcreteObjectSize, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr) final;

    void applyFragmentURL(const URL&);
    void applyLinkParameters(const Style::LinkParameters&);
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    void applyInvertContent(InvertContent);
#endif

    RefPtr<Page> m_page;
    FloatSize m_intrinsicSize;

    Style::LinkParameters m_appliedLinkParameters;
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    bool m_fallbackInvertContent { false };
#endif

    Timer m_startAnimationTimer;
};

bool isInSVGImage(const Element*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_IMAGE(SVGImage)
