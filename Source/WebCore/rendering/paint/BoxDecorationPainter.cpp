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
#include "BoxDecorationPainter.h"

#include "BackgroundPainter.h"
#include "BorderPainter.h"
#include "BorderShape.h"
#include "ContainerNodeInlines.h"
#include "DocumentPage.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "InlineIteratorInlineBox.h"
#include "LocalFrameView.h"
#include "LocalFrameViewInlines.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleImage.h"

namespace WebCore {
BoxDecorationPainter::BoxDecorationPainter(RenderBox& renderer, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_renderer(renderer)
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
{
}

void BoxDecorationPainter::paintBoxDecorations()
{
    if (!m_paintInfo.shouldPaintWithinRoot(m_renderer))
        return;

    if (CheckedPtr view = dynamicDowncast<RenderView>(m_renderer)) {
        paintViewBackground(*view);
        return;
    }
    if (CheckedPtr table = dynamicDowncast<RenderTable>(m_renderer)) {
        paintTableDecorations(*table);
        return;
    }
    if (CheckedPtr cell = dynamicDowncast<RenderTableCell>(m_renderer)) {
        paintTableCellDecorations(*cell);
        return;
    }

    LayoutRect paintRect = m_renderer->borderBoxRect();
    paintRect.moveBy(m_paintOffset);
    m_renderer->adjustBorderBoxRectForPainting(paintRect);

    auto& theme = m_renderer->theme();
    paintRect = theme.adjustedPaintRect(m_renderer, paintRect);
    auto bleedAvoidance = determineBleedAvoidance();

    BackgroundPainter backgroundPainter { m_renderer, m_paintInfo };

    // FIXME: Should eventually give the theme control over whether the box shadow should paint, since controls could have
    // custom shadows of their own.
    if (!BackgroundPainter::boxShadowShouldBeAppliedToBackground(m_renderer, paintRect.location(), bleedAvoidance, { }))
        backgroundPainter.paintBoxShadow(paintRect, m_renderer->usedStyle(), Style::ShadowStyle::Normal);

    GraphicsContextStateSaver stateSaver(m_paintInfo.context(), false);
    if (bleedAvoidance == BleedAvoidance::UseTransparencyLayer) {
        // To avoid the background color bleeding out behind the border, we'll render background and border
        // into a transparency layer, and then clip that in one go (which requires setting up the clip before
        // beginning the layer).
        stateSaver.save();
        auto borderShape = BorderShape::shapeForBorderRect(m_renderer->usedStyle(), paintRect);
        borderShape.clipToOuterShape(m_paintInfo.context(), m_renderer->document().pixelSnappingScaleFactor());
        m_paintInfo.context().beginTransparencyLayer(1);
    }

    CheckedRef style = m_renderer->style();

    // If we have a native theme appearance, paint that before painting our background.
    // The theme will tell us whether or not we should also paint the CSS background.
    bool borderOrBackgroundPaintingIsNeeded = true;
    if (style->hasUsedAppearance()) {
        if (RefPtr control = m_renderer->ensureControlPartForRenderer())
            borderOrBackgroundPaintingIsNeeded = theme.paint(m_renderer, *control, m_paintInfo, paintRect);
        else
            borderOrBackgroundPaintingIsNeeded = theme.paint(m_renderer, m_paintInfo, paintRect);
    }

    BorderPainter borderPainter { m_renderer, m_paintInfo };

    if (borderOrBackgroundPaintingIsNeeded) {
        if (bleedAvoidance == BleedAvoidance::BackgroundOverBorder)
            borderPainter.paintBorder(paintRect, m_renderer->usedStyle(), bleedAvoidance);

        backgroundPainter.paintBackground(paintRect, bleedAvoidance);

        if (style->hasUsedAppearance()) {
            if (RefPtr control = m_renderer->ensureControlPartForDecorations())
                theme.paint(m_renderer, *control, m_paintInfo, paintRect);
            else
                theme.paintDecorations(m_renderer, m_paintInfo, paintRect);
        }
    }

    backgroundPainter.paintBoxShadow(paintRect, m_renderer->usedStyle(), Style::ShadowStyle::Inset);

    if (bleedAvoidance != BleedAvoidance::BackgroundOverBorder) {
        bool paintCSSBorder = false;

        if (!style->hasUsedAppearance())
            paintCSSBorder = true;
        else if (borderOrBackgroundPaintingIsNeeded) {
            // The theme will tell us whether or not we should also paint the CSS border.
            if (RefPtr control = m_renderer->ensureControlPartForBorderOnly())
                paintCSSBorder = theme.paint(m_renderer, *control, m_paintInfo, paintRect);
            else
                paintCSSBorder = theme.paintBorderOnly(m_renderer, m_paintInfo);
        }

        if (paintCSSBorder && style->border().hasVisibleBorderDecoration())
            borderPainter.paintBorder(paintRect, m_renderer->usedStyle(), bleedAvoidance);
    }

    if (bleedAvoidance == BleedAvoidance::UseTransparencyLayer)
        m_paintInfo.context().endTransparencyLayer();
}

void BoxDecorationPainter::paintTableDecorations(RenderTable& table)
{
    LayoutRect rect(m_paintOffset, table.borderBoxSize());
    m_renderer->adjustBorderBoxRectForPainting(rect);

    BackgroundPainter backgroundPainter { table, m_paintInfo };

    auto bleedAvoidance = determineBleedAvoidance();
    if (!BackgroundPainter::boxShadowShouldBeAppliedToBackground(table, rect.location(), bleedAvoidance, { }))
        backgroundPainter.paintBoxShadow(rect, table.usedStyle(), Style::ShadowStyle::Normal);

    GraphicsContextStateSaver stateSaver(m_paintInfo.context(), false);
    if (bleedAvoidance == BleedAvoidance::UseTransparencyLayer) {
        // To avoid the background color bleeding out behind the border, we'll render background and border
        // into a transparency layer, and then clip that in one go (which requires setting up the clip before
        // beginning the layer).
        stateSaver.save();
        auto borderShape = BorderShape::shapeForBorderRect(table.usedStyle(), rect);
        borderShape.clipToOuterShape(m_paintInfo.context(), table.style().deviceScaleFactor());
        m_paintInfo.context().beginTransparencyLayer(1);
    }

    backgroundPainter.paintBackground(rect, bleedAvoidance);
    backgroundPainter.paintBoxShadow(rect, table.usedStyle(), Style::ShadowStyle::Inset);

    if (table.style().border().hasVisibleBorderDecoration() && !table.collapseBorders())
        BorderPainter { table, m_paintInfo }.paintBorder(rect, table.usedStyle());

    if (bleedAvoidance == BleedAvoidance::UseTransparencyLayer)
        m_paintInfo.context().endTransparencyLayer();
}

void BoxDecorationPainter::paintTableCellDecorations(RenderTableCell& cell)
{
    if (cell.isHiddenEmptyCell())
        return;

    CheckedPtr table = cell.table();

    LayoutRect paintRect = LayoutRect(m_paintOffset, cell.borderBoxSize());
    m_renderer->adjustBorderBoxRectForPainting(paintRect);

    BackgroundPainter backgroundPainter { cell, m_paintInfo };
    backgroundPainter.paintBoxShadow(paintRect, cell.usedStyle(), Style::ShadowStyle::Normal);

    // Paint our cell background.
    cell.paintBackgroundsBehindCell(m_paintInfo, m_paintOffset, &cell, m_paintOffset);

    // For collapsed borders, we expand from inner edge to outer edge for inset shadow painting.
    LayoutRect insetShadowRect = paintRect;
    if (table->collapseBorders())
        insetShadowRect.expand(RectEdges<LayoutUnit> { cell.borderTop(), cell.borderRight(), cell.borderBottom(), cell.borderLeft() });

    backgroundPainter.paintBoxShadow(insetShadowRect, cell.usedStyle(), Style::ShadowStyle::Inset);

    if (!cell.style().border().hasBorder() || table->collapseBorders())
        return;

    BorderPainter borderPainter { cell, m_paintInfo };
    borderPainter.paintBorder(paintRect, cell.usedStyle());
}

static inline bool rendererObscuresBackground(const RenderElement& rootElement)
{
    CheckedRef style = rootElement.style();
    if (rootElement.usedStyle().visibility() != UsedVisibility::Visible || !style->opacity().isOpaque() || !style->transform().isNone() || !style->offsetPath().isNone())
        return false;

    if (style->border().hasBorderRadius())
        return false;

    if (rootElement.isComposited())
        return false;

    if (rootElement.hasClipPath() && rootElement.isRenderOrLegacyRenderSVGRoot())
        return false;

    CheckedPtr rendererForBackground = protect(rootElement.view())->rendererForRootBackground();
    if (!rendererForBackground)
        return false;

    if (rendererForBackground->style().backgroundLayers().usedFirst().clip() == FillBox::Text
        || rendererForBackground->style().backgroundLayers().usedFirst().clip() == FillBox::BorderAreaText)
        return false;

    return true;
}

void BoxDecorationPainter::paintViewBackground(RenderView& view)
{
    // Check to see if we are enclosed by a layer that requires complex painting rules. If so, we cannot blit
    // when scrolling, and we need to use slow repaints. Examples of layers that require this are transparent layers,
    // layers with reflections, or transformed layers.
    // FIXME: This needs to be dynamic. We should be able to go back to blitting if we ever stop being inside
    // a transform, transparency layer, etc.
    Ref document = view.document();
    Ref frameView = view.frameView();
    for (RefPtr element = document->ownerElement(); element && element->renderer(); element = element->document().ownerElement()) {
        CheckedPtr layer = element->renderer()->enclosingLayer();
        if (layer->cannotBlitToWindow()) {
            frameView->setCannotBlitToWindow();
            break;
        }

        if (CheckedPtr compositingLayer = layer->enclosingCompositingLayerForRepaint().layer) {
            if (!compositingLayer->backing()->paintsIntoWindow()) {
                frameView->setCannotBlitToWindow();
                break;
            }
        }
    }

    if (!view.shouldPaintBaseBackground())
        return;

    if (m_paintInfo.skipRootBackground())
        return;

    bool rootFillsViewport = false;
    bool rootObscuresBackground = false;
    auto shouldPropagateBackgroundPaintingToInitialContainingBlock = true;
    RefPtr documentElement = document->documentElement();
    if (CheckedPtr rootRenderer = documentElement ? documentElement->renderer() : nullptr) {
        // The document element's renderer is currently forced to be a block, but may not always be.
        CheckedPtr rootBox = dynamicDowncast<RenderBox>(*rootRenderer);
        rootFillsViewport = rootBox && !rootBox->x() && !rootBox->y() && rootBox->borderBoxWidth() >= view.borderBoxWidth() && rootBox->borderBoxHeight() >= view.borderBoxHeight();
        rootObscuresBackground = rendererObscuresBackground(*rootRenderer);
        shouldPropagateBackgroundPaintingToInitialContainingBlock = !!view.rendererForRootBackground();
    }

    protect(view.compositor())->rootBackgroundColorOrTransparencyChanged();

    RefPtr page = document->page();
    float pageScaleFactor = page ? page->pageScaleFactor() : 1;

    // If painting will entirely fill the view, no need to fill the background.
    if (rootFillsViewport && rootObscuresBackground && pageScaleFactor >= 1 && view.rootElementShouldPaintBaseBackground())
        return;

    // This code typically only executes if the root element's visibility has been set to hidden,
    // if there is a transform on the <html>, or if there is a page scale factor less than 1.
    // Only fill with a background color (typically white) if we're the root document,
    // since iframes/frames with no background in the child document should show the parent's background.
    // We use the base background color unless the backgroundShouldExtendBeyondPage setting is set,
    // in which case we use the document's background color.
    if (frameView->isTransparent()) // FIXME: This needs to be dynamic. We should be able to go back to blitting if we ever stop being transparent.
        frameView->setCannotBlitToWindow(); // The parent must show behind the child.
    else {
        const Color& documentBackgroundColor = frameView->documentBackgroundColor();
        const Color& backgroundColor = (shouldPropagateBackgroundPaintingToInitialContainingBlock && view.settings().backgroundShouldExtendBeyondPage() && documentBackgroundColor.isValid()) ? documentBackgroundColor : frameView->baseBackgroundColor();
        if (backgroundColor.isVisible()) {
            CompositeOperator previousOperator = m_paintInfo.context().compositeOperation();
            m_paintInfo.context().setCompositeOperation(CompositeOperator::Copy);
            m_paintInfo.context().fillRect(m_paintInfo.rect, backgroundColor);
            m_paintInfo.context().setCompositeOperation(previousOperator);
        } else
            m_paintInfo.context().clearRect(m_paintInfo.rect);
    }
}

BleedAvoidance BoxDecorationPainter::determineBleedAvoidance() const
{
    auto& context = m_paintInfo.context();

    if (context.paintingDisabled())
        return BleedAvoidance::None;

    CheckedRef style = m_renderer->style();

    if (!style->hasBackground() || !style->border().hasBorder() || !style->border().hasBorderRadius() || m_renderer->borderImageIsLoadedAndCanBeRendered())
        return BleedAvoidance::None;

    if (!m_renderer->theme().mayNeedBleedAvoidance(style))
        return BleedAvoidance::None;

    AffineTransform ctm = context.getCTM();
    FloatSize contextScaling(static_cast<float>(ctm.xScale()), static_cast<float>(ctm.yScale()));

    // Because LayoutRoundedRect uses IntRect internally the inset applied by the
    // BleedAvoidance::ShrinkBackground strategy cannot be less than one integer
    // layout coordinate, even with subpixel layout enabled. To take that into
    // account, we clamp the contextScaling to 1.0 for the following test so
    // that borderObscuresBackgroundEdge can only return true if the border
    // widths are greater than 2 in both layout coordinates and screen
    // coordinates.
    // This precaution will become obsolete if LayoutRoundedRect is ever promoted to
    // a sub-pixel representation.
    if (contextScaling.width() > 1)
        contextScaling.setWidth(1);
    if (contextScaling.height() > 1)
        contextScaling.setHeight(1);

    if (m_renderer->borderObscuresBackgroundEdge(contextScaling))
        return BleedAvoidance::ShrinkBackground;

    if (!style->hasUsedAppearance() && m_renderer->borderObscuresBackground() && backgroundHasOpaqueTopLayer())
        return BleedAvoidance::BackgroundOverBorder;

    return BleedAvoidance::UseTransparencyLayer;
}

bool BoxDecorationPainter::backgroundHasOpaqueTopLayer() const
{
    CheckedRef style = m_renderer->style();

    auto& topLayer = style->backgroundLayers().usedFirst();
    if (topLayer.clip() != FillBox::BorderBox)
        return false;

    // Clipped with local scrolling
    if (m_renderer->hasNonVisibleOverflow() && topLayer.attachment() == FillAttachment::LocalBackground)
        return false;

    if (topLayer.hasOpaqueImage(m_renderer) && topLayer.hasRepeatXY() && topLayer.image().tryStyleImage()->canRender(m_renderer.ptr()))
        return true;

    // If there is only one layer and no image, check whether the background color is opaque.
    if (style->backgroundLayers().usedLength() == 1 && !topLayer.hasImage()) {
        Color bgColor = style->visitedDependentUsedBackgroundColorApplyingColorFilter();
        if (bgColor.isOpaque())
            return true;
    }

    return false;
}

}
