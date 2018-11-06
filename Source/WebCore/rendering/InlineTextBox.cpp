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
#include "MarkedText.h"
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
    TextPainter::removeGlyphDisplayList(*this);
}

bool InlineTextBox::hasTextContent() const
{
    if (m_len > 1)
        return true;
    if (auto* combinedText = this->combinedText()) {
        ASSERT(m_len == 1);
        return !combinedText->combinedStringForRendering().isEmpty();
    }
    return m_len;
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

// FIXME: Share more code with paintMarkedTextBackground().
LayoutRect InlineTextBox::localSelectionRect(unsigned startPos, unsigned endPos) const
{
    unsigned sPos = clampedOffset(startPos);
    unsigned ePos = clampedOffset(endPos);

    if (sPos >= ePos && !(startPos == endPos && startPos >= start() && startPos <= (start() + len())))
        return { };

    LayoutUnit selectionTop = this->selectionTop();
    LayoutUnit selectionHeight = this->selectionHeight();

    TextRun textRun = createTextRun();

    LayoutRect selectionRect = LayoutRect(LayoutPoint(logicalLeft(), selectionTop), LayoutSize(logicalWidth(), selectionHeight));
    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (sPos || ePos != textRun.length())
        lineFont().adjustSelectionRectForText(textRun, selectionRect, sPos, ePos);
    // FIXME: The computation of the snapped selection rect differs from the computation of this rect
    // in paintMarkedTextBackground(). See <https://bugs.webkit.org/show_bug.cgi?id=138913>.
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

std::optional<bool> InlineTextBox::emphasisMarkExistsAndIsAbove(const RenderStyle& style) const
{
    // This function returns true if there are text emphasis marks and they are suppressed by ruby text.
    if (style.textEmphasisMark() == TextEmphasisMark::None)
        return std::nullopt;

    const OptionSet<TextEmphasisPosition> horizontalMask { TextEmphasisPosition::Left, TextEmphasisPosition::Right };

    auto emphasisPosition = style.textEmphasisPosition();
    auto emphasisPositionHorizontalValue = emphasisPosition & horizontalMask;
    ASSERT(!((emphasisPosition & TextEmphasisPosition::Over) && (emphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(emphasisPositionHorizontalValue != horizontalMask);

    bool isAbove = false;
    if (!emphasisPositionHorizontalValue)
        isAbove = emphasisPosition.contains(TextEmphasisPosition::Over);
    else if (style.isHorizontalWritingMode())
        isAbove = emphasisPosition.contains(TextEmphasisPosition::Over);
    else
        isAbove = emphasisPositionHorizontalValue == TextEmphasisPosition::Right;

    if ((style.isHorizontalWritingMode() && (emphasisPosition & TextEmphasisPosition::Under))
        || (!style.isHorizontalWritingMode() && (emphasisPosition & TextEmphasisPosition::Left)))
        return isAbove; // Ruby text is always over, so it cannot suppress emphasis marks under.

    RenderBlock* containingBlock = renderer().containingBlock();
    if (!containingBlock->isRubyBase())
        return isAbove; // This text is not inside a ruby base, so it does not have ruby text over it.

    if (!is<RenderRubyRun>(*containingBlock->parent()))
        return isAbove; // Cannot get the ruby text.

    RenderRubyText* rubyText = downcast<RenderRubyRun>(*containingBlock->parent()).rubyText();

    // The emphasis marks over are suppressed only if there is a ruby text box and it not empty.
    if (rubyText && rubyText->hasLines())
        return std::nullopt;

    return isAbove;
}

struct InlineTextBox::MarkedTextStyle {
    static bool areBackgroundMarkedTextStylesEqual(const MarkedTextStyle& a, const MarkedTextStyle& b)
    {
        return a.backgroundColor == b.backgroundColor;
    }
    static bool areForegroundMarkedTextStylesEqual(const MarkedTextStyle& a, const MarkedTextStyle& b)
    {
        return a.textStyles == b.textStyles && a.textShadow == b.textShadow && a.alpha == b.alpha;
    }
    static bool areDecorationMarkedTextStylesEqual(const MarkedTextStyle& a, const MarkedTextStyle& b)
    {
        return a.textDecorationStyles == b.textDecorationStyles && a.textShadow == b.textShadow && a.alpha == b.alpha;
    }

    Color backgroundColor;
    TextPaintStyle textStyles;
    TextDecorationPainter::Styles textDecorationStyles;
    std::optional<ShadowData> textShadow;
    float alpha;
};

struct InlineTextBox::StyledMarkedText : MarkedText {
    StyledMarkedText(const MarkedText& marker)
        : MarkedText { marker }
    {
    }

    MarkedTextStyle style;
};

static MarkedText createMarkedTextFromSelectionInBox(const InlineTextBox& box)
{
    unsigned selectionStart;
    unsigned selectionEnd;
    std::tie(selectionStart, selectionEnd) = box.selectionStartEnd();
    if (selectionStart < selectionEnd)
        return { selectionStart, selectionEnd, MarkedText::Selection };
    return { };
}

void InlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /*lineTop*/, LayoutUnit /*lineBottom*/)
{
    if (isLineBreak() || !paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != Visibility::Visible
        || m_truncation == cFullTruncation || paintInfo.phase == PaintPhase::Outline || !hasTextContent())
        return;

    ASSERT(paintInfo.phase != PaintPhase::SelfOutline && paintInfo.phase != PaintPhase::ChildOutlines);

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
    bool haveSelection = !isPrinting && paintInfo.phase != PaintPhase::TextClip && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && paintInfo.phase == PaintPhase::Selection) {
        // When only painting the selection, don't bother to paint if there is none.
        return;
    }

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
            LayoutUnit widthOfHiddenText = logicalWidth() - widthOfVisibleText;
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

    MarkedTextStyle unmarkedStyle = computeStyleForUnmarkedMarkedText(paintInfo);

    // 1. Paint backgrounds behind text if needed. Examples of such backgrounds include selection
    // and composition underlines.
    if (paintInfo.phase != PaintPhase::Selection && paintInfo.phase != PaintPhase::TextClip && !isPrinting) {
        if (containsComposition && !useCustomUnderlines)
            paintCompositionBackground(paintInfo, boxOrigin);

        Vector<MarkedText> markedTexts = collectMarkedTextsForDocumentMarkers(TextPaintPhase::Background);
#if ENABLE(TEXT_SELECTION)
        if (haveSelection && !useCustomUnderlines && !context.paintingDisabled()) {
            auto selectionMarkedText = createMarkedTextFromSelectionInBox(*this);
            if (!selectionMarkedText.isEmpty())
                markedTexts.append(WTFMove(selectionMarkedText));
        }
#endif
        auto styledMarkedTexts = subdivideAndResolveStyle(markedTexts, unmarkedStyle, paintInfo);

        // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
        auto coalescedStyledMarkedTexts = coalesceAdjacentMarkedTexts(styledMarkedTexts, &MarkedTextStyle::areBackgroundMarkedTextStylesEqual);

        paintMarkedTexts(paintInfo, TextPaintPhase::Background, boxRect, coalescedStyledMarkedTexts);
    }

    // FIXME: Right now, InlineTextBoxes never call addRelevantUnpaintedObject() even though they might
    // legitimately be unpainted if they are waiting on a slow-loading web font. We should fix that, and
    // when we do, we will have to account for the fact the InlineTextBoxes do not always have unique
    // renderers and Page currently relies on each unpainted object having a unique renderer.
    if (paintInfo.phase == PaintPhase::Foreground)
        renderer().page().addRelevantRepaintedObject(&renderer(), IntRect(boxOrigin.x(), boxOrigin.y(), logicalWidth(), logicalHeight()));

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    bool shouldPaintSelectionForeground = haveSelection && !useCustomUnderlines;
    Vector<MarkedText> markedTexts;
    if (paintInfo.phase != PaintPhase::Selection) {
        // The marked texts for the gaps between document markers and selection are implicitly created by subdividing the entire line.
        markedTexts.append({ clampedOffset(m_start), clampedOffset(end() + 1), MarkedText::Unmarked });
        if (!isPrinting) {
            markedTexts.appendVector(collectMarkedTextsForDocumentMarkers(TextPaintPhase::Foreground));

            bool shouldPaintDraggedContent = !(paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection));
            if (shouldPaintDraggedContent) {
                auto markedTextsForDraggedContent = collectMarkedTextsForDraggedContent();
                if (!markedTextsForDraggedContent.isEmpty()) {
                    shouldPaintSelectionForeground = false;
                    markedTexts.appendVector(markedTextsForDraggedContent);
                }
            }
        }
    }
    // The selection marked text acts as a placeholder when computing the marked texts for the gaps...
    if (shouldPaintSelectionForeground) {
        ASSERT(!isPrinting);
        auto selectionMarkedText = createMarkedTextFromSelectionInBox(*this);
        if (!selectionMarkedText.isEmpty())
            markedTexts.append(WTFMove(selectionMarkedText));
    }

    auto styledMarkedTexts = subdivideAndResolveStyle(markedTexts, unmarkedStyle, paintInfo);

    // ... now remove the selection marked text if we are excluding selection.
    if (!isPrinting && paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection))
        styledMarkedTexts.removeAllMatching([] (const StyledMarkedText& markedText) { return markedText.type == MarkedText::Selection; });

    // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
    auto coalescedStyledMarkedTexts = coalesceAdjacentMarkedTexts(styledMarkedTexts, &MarkedTextStyle::areForegroundMarkedTextStylesEqual);

    paintMarkedTexts(paintInfo, TextPaintPhase::Foreground, boxRect, coalescedStyledMarkedTexts);

    // Paint decorations
    auto textDecorations = lineStyle.textDecorationsInEffect();
    if (!textDecorations.isEmpty() && paintInfo.phase != PaintPhase::Selection) {
        TextRun textRun = createTextRun();
        unsigned length = textRun.length();
        if (m_truncation != cNoTruncation)
            length = m_truncation;
        unsigned selectionStart = 0;
        unsigned selectionEnd = 0;
        if (haveSelection)
            std::tie(selectionStart, selectionEnd) = selectionStartEnd();

        FloatRect textDecorationSelectionClipOutRect;
        if ((paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection)) && selectionStart < selectionEnd && selectionEnd <= length) {
            textDecorationSelectionClipOutRect = logicalOverflowRect();
            textDecorationSelectionClipOutRect.moveBy(localPaintOffset);
            float logicalWidthBeforeRange;
            float logicalWidthAfterRange;
            float logicalSelectionWidth = lineFont().widthOfTextRange(textRun, selectionStart, selectionEnd, nullptr, &logicalWidthBeforeRange, &logicalWidthAfterRange);
            // FIXME: Do we need to handle vertical bottom to top text?
            if (!isHorizontal()) {
                textDecorationSelectionClipOutRect.move(0, logicalWidthBeforeRange);
                textDecorationSelectionClipOutRect.setHeight(logicalSelectionWidth);
            } else if (direction() == TextDirection::RTL) {
                textDecorationSelectionClipOutRect.move(logicalWidthAfterRange, 0);
                textDecorationSelectionClipOutRect.setWidth(logicalSelectionWidth);
            } else {
                textDecorationSelectionClipOutRect.move(logicalWidthBeforeRange, 0);
                textDecorationSelectionClipOutRect.setWidth(logicalSelectionWidth);
            }
        }

        // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
        auto coalescedStyledMarkedTexts = coalesceAdjacentMarkedTexts(styledMarkedTexts, &MarkedTextStyle::areDecorationMarkedTextStylesEqual);

        paintMarkedTexts(paintInfo, TextPaintPhase::Decoration, boxRect, coalescedStyledMarkedTexts, textDecorationSelectionClipOutRect);
    }

    // 3. Paint fancy decorations, including composition underlines and platform-specific underlines for spelling errors, grammar errors, et cetera.
    if (paintInfo.phase == PaintPhase::Foreground) {
        paintPlatformDocumentMarkers(context, boxOrigin);

        if (useCustomUnderlines)
            paintCompositionUnderlines(paintInfo, boxOrigin);
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
    for (auto& markedText : subdivide(collectMarkedTextsForDocumentMarkers(TextPaintPhase::Decoration), OverlapStrategy::Frontmost))
        paintPlatformDocumentMarker(context, boxOrigin, markedText);
}

void InlineTextBox::paintPlatformDocumentMarker(GraphicsContext& context, const FloatPoint& boxOrigin, const MarkedText& markedText)
{
    // Never print spelling/grammar markers (5327887)
    if (renderer().document().printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    float start = 0; // start of line to draw, relative to tx
    float width = logicalWidth(); // how much line to draw

    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (markedText.startOffset || markedText.endOffset != clampedOffset(end() + 1)) {
        // Calculate start & width
        int deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
        int selHeight = selectionHeight();
        FloatPoint startPoint(boxOrigin.x(), boxOrigin.y() - deltaY);
        TextRun run = createTextRun();

        LayoutRect selectionRect = LayoutRect(startPoint, FloatSize(0, selHeight));
        lineFont().adjustSelectionRectForText(run, selectionRect, markedText.startOffset, markedText.endOffset);
        IntRect markerRect = enclosingIntRect(selectionRect);
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();
    }

    auto lineStyleForMarkedTextType = [&] (MarkedText::Type type) -> DocumentMarkerLineStyle {
        bool shouldUseDarkAppearance = renderer().document().useDarkAppearance();
        switch (type) {
        case MarkedText::SpellingError:
            return { DocumentMarkerLineStyle::Mode::Spelling, shouldUseDarkAppearance };
        case MarkedText::GrammarError:
            return { DocumentMarkerLineStyle::Mode::Grammar, shouldUseDarkAppearance };
        case MarkedText::Correction:
            return { DocumentMarkerLineStyle::Mode::AutocorrectionReplacement, shouldUseDarkAppearance };
        case MarkedText::DictationAlternatives:
            return { DocumentMarkerLineStyle::Mode::DictationAlternatives, shouldUseDarkAppearance };
#if PLATFORM(IOS_FAMILY)
        case MarkedText::DictationPhraseWithAlternatives:
            // FIXME: Rename DocumentMarkerLineStyle::TextCheckingDictationPhraseWithAlternatives and remove the PLATFORM(IOS_FAMILY)-guard.
            return { DocumentMarkerLineStyle::Mode::TextCheckingDictationPhraseWithAlternatives, shouldUseDarkAppearance };
#endif
        default:
            ASSERT_NOT_REACHED();
            return { DocumentMarkerLineStyle::Mode::Spelling, shouldUseDarkAppearance };
        }
    };

    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = 3;
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

    context.drawDotsForDocumentMarker(FloatRect(boxOrigin.x() + start, boxOrigin.y() + underlineOffset, width, lineThickness), lineStyleForMarkedTextType(markedText.type));
}

auto InlineTextBox::computeStyleForUnmarkedMarkedText(const PaintInfo& paintInfo) const -> MarkedTextStyle
{
    auto& lineStyle = this->lineStyle();

    MarkedTextStyle style;
    style.textDecorationStyles = TextDecorationPainter::stylesForRenderer(renderer(), lineStyle.textDecorationsInEffect(), isFirstLine());
    style.textStyles = computeTextPaintStyle(renderer().frame(), lineStyle, paintInfo);
    style.textShadow = ShadowData::clone(paintInfo.forceTextColor() ? nullptr : lineStyle.textShadow());
    style.alpha = 1;
    return style;
}

auto InlineTextBox::resolveStyleForMarkedText(const MarkedText& markedText, const MarkedTextStyle& baseStyle, const PaintInfo& paintInfo) -> StyledMarkedText
{
    MarkedTextStyle style = baseStyle;
    switch (markedText.type) {
    case MarkedText::Correction:
    case MarkedText::DictationAlternatives:
#if PLATFORM(IOS_FAMILY)
    // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
    case MarkedText::DictationPhraseWithAlternatives:
#endif
    case MarkedText::GrammarError:
    case MarkedText::SpellingError:
    case MarkedText::Unmarked:
        break;
    case MarkedText::DraggedContent:
        style.alpha = 0.25;
        break;
    case MarkedText::Selection: {
        style.textStyles = computeTextSelectionPaintStyle(style.textStyles, renderer(), lineStyle(), paintInfo, style.textShadow);

        Color selectionBackgroundColor = renderer().selectionBackgroundColor();
        style.backgroundColor = selectionBackgroundColor;
        if (selectionBackgroundColor.isValid() && selectionBackgroundColor.alpha() && style.textStyles.fillColor == selectionBackgroundColor)
            style.backgroundColor = { 0xff - selectionBackgroundColor.red(), 0xff - selectionBackgroundColor.green(), 0xff - selectionBackgroundColor.blue() };
        break;
    }
    case MarkedText::TextMatch: {
        // Text matches always use the light system appearance.
        OptionSet<StyleColor::Options> styleColorOptions = { StyleColor::Options::UseSystemAppearance };
#if PLATFORM(MAC)
        style.textStyles.fillColor = renderer().theme().systemColor(CSSValueAppleSystemLabel, styleColorOptions);
#endif
        style.backgroundColor = markedText.marker->isActiveMatch() ? renderer().theme().activeTextSearchHighlightColor(styleColorOptions) : renderer().theme().inactiveTextSearchHighlightColor(styleColorOptions);
        break;
    }
    }
    StyledMarkedText styledMarkedText = markedText;
    styledMarkedText.style = WTFMove(style);
    return styledMarkedText;
}

auto InlineTextBox::subdivideAndResolveStyle(const Vector<MarkedText>& textsToSubdivide, const MarkedTextStyle& baseStyle, const PaintInfo& paintInfo) -> Vector<StyledMarkedText>
{
    if (textsToSubdivide.isEmpty())
        return { };

    auto markedTexts = subdivide(textsToSubdivide);
    ASSERT(!markedTexts.isEmpty());
    if (UNLIKELY(markedTexts.isEmpty()))
        return { };

    // Compute frontmost overlapping styled marked texts.
    Vector<StyledMarkedText> frontmostMarkedTexts;
    frontmostMarkedTexts.reserveInitialCapacity(markedTexts.size());
    frontmostMarkedTexts.uncheckedAppend(resolveStyleForMarkedText(markedTexts[0], baseStyle, paintInfo));
    for (auto it = markedTexts.begin() + 1, end = markedTexts.end(); it != end; ++it) {
        StyledMarkedText& previousStyledMarkedText = frontmostMarkedTexts.last();
        if (previousStyledMarkedText.startOffset == it->startOffset && previousStyledMarkedText.endOffset == it->endOffset) {
            // Marked texts completely cover each other.
            previousStyledMarkedText = resolveStyleForMarkedText(*it, previousStyledMarkedText.style, paintInfo);
            continue;
        }
        frontmostMarkedTexts.uncheckedAppend(resolveStyleForMarkedText(*it, baseStyle, paintInfo));
    }

    return frontmostMarkedTexts;
}

auto InlineTextBox::coalesceAdjacentMarkedTexts(const Vector<StyledMarkedText>& textsToCoalesce, MarkedTextStylesEqualityFunction areMarkedTextStylesEqual) -> Vector<StyledMarkedText>
{
    if (textsToCoalesce.isEmpty())
        return { };

    auto areAdjacentMarkedTextsWithSameStyle = [&] (const StyledMarkedText& a, const StyledMarkedText& b) {
        return a.endOffset == b.startOffset && areMarkedTextStylesEqual(a.style, b.style);
    };

    Vector<StyledMarkedText> styledMarkedTexts;
    styledMarkedTexts.reserveInitialCapacity(textsToCoalesce.size());
    styledMarkedTexts.uncheckedAppend(textsToCoalesce[0]);
    for (auto it = textsToCoalesce.begin() + 1, end = textsToCoalesce.end(); it != end; ++it) {
        StyledMarkedText& previousStyledMarkedText = styledMarkedTexts.last();
        if (areAdjacentMarkedTextsWithSameStyle(previousStyledMarkedText, *it)) {
            previousStyledMarkedText.endOffset = it->endOffset;
            continue;
        }
        styledMarkedTexts.uncheckedAppend(*it);
    }

    return styledMarkedTexts;
}

Vector<MarkedText> InlineTextBox::collectMarkedTextsForDraggedContent()
{
    using DraggendContentRange = std::pair<unsigned, unsigned>;
    auto draggedContentRanges = renderer().draggedContentRangesBetweenOffsets(m_start, m_start + m_len);
    Vector<MarkedText> result = draggedContentRanges.map([this] (const DraggendContentRange& range) -> MarkedText {
        return { clampedOffset(range.first), clampedOffset(range.second), MarkedText::DraggedContent };
    });
    return result;
}

Vector<MarkedText> InlineTextBox::collectMarkedTextsForDocumentMarkers(TextPaintPhase phase)
{
    ASSERT_ARG(phase, phase == TextPaintPhase::Background || phase == TextPaintPhase::Foreground || phase == TextPaintPhase::Decoration);

    if (!renderer().textNode())
        return { };

    Vector<RenderedDocumentMarker*> markers = renderer().document().markers().markersFor(renderer().textNode());

    auto markedTextTypeForMarkerType = [] (DocumentMarker::MarkerType type) {
        switch (type) {
        case DocumentMarker::Spelling:
            return MarkedText::SpellingError;
        case DocumentMarker::Grammar:
            return MarkedText::GrammarError;
        case DocumentMarker::CorrectionIndicator:
            return MarkedText::Correction;
        case DocumentMarker::TextMatch:
            return MarkedText::TextMatch;
        case DocumentMarker::DictationAlternatives:
            return MarkedText::DictationAlternatives;
#if PLATFORM(IOS_FAMILY)
        case DocumentMarker::DictationPhraseWithAlternatives:
            return MarkedText::DictationPhraseWithAlternatives;
#endif
        default:
            return MarkedText::Unmarked;
        }
    };

    Vector<MarkedText> markedTexts;
    markedTexts.reserveInitialCapacity(markers.size());

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
#if PLATFORM(IOS_FAMILY)
        // FIXME: Remove the PLATFORM(IOS_FAMILY)-guard.
        case DocumentMarker::DictationPhraseWithAlternatives:
#endif
            if (phase != TextPaintPhase::Decoration)
                continue;
            break;
        case DocumentMarker::TextMatch:
            if (!renderer().frame().editor().markedTextMatchesAreHighlighted())
                continue;
            if (phase == TextPaintPhase::Decoration)
                continue;
            break;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        case DocumentMarker::TelephoneNumber:
            if (!renderer().frame().editor().markedTextMatchesAreHighlighted())
                continue;
            if (phase != TextPaintPhase::Background)
                continue;
            break;
#endif
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
#if PLATFORM(IOS_FAMILY)
        // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
        case DocumentMarker::DictationPhraseWithAlternatives:
#endif
        case DocumentMarker::TextMatch:
            markedTexts.uncheckedAppend({ clampedOffset(marker->startOffset()), clampedOffset(marker->endOffset()), markedTextTypeForMarkerType(marker->type()), marker });
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
    return markedTexts;
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

void InlineTextBox::paintMarkedTexts(PaintInfo& paintInfo, TextPaintPhase phase, const FloatRect& boxRect, const Vector<StyledMarkedText>& markedTexts, const FloatRect& decorationClipOutRect)
{
    switch (phase) {
    case TextPaintPhase::Background:
        for (auto& markedText : markedTexts)
            paintMarkedTextBackground(paintInfo, boxRect.location(), markedText.style.backgroundColor, markedText.startOffset, markedText.endOffset);
        return;
    case TextPaintPhase::Foreground:
        for (auto& markedText : markedTexts)
            paintMarkedTextForeground(paintInfo, boxRect, markedText);
        return;
    case TextPaintPhase::Decoration:
        for (auto& markedText : markedTexts)
            paintMarkedTextDecoration(paintInfo, boxRect, decorationClipOutRect, markedText);
        return;
    }
}

void InlineTextBox::paintMarkedTextBackground(PaintInfo& paintInfo, const FloatPoint& boxOrigin, const Color& color, unsigned clampedStartOffset, unsigned clampedEndOffset)
{
    if (clampedStartOffset >= clampedEndOffset)
        return;

    GraphicsContext& context = paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };
    updateGraphicsContext(context, TextPaintStyle { color }); // Don't draw text at all!

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own highlight.
    TextRun textRun = createTextRun();

    const RootInlineBox& rootBox = root();
    LayoutUnit selectionBottom = rootBox.selectionBottom();
    LayoutUnit selectionTop = rootBox.selectionTopAdjustedForPrecedingBlock();

    // Use same y positioning and height as for selection, so that when the selection and this subrange are on
    // the same word there are no pieces sticking out.
    LayoutUnit deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom - logicalBottom() : logicalTop() - selectionTop;
    LayoutUnit selectionHeight = std::max<LayoutUnit>(0, selectionBottom - selectionTop);

    LayoutRect selectionRect = LayoutRect(boxOrigin.x(), boxOrigin.y() - deltaY, logicalWidth(), selectionHeight);
    lineFont().adjustSelectionRectForText(textRun, selectionRect, clampedStartOffset, clampedEndOffset);

    // FIXME: Support painting combined text. See <https://bugs.webkit.org/show_bug.cgi?id=180993>.
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()), color);
}

void InlineTextBox::paintMarkedTextForeground(PaintInfo& paintInfo, const FloatRect& boxRect, const StyledMarkedText& markedText)
{
    if (markedText.startOffset >= markedText.endOffset)
        return;

    GraphicsContext& context = paintInfo.context();
    const FontCascade& font = lineFont();
    const RenderStyle& lineStyle = this->lineStyle();

    float emphasisMarkOffset = 0;
    std::optional<bool> markExistsAndIsAbove = emphasisMarkExistsAndIsAbove(lineStyle);
    const AtomicString& emphasisMark = markExistsAndIsAbove ? lineStyle.textEmphasisMarkString() : nullAtom();
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = *markExistsAndIsAbove ? -font.fontMetrics().ascent() - font.emphasisMarkDescent(emphasisMark) : font.fontMetrics().descent() + font.emphasisMarkAscent(emphasisMark);

    TextPainter textPainter { context };
    textPainter.setFont(font);
    textPainter.setStyle(markedText.style.textStyles);
    textPainter.setIsHorizontal(isHorizontal());
    if (markedText.style.textShadow) {
        textPainter.setShadow(&markedText.style.textShadow.value());
        if (lineStyle.hasAppleColorFilter())
            textPainter.setShadowColorFilter(&lineStyle.appleColorFilter());
    }
    textPainter.setEmphasisMark(emphasisMark, emphasisMarkOffset, combinedText());

    TextRun textRun = createTextRun();
    textPainter.setGlyphDisplayListIfNeeded(*this, paintInfo, font, context, textRun);

    GraphicsContextStateSaver stateSaver { context, false };
    if (markedText.type == MarkedText::DraggedContent) {
        stateSaver.save();
        context.setAlpha(markedText.style.alpha);
    }
    // TextPainter wants the box rectangle and text origin of the entire line box.
    textPainter.paintRange(textRun, boxRect, textOriginFromBoxRect(boxRect), markedText.startOffset, markedText.endOffset);
}

void InlineTextBox::paintMarkedTextDecoration(PaintInfo& paintInfo, const FloatRect& boxRect, const FloatRect& clipOutRect, const StyledMarkedText& markedText)
{
    if (m_truncation == cFullTruncation)
        return;

    GraphicsContext& context = paintInfo.context();
    updateGraphicsContext(context, markedText.style.textStyles);

    bool isCombinedText = combinedText();
    if (isCombinedText)
        context.concatCTM(rotation(boxRect, Clockwise));

    // 1. Compute text selection
    unsigned startOffset = markedText.startOffset;
    unsigned endOffset = markedText.endOffset;
    if (startOffset >= endOffset)
        return;

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own decoration.
    TextRun textRun = createTextRun();

    // Avoid measuring the text when the entire line box is selected as an optimization.
    FloatRect snappedSelectionRect = boxRect;
    if (startOffset || endOffset != textRun.length()) {
        LayoutRect selectionRect = { boxRect.x(), boxRect.y(), boxRect.width(), boxRect.height() };
        lineFont().adjustSelectionRectForText(textRun, selectionRect, startOffset, endOffset);
        snappedSelectionRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr());
    }

    // 2. Paint
    TextDecorationPainter decorationPainter { context, lineStyle().textDecorationsInEffect(), renderer(), isFirstLine(), lineFont(), markedText.style.textDecorationStyles };
    decorationPainter.setInlineTextBox(this);
    decorationPainter.setWidth(snappedSelectionRect.width());
    decorationPainter.setIsHorizontal(isHorizontal());
    if (markedText.style.textShadow) {
        decorationPainter.setTextShadow(&markedText.style.textShadow.value());
        if (lineStyle().hasAppleColorFilter())
            decorationPainter.setShadowColorFilter(&lineStyle().appleColorFilter());
    }

    {
        GraphicsContextStateSaver stateSaver { context, false };
        bool isDraggedContent = markedText.type == MarkedText::DraggedContent;
        if (isDraggedContent || !clipOutRect.isEmpty()) {
            stateSaver.save();
            if (isDraggedContent)
                context.setAlpha(markedText.style.alpha);
            if (!clipOutRect.isEmpty())
                context.clipOut(clipOutRect);
        }
        decorationPainter.paintTextDecoration(textRun.subRun(startOffset, endOffset - startOffset), textOriginFromBoxRect(snappedSelectionRect), snappedSelectionRect.location());
    }

    if (isCombinedText)
        context.concatCTM(rotation(boxRect, Counterclockwise));
}

void InlineTextBox::paintCompositionBackground(PaintInfo& paintInfo, const FloatPoint& boxOrigin)
{
    paintMarkedTextBackground(paintInfo, boxOrigin, Color::compositionFill, clampedOffset(renderer().frame().editor().compositionStart()), clampedOffset(renderer().frame().editor().compositionEnd()));
}

void InlineTextBox::paintCompositionUnderlines(PaintInfo& paintInfo, const FloatPoint& boxOrigin) const
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
        paintCompositionUnderline(paintInfo, boxOrigin, underline);

        if (underline.endOffset > end() + 1)
            break; // Underline also runs into the next run. Bail now, no more marker advancement.
    }
}

static inline void mirrorRTLSegment(float logicalWidth, TextDirection direction, float& start, float width)
{
    if (direction == TextDirection::LTR)
        return;
    start = logicalWidth - width - start;
}

void InlineTextBox::paintCompositionUnderline(PaintInfo& paintInfo, const FloatPoint& boxOrigin, const CompositionUnderline& underline) const
{
    if (m_truncation == cFullTruncation)
        return;
    
    float start = 0; // start of line to draw, relative to tx
    float width = logicalWidth(); // how much line to draw
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
        mirrorRTLSegment(logicalWidth(), direction(), start, width);
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

    GraphicsContext& context = paintInfo.context();
    Color underlineColor = underline.compositionUnderlineColor == CompositionUnderlineColor::TextColor ? renderer().style().visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor) : renderer().style().colorByApplyingColorFilter(underline.color);
    context.setStrokeColor(underlineColor);
    context.setStrokeThickness(lineThickness);
    context.drawLineForText(FloatRect(boxOrigin.x() + start, boxOrigin.y() + logicalHeight() - lineThickness, width, lineThickness), renderer().document().printing());
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
    return lineFont().offsetForPosition(createTextRun(ignoreCombinedText, ignoreHyphen), lineOffset - logicalLeft(), includePartialGlyphs);
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
    TextRun textRun = createTextRun(ignoreCombinedText, ignoreHyphen);
    lineFont().adjustSelectionRectForText(textRun, selectionRect, startOffset, endOffset);
    return snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()).maxX();
}

TextRun InlineTextBox::createTextRun(bool ignoreCombinedText, bool ignoreHyphen) const
{
    const auto& style = lineStyle();
    TextRun textRun { text(ignoreCombinedText, ignoreHyphen), textPos(), expansion(), expansionBehavior(), direction(), dirOverride() || style.rtlOrdering() == Order::Visual, !renderer().canUseSimpleFontCodePath() };
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
