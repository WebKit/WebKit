/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "LegacyRenderSVGContainer.h"

namespace WebCore {

// This is used for non-root <svg> elements and <marker> elements, neither of which are SVGTransformable
// thus we inherit from LegacyRenderSVGContainer instead of LegacyRenderSVGTransformableContainer
class LegacyRenderSVGViewportContainer final : public LegacyRenderSVGContainer {
    WTF_MAKE_ISO_ALLOCATED(LegacyRenderSVGViewportContainer);
public:
    LegacyRenderSVGViewportContainer(SVGSVGElement&, RenderStyle&&);

    SVGSVGElement& svgSVGElement() const;

    FloatRect viewport() const { return m_viewport; }

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    bool didTransformToRootUpdate() override { return m_didTransformToRootUpdate; }

    void determineIfLayoutSizeChanged() override;
    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    void paint(PaintInfo&, const LayoutPoint&) override;

private:
    void element() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGViewportContainer"_s; }

    AffineTransform viewportTransform() const;
    const AffineTransform& localToParentTransform() const override { return m_localToParentTransform; }

    void calcViewport() override;
    bool calculateLocalTransform() override;

    void applyViewportClip(PaintInfo&) override;
    bool pointIsInsideViewportClip(const FloatPoint& pointInParent) override;

    bool m_didTransformToRootUpdate : 1;
    bool m_isLayoutSizeChanged : 1;
    bool m_needsTransformUpdate : 1;

    FloatRect m_viewport;
    mutable AffineTransform m_localToParentTransform;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(LegacyRenderSVGViewportContainer, isLegacyRenderSVGViewportContainer())
