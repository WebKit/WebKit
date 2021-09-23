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

#include "GraphicsContext.h"
#include "LegacyInlineFlowBox.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderView.h"

namespace WebCore {

InlineBoxPainter::InlineBoxPainter(const LegacyInlineFlowBox& inlineBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_inlineBox(inlineBox)
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
    , m_renderer(m_inlineBox.renderer())
    , m_isFirstLine(m_inlineBox.isFirstLine())
    , m_isRootInlineBox(is<RenderBlockFlow>(m_renderer))
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
            m_paintInfo.outlineObjects->add(&inlineFlow);

        return;
    }

    if (m_paintInfo.phase == PaintPhase::Mask) {
        paintMask();
        return;
    }

    paintDecorations();
}

static LayoutRect clipRectForNinePieceImageStrip(const LegacyInlineFlowBox* box, const NinePieceImage& image, const LayoutRect& paintRect)
{
    LayoutRect clipRect(paintRect);
    auto& style = box->renderer().style();
    LayoutBoxExtent outsets = style.imageOutsets(image);
    if (box->isHorizontal()) {
        clipRect.setY(paintRect.y() - outsets.top());
        clipRect.setHeight(paintRect.height() + outsets.top() + outsets.bottom());
        if (box->includeLogicalLeftEdge()) {
            clipRect.setX(paintRect.x() - outsets.left());
            clipRect.setWidth(paintRect.width() + outsets.left());
        }
        if (box->includeLogicalRightEdge())
            clipRect.setWidth(clipRect.width() + outsets.right());
    } else {
        clipRect.setX(paintRect.x() - outsets.left());
        clipRect.setWidth(paintRect.width() + outsets.left() + outsets.right());
        if (box->includeLogicalLeftEdge()) {
            clipRect.setY(paintRect.y() - outsets.top());
            clipRect.setHeight(paintRect.height() + outsets.top());
        }
        if (box->includeLogicalRightEdge())
            clipRect.setHeight(clipRect.height() + outsets.bottom());
    }
    return clipRect;
}

void InlineBoxPainter::paintMask()
{
    if (!m_paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != Visibility::Visible || m_paintInfo.phase != PaintPhase::Mask)
        return;

    LayoutRect frameRect(m_inlineBox.frameRect());
    constrainToLineTopAndBottomIfNeeded(frameRect);

    // Move x/y to our coordinates.
    LayoutRect localRect(frameRect);
    m_inlineBox.root().blockFlow().flipForWritingMode(localRect);
    LayoutPoint adjustedPaintOffset = m_paintOffset + localRect.location();

    const NinePieceImage& maskNinePieceImage = renderer().style().maskBoxImage();
    StyleImage* maskBoxImage = renderer().style().maskBoxImage().image();

    // Figure out if we need to push a transparency layer to render our mask.
    bool pushTransparencyLayer = false;
    bool compositedMask = renderer().hasLayer() && renderer().layer()->hasCompositedMask();
    bool flattenCompositingLayers = renderer().view().frameView().paintBehavior().contains(PaintBehavior::FlattenCompositingLayers);
    CompositeOperator compositeOp = CompositeOperator::SourceOver;
    if (!compositedMask || flattenCompositingLayers) {
        if ((maskBoxImage && renderer().style().maskLayers().hasImage()) || renderer().style().maskLayers().next())
            pushTransparencyLayer = true;

        compositeOp = CompositeOperator::DestinationIn;
        if (pushTransparencyLayer) {
            m_paintInfo.context().setCompositeOperation(CompositeOperator::DestinationIn);
            m_paintInfo.context().beginTransparencyLayer(1.0f);
            compositeOp = CompositeOperator::SourceOver;
        }
    }

    LayoutRect paintRect = LayoutRect(adjustedPaintOffset, frameRect.size());

    paintFillLayers(Color(), renderer().style().maskLayers(), paintRect, compositeOp);

    bool hasBoxImage = maskBoxImage && maskBoxImage->canRender(&renderer(), renderer().style().effectiveZoom());
    if (!hasBoxImage || !maskBoxImage->isLoaded()) {
        if (pushTransparencyLayer)
            m_paintInfo.context().endTransparencyLayer();
        return; // Don't paint anything while we wait for the image to load.
    }

    bool hasSingleLine = !m_inlineBox.prevLineBox() && !m_inlineBox.nextLineBox();
    if (hasSingleLine)
        renderer().paintNinePieceImage(m_paintInfo.context(), LayoutRect(adjustedPaintOffset, frameRect.size()), renderer().style(), maskNinePieceImage, compositeOp);
    else {
        // We have a mask image that spans multiple lines.
        // We need to adjust _tx and _ty by the width of all previous lines.
        LayoutUnit logicalOffsetOnLine;
        for (auto* curr = m_inlineBox.prevLineBox(); curr; curr = curr->prevLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        LayoutUnit totalLogicalWidth = logicalOffsetOnLine;
        for (auto* curr = &m_inlineBox; curr; curr = curr->nextLineBox())
            totalLogicalWidth += curr->logicalWidth();
        LayoutUnit stripX = adjustedPaintOffset.x() - (isHorizontal() ? logicalOffsetOnLine : 0_lu);
        LayoutUnit stripY = adjustedPaintOffset.y() - (isHorizontal() ? 0_lu : logicalOffsetOnLine);
        LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : frameRect.width();
        LayoutUnit stripHeight = isHorizontal() ? frameRect.height() : totalLogicalWidth;

        LayoutRect clipRect = clipRectForNinePieceImageStrip(&m_inlineBox, maskNinePieceImage, paintRect);
        GraphicsContextStateSaver stateSaver(m_paintInfo.context());
        m_paintInfo.context().clip(clipRect);
        renderer().paintNinePieceImage(m_paintInfo.context(), LayoutRect(stripX, stripY, stripWidth, stripHeight), renderer().style(), maskNinePieceImage, compositeOp);
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
    if (m_isRootInlineBox && (!m_isFirstLine || &style == &renderer().style()))
        return;

    LayoutRect frameRect(m_inlineBox.frameRect());
    constrainToLineTopAndBottomIfNeeded(frameRect);

    // Move x/y to our coordinates.
    LayoutRect localRect(frameRect);
    m_inlineBox.root().blockFlow().flipForWritingMode(localRect);

    LayoutPoint adjustedPaintoffset = m_paintOffset + localRect.location();
    GraphicsContext& context = m_paintInfo.context();
    LayoutRect paintRect = LayoutRect(adjustedPaintoffset, frameRect.size());
    // Shadow comes first and is behind the background and border.
    if (!renderer().boxShadowShouldBeAppliedToBackground(adjustedPaintoffset, BackgroundBleedNone, &m_inlineBox))
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
    if (hasBorderImage && !borderImageSource->isLoaded())
        return; // Don't paint anything while we wait for the image to load.

    bool hasSingleLine = !m_inlineBox.prevLineBox() && !m_inlineBox.nextLineBox();
    if (!hasBorderImage || hasSingleLine) {
        renderer().paintBorder(m_paintInfo, paintRect, style, BackgroundBleedNone, m_inlineBox.includeLogicalLeftEdge(), m_inlineBox.includeLogicalRightEdge());
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
    for (auto* curr = m_inlineBox.prevLineBox(); curr; curr = curr->prevLineBox())
        logicalOffsetOnLine += curr->logicalWidth();
    LayoutUnit totalLogicalWidth = logicalOffsetOnLine;
    for (auto* curr = &m_inlineBox; curr; curr = curr->nextLineBox())
        totalLogicalWidth += curr->logicalWidth();

    LayoutUnit stripX = adjustedPaintoffset.x() - (isHorizontal() ? logicalOffsetOnLine : 0_lu);
    LayoutUnit stripY = adjustedPaintoffset.y() - (isHorizontal() ? 0_lu : logicalOffsetOnLine);
    LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : frameRect.width();
    LayoutUnit stripHeight = isHorizontal() ? frameRect.height() : totalLogicalWidth;

    LayoutRect clipRect = clipRectForNinePieceImageStrip(&m_inlineBox, borderImage, paintRect);
    GraphicsContextStateSaver stateSaver(context);
    context.clip(clipRect);
    renderer().paintBorder(m_paintInfo, LayoutRect(stripX, stripY, stripWidth, stripHeight), style);
}

void InlineBoxPainter::paintFillLayers(const Color& color, const FillLayer& fillLayer, const LayoutRect& rect, CompositeOperator op)
{
    Vector<const FillLayer*, 8> layers;
    for (auto* layer = &fillLayer; layer; layer = layer->next())
        layers.append(layer);
    layers.reverse();
    for (auto* layer : layers)
        paintFillLayer(color, *layer, rect, op);
}

void InlineBoxPainter::paintFillLayer(const Color& color, const FillLayer& fillLayer, const LayoutRect& rect, CompositeOperator op)
{
    auto* image = fillLayer.image();
    bool hasFillImage = image && image->canRender(&renderer(), renderer().style().effectiveZoom());
    bool hasFillImageOrBorderRadious = hasFillImage || renderer().style().hasBorderRadius();
    bool hasSingleLine = !m_inlineBox.prevLineBox() && !m_inlineBox.nextLineBox();

    if (!hasFillImageOrBorderRadious || hasSingleLine || m_isRootInlineBox) {
        renderer().paintFillLayerExtended(m_paintInfo, color, fillLayer, rect, BackgroundBleedNone, &m_inlineBox, rect.size(), op);
        return;
    }

#if ENABLE(CSS_BOX_DECORATION_BREAK)
    if (renderer().style().boxDecorationBreak() == BoxDecorationBreak::Clone) {
        GraphicsContextStateSaver stateSaver(m_paintInfo.context());
        m_paintInfo.context().clip({ rect.x(), rect.y(), LayoutUnit(m_inlineBox.width()), LayoutUnit(m_inlineBox.height()) });
        renderer().paintFillLayerExtended(m_paintInfo, color, fillLayer, rect, BackgroundBleedNone, &m_inlineBox, rect.size(), op);
        return;
    }
#endif

    // We have a fill image that spans multiple lines.
    // We need to adjust tx and ty by the width of all previous lines.
    // Think of background painting on inlines as though you had one long line, a single continuous
    // strip. Even though that strip has been broken up across multiple lines, you still paint it
    // as though you had one single line. This means each line has to pick up the background where
    // the previous line left off.
    LayoutUnit logicalOffsetOnLine;
    LayoutUnit totalLogicalWidth;
    if (renderer().style().direction() == TextDirection::LTR) {
        for (auto* curr = m_inlineBox.prevLineBox(); curr; curr = curr->prevLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        totalLogicalWidth = logicalOffsetOnLine;
        for (auto* curr = &m_inlineBox; curr; curr = curr->nextLineBox())
            totalLogicalWidth += curr->logicalWidth();
    } else {
        for (auto* curr = m_inlineBox.nextLineBox(); curr; curr = curr->nextLineBox())
            logicalOffsetOnLine += curr->logicalWidth();
        totalLogicalWidth = logicalOffsetOnLine;
        for (auto* curr = &m_inlineBox; curr; curr = curr->prevLineBox())
            totalLogicalWidth += curr->logicalWidth();
    }
    LayoutUnit stripX = rect.x() - (isHorizontal() ? logicalOffsetOnLine : 0_lu);
    LayoutUnit stripY = rect.y() - (isHorizontal() ? 0_lu : logicalOffsetOnLine);
    LayoutUnit stripWidth = isHorizontal() ? totalLogicalWidth : LayoutUnit(m_inlineBox.width());
    LayoutUnit stripHeight = isHorizontal() ? LayoutUnit(m_inlineBox.height()) : totalLogicalWidth;

    GraphicsContextStateSaver stateSaver(m_paintInfo.context());
    m_paintInfo.context().clip({ rect.x(), rect.y(), LayoutUnit(m_inlineBox.width()), LayoutUnit(m_inlineBox.height()) });
    renderer().paintFillLayerExtended(m_paintInfo, color, fillLayer, LayoutRect(stripX, stripY, stripWidth, stripHeight), BackgroundBleedNone, &m_inlineBox, rect.size(), op);
}

void InlineBoxPainter::paintBoxShadow(ShadowStyle shadowStyle, const LayoutRect& paintRect)
{
    bool hasSingleLine = !m_inlineBox.prevLineBox() && !m_inlineBox.nextLineBox();
    if (hasSingleLine || m_isRootInlineBox) {
        renderer().paintBoxShadow(m_paintInfo, paintRect, style(), shadowStyle);
        return;
    }

    // FIXME: We can do better here in the multi-line case. We want to push a clip so that the shadow doesn't
    // protrude incorrectly at the edges, and we want to possibly include shadows cast from the previous/following lines
    renderer().paintBoxShadow(m_paintInfo, paintRect, style(), shadowStyle, m_inlineBox.includeLogicalLeftEdge(), m_inlineBox.includeLogicalRightEdge());
}

void InlineBoxPainter::constrainToLineTopAndBottomIfNeeded(LayoutRect& rect) const
{
    if (renderer().document().inNoQuirksMode())
        return;
    if (m_inlineBox.hasTextChildren())
        return;
    if (m_inlineBox.descendantsHaveSameLineHeightAndBaseline() && m_inlineBox.hasTextDescendants())
        return;

    LayoutUnit logicalTop = isHorizontal() ? rect.y() : rect.x();
    LayoutUnit logicalHeight = isHorizontal() ? rect.height() : rect.width();

    LayoutUnit bottom = std::min(m_inlineBox.root().lineBottom(), logicalTop + logicalHeight);
    logicalTop = std::max(m_inlineBox.root().lineTop(), logicalTop);
    logicalHeight = bottom - logicalTop;
    if (isHorizontal()) {
        rect.setY(logicalTop);
        rect.setHeight(logicalHeight);
    } else {
        rect.setX(logicalTop);
        rect.setWidth(logicalHeight);
    }
}

const RenderStyle& InlineBoxPainter::style() const
{
    return m_isFirstLine ? renderer().firstLineStyle() : renderer().style();
}

}
