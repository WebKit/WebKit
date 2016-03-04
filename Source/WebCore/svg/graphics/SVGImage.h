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

#ifndef SVGImage_h
#define SVGImage_h

#include "Image.h"
#include "URL.h"

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
    static Ref<SVGImage> create(ImageObserver& observer, const URL& url)
    {
        return adoptRef(*new SVGImage(observer, url));
    }

    RenderBox* embeddedContentBox() const;
    FrameView* frameView() const;

    bool isSVGImage() const override { return true; }
    FloatSize size() const override { return m_intrinsicSize; }

    void setURL(const URL& url) { m_url = url; }

    bool hasSingleSecurityOrigin() const override;

    bool hasRelativeWidth() const override;
    bool hasRelativeHeight() const override;

    void startAnimation(CatchUpAnimation = CatchUp) override;
    void stopAnimation() override;
    void resetAnimation() override;

#if USE(CAIRO)
    PassNativeImagePtr nativeImageForCurrentFrame() override;
#endif

private:
    friend class SVGImageChromeClient;
    friend class SVGImageForContainer;

    virtual ~SVGImage();

    String filenameExtension() const override;

    void setContainerSize(const FloatSize&) override;
    IntSize containerSize() const;
    bool usesContainerSize() const override { return true; }
    void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) override;

    bool dataChanged(bool allDataReceived) override;

    // FIXME: SVGImages will be unable to prune because this function is not implemented yet.
    void destroyDecodedData(bool) override { }

    // FIXME: Implement this to be less conservative.
    bool currentFrameKnownToBeOpaque() override { return false; }

    void dump(TextStream&) const override;

    SVGImage(ImageObserver&, const URL&);
    void draw(GraphicsContext&, const FloatRect& fromRect, const FloatRect& toRect, CompositeOperator, BlendMode, ImageOrientationDescription) override;
    void drawForContainer(GraphicsContext&, const FloatSize, float, const FloatRect&, const FloatRect&, CompositeOperator, BlendMode);
    void drawPatternForContainer(GraphicsContext&, const FloatSize& containerSize, float zoom, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing,
        CompositeOperator, const FloatRect&, BlendMode);

    SVGSVGElement* rootElement() const;

    std::unique_ptr<SVGImageChromeClient> m_chromeClient;
    std::unique_ptr<Page> m_page;
    FloatSize m_intrinsicSize;
    URL m_url;
};

bool isInSVGImage(const Element*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_IMAGE(SVGImage)

#endif // SVGImage_h
