/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "RenderReplaced.h"

#include "Frame.h"
#include "GraphicsContext.h"
#include "LayoutRepainter.h"
#include "Page.h"
#include "RenderBlock.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "VisiblePosition.h"

using namespace std;

namespace WebCore {

const int cDefaultWidth = 300;
const int cDefaultHeight = 150;

RenderReplaced::RenderReplaced(Node* node)
    : RenderBox(node)
    , m_intrinsicSize(cDefaultWidth, cDefaultHeight)
    , m_hasIntrinsicSize(false)
{
    setReplaced(true);
}

RenderReplaced::RenderReplaced(Node* node, const IntSize& intrinsicSize)
    : RenderBox(node)
    , m_intrinsicSize(intrinsicSize)
    , m_hasIntrinsicSize(true)
{
    setReplaced(true);
}

RenderReplaced::~RenderReplaced()
{
}

void RenderReplaced::willBeDestroyed()
{
    if (!documentBeingDestroyed() && parent())
        parent()->dirtyLinesFromChangedChild(this);

    RenderBox::willBeDestroyed();
}

void RenderReplaced::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);

    bool hadStyle = (oldStyle != 0);
    float oldZoom = hadStyle ? oldStyle->effectiveZoom() : RenderStyle::initialZoom();
    if (style() && style()->effectiveZoom() != oldZoom)
        intrinsicSizeChanged();
}

void RenderReplaced::layout()
{
    ASSERT(needsLayout());
    
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    
    setHeight(minimumReplacedHeight());

    computeLogicalWidth();
    computeLogicalHeight();

    m_overflow.clear();
    addVisualEffectOverflow();
    updateLayerTransform();
    
    repainter.repaintAfterLayout();
    setNeedsLayout(false);
}
 
void RenderReplaced::intrinsicSizeChanged()
{
    int scaledWidth = static_cast<int>(cDefaultWidth * style()->effectiveZoom());
    int scaledHeight = static_cast<int>(cDefaultHeight * style()->effectiveZoom());
    m_intrinsicSize = IntSize(scaledWidth, scaledHeight);
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderReplaced::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!shouldPaint(paintInfo, paintOffset))
        return;
    
    LayoutPoint adjustedPaintOffset = paintOffset + location();
    
    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, adjustedPaintOffset);
    
    if (paintInfo.phase == PaintPhaseMask) {
        paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    LayoutRect paintRect = LayoutRect(adjustedPaintOffset, size());
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth())
        paintOutline(paintInfo.context, paintRect);
    
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection && !canHaveChildren())
        return;
    
    if (!paintInfo.shouldPaintWithinRoot(this))
        return;
    
    bool drawSelectionTint = selectionState() != SelectionNone && !document()->printing();
    if (paintInfo.phase == PaintPhaseSelection) {
        if (selectionState() == SelectionNone)
            return;
        drawSelectionTint = false;
    }

    if (Frame* frame = this->frame()) {
        if (Page* page = frame->page())
            page->addRelevantRepaintedObject(this, paintInfo.rect);
    }

    bool completelyClippedOut = false;
    if (style()->hasBorderRadius()) {
        LayoutRect borderRect = LayoutRect(adjustedPaintOffset, size());

        if (borderRect.isEmpty())
            completelyClippedOut = true;
        else {
            // Push a clip if we have a border radius, since we want to round the foreground content that gets painted.
            paintInfo.context->save();
            paintInfo.context->addRoundedRectClip(style()->getRoundedBorderFor(paintRect));
        }
    }

    if (!completelyClippedOut) {
        paintReplaced(paintInfo, adjustedPaintOffset);

        if (style()->hasBorderRadius())
            paintInfo.context->restore();
    }
        
    // The selection tint never gets clipped by border-radius rounding, since we want it to run right up to the edges of
    // surrounding content.
    if (drawSelectionTint) {
        LayoutRect selectionPaintingRect = localSelectionRect();
        selectionPaintingRect.moveBy(adjustedPaintOffset);
        paintInfo.context->fillRect(selectionPaintingRect, selectionBackgroundColor(), style()->colorSpace());
    }
}

bool RenderReplaced::shouldPaint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseOutline && paintInfo.phase != PaintPhaseSelfOutline 
            && paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseMask)
        return false;

    if (!paintInfo.shouldPaintWithinRoot(this))
        return false;
        
    // if we're invisible or haven't received a layout yet, then just bail.
    if (style()->visibility() != VISIBLE)
        return false;

    LayoutPoint adjustedPaintOffset = paintOffset + location();

    // Early exit if the element touches the edges.
    LayoutUnit top = adjustedPaintOffset.y() + minYVisualOverflow();
    LayoutUnit bottom = adjustedPaintOffset.y() + maxYVisualOverflow();
    if (isSelected() && m_inlineBoxWrapper) {
        LayoutUnit selTop = paintOffset.y() + m_inlineBoxWrapper->root()->selectionTop();
        LayoutUnit selBottom = paintOffset.y() + selTop + m_inlineBoxWrapper->root()->selectionHeight();
        top = min(selTop, top);
        bottom = max(selBottom, bottom);
    }
    
    LayoutUnit os = 2 * maximalOutlineSize(paintInfo.phase);
    if (adjustedPaintOffset.x() + minXVisualOverflow() >= paintInfo.rect.maxX() + os || adjustedPaintOffset.x() + maxXVisualOverflow() <= paintInfo.rect.x() - os)
        return false;
    if (top >= paintInfo.rect.maxY() + os || bottom <= paintInfo.rect.y() - os)
        return false;

    return true;
}

int RenderReplaced::computeIntrinsicLogicalWidth(RenderBox* contentRenderer, bool includeMaxWidth) const
{
    if (m_hasIntrinsicSize)
        return computeReplacedLogicalWidthRespectingMinMaxWidth(calcAspectRatioLogicalWidth(), includeMaxWidth);
    ASSERT(contentRenderer);
    ASSERT(contentRenderer->style());
    return contentRenderer->computeReplacedLogicalWidthRespectingMinMaxWidth(contentRenderer->computeReplacedLogicalWidthUsing(contentRenderer->style()->logicalWidth()), includeMaxWidth);
}

int RenderReplaced::computeIntrinsicLogicalHeight(RenderBox* contentRenderer) const
{
    if (m_hasIntrinsicSize)
        return computeReplacedLogicalHeightRespectingMinMaxHeight(calcAspectRatioLogicalHeight());
    ASSERT(contentRenderer);
    ASSERT(contentRenderer->style());
    return contentRenderer->computeReplacedLogicalHeightRespectingMinMaxHeight(contentRenderer->computeReplacedLogicalHeightUsing(contentRenderer->style()->logicalHeight()));
}

static inline RenderBlock* firstContainingBlockWithLogicalWidth(const RenderReplaced* replaced)
{
    // We have to lookup the containing block, which has an explicit width, which must not be equal to our direct containing block.
    // If the embedded document appears _after_ we performed the initial layout, our intrinsic size is 300x150. If our containing
    // block doesn't provide an explicit width, it's set to the 300 default, coming from the initial layout run.
    RenderBlock* containingBlock = replaced->containingBlock();
    if (!containingBlock)
        return 0;

    for (; !containingBlock->isRenderView() && !containingBlock->isBody(); containingBlock = containingBlock->containingBlock()) {
        if (containingBlock->style()->logicalWidth().isSpecified())
            return containingBlock;
    }

    return 0;
}

bool RenderReplaced::hasReplacedLogicalWidth() const
{
    if (style()->logicalWidth().isSpecified())
        return true;

    if (style()->logicalWidth().isAuto())
        return false;

    return firstContainingBlockWithLogicalWidth(this);
}

static inline bool hasAutoHeightOrContainingBlockWithAutoHeight(const RenderReplaced* replaced)
{
    Length logicalHeightLength = replaced->style()->logicalHeight();
    if (logicalHeightLength.isAuto())
        return true;

    // For percentage heights: The percentage is calculated with respect to the height of the generated box's
    // containing block. If the height of the containing block is not specified explicitly (i.e., it depends
    // on content height), and this element is not absolutely positioned, the value computes to 'auto'.
    if (!logicalHeightLength.isPercent() || replaced->isPositioned() || replaced->document()->inQuirksMode())
        return false;

    for (RenderBlock* cb = replaced->containingBlock(); !cb->isRenderView(); cb = cb->containingBlock()) {
        if (cb->isTableCell() || (!cb->style()->logicalHeight().isAuto() || (!cb->style()->top().isAuto() && !cb->style()->bottom().isAuto())))
            return false;
    }

    return true;
}

bool RenderReplaced::hasReplacedLogicalHeight() const
{
    if (style()->logicalHeight().isAuto())
        return false;

    if (style()->logicalHeight().isSpecified()) {
        if (hasAutoHeightOrContainingBlockWithAutoHeight(this))
            return false;
        return true;
    }

    return false;
}

LayoutUnit RenderReplaced::computeReplacedLogicalWidth(bool includeMaxWidth) const
{
    if (style()->logicalWidth().isSpecified())
        return computeReplacedLogicalWidthRespectingMinMaxWidth(computeReplacedLogicalWidthUsing(style()->logicalWidth()), includeMaxWidth);

    RenderBox* contentRenderer = embeddedContentBox();

    // 10.3.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    bool isPercentageIntrinsicSize = false;
    double intrinsicRatio = 0;
    FloatSize intrinsicSize;
    if (contentRenderer)
        contentRenderer->computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio, isPercentageIntrinsicSize);
    else
        computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio, isPercentageIntrinsicSize);

    if (intrinsicRatio && !isHorizontalWritingMode())
        intrinsicRatio = 1 / intrinsicRatio;

    if (style()->logicalWidth().isAuto()) {
        bool heightIsAuto = style()->logicalHeight().isAuto();
        bool hasIntrinsicWidth = m_hasIntrinsicSize || (!isPercentageIntrinsicSize && intrinsicSize.width() > 0);

        // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (heightIsAuto && hasIntrinsicWidth) {
            if (m_hasIntrinsicSize)
                return computeReplacedLogicalWidthRespectingMinMaxWidth(calcAspectRatioLogicalWidth(), includeMaxWidth);
            return static_cast<LayoutUnit>(intrinsicSize.width() * style()->effectiveZoom());
        }

        bool hasIntrinsicHeight = m_hasIntrinsicSize || (!isPercentageIntrinsicSize && intrinsicSize.height() > 0);
        if (intrinsicRatio || isPercentageIntrinsicSize) {
            // If 'height' and 'width' both have computed values of 'auto' and the element has no intrinsic width, but does have an intrinsic height and intrinsic ratio;
            // or if 'width' has a computed value of 'auto', 'height' has some other computed value, and the element does have an intrinsic ratio; then the used value
            // of 'width' is: (used height) * (intrinsic ratio)
            if (intrinsicRatio && ((heightIsAuto && !hasIntrinsicWidth && hasIntrinsicHeight) || !heightIsAuto)) {
                LayoutUnit logicalHeight = computeReplacedLogicalHeightUsing(style()->logicalHeight());
                return computeReplacedLogicalWidthRespectingMinMaxWidth(static_cast<LayoutUnit>(ceil(logicalHeight * intrinsicRatio)));
            }

            // If 'height' and 'width' both have computed values of 'auto' and the element has an intrinsic ratio but no intrinsic height or width, then the used value of
            // 'width' is undefined in CSS 2.1. However, it is suggested that, if the containing block's width does not itself depend on the replaced element's width, then
            // the used value of 'width' is calculated from the constraint equation used for block-level, non-replaced elements in normal flow.
            if (heightIsAuto && !hasIntrinsicWidth && !hasIntrinsicHeight && contentRenderer) {
                // The aforementioned 'constraint equation' used for block-level, non-replaced elements in normal flow:
                // 'margin-left' + 'border-left-width' + 'padding-left' + 'width' + 'padding-right' + 'border-right-width' + 'margin-right' = width of containing block
                LayoutUnit logicalWidth;
                if (RenderBlock* blockWithWidth = firstContainingBlockWithLogicalWidth(this))
                    logicalWidth = blockWithWidth->computeReplacedLogicalWidthRespectingMinMaxWidth(blockWithWidth->computeReplacedLogicalWidthUsing(blockWithWidth->style()->logicalWidth()), false);
                else
                    logicalWidth = containingBlock()->availableLogicalWidth();

                // This solves above equation for 'width' (== logicalWidth).
                LayoutUnit marginStart = style()->marginStart().calcMinValue(logicalWidth);
                LayoutUnit marginEnd = style()->marginEnd().calcMinValue(logicalWidth);
                logicalWidth = max(0, logicalWidth - (marginStart + marginEnd + (width() - clientWidth())));
                if (isPercentageIntrinsicSize)
                    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
                    logicalWidth = static_cast<LayoutUnit>(round(logicalWidth * intrinsicSize.width() / 100));
                return computeReplacedLogicalWidthRespectingMinMaxWidth(logicalWidth);
            }
        }

        // Otherwise, if 'width' has a computed value of 'auto', and the element has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (hasIntrinsicWidth) {
            if (isPercentageIntrinsicSize || m_hasIntrinsicSize)
                return computeReplacedLogicalWidthRespectingMinMaxWidth(calcAspectRatioLogicalWidth(), includeMaxWidth);
            return static_cast<LayoutUnit>(intrinsicSize.width() * style()->effectiveZoom());
        }

        // Otherwise, if 'width' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'width' becomes 300px. If 300px is too
        // wide to fit the device, UAs should use the width of the largest rectangle that has a 2:1 ratio and fits the device instead.
        return computeReplacedLogicalWidthRespectingMinMaxWidth(cDefaultWidth, includeMaxWidth);
    }

    return computeReplacedLogicalWidthRespectingMinMaxWidth(intrinsicLogicalWidth(), includeMaxWidth);
}

LayoutUnit RenderReplaced::computeReplacedLogicalHeight() const
{
    // 10.5 Content height: the 'height' property: http://www.w3.org/TR/CSS21/visudet.html#propdef-height
    if (hasReplacedLogicalHeight())
        return computeReplacedLogicalHeightRespectingMinMaxHeight(computeReplacedLogicalHeightUsing(style()->logicalHeight()));

    RenderBox* contentRenderer = embeddedContentBox();

    // 10.6.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
    bool isPercentageIntrinsicSize = false;
    double intrinsicRatio = 0;
    FloatSize intrinsicSize;
    if (contentRenderer)
        contentRenderer->computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio, isPercentageIntrinsicSize);
    else
        computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio, isPercentageIntrinsicSize);

    if (intrinsicRatio && !isHorizontalWritingMode())
        intrinsicRatio = 1 / intrinsicRatio;

    bool widthIsAuto = style()->logicalWidth().isAuto();
    bool hasIntrinsicHeight = m_hasIntrinsicSize || (!isPercentageIntrinsicSize && intrinsicSize.height() > 0);

    // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic height, then that intrinsic height is the used value of 'height'.
    if (widthIsAuto && hasIntrinsicHeight) {
        if (m_hasIntrinsicSize)
            return computeReplacedLogicalHeightRespectingMinMaxHeight(calcAspectRatioLogicalHeight());
        return static_cast<LayoutUnit>(intrinsicSize.height() * style()->effectiveZoom());
    }

    // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic ratio then the used value of 'height' is:
    // (used width) / (intrinsic ratio)
    if (intrinsicRatio && !isPercentageIntrinsicSize) {
        // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
        return computeReplacedLogicalHeightRespectingMinMaxHeight(round(availableLogicalWidth() / intrinsicRatio));
    }

    // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic height, then that intrinsic height is the used value of 'height'.
    if (hasIntrinsicHeight) {
        if (m_hasIntrinsicSize)
            return computeReplacedLogicalHeightRespectingMinMaxHeight(calcAspectRatioLogicalHeight());
        return static_cast<LayoutUnit>(intrinsicSize.height() * style()->effectiveZoom());
    }

    // Otherwise, if 'height' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'height' must be set to the height
    // of the largest rectangle that has a 2:1 ratio, has a height not greater than 150px, and has a width not greater than the device width.
    return computeReplacedLogicalHeightRespectingMinMaxHeight(cDefaultHeight);
}

int RenderReplaced::calcAspectRatioLogicalWidth() const
{
    int intrinsicWidth = intrinsicLogicalWidth();
    int intrinsicHeight = intrinsicLogicalHeight();
    if (!intrinsicHeight)
        return 0;
    return RenderBox::computeReplacedLogicalHeight() * intrinsicWidth / intrinsicHeight;
}

int RenderReplaced::calcAspectRatioLogicalHeight() const
{
    int intrinsicWidth = intrinsicLogicalWidth();
    int intrinsicHeight = intrinsicLogicalHeight();
    if (!intrinsicWidth)
        return 0;
    return RenderBox::computeReplacedLogicalWidth() * intrinsicHeight / intrinsicWidth;
}

void RenderReplaced::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    LayoutUnit borderAndPadding = borderAndPaddingWidth();
    m_maxPreferredLogicalWidth = computeReplacedLogicalWidth(false) + borderAndPadding;

    if (style()->maxWidth().isFixed())
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, style()->maxWidth().value() + (style()->boxSizing() == CONTENT_BOX ? borderAndPadding : 0));

    if (style()->width().isPercent() || style()->height().isPercent()
        || style()->maxWidth().isPercent() || style()->maxHeight().isPercent()
        || style()->minWidth().isPercent() || style()->minHeight().isPercent())
        m_minPreferredLogicalWidth = 0;
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;

    setPreferredLogicalWidthsDirty(false);
}

VisiblePosition RenderReplaced::positionForPoint(const LayoutPoint& point)
{
    // FIXME: This code is buggy if the replaced element is relative positioned.
    InlineBox* box = inlineBoxWrapper();
    RootInlineBox* rootBox = box ? box->root() : 0;
    
    LayoutUnit top = rootBox ? rootBox->selectionTop() : logicalTop();
    LayoutUnit bottom = rootBox ? rootBox->selectionBottom() : logicalBottom();
    
    LayoutUnit blockDirectionPosition = isHorizontalWritingMode() ? point.y() + y() : point.x() + x();
    LayoutUnit lineDirectionPosition = isHorizontalWritingMode() ? point.x() + x() : point.y() + y();
    
    if (blockDirectionPosition < top)
        return createVisiblePosition(caretMinOffset(), DOWNSTREAM); // coordinates are above
    
    if (blockDirectionPosition >= bottom)
        return createVisiblePosition(caretMaxOffset(), DOWNSTREAM); // coordinates are below
    
    if (node()) {
        if (lineDirectionPosition <= logicalLeft() + (logicalWidth() / 2))
            return createVisiblePosition(0, DOWNSTREAM);
        return createVisiblePosition(1, DOWNSTREAM);
    }

    return RenderBox::positionForPoint(point);
}

LayoutRect RenderReplaced::selectionRectForRepaint(RenderBoxModelObject* repaintContainer, bool clipToVisibleContent)
{
    ASSERT(!needsLayout());

    if (!isSelected())
        return LayoutRect();
    
    LayoutRect rect = localSelectionRect();
    if (clipToVisibleContent)
        computeRectForRepaint(repaintContainer, rect);
    else
        rect = localToContainerQuad(FloatRect(rect), repaintContainer).enclosingBoundingBox();
    
    return rect;
}

IntRect RenderReplaced::localSelectionRect(bool checkWhetherSelected) const
{
    if (checkWhetherSelected && !isSelected())
        return IntRect();

    if (!m_inlineBoxWrapper)
        // We're a block-level replaced element.  Just return our own dimensions.
        return IntRect(0, 0, width(), height());
    
    RootInlineBox* root = m_inlineBoxWrapper->root();
    int newLogicalTop = root->block()->style()->isFlippedBlocksWritingMode() ? m_inlineBoxWrapper->logicalBottom() - root->selectionBottom() : root->selectionTop() - m_inlineBoxWrapper->logicalTop();
    if (root->block()->style()->isHorizontalWritingMode())
        return IntRect(0, newLogicalTop, width(), root->selectionHeight());
    return IntRect(newLogicalTop, 0, root->selectionHeight(), height());
}

void RenderReplaced::setSelectionState(SelectionState s)
{
    RenderBox::setSelectionState(s); // The selection state for our containing block hierarchy is updated by the base class call.
    if (m_inlineBoxWrapper) {
        RootInlineBox* line = m_inlineBoxWrapper->root();
        if (line)
            line->setHasSelectedChildren(isSelected());
    }
}

bool RenderReplaced::isSelected() const
{
    SelectionState s = selectionState();
    if (s == SelectionNone)
        return false;
    if (s == SelectionInside)
        return true;

    int selectionStart, selectionEnd;
    selectionStartEnd(selectionStart, selectionEnd);
    if (s == SelectionStart)
        return selectionStart == 0;
        
    int end = node()->hasChildNodes() ? node()->childNodeCount() : 1;
    if (s == SelectionEnd)
        return selectionEnd == end;
    if (s == SelectionBoth)
        return selectionStart == 0 && selectionEnd == end;
        
    ASSERT(0);
    return false;
}

IntSize RenderReplaced::intrinsicSize() const
{
    return m_intrinsicSize;
}

void RenderReplaced::setIntrinsicSize(const IntSize& size)
{
    ASSERT(m_hasIntrinsicSize);
    m_intrinsicSize = size;
}

LayoutRect RenderReplaced::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer) const
{
    if (style()->visibility() != VISIBLE && !enclosingLayer()->hasVisibleContent())
        return LayoutRect();

    // The selectionRect can project outside of the overflowRect, so take their union
    // for repainting to avoid selection painting glitches.
    LayoutRect r = unionRect(localSelectionRect(false), visualOverflowRect());

    RenderView* v = view();
    if (v) {
        // FIXME: layoutDelta needs to be applied in parts before/after transforms and
        // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
        r.move(v->layoutDelta());
    }

    if (style()) {
        if (v)
            r.inflate(style()->outlineSize());
    }
    computeRectForRepaint(repaintContainer, r);
    return r;
}

}
