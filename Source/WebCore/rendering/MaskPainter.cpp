/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2005-2025 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2019 Google Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 *
 */

#include "config.h"
#include "MaskPainter.h"

#include "BackgroundPainter.h"
#include "BorderPainter.h"
#include "GraphicsContext.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleImage.h"

namespace WebCore {

MaskPainter::MaskPainter(RenderBox& renderer, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_renderer(renderer)
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
{
}

void MaskPainter::paintMask()
{
    auto shouldPaint = [&] {
        if (m_renderer->usedStyle().visibility() != UsedVisibility::Visible || m_paintInfo.phase != PaintPhase::Mask)
            return false;
        if (is<RenderTable>(m_renderer))
            return true;
        if (CheckedPtr cell = dynamicDowncast<RenderTableCell>(m_renderer))
            return !cell->isHiddenEmptyCell();
        return m_paintInfo.shouldPaintWithinRoot(m_renderer) && !m_paintInfo.context().paintingDisabled();
    };
    if (!shouldPaint())
        return;

    LayoutRect paintRect = LayoutRect(m_paintOffset, m_renderer->borderBoxSize());
    m_renderer->adjustBorderBoxRectForPainting(paintRect);
    paintMaskImages(paintRect);
}

void MaskPainter::paintClippingMask()
{
    if (!m_paintInfo.shouldPaintWithinRoot(m_renderer) || m_renderer->usedStyle().visibility() != UsedVisibility::Visible || m_paintInfo.phase != PaintPhase::ClippingMask || m_paintInfo.context().paintingDisabled())
        return;

    LayoutRect paintRect = LayoutRect(m_paintOffset, m_renderer->borderBoxSize());

    if (m_renderer->document().settings().layerBasedSVGEngineEnabled() && WTF::holdsAlternative<Style::ReferencePath>(m_renderer->style().clipPath())) {
        m_renderer->paintSVGClippingMask(m_paintInfo, paintRect);
        return;
    }

    m_paintInfo.context().fillRect(snappedIntRect(paintRect), Color::black);
}

void MaskPainter::paintMaskImages(const LayoutRect& paintRect)
{
    CheckedRef style = m_renderer->style();

    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = m_renderer->hasLayer() && m_renderer->layer()->hasCompositedMask();
    bool flattenCompositingLayers = m_paintInfo.paintBehavior.contains(PaintBehavior::FlattenCompositingLayers);
    CompositeOperator compositeOp = CompositeOperator::SourceOver;

    bool allMaskImagesLoaded = true;

    if (!compositedMask || flattenCompositingLayers) {
        pushTransparencyLayer = true;

        // Don't render a masked element until all the mask images have loaded, to prevent a flash of unmasked content.
        if (RefPtr maskBorder = style->maskBorderSource().tryStyleImage())
            allMaskImagesLoaded &= maskBorder->isLoaded(m_renderer.ptr());

        allMaskImagesLoaded &= Style::imagesAreLoaded(style->maskLayers(), m_renderer);

        m_paintInfo.context().setCompositeOperation(CompositeOperator::DestinationIn);
        m_paintInfo.context().beginTransparencyLayer(1);
        compositeOp = CompositeOperator::SourceOver;
    }

    if (allMaskImagesLoaded) {
        BackgroundPainter { m_renderer, m_paintInfo }.paintFillLayers(Color(), style->maskLayers(), style->usedZoomForLength(), paintRect, BleedAvoidance::None, compositeOp);
        BorderPainter { m_renderer, m_paintInfo }.paintNinePieceImage(paintRect, m_renderer->usedStyle(), style->maskBorder(), compositeOp);
    }

    if (pushTransparencyLayer)
        m_paintInfo.context().endTransparencyLayer();
}

}
