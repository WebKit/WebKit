/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (c) 2020, 2021, 2022 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderImageResource.h"
#include "RenderSVGModelObject.h"
#include "SVGBoundingBoxComputation.h"

namespace WebCore {

class SVGImageElement;

class RenderSVGImage final : public RenderSVGModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGImage);
public:
    RenderSVGImage(SVGImageElement&, RenderStyle&&);
    virtual ~RenderSVGImage();

    SVGImageElement& imageElement() const;

    RenderImageResource& imageResource() { return *m_imageResource; }
    const RenderImageResource& imageResource() const { return *m_imageResource; }

    bool updateImageViewport();

private:
    void willBeDestroyed() final;

    void element() const = delete;

    ASCIILiteral renderName() const final { return "RenderSVGImage"_s; }
    bool canHaveChildren() const final { return false; }

    FloatRect calculateObjectBoundingBox() const;
    FloatRect objectBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect strokeBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) final;

    void layout() final;
    void paint(PaintInfo&, const LayoutPoint&) final;

    void paintForeground(PaintInfo&, const LayoutPoint&);
    ImageDrawResult paintIntoRect(PaintInfo&, const FloatRect&, const FloatRect&);

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    void repaintOrMarkForLayout(const IntRect* = nullptr);
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;
    bool bufferForeground(PaintInfo&, const LayoutPoint&);

    bool needsHasSVGTransformFlags() const final;

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption>) const final;

    CachedImage* cachedImage() const { return imageResource().cachedImage(); }

    FloatRect m_objectBoundingBox;
    std::unique_ptr<RenderImageResource> m_imageResource;
    RefPtr<ImageBuffer> m_bufferedForeground;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGImage, isRenderSVGImage())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
