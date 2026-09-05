/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include <WebCore/AffineTransform.h>
#include <WebCore/DisplayList.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/Image.h>
#include <WebCore/SVGImage.h>
#include <WebCore/StyleLinkParameters.h>
#include <wtf/URL.h>

namespace WebCore {

class SVGImageForContainer final : public Image {
public:
    static Ref<SVGImageForContainer> create(SVGImage* image, SVGImage::ContainerContext&& containerContext)
    {
        return adoptRef(*new SVGImageForContainer(image, WTF::move(containerContext)));
    }

    bool NODELETE isSVGImageForContainer() const final { return true; }

    FloatSize size(ImageOrientation = ImageOrientation::Orientation::FromImage) const final;

    bool usesContainerSize() const final { return m_image->usesContainerSize(); }
    bool hasRelativeWidth() const final { return m_image->hasRelativeWidth(); }
    bool hasRelativeHeight() const final { return m_image->hasRelativeHeight(); }
    void computeIntrinsicDimensions(float& intrinsicWidth, float& intrinsicHeight, FloatSize& intrinsicRatio) final
    {
        protect(m_image)->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
    }

    ImageDrawResult draw(GraphicsContext&, const FloatRect&, const FloatRect&, ImagePaintingOptions = { }) final;

    void drawPattern(GraphicsContext&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, ImagePaintingOptions = { }) final;

    // FIXME: Implement this to be less conservative.
    bool currentFrameKnownToBeOpaque() const final { return false; }
    bool currentFrameIsComplete() const final { return !!m_image; }

    RefPtr<NativeImage> currentNativeImage() final;

private:
    WEBCORE_EXPORT SVGImageForContainer(SVGImage*, SVGImage::ContainerContext&&);

    // FIXME: SVGImage::draw sets composite operator, blend mode and orientation,
    // so those are recorded into m_displayList but not compared here yet.
    struct DrawParameters {
        FloatRect dstRect;
        FloatRect srcRect;

        friend bool operator==(const DrawParameters&, const DrawParameters&) = default;
    };

    WeakPtr<SVGImage> m_image;
    const SVGImage::ContainerContext m_containerContext;

    RefPtr<const DisplayList::DisplayList> m_displayList;
    DrawParameters m_drawParameters;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_IMAGE(SVGImageForContainer)
