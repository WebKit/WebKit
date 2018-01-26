/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "InlineTextBox.h"

#include "BreakLines.h"
#include "DashArray.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "EllipsisBox.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "InlineTextBoxStyle.h"
#include "MarkerSubrange.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBlock.h"
#include "RenderCombineText.h"
#include "RenderLineBreak.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "Text.h"
#include "TextDecorationPainter.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"
#include <stdio.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineTextBox);

struct SameSizeAsInlineTextBox : public InlineBox {
    unsigned variables[1];
    unsigned short variables2[2];
    void* pointers[2];
};

COMPILE_ASSERT(sizeof(InlineTextBox) == sizeof(SameSizeAsInlineTextBox), InlineTextBox_should_stay_small);

typedef WTF::HashMap<const InlineTextBox*, LayoutRect> InlineTextBoxOverflowMap;
static InlineTextBoxOverflowMap* gTextBoxesWithOverflow;

InlineTextBox::~InlineTextBox()
{
    if (!knownToHaveNoOverflow() && gTextBoxesWithOverflow)
        gTextBoxesWithOverflow->remove(this);
}

void InlineTextBox::markDirty(bool dirty)
{
    if (dirty) {
        m_len = 0;
        m_start = 0;
    }
    InlineBox::markDirty(dirty);
}

LayoutRect InlineTextBox::logicalOverflowRect() const
{
    if (knownToHaveNoOverflow() || !gTextBoxesWithOverflow)
        return enclosingIntRect(logicalFrameRect());
    return gTextBoxesWithOverflow->get(this);
}

void InlineTextBox::setLogicalOverflowRect(const LayoutRect& rect)
{
    ASSERT(!knownToHaveNoOverflow());
    if (!gTextBoxesWithOverflow)
        gTextBoxesWithOverflow = new InlineTextBoxOverflowMap;
    gTextBoxesWithOverflow->add(this, rect);
}

int InlineTextBox::baselinePosition(FontBaseline baselineType) const
{
    if (!parent())
        return 0;
    if (&parent()->renderer() == renderer().parent())
        return parent()->baselinePosition(baselineType);
    return downcast<RenderBoxModelObject>(*renderer().parent()).baselinePosition(baselineType, isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineTextBox::lineHeight() const
{
    if (!renderer().parent())
        return 0;
    if (&parent()->renderer() == renderer().parent())
        return parent()->lineHeight();
    return downcast<RenderBoxModelObject>(*renderer().parent()).lineHeight(isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineTextBox::selectionTop() const
{
    return root().selectionTop();
}

LayoutUnit InlineTextBox::selectionBottom() const
{
    return root().selectionBottom();
}

LayoutUnit InlineTextBox::selectionHeight() const
{
    return root().selectionHeight();
}

bool InlineTextBox::isSelected(unsigned startPosition, unsigned endPosition) const
{
    return clampedOffset(startPosition) < clampedOffset(endPosition);
}

RenderObject::SelectionState InlineTextBox::selectionState()
{
    RenderObject::SelectionState state = renderer().selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd || state == RenderObject::SelectionBoth) {
        auto& selection = renderer().view().selection();
        auto startPos = selection.startPosition();
        auto endPos = selection.endPosition();
        // The position after a hard line break is considered to be past its end.
        ASSERT(start() + len() >= (isLineBreak() ? 1 : 0));
        unsigned lastSelectable = start() + len() - (isLineBreak() ? 1 : 0);

        bool start = (state != RenderObject::SelectionEnd && startPos >= m_start && startPos < m_start + m_len);
        bool end = (state != RenderObject::SelectionStart && endPos > m_start && endPos <= lastSelectable);
        if (start && end)
            state = RenderObject::SelectionBoth;
        else if (start)
            state = RenderObject::SelectionStart;
        else if (end)
            state = RenderObject::SelectionEnd;
        else if ((state == RenderObject::SelectionEnd || startPos < m_start) &&
                 (state == RenderObject::SelectionStart || endPos > lastSelectable))
            state = RenderObject::SelectionInside;
        else if (state == RenderObject::SelectionBoth)
            state = RenderObject::SelectionNone;
    }

    // If there are ellipsis following, make sure their selection is updated.
    if (m_truncation != cNoTruncation && root().ellipsisBox()) {
        EllipsisBox* ellipsis = root().ellipsisBox();
        if (state != RenderObject::SelectionNone) {
            unsigned selectionStart;
            unsigned selectionEnd;
            std::tie(selectionStart, selectionEnd) = selectionStartEnd();
            // The ellipsis should be considered to be selected if the end of
            // the selection is past the beginning of the truncation and the
            // beginning of the selection is before or at the beginning of the
            // truncation.
            ellipsis->setSelectionState(selectionEnd >= m_truncation && selectionStart <= m_truncation ?
                RenderObject::SelectionInside : RenderObject::SelectionNone);
        } else
            ellipsis->setSelectionState(RenderObject::SelectionNone);
    }

    return state;
}

inline const FontCascade& InlineTextBox::lineFont() const
{
    return combinedText() ? combinedText()->textCombineFont() : lineStyle().fontCascade();
}

// FIXME: Share more code with paintTextSubrangeBackground().
LayoutRect InlineTextBox::localSelectionRect(unsigned startPos, unsigned endPos) const
{
    unsigned sPos = clampedOffset(startPos);
    unsigned ePos = clampedOffset(endPos);

    if (sPos >= ePos && !(startPos == endPos && startPos >= start() && startPos <= (start() + len())))
        return { };

    LayoutUnit selectionTop = this->selectionTop();
    LayoutUnit selectionHeight = this->selectionHeight();

    auto text = this->text();
    TextRun textRun = createTextRun(text);

    LayoutRect selectionRect = LayoutRect(LayoutPoint(logicalLeft(), selectionTop), LayoutSize(m_logicalWidth, selectionHeight));
    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (sPos || ePos != textRun.length())
        lineFont().adjustSelectionRectForText(textRun, selectionRect, sPos, ePos);
    // FIXME: The computation of the snapped selection rect differs from the computation of this rect
    // in paintTextSubrangeBackground(). See <https://bugs.webkit.org/show_bug.cgi?id=138913>.
    IntRect snappedSelectionRect = enclosingIntRect(selectionRect);
    LayoutUnit logicalWidth = snappedSelectionRect.width();
    if (snappedSelectionRect.x() > logicalRight())
        logicalWidth  = 0;
    else if (snappedSelectionRect.maxX() > logicalRight())
        logicalWidth = logicalRight() - snappedSelectionRect.x();

    LayoutPoint topPoint = isHorizontal() ? LayoutPoint(snappedSelectionRect.x(), selectionTop) : LayoutPoint(selectionTop, snappedSelectionRect.x());
    LayoutUnit width = isHorizontal() ? logicalWidth : selectionHeight;
    LayoutUnit height = isHorizontal() ? selectionHeight : logicalWidth;

    return LayoutRect(topPoint, LayoutSize(width, height));
}

void InlineTextBox::deleteLine()
{
    renderer().removeTextBox(*this);
    delete this;
}

void InlineTextBox::extractLine()
{
    if (extracted())
        return;

    renderer().extractTextBox(*this);
}

void InlineTextBox::attachLine()
{
    if (!extracted())
        return;
    
    renderer().attachTextBox(*this);
}

float InlineTextBox::placeEllipsisBox(bool flowIsLTR, float visibleLeftEdge, float visibleRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox)
{
    if (foundBox) {
        m_truncation = cFullTruncation;
        return -1;
    }

    // For LTR this is the left edge of the box, for RTL, the right edge in parent coordinates.
    float ellipsisX = flowIsLTR ? visibleRightEdge - ellipsisWidth : visibleLeftEdge + ellipsisWidth;
    
    // Criteria for full truncation:
    // LTR: the left edge of the ellipsis is to the left of our text run.
    // RTL: the right edge of the ellipsis is to the right of our text run.
    bool ltrFullTruncation = flowIsLTR && ellipsisX <= left();
    bool rtlFullTruncation = !flowIsLTR && ellipsisX >= left() + logicalWidth();
    if (ltrFullTruncation || rtlFullTruncation) {
        // Too far.  Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
        m_truncation = cFullTruncation;
        foundBox = true;
        return -1;
    }

    bool ltrEllipsisWithinBox = flowIsLTR && (ellipsisX < right());
    bool rtlEllipsisWithinBox = !flowIsLTR && (ellipsisX > left());
    if (ltrEllipsisWithinBox || rtlEllipsisWithinBox) {
        foundBox = true;

        // The inline box may have different directionality than it's parent.  Since truncation
        // behavior depends both on both the parent and the inline block's directionality, we
        // must keep track of these separately.
        bool ltr = isLeftToRightDirection();
        if (ltr != flowIsLTR) {
          // Width in pixels of the visible portion of the box, excluding the ellipsis.
          int visibleBoxWidth = visibleRightEdge - visibleLeftEdge  - ellipsisWidth;
          ellipsisX = ltr ? left() + visibleBoxWidth : right() - visibleBoxWidth;
        }

        int offset = offsetForPosition(ellipsisX, false);
        if (offset == 0) {
            // No characters should be rendered.  Set ourselves to full truncation and place the ellipsis at the min of our start
            // and the ellipsis edge.
            m_truncation = cFullTruncation;
            truncatedWidth += ellipsisWidth;
            return flowIsLTR ? std::min(ellipsisX, x()) : std::max(ellipsisX, right() - ellipsisWidth);
        }

        // Set the truncation index on the text run.
        m_truncation = offset;

        // If we got here that means that we were only partially truncated and we need to return the pixel offset at which
        // to place the ellipsis.
        float widthOfVisibleText = renderer().width(m_start, offset, textPos(), isFirstLine());

        // The ellipsis needs to be placed just after the last visible character.
        // Where "after" is defined by the flow directionality, not the inline
        // box directionality.
        // e.g. In the case of an LTR inline box truncated in an RTL flow then we can
        // have a situation such as |Hello| -> |...He|
        truncatedWidth += widthOfVisibleText + ellipsisWidth;
        if (flowIsLTR)
            return left() + widthOfVisibleText;
        else
            return right() - widthOfVisibleText - ellipsisWidth;
    }
    truncatedWidth += logicalWidth();
    return -1;
}



bool InlineTextBox::isLineBreak() const
{
    return renderer().style().preserveNewline() && len() == 1 && renderer().text()[start()] == '\n';
}

bool InlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/,
    HitTestAction /*hitTestAction*/)
{
    if (!visibleToHitTesting())
        return false;

    if (isLineBreak())
        return false;

    if (m_truncation == cFullTruncation)
        return false;

    FloatRect rect(locationIncludingFlipping(), size());
    // Make sure truncated text is ignored while hittesting.
    if (m_truncation != cNoTruncation) {
        LayoutUnit widthOfVisibleText = renderer().width(m_start, m_truncation, textPos(), isFirstLine());

        if (isHorizontal())
            renderer().style().isLeftToRightDirection() ? rect.setWidth(widthOfVisibleText) : rect.shiftXEdgeTo(right() - widthOfVisibleText);
        else
            rect.setHeight(widthOfVisibleText);
    }

    rect.moveBy(accumulatedOffset);

    if (locationInContainer.intersects(rect)) {
        renderer().updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (result.addNodeToListBasedTestResult(renderer().textNode(), request, locationInContainer, rect) == HitTestProgress::Stop)
            return true;
    }
    return false;
}

static inline bool emphasisPositionHasNeitherLeftNorRight(TextEmphasisPosition emphasisPosition)
{
    return !(emphasisPosition & TextEmphasisPositionLeft) && !(emphasisPosition & TextEmphasisPositionRight);
}

bool InlineTextBox::emphasisMarkExistsAndIsAbove(const RenderStyle& style, bool& above) const
{
    // This function returns true if there are text emphasis marks and they are suppressed by ruby text.
    if (style.textEmphasisMark() == TextEmphasisMarkNone)
        return false;

    TextEmphasisPosition emphasisPosition = style.textEmphasisPosition();
    ASSERT(!((emphasisPosition & TextEmphasisPositionOver) && (emphasisPosition & TextEmphasisPositionUnder)));
    ASSERT(!((emphasisPosition & TextEmphasisPositionLeft) && (emphasisPosition & TextEmphasisPositionRight)));
    
    if (emphasisPositionHasNeitherLeftNorRight(emphasisPosition))
        above = emphasisPosition & TextEmphasisPositionOver;
    else if (style.isHorizontalWritingMode())
        above = emphasisPosition & TextEmphasisPositionOver;
    else
        above = emphasisPosition & TextEmphasisPositionRight;
    
    if ((style.isHorizontalWritingMode() && (emphasisPosition & TextEmphasisPositionUnder))
        || (!style.isHorizontalWritingMode() && (emphasisPosition & TextEmphasisPositionLeft)))
        return true; // Ruby text is always over, so it cannot suppress emphasis marks under.

    RenderBlock* containingBlock = renderer().containingBlock();
    if (!containingBlock->isRubyBase())
        return true; // This text is not inside a ruby base, so it does not have ruby text over it.

    if (!is<RenderRubyRun>(*containingBlock->parent()))
        return true; // Cannot get the ruby text.

    RenderRubyText* rubyText = downcast<RenderRubyRun>(*containingBlock->parent()).rubyText();

    // The emphasis marks over are suppressed only if there is a ruby text box and it not empty.
    return !rubyText || !rubyText->hasLines();
}

struct InlineTextBox::MarkerSubrangeStyle {
    bool operator==(const MarkerSubrangeStyle& other) const = delete;
    bool operator!=(const MarkerSubrangeStyle& other) const = delete;
    static bool areBackgroundMarkerSubrangeStylesEqual(const MarkerSubrangeStyle& a, const MarkerSubrangeStyle& b)
    {
        return a.backgroundColor == b.backgroundColor;
    }
    static bool areForegroundMarkerSubrangeStylesEqual(const MarkerSubrangeStyle& a, const MarkerSubrangeStyle& b)
    {
        return a.textStyles == b.textStyles && a.textShadow == b.textShadow && a.alpha == b.alpha;
    }
    static bool areDecorationMarkerSubrangeStylesEqual(const MarkerSubrangeStyle& a, const MarkerSubrangeStyle& b)
    {
        return a.textDecorationStyles == b.textDecorationStyles && a.textShadow == b.textShadow;
    }

    Color backgroundColor;
    TextPaintStyle textStyles;
    TextDecorationPainter::Styles textDecorationStyles;
    const ShadowData* textShadow;
    float alpha;
};

struct InlineTextBox::StyledMarkerSubrange : MarkerSubrange {
    StyledMarkerSubrange(const MarkerSubrange& marker)
        : MarkerSubrange { marker }
    {
    }

    MarkerSubrangeStyle style;
};

static MarkerSubrange createMarkerSubrangeFromSelectionInBox(const InlineTextBox& box)
{
    unsigned selectionStart;
    unsigned selectionEnd;
    std::tie(selectionStart, selectionEnd) = box.selectionStartEnd();
    if (selectionStart < selectionEnd)
        return { selectionStart, selectionEnd, MarkerSubrange::Selection };
    return { };
}

void InlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /*lineTop*/, LayoutUnit /*lineBottom*/)
{
    if (isLineBreak() || !paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != VISIBLE
        || m_truncation == cFullTruncation || paintInfo.phase == PaintPhaseOutline || !m_len)
        return;

    ASSERT(paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines);

    LayoutUnit logicalLeftSide = logicalLeftVisualOverflow();
    LayoutUnit logicalRightSide = logicalRightVisualOverflow();
    LayoutUnit logicalStart = logicalLeftSide + (isHorizontal() ? paintOffset.x() : paintOffset.y());
    LayoutUnit logicalExtent = logicalRightSide - logicalLeftSide;
    
    LayoutUnit paintEnd = isHorizontal() ? paintInfo.rect.maxX() : paintInfo.rect.maxY();
    LayoutUnit paintStart = isHorizontal() ? paintInfo.rect.x() : paintInfo.rect.y();
    
    FloatPoint localPaintOffset(paintOffset);
    
    if (logicalStart >= paintEnd || logicalStart + logicalExtent <= paintStart)
        return;

    bool isPrinting = renderer().document().printing();
    
    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && paintInfo.phase != PaintPhaseTextClip && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && paintInfo.phase == PaintPhaseSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    if (m_truncation != cNoTruncation) {
        if (renderer().containingBlock()->style().isLeftToRightDirection() != isLeftToRightDirection()) {
            // Make the visible fragment of text hug the edge closest to the rest of the run by moving the origin
            // at which we start drawing text.
            // e.g. In the case of LTR text truncated in an RTL Context, the correct behavior is:
            // |Hello|CBA| -> |...He|CBA|
            // In order to draw the fragment "He" aligned to the right edge of it's box, we need to start drawing
            // farther to the right.
            // NOTE: WebKit's behavior differs from that of IE which appears to just overlay the ellipsis on top of the
            // truncated string i.e.  |Hello|CBA| -> |...lo|CBA|
            LayoutUnit widthOfVisibleText = renderer().width(m_start, m_truncation, textPos(), isFirstLine());
            LayoutUnit widthOfHiddenText = m_logicalWidth - widthOfVisibleText;
            LayoutSize truncationOffset(isLeftToRightDirection() ? widthOfHiddenText : -widthOfHiddenText, 0);
            localPaintOffset.move(isHorizontal() ? truncationOffset : truncationOffset.transposedSize());
        }
    }

    GraphicsContext& context = paintInfo.context();

    const RenderStyle& lineStyle = this->lineStyle();
    
    localPaintOffset.move(0, lineStyle.isHorizontalWritingMode() ? 0 : -logicalHeight());

    FloatPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.moveBy(localPaintOffset);
    FloatRect boxRect(boxOrigin, FloatSize(logicalWidth(), logicalHeight()));

    auto* combinedText = this->combinedText();

    bool shouldRotate = !isHorizontal() && !combinedText;
    if (shouldRotate)
        context.concatCTM(rotation(boxRect, Clockwise));

    // Determine whether or not we have composition underlines to draw.
    bool containsComposition = renderer().textNode() && renderer().frame().editor().compositionNode() == renderer().textNode();
    bool useCustomUnderlines = containsComposition && renderer().frame().editor().compositionUsesCustomUnderlines();

    MarkerSubrangeStyle unmarkedStyle = computeStyleForUnmarkedMarkerSubrange(paintInfo);

    // 1. Paint backgrounds behind text if needed. Examples of such backgrounds include selection
    // and composition underlines.
    if (paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseTextClip && !isPrinting) {
        if (containsComposition && !useCustomUnderlines)
            paintCompositionBackground(context, boxOrigin);

        Vector<MarkerSubrange> subranges = collectSubrangesForDocumentMarkers(TextPaintPhase::Background);
#if ENABLE(TEXT_SELECTION)
        if (haveSelection && !useCustomUnderlines && !context.paintingDisabled()) {
            auto selectionSubrange = createMarkerSubrangeFromSelectionInBox(*this);
            if (!selectionSubrange.isEmpty())
                subranges.append(WTFMove(selectionSubrange));
        }
#endif
        auto styledSubranges = subdivideAndResolveStyle(subranges, unmarkedStyle, paintInfo);

        // Coalesce styles of adjacent subranges to minimize the number of drawing commands.
        auto coalescedStyledSubranges = coalesceAdjacentSubranges(styledSubranges, &MarkerSubrangeStyle::areBackgroundMarkerSubrangeStylesEqual);

        paintMarkerSubranges(context, TextPaintPhase::Background, boxRect, coalescedStyledSubranges);
    }

    // FIXME: Right now, InlineTextBoxes never call addRelevantUnpaintedObject() even though they might
    // legitimately be unpainted if they are waiting on a slow-loading web font. We should fix that, and
    // when we do, we will have to account for the fact the InlineTextBoxes do not always have unique
    // renderers and Page currently relies on each unpainted object having a unique renderer.
    if (paintInfo.phase == PaintPhaseForeground)
        renderer().page().addRelevantRepaintedObject(&renderer(), IntRect(boxOrigin.x(), boxOrigin.y(), logicalWidth(), logicalHeight()));

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    bool shouldPaintSelectionForeground = haveSelection && !useCustomUnderlines;
    Vector<MarkerSubrange> subranges;
    if (paintInfo.phase != PaintPhaseSelection) {
        // The subranges for the gaps between document markers and selection are implicitly created by subdividing the entire line.
        subranges.append({ clampedOffset(m_start), clampedOffset(end() + 1), MarkerSubrange::Unmarked });
        if (!isPrinting) {
            subranges.appendVector(collectSubrangesForDocumentMarkers(TextPaintPhase::Foreground));

            bool shouldPaintDraggedContent = !(paintInfo.paintBehavior & PaintBehaviorExcludeSelection);
            if (shouldPaintDraggedContent) {
                auto subrangesForDraggedContent = collectSubrangesForDraggedContent();
                if (!subrangesForDraggedContent.isEmpty()) {
                    shouldPaintSelectionForeground = false;
                    subranges.appendVector(subrangesForDraggedContent);
                }
            }
        }
    }
    // The selection subrange acts as a placeholder when computing the subranges for the gaps...
    if (shouldPaintSelectionForeground) {
        ASSERT(!isPrinting);
        auto selectionSubrange = createMarkerSubrangeFromSelectionInBox(*this);
        if (!selectionSubrange.isEmpty())
            subranges.append(WTFMove(selectionSubrange));
    }

    auto styledSubranges = subdivideAndResolveStyle(subranges, unmarkedStyle, paintInfo);

    // ... now remove the selection subrange if we are excluding selection.
    if (!isPrinting && paintInfo.paintBehavior & PaintBehaviorExcludeSelection)
        styledSubranges.removeAllMatching([] (const StyledMarkerSubrange& subrange) { return subrange.type == MarkerSubrange::Selection; });

    // Coalesce styles of adjacent subranges to minimize the number of drawing commands.
    auto coalescedStyledSubranges = coalesceAdjacentSubranges(styledSubranges, &MarkerSubrangeStyle::areForegroundMarkerSubrangeStylesEqual);

    paintMarkerSubranges(context, TextPaintPhase::Foreground, boxRect, coalescedStyledSubranges);

    // Paint decorations
    TextDecoration textDecorations = lineStyle.textDecorationsInEffect();
    if (textDecorations != TextDecorationNone && paintInfo.phase != PaintPhaseSelection) {
        auto text = this->text();
        TextRun textRun = createTextRun(text);
        unsigned length = textRun.length();
        if (m_truncation != cNoTruncation)
            length = m_truncation;
        unsigned selectionStart = 0;
        unsigned selectionEnd = 0;
        if (haveSelection)
            std::tie(selectionStart, selectionEnd) = selectionStartEnd();

        FloatRect textDecorationSelectionClipOutRect;
        if ((paintInfo.paintBehavior & PaintBehaviorExcludeSelection) && selectionStart < selectionEnd && selectionEnd <= length) {
            textDecorationSelectionClipOutRect = logicalOverflowRect();
            textDecorationSelectionClipOutRect.moveBy(localPaintOffset);
            float logicalWidthBeforeRange;
            float logicalWidthAfterRange;
            float logicalSelectionWidth = lineFont().widthOfTextRange(textRun, selectionStart, selectionEnd, nullptr, &logicalWidthBeforeRange, &logicalWidthAfterRange);
            // FIXME: Do we need to handle vertical bottom to top text?
            if (!isHorizontal()) {
                textDecorationSelectionClipOutRect.move(0, logicalWidthBeforeRange);
                textDecorationSelectionClipOutRect.setHeight(logicalSelectionWidth);
            } else if (direction() == RTL) {
                textDecorationSelectionClipOutRect.move(logicalWidthAfterRange, 0);
                textDecorationSelectionClipOutRect.setWidth(logicalSelectionWidth);
            } else {
                textDecorationSelectionClipOutRect.move(logicalWidthBeforeRange, 0);
                textDecorationSelectionClipOutRect.setWidth(logicalSelectionWidth);
            }
        }

        // Coalesce styles of adjacent subranges to minimize the number of drawing commands.
        auto coalescedStyledSubranges = coalesceAdjacentSubranges(styledSubranges, &MarkerSubrangeStyle::areDecorationMarkerSubrangeStylesEqual);

        paintMarkerSubranges(context, TextPaintPhase::Decoration, boxRect, coalescedStyledSubranges, textDecorationSelectionClipOutRect);
    }

    // 3. Paint fancy decorations, including composition underlines and platform-specific underlines for spelling errors, grammar errors, et cetera.
    if (paintInfo.phase == PaintPhaseForeground) {
        paintPlatformDocumentMarkers(context, boxOrigin);

        if (useCustomUnderlines)
            paintCompositionUnderlines(context, boxOrigin);
    }
    
    if (shouldRotate)
        context.concatCTM(rotation(boxRect, Counterclockwise));
}

unsigned InlineTextBox::clampedOffset(unsigned x) const
{
    unsigned offset = std::max(std::min(x, m_start + m_len), m_start) - m_start;
    if (m_truncation == cFullTruncation)
        return offset;
    if (m_truncation != cNoTruncation)
        offset = std::min<unsigned>(offset, m_truncation);
    else if (offset == m_len) {
        // Fix up the offset if we are combined text or have a hyphen because we manage these embellishments.
        // That is, they are not reflected in renderer().text(). We treat combined text as a single unit.
        // We also treat the last codepoint in this box and the hyphen as a single unit.
        if (auto* combinedText = this->combinedText())
            offset = combinedText->combinedStringForRendering().length();
        else if (hasHyphen())
            offset += lineStyle().hyphenString().length();
    }
    return offset;
}

std::pair<unsigned, unsigned> InlineTextBox::selectionStartEnd() const
{
    auto selectionState = renderer().selectionState();
    if (selectionState == RenderObject::SelectionInside)
        return { 0, clampedOffset(m_start + m_len) };
    
    auto start = renderer().view().selection().startPosition();
    auto end = renderer().view().selection().endPosition();
    if (selectionState == RenderObject::SelectionStart)
        end = renderer().text().length();
    else if (selectionState == RenderObject::SelectionEnd)
        start = 0;
    return { clampedOffset(start), clampedOffset(end) };
}

void InlineTextBox::paintPlatformDocumentMarkers(GraphicsContext& context, const FloatPoint& boxOrigin)
{
    for (auto& subrange : subdivide(collectSubrangesForDocumentMarkers(TextPaintPhase::Foreground), OverlapStrategy::Frontmost))
        paintPlatformDocumentMarker(context, boxOrigin, subrange);
}

void InlineTextBox::paintPlatformDocumentMarker(GraphicsContext& context, const FloatPoint& boxOrigin, const MarkerSubrange& subrange)
{
    // Never print spelling/grammar markers (5327887)
    if (renderer().document().printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    float start = 0; // start of line to draw, relative to tx
    float width = m_logicalWidth; // how much line to draw

    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (subrange.startOffset || subrange.endOffset != clampedOffset(end() + 1)) {
        // Calculate start & width
        int deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
        int selHeight = selectionHeight();
        FloatPoint startPoint(boxOrigin.x(), boxOrigin.y() - deltaY);
        auto text = this->text();
        TextRun run = createTextRun(text);

        LayoutRect selectionRect = LayoutRect(startPoint, FloatSize(0, selHeight));
        lineFont().adjustSelectionRectForText(run, selectionRect, subrange.startOffset, subrange.endOffset);
        IntRect markerRect = enclosingIntRect(selectionRect);
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();
    }

    auto lineStyleForSubrangeType = [] (MarkerSubrange::Type type) {
        switch (type) {
        case MarkerSubrange::SpellingError:
            return GraphicsContext::DocumentMarkerSpellingLineStyle;
        case MarkerSubrange::GrammarError:
            return GraphicsContext::DocumentMarkerGrammarLineStyle;
        case MarkerSubrange::Correction:
            return GraphicsContext::DocumentMarkerAutocorrectionReplacementLineStyle;
        case MarkerSubrange::DictationAlternatives:
            return GraphicsContext::DocumentMarkerDictationAlternativesLineStyle;
#if PLATFORM(IOS)
        case MarkerSubrange::DictationPhraseWithAlternatives:
            // FIXME: Rename TextCheckingDictationPhraseWithAlternativesLineStyle and remove the PLATFORM(IOS)-guard.
            return GraphicsContext::TextCheckingDictationPhraseWithAlternativesLineStyle;
#endif
        default:
            ASSERT_NOT_REACHED();
            return GraphicsContext::DocumentMarkerSpellingLineStyle;
        }
    };

    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = cMisspellingLineThickness;
    int baseline = lineStyle().fontMetrics().ascent();
    int descent = logicalHeight() - baseline;
    int underlineOffset;
    if (descent <= (2 + lineThickness)) {
        // Place the underline at the very bottom of the text in small/medium fonts.
        underlineOffset = logicalHeight() - lineThickness;
    } else {
        // In larger fonts, though, place the underline up near the baseline to prevent a big gap.
        underlineOffset = baseline + 2;
    }
    context.drawLineForDocumentMarker(FloatPoint(boxOrigin.x() + start, boxOrigin.y() + underlineOffset), width, lineStyleForSubrangeType(subrange.type));
}

auto InlineTextBox::computeStyleForUnmarkedMarkerSubrange(const PaintInfo& paintInfo) const -> MarkerSubrangeStyle
{
    auto& lineStyle = this->lineStyle();

    MarkerSubrangeStyle style;
    style.textDecorationStyles = TextDecorationPainter::stylesForRenderer(renderer(), lineStyle.textDecorationsInEffect(), isFirstLine());
    style.textStyles = computeTextPaintStyle(renderer().frame(), lineStyle, paintInfo);
    style.textShadow = paintInfo.forceTextColor() ? nullptr : lineStyle.textShadow();
    style.alpha = 1;
    return style;
}

auto InlineTextBox::resolveStyleForSubrange(const MarkerSubrange& subrange, const MarkerSubrangeStyle& baseStyle, const PaintInfo& paintInfo) -> StyledMarkerSubrange
{
    MarkerSubrangeStyle style = baseStyle;
    switch (subrange.type) {
    case MarkerSubrange::Correction:
    case MarkerSubrange::DictationAlternatives:
#if PLATFORM(IOS)
    // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS)-guard.
    case MarkerSubrange::DictationPhraseWithAlternatives:
#endif
    case MarkerSubrange::GrammarError:
    case MarkerSubrange::SpellingError:
    case MarkerSubrange::Unmarked:
        break;
    case MarkerSubrange::DraggedContent:
        style.alpha = 0.25;
        break;
    case MarkerSubrange::Selection: {
        const ShadowData* selectionShadow = nullptr;
        style.textStyles = computeTextSelectionPaintStyle(style.textStyles, renderer(), lineStyle(), paintInfo, selectionShadow);
        style.textShadow = selectionShadow;

        Color selectionBackgroundColor = renderer().selectionBackgroundColor();
        style.backgroundColor = selectionBackgroundColor;
        if (selectionBackgroundColor.isValid() && selectionBackgroundColor.alpha() && style.textStyles.fillColor == selectionBackgroundColor)
            style.backgroundColor = { 0xff - selectionBackgroundColor.red(), 0xff - selectionBackgroundColor.green(), 0xff - selectionBackgroundColor.blue() };
        break;
    }
    case MarkerSubrange::TextMatch:
        style.backgroundColor = subrange.marker->isActiveMatch() ? renderer().theme().platformActiveTextSearchHighlightColor() : renderer().theme().platformInactiveTextSearchHighlightColor();
        break;
    }
    StyledMarkerSubrange styledSubrange = subrange;
    styledSubrange.style = WTFMove(style);
    return styledSubrange;
}

auto InlineTextBox::subdivideAndResolveStyle(const Vector<MarkerSubrange>& subrangesToSubdivide, const MarkerSubrangeStyle& baseStyle, const PaintInfo& paintInfo) -> Vector<StyledMarkerSubrange>
{
    if (subrangesToSubdivide.isEmpty())
        return { };

    auto subranges = subdivide(subrangesToSubdivide);

    // Compute frontmost overlapping styled subranges.
    Vector<StyledMarkerSubrange> frontmostSubranges;
    frontmostSubranges.reserveInitialCapacity(subranges.size());
    frontmostSubranges.uncheckedAppend(resolveStyleForSubrange(subranges[0], baseStyle, paintInfo));
    for (auto it = subranges.begin() + 1, end = subranges.end(); it != end; ++it) {
        StyledMarkerSubrange& previousStyledSubrange = frontmostSubranges.last();
        if (previousStyledSubrange.startOffset == it->startOffset && previousStyledSubrange.endOffset == it->endOffset) {
            // Subranges completely cover each other.
            previousStyledSubrange = resolveStyleForSubrange(*it, previousStyledSubrange.style, paintInfo);
            continue;
        }
        frontmostSubranges.uncheckedAppend(resolveStyleForSubrange(*it, baseStyle, paintInfo));
    }

    return frontmostSubranges;
}

auto InlineTextBox::coalesceAdjacentSubranges(const Vector<StyledMarkerSubrange>& subrangesToCoalesce, MarkerSubrangeStylesEqualityFunction areMarkerSubrangeStylesEqual) -> Vector<StyledMarkerSubrange>
{
    if (subrangesToCoalesce.isEmpty())
        return { };

    auto areAdjacentSubrangesWithSameStyle = [&] (const StyledMarkerSubrange& a, const StyledMarkerSubrange& b) {
        return a.endOffset == b.startOffset && areMarkerSubrangeStylesEqual(a.style, b.style);
    };

    Vector<StyledMarkerSubrange> styledSubranges;
    styledSubranges.reserveInitialCapacity(subrangesToCoalesce.size());
    styledSubranges.uncheckedAppend(subrangesToCoalesce[0]);
    for (auto it = subrangesToCoalesce.begin() + 1, end = subrangesToCoalesce.end(); it != end; ++it) {
        StyledMarkerSubrange& previousStyledSubrange = styledSubranges.last();
        if (areAdjacentSubrangesWithSameStyle(previousStyledSubrange, *it)) {
            previousStyledSubrange.endOffset = it->endOffset;
            continue;
        }
        styledSubranges.uncheckedAppend(*it);
    }

    return styledSubranges;
}

Vector<MarkerSubrange> InlineTextBox::collectSubrangesForDraggedContent()
{
    using DraggendContentRange = std::pair<unsigned, unsigned>;
    auto draggedContentRanges = renderer().draggedContentRangesBetweenOffsets(m_start, m_start + m_len);
    Vector<MarkerSubrange> result = draggedContentRanges.map([this] (const DraggendContentRange& range) -> MarkerSubrange {
        return { clampedOffset(range.first), clampedOffset(range.second), MarkerSubrange::DraggedContent };
    });
    return result;
}

Vector<MarkerSubrange> InlineTextBox::collectSubrangesForDocumentMarkers(TextPaintPhase phase)
{
    ASSERT(phase == TextPaintPhase::Background || phase == TextPaintPhase::Foreground);
    if (!renderer().textNode())
        return { };

    Vector<RenderedDocumentMarker*> markers = renderer().document().markers().markersFor(renderer().textNode());

    auto markerTypeForSubrangeType = [] (DocumentMarker::MarkerType type) {
        switch (type) {
        case DocumentMarker::Spelling:
            return MarkerSubrange::SpellingError;
        case DocumentMarker::Grammar:
            return MarkerSubrange::GrammarError;
        case DocumentMarker::CorrectionIndicator:
            return MarkerSubrange::Correction;
        case DocumentMarker::TextMatch:
            return MarkerSubrange::TextMatch;
        case DocumentMarker::DictationAlternatives:
            return MarkerSubrange::DictationAlternatives;
#if PLATFORM(IOS)
        case DocumentMarker::DictationPhraseWithAlternatives:
            return MarkerSubrange::DictationPhraseWithAlternatives;
#endif
        default:
            return MarkerSubrange::Unmarked;
        }
    };

    Vector<MarkerSubrange> subranges;
    subranges.reserveInitialCapacity(markers.size());

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for (auto* marker : markers) {
        // Collect either the background markers or the foreground markers, but not both
        switch (marker->type()) {
        case DocumentMarker::Grammar:
        case DocumentMarker::Spelling:
        case DocumentMarker::CorrectionIndicator:
        case DocumentMarker::Replacement:
        case DocumentMarker::DictationAlternatives:
#if PLATFORM(IOS)
        // FIXME: Remove the PLATFORM(IOS)-guard.
        case DocumentMarker::DictationPhraseWithAlternatives:
#endif
            if (phase == TextPaintPhase::Background)
                continue;
            break;
        case DocumentMarker::TextMatch:
            if (!renderer().frame().editor().markedTextMatchesAreHighlighted())
                continue;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
            FALLTHROUGH;
        case DocumentMarker::TelephoneNumber:
#endif
            if (phase == TextPaintPhase::Foreground)
                continue;
            break;
        default:
            continue;
        }

        if (marker->endOffset() <= start()) {
            // Marker is completely before this run. This might be a marker that sits before the
            // first run we draw, or markers that were within runs we skipped due to truncation.
            continue;
        }

        if (marker->startOffset() > end()) {
            // Marker is completely after this run, bail. A later run will paint it.
            break;
        }

        // Marker intersects this run. Collect it.
        switch (marker->type()) {
        case DocumentMarker::Spelling:
        case DocumentMarker::CorrectionIndicator:
        case DocumentMarker::DictationAlternatives:
        case DocumentMarker::Grammar:
#if PLATFORM(IOS)
        // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS)-guard.
        case DocumentMarker::DictationPhraseWithAlternatives:
#endif
        case DocumentMarker::TextMatch:
            subranges.uncheckedAppend({ clampedOffset(marker->startOffset()), clampedOffset(marker->endOffset()), markerTypeForSubrangeType(marker->type()), marker });
            break;
        case DocumentMarker::Replacement:
            break;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        case DocumentMarker::TelephoneNumber:
            break;
#endif
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return subranges;
}

FloatPoint InlineTextBox::textOriginFromBoxRect(const FloatRect& boxRect) const
{
    FloatPoint textOrigin { boxRect.x(), boxRect.y() + lineFont().fontMetrics().ascent() };
    if (auto* combinedText = this->combinedText()) {
        if (auto newOrigin = combinedText->computeTextOrigin(boxRect))
            textOrigin = newOrigin.value();
    }
    if (isHorizontal())
        textOrigin.setY(roundToDevicePixel(LayoutUnit { textOrigin.y() }, renderer().document().deviceScaleFactor()));
    else
        textOrigin.setX(roundToDevicePixel(LayoutUnit { textOrigin.x() }, renderer().document().deviceScaleFactor()));
    return textOrigin;
}

void InlineTextBox::paintMarkerSubranges(GraphicsContext& context, TextPaintPhase phase, const FloatRect& boxRect, const Vector<StyledMarkerSubrange>& subranges, const FloatRect& decorationClipOutRect)
{
    switch (phase) {
    case TextPaintPhase::Background:
        for (auto& subrange : subranges)
            paintTextSubrangeBackground(context, boxRect.location(), subrange.style.backgroundColor, subrange.startOffset, subrange.endOffset);
        return;
    case TextPaintPhase::Foreground:
        for (auto& subrange : subranges)
            paintTextSubrangeForeground(context, boxRect, subrange);
        return;
    case TextPaintPhase::Decoration:
        for (auto& subrange : subranges)
            paintTextSubrangeDecoration(context, boxRect, decorationClipOutRect, subrange);
        return;
    }
}

void InlineTextBox::paintTextSubrangeBackground(GraphicsContext& context, const FloatPoint& boxOrigin, const Color& color, unsigned clampedStartOffset, unsigned clampedEndOffset)
{
    if (clampedStartOffset >= clampedEndOffset)
        return;

    GraphicsContextStateSaver stateSaver { context };
    updateGraphicsContext(context, TextPaintStyle { color }); // Don't draw text at all!

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own highlight.
    auto text = this->text();
    TextRun textRun = createTextRun(text);

    const RootInlineBox& rootBox = root();
    LayoutUnit selectionBottom = rootBox.selectionBottom();
    LayoutUnit selectionTop = rootBox.selectionTopAdjustedForPrecedingBlock();

    // Use same y positioning and height as for selection, so that when the selection and this subrange are on
    // the same word there are no pieces sticking out.
    LayoutUnit deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom - logicalBottom() : logicalTop() - selectionTop;
    LayoutUnit selectionHeight = std::max<LayoutUnit>(0, selectionBottom - selectionTop);

    LayoutRect selectionRect = LayoutRect(boxOrigin.x(), boxOrigin.y() - deltaY, m_logicalWidth, selectionHeight);
    lineFont().adjustSelectionRectForText(textRun, selectionRect, clampedStartOffset, clampedEndOffset);

    // FIXME: Support painting combined text. See <https://bugs.webkit.org/show_bug.cgi?id=180993>.
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()), color);
}

void InlineTextBox::paintTextSubrangeForeground(GraphicsContext& context, const FloatRect& boxRect, const StyledMarkerSubrange& subrange)
{
    if (subrange.startOffset >= subrange.endOffset)
        return;

    const FontCascade& font = lineFont();
    const RenderStyle& lineStyle = this->lineStyle();

    float emphasisMarkOffset = 0;
    bool emphasisMarkAbove;
    bool hasTextEmphasis = emphasisMarkExistsAndIsAbove(lineStyle, emphasisMarkAbove);
    const AtomicString& emphasisMark = hasTextEmphasis ? lineStyle.textEmphasisMarkString() : nullAtom();
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = emphasisMarkAbove ? -font.fontMetrics().ascent() - font.emphasisMarkDescent(emphasisMark) : font.fontMetrics().descent() + font.emphasisMarkAscent(emphasisMark);

    TextPainter textPainter { context };
    textPainter.setFont(font);
    textPainter.setStyle(subrange.style.textStyles);
    textPainter.setIsHorizontal(isHorizontal());
    textPainter.setShadow(subrange.style.textShadow);
    textPainter.setEmphasisMark(emphasisMark, emphasisMarkOffset, combinedText());

    GraphicsContextStateSaver stateSaver { context, false };
    if (subrange.type == MarkerSubrange::DraggedContent) {
        stateSaver.save();
        context.setAlpha(subrange.style.alpha);
    }
    // TextPainter wants the box rectangle and text origin of the entire line box.
    auto text = this->text();
    textPainter.paintRange(createTextRun(text), boxRect, textOriginFromBoxRect(boxRect), subrange.startOffset, subrange.endOffset);
}

void InlineTextBox::paintTextSubrangeDecoration(GraphicsContext& context, const FloatRect& boxRect, const FloatRect& clipOutRect, const StyledMarkerSubrange& subrange)
{
    if (m_truncation == cFullTruncation)
        return;

    updateGraphicsContext(context, subrange.style.textStyles);

    bool isCombinedText = combinedText();
    if (isCombinedText)
        context.concatCTM(rotation(boxRect, Clockwise));

    // 1. Compute text selection
    unsigned startOffset = subrange.startOffset;
    unsigned endOffset = subrange.endOffset;
    if (startOffset >= endOffset)
        return;

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own decoration.
    auto text = this->text();
    TextRun textRun = createTextRun(text);

    // Avoid measuring the text when the entire line box is selected as an optimization.
    FloatRect snappedSelectionRect = boxRect;
    if (startOffset || endOffset != textRun.length()) {
        LayoutRect selectionRect = { boxRect.x(), boxRect.y(), boxRect.width(), boxRect.height() };
        lineFont().adjustSelectionRectForText(textRun, selectionRect, startOffset, endOffset);
        snappedSelectionRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr());
    }

    // 2. Paint
    TextDecorationPainter decorationPainter { context, static_cast<unsigned>(lineStyle().textDecorationsInEffect()), renderer(), isFirstLine(), subrange.style.textDecorationStyles };
    decorationPainter.setInlineTextBox(this);
    decorationPainter.setFont(lineFont());
    decorationPainter.setWidth(snappedSelectionRect.width());
    decorationPainter.setBaseline(lineStyle().fontMetrics().ascent());
    decorationPainter.setIsHorizontal(isHorizontal());
    decorationPainter.addTextShadow(subrange.style.textShadow);

    {
        GraphicsContextStateSaver stateSaver { context, false };
        if (!clipOutRect.isEmpty()) {
            stateSaver.save();
            context.clipOut(clipOutRect);
        }
        decorationPainter.paintTextDecoration(textRun.subRun(startOffset, endOffset - startOffset), textOriginFromBoxRect(snappedSelectionRect), snappedSelectionRect.location());
    }

    if (isCombinedText)
        context.concatCTM(rotation(boxRect, Counterclockwise));
}

void InlineTextBox::paintCompositionBackground(GraphicsContext& context, const FloatPoint& boxOrigin)
{
    paintTextSubrangeBackground(context, boxOrigin, Color::compositionFill, clampedOffset(renderer().frame().editor().compositionStart()), clampedOffset(renderer().frame().editor().compositionEnd()));
}

void InlineTextBox::paintCompositionUnderlines(GraphicsContext& context, const FloatPoint& boxOrigin) const
{
    if (m_truncation == cFullTruncation)
        return;

    for (auto& underline : renderer().frame().editor().customCompositionUnderlines()) {
        if (underline.endOffset <= m_start) {
            // Underline is completely before this run. This might be an underline that sits
            // before the first run we draw, or underlines that were within runs we skipped
            // due to truncation.
            continue;
        }

        if (underline.startOffset > end())
            break; // Underline is completely after this run, bail. A later run will paint it.

        // Underline intersects this run. Paint it.
        paintCompositionUnderline(context, boxOrigin, underline);

        if (underline.endOffset > end() + 1)
            break; // Underline also runs into the next run. Bail now, no more marker advancement.
    }
}

static inline void mirrorRTLSegment(float logicalWidth, TextDirection direction, float& start, float width)
{
    if (direction == LTR)
        return;
    start = logicalWidth - width - start;
}

void InlineTextBox::paintCompositionUnderline(GraphicsContext& context, const FloatPoint& boxOrigin, const CompositionUnderline& underline) const
{
    if (m_truncation == cFullTruncation)
        return;
    
    float start = 0; // start of line to draw, relative to tx
    float width = m_logicalWidth; // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = m_start;
    unsigned paintEnd = end() + 1; // end points at the last char, not past it
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = renderer().width(m_start, paintStart - m_start, textPos(), isFirstLine());
    }
    if (paintEnd != underline.endOffset) {      // end points at the last char, not past it
        paintEnd = std::min(paintEnd, (unsigned)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = std::min(paintEnd, (unsigned)m_start + m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = renderer().width(paintStart, paintEnd - paintStart, textPos() + start, isFirstLine());
        mirrorRTLSegment(m_logicalWidth, direction(), start, width);
    }

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    int baseline = lineStyle().fontMetrics().ascent();
    if (underline.thick && logicalHeight() - baseline >= 2)
        lineThickness = 2;

    // We need to have some space between underlines of subsequent clauses, because some input methods do not use different underline styles for those.
    // We make each line shorter, which has a harmless side effect of shortening the first and last clauses, too.
    start += 1;
    width -= 2;

    context.setStrokeColor(underline.compositionUnderlineColor == CompositionUnderlineColor::TextColor ? renderer().style().visitedDependentColor(CSSPropertyWebkitTextFillColor) : underline.color);
    context.setStrokeThickness(lineThickness);
    context.drawLineForText(FloatPoint(boxOrigin.x() + start, boxOrigin.y() + logicalHeight() - lineThickness), width, renderer().document().printing());
}

int InlineTextBox::caretMinOffset() const
{
    return m_start;
}

int InlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

float InlineTextBox::textPos() const
{
    // When computing the width of a text run, RenderBlock::computeInlineDirectionPositionsForLine() doesn't include the actual offset
    // from the containing block edge in its measurement. textPos() should be consistent so the text are rendered in the same width.
    if (logicalLeft() == 0)
        return 0;
    return logicalLeft() - root().logicalLeft();
}

int InlineTextBox::offsetForPosition(float lineOffset, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;
    if (lineOffset - logicalLeft() > logicalWidth())
        return isLeftToRightDirection() ? len() : 0;
    if (lineOffset - logicalLeft() < 0)
        return isLeftToRightDirection() ? 0 : len();
    bool ignoreCombinedText = true;
    bool ignoreHyphen = true;
    auto text = this->text(ignoreCombinedText, ignoreHyphen);
    return lineFont().offsetForPosition(createTextRun(text), lineOffset - logicalLeft(), includePartialGlyphs);
}

float InlineTextBox::positionForOffset(unsigned offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + len());

    if (isLineBreak())
        return logicalLeft();

    unsigned startOffset;
    unsigned endOffset;
    if (isLeftToRightDirection()) {
        startOffset = 0;
        endOffset = clampedOffset(offset);
    } else {
        startOffset = clampedOffset(offset);
        endOffset = m_len;
    }

    // FIXME: Do we need to add rightBearing here?
    LayoutRect selectionRect = LayoutRect(logicalLeft(), 0, 0, 0);
    bool ignoreCombinedText = true;
    bool ignoreHyphen = true;
    auto text = this->text(ignoreCombinedText, ignoreHyphen);
    TextRun textRun = createTextRun(text);
    lineFont().adjustSelectionRectForText(textRun, selectionRect, startOffset, endOffset);
    return snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()).maxX();
}

TextRun InlineTextBox::createTextRun(String& string) const
{
    const auto& style = lineStyle();
    TextRun textRun { string, textPos(), expansion(), expansionBehavior(), direction(), dirOverride() || style.rtlOrdering() == VisualOrder, !renderer().canUseSimpleFontCodePath() };
    textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    return textRun;
}

String InlineTextBox::text(bool ignoreCombinedText, bool ignoreHyphen) const
{
    if (auto* combinedText = this->combinedText()) {
        if (ignoreCombinedText)
            return renderer().text().substring(m_start, m_len);
        return combinedText->combinedStringForRendering();
    }
    if (hasHyphen()) {
        if (ignoreHyphen)
            return renderer().text().substring(m_start, m_len);
        return makeString(StringView(renderer().text()).substring(m_start, m_len), lineStyle().hyphenString());
    }
    return renderer().text().substring(m_start, m_len);
}

inline const RenderCombineText* InlineTextBox::combinedText() const
{
    return lineStyle().hasTextCombine() && is<RenderCombineText>(renderer()) && downcast<RenderCombineText>(renderer()).isCombined() ? &downcast<RenderCombineText>(renderer()) : nullptr;
}

ExpansionBehavior InlineTextBox::expansionBehavior() const
{
    ExpansionBehavior leadingBehavior;
    if (forceLeadingExpansion())
        leadingBehavior = ForceLeadingExpansion;
    else if (canHaveLeadingExpansion())
        leadingBehavior = AllowLeadingExpansion;
    else
        leadingBehavior = ForbidLeadingExpansion;

    ExpansionBehavior trailingBehavior;
    if (forceTrailingExpansion())
        trailingBehavior = ForceTrailingExpansion;
    else if (expansion() && nextLeafChild() && !nextLeafChild()->isLineBreak())
        trailingBehavior = AllowTrailingExpansion;
    else
        trailingBehavior = ForbidTrailingExpansion;

    return leadingBehavior | trailingBehavior;
}

#if ENABLE(TREE_DEBUGGING)

const char* InlineTextBox::boxName() const
{
    return "InlineTextBox";
}

void InlineTextBox::outputLineBox(TextStream& stream, bool mark, int depth) const
{
    stream << "-------- " << (isDirty() ? "D" : "-") << "-";

    int printedCharacters = 0;
    if (mark) {
        stream << "*";
        ++printedCharacters;
    }
    while (++printedCharacters <= depth * 2)
        stream << " ";

    String value = renderer().text();
    value = value.substring(start(), len());
    value.replaceWithLiteral('\\', "\\\\");
    value.replaceWithLiteral('\n', "\\n");
    stream << boxName() << " " << FloatRect(x(), y(), width(), height()) << " (" << this << ") renderer->(" << &renderer() << ") run(" << start() << ", " << start() + len() << ") \"" << value.utf8().data() << "\"";
    stream.nextLine();
}

#endif

} // namespace WebCore
