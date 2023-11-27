/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InlineBoxPainter.h"

#include "BackgroundPainter.h"
#include "BorderPainter.h"
#include "GraphicsContext.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLineBox.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderElementInlines.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderView.h"

namespace WebCore {

InlineBoxPainter::InlineBoxPainter(const LegacyInlineFlowBox& inlineBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : InlineBoxPainter(*InlineIterator::inlineBoxFor(inlineBox), paintInfo, paintOffset)
{
}

InlineBoxPainter::InlineBoxPainter(const LayoutIntegration::InlineContent& inlineContent, const InlineDisplay::Box& box, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : InlineBoxPainter(*InlineIterator::inlineBoxFor(inlineContent, box), paintInfo, paintOffset)
{
}

InlineBoxPainter::InlineBoxPainter(const InlineIterator::InlineBox& inlineBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_inlineBox(inlineBox)
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
    , m_renderer(m_inlineBox.renderer())
    , m_isFirstLineBox(m_inlineBox.lineBox()->isFirst())
    , m_isRootInlineBox(m_inlineBox.isRootInlineBox())
    , m_isHorizontal(m_inlineBox.isHorizontal())
{
}

InlineBoxPainter::~InlineBoxPainter() = default;

void InlineBoxPainter::paint()
{
    if (m_paintInfo.phase == PaintPhase::Outline || m_paintInfo.phase == PaintPhase::SelfOutline) {
        if (renderer().style().visibility() != Visibility::Visible || !renderer().hasOutline() || m_isRootInlineBox)
            return;

        auto& inlineFlow = downcast<RenderInline>(renderer());
        RenderBlock* containingBlock = nullptr;

        bool containingBlockPaintsContinuationOutline = inlineFlow.continuation() || inlineFlow.isContinuation();
        if (containingBlockPaintsContinuationOutline) {
            // FIXME: See https://bugs.webkit.org/show_bug.cgi?id=54690. We currently don't reconnect inline continuations
            // after a child removal. As a result, those merged inlines do not get seperated and hence not get enclosed by
            // anonymous blocks. In this case, it is better to bail out and paint it ourself.
            RenderBlock* enclosingAnonymousBlock = renderer().containingBlock();
            if (!enclosingAnonymousBlock->isAnonymousBlock())
                containingBlockPaintsContinuationOutline = false;
            else {
                containingBlock = enclosingAnonymousBlock->containingBlock();
                for (auto* box = &renderer(); box != containingBlock; box = &box->parent()->enclosingBoxModelObject()) {
                    if (box->hasSelfPaintingLayer()) {
                        containingBlockPaintsContinuationOutline = false;
                        break;
                    }
                }
            }
        }

        if (containingBlockPaintsContinuationOutline) {
            // Add ourselves to the containing block of the entire continuation so that it can
            // paint us atomically.
            containingBlock->addContinuationWithOutline(downcast<RenderInline>(renderer().element()->renderer()));
        } else if (!inlineFlow.isContinuation())
            m_paintInfo.outlineObjects->add(inlineFlow);

        return;
    }

    if (m_paintInfo.phase == PaintPhase::Mask) {
        paintMask();
        return;
    }

    if (m_paintInfo.phase == PaintPhase::Accessibility) {
        if (auto* renderInline = dynamicDowncast<RenderInline>(m_renderer)) {
            auto linesBoundingBox = enclosingIntRect(renderInline->linesVisualOverflowBoundingBox());
            linesBoundingBox.moveBy(roundedIntPoint(m_paintOffset));
            m_paintInfo.accessibilityRegionContext()->takeBounds(dynamicDowncast<RenderInline>(m_renderer), WTFMove(linesBoundingBox));
        }
        return;
    }

    paintDecorations();
}

static LayoutRect clipRectForNinePieceImageStrip(const InlineIterator::InlineBox& box, const NinePieceImage& image, const LayoutRect& paintRect)
{
    LayoutRect clipRect(paintRect);
    auto& style = box.renderer().style();
    LayoutBoxExtent outsets = style.imageOutsets(image);
    auto [hasClosedLeftEdge, hasClosedRightEdge] = box.hasClosedLeftAndRightEdge();
    if (box.isHorizontal()) {
        clipRect.setY(paintRect.y() - outsets.top());
        clipRect.setHeight(paintRect.height() + outsets.top() + outsets.bottom());
        if (hasClosedLeftEdge) {
            clipRect.setX(paintRect.x() - outsets.left());
            clipRect.setWidth(paintRect.width() + outsets.left());
        }
        if (hasClosedRightEdge)
            clipRect.setWidth(clipRect.width() + outsets.right());
    } else {
        clipRect.setX(paintRect.x() - outsets.left());
        clipRect.setWidth(paintRect.width() + outsets.left() + outsets.right());
        if (hasClosedLeftEdge) {
            clipRect.setY(paintRect.y() - outsets.top());
            clipRect.setHeight(paintRect.height() + outsets.top());
        }
        if (hasClosedRightEdge)
            clipRect.setHeight(clipRect.height() + outsets.bottom());
    }
    return clipRect;
}

void InlineBoxPainter::paintMask()
{
    if (!m_paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != Visibility::Visible || m_paintInfo.phase != PaintPhase::Mask)
        return;

    // Move x/y to our coordinates.
    auto localRect = LayoutRect { m_inlineBox.visualRect() };
    LayoutPoint adjustedPaintOffset = m_paintOffset + localRect.location();

    const NinePieceImage& maskNinePieceImage = renderer().style().maskBorder();
    StyleImage* maskBorder = renderer().style().maskBorder().image();

    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = renderer().hasLayer() && renderer().layer()->hasCompositedMask();
    bool flattenCompositingLayers = renderer().view().frameView().paintBehavior().contains(PaintBehavior::FlattenCompositingLayers);
    CompositeOperator compositeOp = CompositeOperator::SourceOver;
    if (!compositedMask || flattenCompositingLayers) {
        if ((maskBorder && renderer().style().maskLayers().hasImage()) || renderer().style().maskLayers().next())
            pushTransparencyLayer = true;

        compositeOp = CompositeOperator::DestinationIn;
        if (pushTransparencyLayer) {
            m_paintInfo.context().setCompositeOperation(CompositeOperator::DestinationIn);
            m_paintInfo.context().beginTransparencyLayer(1.0f);
            compositeOp = CompositeOperator::SourceOver;
        }
    }

    LayoutRect paintRect = LayoutRect(adjustedPaintOffset, localRect.size());

    paintFillLayers(Color(), renderer().style().maskLayers(), paintRect, compositeOp);

    bool hasBoxImage = maskBorder && maskBorder->canRender(&renderer(), renderer().style().effectiveZoom());
    if (!hasBoxImage || !maskBorder->isLoaded(&renderer())) {
        if (pushTransparencyLayer)
            m_paintInfo.context().endTransparencyLayer();
        return; // Don't paint anything while we wait for the image to load.
    }

    bool hasSingleLine = !m_inlineBox.previousInlineBox() && !m_inlineBox.nextInlineBox();

    BorderPainter borderPainter { renderer(), m_paintInfo };

    if (hasSingleLine)
        borderPainter.paintNinePieceImage(LayoutRect(adjustedPaintOffset, localRect.size()), renderer().style(), maskNinePieceImage, compositeOp);
    else {
        // We have a mask image that spans multiple lines.
        // We need to adjust _tx and _ty by the width of all previous lines.
        LayoutUnit logicalOffsetOnLine;
        for (auto box = m_inlineBox.previousInlineBox(); box; box.traversePreviousInlineBox())
            logicalOffsetOnLine += box->logicalWidth();
        LayoutUnit totalLogicalWidth = logicalOffsetOnLine;
        for (auto box = m_inlineBox.iterator(); box; box.traverseNextInlineBox())
            totalLogicalWidth += box->logicalWidth();
        LayoutUnit stripX = adjustedPaintOffset.x() - (isHorizontal() ? logicalOffsetOnLine : 0_lu);
        LayoutUnit stripY = adjustedPaintOffset.y() - (isHorizontal() ? 0_lu : logicalOffsetOnLine);
        LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : localRect.width();
        LayoutUnit stripHeight = isHorizontal() ? localRect.height() : totalLogicalWidth;

        LayoutRect clipRect = clipRectForNinePieceImageStrip(m_inlineBox, maskNinePieceImage, paintRect);
        GraphicsContextStateSaver stateSaver(m_paintInfo.context());
        m_paintInfo.context().clip(clipRect);
        borderPainter.paintNinePieceImage(LayoutRect(stripX, stripY, stripWidth, stripHeight), renderer().style(), maskNinePieceImage, compositeOp);
    }

    if (pushTransparencyLayer)
        m_paintInfo.context().endTransparencyLayer();
}

void InlineBoxPainter::paintDecorations()
{
    if (!m_paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != Visibility::Visible || m_paintInfo.phase != PaintPhase::Foreground)
        return;

    if (!m_isRootInlineBox && !renderer().hasVisibleBoxDecorations())
        return;

    auto& style = this->style();
    // You can use p::first-line to specify a background. If so, the root inline boxes for
    // a line may actually have to paint a background.
    if (m_isRootInlineBox && (!m_isFirstLineBox || &style == &renderer().style()))
        return;

    // Move x/y to our coordinates.
    auto localRect = LayoutRect { m_inlineBox.visualRect() };
    LayoutPoint adjustedPaintoffset = m_paintOffset + localRect.location();
    GraphicsContext& context = m_paintInfo.context();
    LayoutRect paintRect = LayoutRect(adjustedPaintoffset, localRect.size());
    // Shadow comes first and is behind the background and border.
    if (!BackgroundPainter::boxShadowShouldBeAppliedToBackground(renderer(), adjustedPaintoffset, BackgroundBleedNone, m_inlineBox))
        paintBoxShadow(ShadowStyle::Normal, paintRect);

    auto color = style.visitedDependentColor(CSSPropertyBackgroundColor);
    auto compositeOp = renderer().document().compositeOperatorForBackgroundColor(color, renderer());

    color = style.colorByApplyingColorFilter(color);

    paintFillLayers(color, style.backgroundLayers(), paintRect, compositeOp);
    paintBoxShadow(ShadowStyle::Inset, paintRect);

    // :first-line cannot be used to put borders on a line. Always paint borders with our
    // non-first-line style.
    if (m_isRootInlineBox || !renderer().style().hasVisibleBorderDecoration())
        return;

    const NinePieceImage& borderImage = renderer().style().borderImage();
    StyleImage* borderImageSource = borderImage.image();
    bool hasBorderImage = borderImageSource && borderImageSource->canRender(&renderer(), style.effectiveZoom());
    if (hasBorderImage && !borderImageSource->isLoaded(&renderer()))
        return; // Don't paint anything while we wait for the image to load.

    BorderPainter borderPainter { renderer(), m_paintInfo };

    bool hasSingleLine = !m_inlineBox.previousInlineBox() && !m_inlineBox.nextInlineBox();
    if (!hasBorderImage || hasSingleLine) {
        auto [hasClosedLeftEdge, hasClosedRightEdge] = m_inlineBox.hasClosedLeftAndRightEdge();
        borderPainter.paintBorder(paintRect, style, BackgroundBleedNone, hasClosedLeftEdge, hasClosedRightEdge);
        return;
    }

    // We have a border image that spans multiple lines.
    // We need to adjust tx and ty by the width of all previous lines.
    // Think of border image painting on inlines as though you had one long line, a single continuous
    // strip. Even though that strip has been broken up across multiple lines, you still paint it
    // as though you had one single line. This means each line has to pick up the image where
    // the previous line left off.
    // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
    // but it isn't even clear how this should work at all.
    LayoutUnit logicalOffsetOnLine;
    for (auto box = m_inlineBox.previousInlineBox(); box; box.traversePreviousInlineBox())
        logicalOffsetOnLine += box->logicalWidth();
    LayoutUnit totalLogicalWidth = logicalOffsetOnLine;
    for (auto box = m_inlineBox.iterator(); box; box.traverseNextInlineBox())
        totalLogicalWidth += box->logicalWidth();

    LayoutUnit stripX = adjustedPaintoffset.x() - (isHorizontal() ? logicalOffsetOnLine : 0_lu);
    LayoutUnit stripY = adjustedPaintoffset.y() - (isHorizontal() ? 0_lu : logicalOffsetOnLine);
    LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : localRect.width();
    LayoutUnit stripHeight = isHorizontal() ? localRect.height() : totalLogicalWidth;

    LayoutRect clipRect = clipRectForNinePieceImageStrip(m_inlineBox, borderImage, paintRect);
    GraphicsContextStateSaver stateSaver(context);
    context.clip(clipRect);
    borderPainter.paintBorder(LayoutRect(stripX, stripY, stripWidth, stripHeight), style);
}

void InlineBoxPainter::paintFillLayers(const Color& color, const FillLayer& fillLayer, const LayoutRect& rect, CompositeOperator op)
{
    Vector<const FillLayer*, 8> layers;
    for (auto* layer = &fillLayer; layer; layer = layer->next())
        layers.append(layer);

    for (auto* layer : makeReversedRange(layers))
        paintFillLayer(color, *layer, rect, op);
}

void InlineBoxPainter::paintFillLayer(const Color& color, const FillLayer& fillLayer, const LayoutRect& rect, CompositeOperator op)
{
    auto* image = fillLayer.image();
    bool hasFillImage = image && image->canRender(&renderer(), renderer().style().effectiveZoom());
    bool hasFillImageOrBorderRadious = hasFillImage || renderer().style().hasBorderRadius();
    bool hasSingleLine = !m_inlineBox.previousInlineBox() && !m_inlineBox.nextInlineBox();

    BackgroundPainter backgroundPainter { renderer(), m_paintInfo };

    if (!hasFillImageOrBorderRadious || hasSingleLine || m_isRootInlineBox) {
        backgroundPainter.paintFillLayer(color, fillLayer, rect, BackgroundBleedNone, m_inlineBox, { }, op);
        return;
    }

    if (renderer().style().boxDecorationBreak() == BoxDecorationBreak::Clone) {
        GraphicsContextStateSaver stateSaver(m_paintInfo.context());
        m_paintInfo.context().clip({ rect.location(), m_inlineBox.visualRectIgnoringBlockDirection().size() });
        backgroundPainter.paintFillLayer(color, fillLayer, rect, BackgroundBleedNone, m_inlineBox, { }, op);
        return;
    }

    // We have a fill image that spans multiple lines.
    // We need to adjust tx and ty by the width of all previous lines.
    // Think of background painting on inlines as though you had one long line, a single continuous
    // strip. Even though that strip has been broken up across multiple lines, you still paint it
    // as though you had one single line. This means each line has to pick up the background where
    // the previous line left off.
    LayoutUnit logicalOffsetOnLine;
    LayoutUnit totalLogicalWidth;
    if (renderer().style().direction() == TextDirection::LTR) {
        for (auto box = m_inlineBox.previousInlineBox(); box; box.traversePreviousInlineBox())
            logicalOffsetOnLine += box->logicalWidth();
        totalLogicalWidth = logicalOffsetOnLine;
        for (auto box = m_inlineBox.iterator(); box; box.traverseNextInlineBox())
            totalLogicalWidth += box->logicalWidth();
    } else {
        for (auto box = m_inlineBox.nextInlineBox(); box; box.traverseNextInlineBox())
            logicalOffsetOnLine += box->logicalWidth();
        totalLogicalWidth = logicalOffsetOnLine;
        for (auto box = m_inlineBox.iterator(); box; box.traversePreviousInlineBox())
            totalLogicalWidth += box->logicalWidth();
    }
    LayoutRect backgroundImageStrip {
        rect.x() - (isHorizontal() ? logicalOffsetOnLine : 0_lu),
        rect.y() - (isHorizontal() ? 0_lu : logicalOffsetOnLine),
        isHorizontal() ? totalLogicalWidth : LayoutUnit(m_inlineBox.visualRectIgnoringBlockDirection().width()),
        isHorizontal() ? LayoutUnit(m_inlineBox.visualRectIgnoringBlockDirection().height()) : totalLogicalWidth
    };

    GraphicsContextStateSaver stateSaver(m_paintInfo.context());
    m_paintInfo.context().clip(FloatRect { rect });
    backgroundPainter.paintFillLayer(color, fillLayer, rect, BackgroundBleedNone, m_inlineBox, backgroundImageStrip, op);
}

void InlineBoxPainter::paintBoxShadow(ShadowStyle shadowStyle, const LayoutRect& paintRect)
{
    BackgroundPainter backgroundPainter { renderer(), m_paintInfo };

    bool hasSingleLine = !m_inlineBox.previousInlineBox() && !m_inlineBox.nextInlineBox();
    if (hasSingleLine || m_isRootInlineBox) {
        backgroundPainter.paintBoxShadow(paintRect, style(), shadowStyle);
        return;
    }

    // FIXME: We can do better here in the multi-line case. We want to push a clip so that the shadow doesn't
    // protrude incorrectly at the edges, and we want to possibly include shadows cast from the previous/following lines
    auto [hasClosedLeftEdge, hasClosedRightEdge] = m_inlineBox.hasClosedLeftAndRightEdge();
    backgroundPainter.paintBoxShadow(paintRect, style(), shadowStyle, hasClosedLeftEdge, hasClosedRightEdge);
}

const RenderStyle& InlineBoxPainter::style() const
{
    return m_isFirstLineBox ? renderer().firstLineStyle() : renderer().style();
}

}
