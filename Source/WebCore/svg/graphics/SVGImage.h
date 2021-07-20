/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "Image.h"
#include <wtf/URL.h>

namespace WebCore {

class Element;
class FrameView;
class ImageBuffer;
class Page;
class RenderBox;
class SVGSVGElement;
class SVGImageChromeClient;
class SVGImageForContainer;

class SVGImage final : public Image {
public:
    static Ref<SVGImage> create(ImageObserver& observer) { return adoptRef(*new SVGImage(observer)); }

    RenderBox* embeddedContentBox() const;
    FrameView* frameView() const;

    bool isSVGImage() const final { return true; }
    FloatSize size(ImageOrientation = ImageOrientation::FromImage) const final { return m_intrinsicSize; }

    bool hasSingleSecurityOrigin() const final;

    bool hasRelativeWidth() const final;
    bool hasRelativeHeight() const final;

    void startAnimation() final;
    void stopAnimation() final;
    void resetAnimation() final;
    bool isAnimating() const final;

    void scheduleStartAnimation();

    Page* internalPage() { return m_page.get(); }

private:
    friend class SVGImageChromeClient;
    friend class SVGImageForContainer;

    virtual ~SVGImage();

    String filenameExtension() const final;

    void setContainerSize(const FloatSize&) final;
    IntSize containerSize() const;
    bool usesContainerSize() const final { return true; }
    void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) final;

    void reportApproximateMemoryCost() const;
    EncodedDataStatus dataChanged(bool allDataReceived) final;

    // FIXME: SVGImages will be unable to prune because this function is not implemented yet.
    void destroyDecodedData(bool) final { }

    // FIXME: Implement this to be less conservative.
    bool currentFrameKnownToBeOpaque() const final { return false; }

    RefPtr<NativeImage> nativeImageForCurrentFrame(const GraphicsContext* = nullptr) final;
    RefPtr<NativeImage> nativeImage(const GraphicsContext* = nullptr) final;
    RefPtr<NativeImage> nativeImage(const FloatSize& imageSize, const FloatRect& sourceRect);

    void startAnimationTimerFired();

    WEBCORE_EXPORT explicit SVGImage(ImageObserver&);
    ImageDrawResult draw(GraphicsContext&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = { }) final;
    ImageDrawResult drawAsNativeImage(GraphicsContext&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = { });
    ImageDrawResult drawForContainer(GraphicsContext&, const FloatSize containerSize, float containerZoom, const URL& initialFragmentURL, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& = { });
    void drawPatternForContainer(GraphicsContext&, const FloatSize& containerSize, float containerZoom, const URL& initialFragmentURL, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const FloatRect&, const ImagePaintingOptions& = { });

    RefPtr<SVGSVGElement> rootElement() const;

    std::unique_ptr<SVGImageChromeClient> m_chromeClient;
    std::unique_ptr<Page> m_page;
    FloatSize m_intrinsicSize;

    Timer m_startAnimationTimer;
};

bool isInSVGImage(const Element*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_IMAGE(SVGImage)
