/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "AffineTransform.h"
#include "FloatRect.h"
#include "LegacyRenderSVGModelObject.h"

namespace WebCore {

class RenderImageResource;
class SVGImageElement;

class LegacyRenderSVGImage final : public LegacyRenderSVGModelObject {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGImage);
public:
    LegacyRenderSVGImage(SVGImageElement&, RenderStyle&&);
    virtual ~LegacyRenderSVGImage();

    SVGImageElement& imageElement() const;

    bool updateImageViewport();
    void setNeedsBoundariesUpdate() override { m_needsBoundariesUpdate = true; }
    bool needsBoundariesUpdate() override { return m_needsBoundariesUpdate; }
    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    RenderImageResource& imageResource() { return *m_imageResource; }
    const RenderImageResource& imageResource() const { return *m_imageResource; }

    // Note: Assumes the PaintInfo context has had all local transforms applied.
    void paintForeground(PaintInfo&);

private:
    void willBeDestroyed() override;

    void element() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGImage"_s; }
    bool canHaveChildren() const override { return false; }

    const AffineTransform& localToParentTransform() const override { return m_localTransform; }

    FloatRect calculateObjectBoundingBox() const;
    FloatRect objectBoundingBox() const override { return m_objectBoundingBox; }
    FloatRect strokeBoundingBox() const override { return m_objectBoundingBox; }
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const override { return m_repaintBoundingBox; }

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) const override;

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

    void layout() override;
    void paint(PaintInfo&, const LayoutPoint&) override;

    void invalidateBufferedForeground();

    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;

    AffineTransform localTransform() const override { return m_localTransform; }

    bool m_needsBoundariesUpdate : 1;
    bool m_needsTransformUpdate : 1;
    AffineTransform m_localTransform;
    FloatRect m_objectBoundingBox;
    FloatRect m_repaintBoundingBox;
    std::unique_ptr<RenderImageResource> m_imageResource;
    RefPtr<ImageBuffer> m_bufferedForeground;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(LegacyRenderSVGImage, isLegacyRenderSVGImage())
