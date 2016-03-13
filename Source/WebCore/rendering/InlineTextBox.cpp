/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2014 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "ChromeClient.h"
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
#include "Page.h"
#include "PaintInfo.h"
#include "RenderedDocumentMarker.h"
#include "RenderBlock.h"
#include "RenderCombineText.h"
#include "RenderLineBreak.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "Text.h"
#include "TextDecorationPainter.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"
#include "break_lines.h"
#include <stdio.h>
#include <wtf/text/CString.h>

namespace WebCore {

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

bool InlineTextBox::isSelected(int startPos, int endPos) const
{
    int sPos = std::max(startPos - m_start, 0);
    int ePos = std::min(endPos - m_start, static_cast<int>(m_len));
    return (sPos < ePos);
}

RenderObject::SelectionState InlineTextBox::selectionState()
{
    RenderObject::SelectionState state = renderer().selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd || state == RenderObject::SelectionBoth) {
        int startPos, endPos;
        renderer().selectionStartEnd(startPos, endPos);
        // The position after a hard line break is considered to be past its end.
        int lastSelectable = start() + len() - (isLineBreak() ? 1 : 0);

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
            int start, end;
            selectionStartEnd(start, end);
            // The ellipsis should be considered to be selected if the end of
            // the selection is past the beginning of the truncation and the
            // beginning of the selection is before or at the beginning of the
            // truncation.
            ellipsis->setSelectionState(end >= m_truncation && start <= m_truncation ?
                RenderObject::SelectionInside : RenderObject::SelectionNone);
        } else
            ellipsis->setSelectionState(RenderObject::SelectionNone);
    }

    return state;
}

static const FontCascade& fontToUse(const RenderStyle& style, const RenderText& renderer)
{
    if (style.hasTextCombine() && is<RenderCombineText>(renderer)) {
        const auto& textCombineRenderer = downcast<RenderCombineText>(renderer);
        if (textCombineRenderer.isCombined())
            return textCombineRenderer.textCombineFont();
    }
    return style.fontCascade();
}

LayoutRect InlineTextBox::localSelectionRect(int startPos, int endPos) const
{
    int sPos = std::max(startPos - m_start, 0);
    int ePos = std::min(endPos - m_start, (int)m_len);
    
    if (sPos > ePos)
        return LayoutRect();

    LayoutUnit selectionTop = this->selectionTop();
    LayoutUnit selectionHeight = this->selectionHeight();
    const RenderStyle& lineStyle = this->lineStyle();
    const FontCascade& font = fontToUse(lineStyle, renderer());

    String hyphenatedStringBuffer;
    bool respectHyphen = ePos == m_len && hasHyphen();
    TextRun textRun = constructTextRun(lineStyle, respectHyphen ? &hyphenatedStringBuffer : 0);

    LayoutRect selectionRect = LayoutRect(LayoutPoint(logicalLeft(), selectionTop), LayoutSize(m_logicalWidth, selectionHeight));
    // Avoid computing the font width when the entire line box is selected as an optimization.
    if (sPos || ePos != static_cast<int>(m_len))
        font.adjustSelectionRectForText(textRun, selectionRect, sPos, ePos);
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
    return renderer().style().preserveNewline() && len() == 1 && (*renderer().text())[start()] == '\n';
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
        if (!result.addNodeToRectBasedTestResult(renderer().textNode(), request, locationInContainer, rect))
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

    RenderCombineText* combinedText = lineStyle.hasTextCombine() && is<RenderCombineText>(renderer()) && downcast<RenderCombineText>(renderer()).isCombined() ? &downcast<RenderCombineText>(renderer()) : nullptr;

    bool shouldRotate = !isHorizontal() && !combinedText;
    if (shouldRotate)
        context.concatCTM(rotation(boxRect, Clockwise));

    // Determine whether or not we have composition underlines to draw.
    bool containsComposition = renderer().textNode() && renderer().frame().editor().compositionNode() == renderer().textNode();
    bool useCustomUnderlines = containsComposition && renderer().frame().editor().compositionUsesCustomUnderlines();

    // Determine the text colors and selection colors.
    TextPaintStyle textPaintStyle = computeTextPaintStyle(renderer().frame(), lineStyle, paintInfo);

    bool paintSelectedTextOnly = false;
    bool paintSelectedTextSeparately = false;
    const ShadowData* selectionShadow = nullptr;
    
    // Text with custom underlines does not have selection background painted, so selection paint style is not appropriate for it.
    TextPaintStyle selectionPaintStyle = haveSelection && !useCustomUnderlines ? computeTextSelectionPaintStyle(textPaintStyle, renderer(), lineStyle, paintInfo, paintSelectedTextOnly, paintSelectedTextSeparately, selectionShadow) : textPaintStyle;

    // Set our font.
    const FontCascade& font = fontToUse(lineStyle, renderer());
    // 1. Paint backgrounds behind text if needed. Examples of such backgrounds include selection
    // and composition underlines.
    if (paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseTextClip && !isPrinting) {
        if (containsComposition && !useCustomUnderlines)
            paintCompositionBackground(context, boxOrigin, lineStyle, font,
                renderer().frame().editor().compositionStart(),
                renderer().frame().editor().compositionEnd());

        paintDocumentMarkers(context, boxOrigin, lineStyle, font, true);

        if (haveSelection && !useCustomUnderlines)
            paintSelection(context, boxOrigin, lineStyle, font, selectionPaintStyle.fillColor);
    }

    if (Page* page = renderer().frame().page()) {
        // FIXME: Right now, InlineTextBoxes never call addRelevantUnpaintedObject() even though they might
        // legitimately be unpainted if they are waiting on a slow-loading web font. We should fix that, and
        // when we do, we will have to account for the fact the InlineTextBoxes do not always have unique
        // renderers and Page currently relies on each unpainted object having a unique renderer.
        if (paintInfo.phase == PaintPhaseForeground)
            page->addRelevantRepaintedObject(&renderer(), IntRect(boxOrigin.x(), boxOrigin.y(), logicalWidth(), logicalHeight()));
    }

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    int length = m_len;
    int maximumLength;
    String string;
    if (!combinedText) {
        string = renderer().text();
        if (static_cast<unsigned>(length) != string.length() || m_start) {
            ASSERT_WITH_SECURITY_IMPLICATION(static_cast<unsigned>(m_start + length) <= string.length());
            string = string.substringSharingImpl(m_start, length);
        }
        maximumLength = renderer().textLength() - m_start;
    } else {
        combinedText->getStringToRender(m_start, string, length);
        maximumLength = length;
    }

    String hyphenatedStringBuffer;
    TextRun textRun = constructTextRun(lineStyle, string, maximumLength, hasHyphen() ? &hyphenatedStringBuffer : nullptr);
    if (hasHyphen())
        length = textRun.length();

    int selectionStart = 0;
    int selectionEnd = 0;
    if (haveSelection && (paintSelectedTextOnly || paintSelectedTextSeparately))
        selectionStartEnd(selectionStart, selectionEnd);

    if (m_truncation != cNoTruncation) {
        selectionStart = std::min<int>(selectionStart, m_truncation);
        selectionEnd = std::min<int>(selectionEnd, m_truncation);
        length = m_truncation;
    }

    int emphasisMarkOffset = 0;
    bool emphasisMarkAbove;
    bool hasTextEmphasis = emphasisMarkExistsAndIsAbove(lineStyle, emphasisMarkAbove);
    const AtomicString& emphasisMark = hasTextEmphasis ? lineStyle.textEmphasisMarkString() : nullAtom;
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = emphasisMarkAbove ? -font.fontMetrics().ascent() - font.emphasisMarkDescent(emphasisMark) : font.fontMetrics().descent() + font.emphasisMarkAscent(emphasisMark);

    const ShadowData* textShadow = (paintInfo.forceTextColor()) ? nullptr : lineStyle.textShadow();

    FloatPoint textOrigin(boxOrigin.x(), boxOrigin.y() + font.fontMetrics().ascent());
    if (combinedText) {
        if (auto newOrigin = combinedText->computeTextOrigin(boxRect))
            textOrigin = newOrigin.value();
    }

    if (isHorizontal())
        textOrigin.setY(roundToDevicePixel(LayoutUnit(textOrigin.y()), renderer().document().deviceScaleFactor()));
    else
        textOrigin.setX(roundToDevicePixel(LayoutUnit(textOrigin.x()), renderer().document().deviceScaleFactor()));

    TextPainter textPainter(context);
    textPainter.setFont(font);
    textPainter.setTextPaintStyle(textPaintStyle);
    textPainter.setSelectionPaintStyle(selectionPaintStyle);
    textPainter.setIsHorizontal(isHorizontal());
    textPainter.addTextShadow(textShadow, selectionShadow);
    textPainter.addEmphasis(emphasisMark, emphasisMarkOffset, combinedText);

    textPainter.paintText(textRun, length, boxRect, textOrigin, selectionStart, selectionEnd, paintSelectedTextOnly, paintSelectedTextSeparately);

    // Paint decorations
    TextDecoration textDecorations = lineStyle.textDecorationsInEffect();
    if (textDecorations != TextDecorationNone && paintInfo.phase != PaintPhaseSelection)
        paintDecoration(context, font, combinedText, textRun, textOrigin, boxRect, textDecorations, textPaintStyle, textShadow);

    if (paintInfo.phase == PaintPhaseForeground) {
        paintDocumentMarkers(context, boxOrigin, lineStyle, font, false);

        if (useCustomUnderlines) {
            const Vector<CompositionUnderline>& underlines = renderer().frame().editor().customCompositionUnderlines();
            size_t numUnderlines = underlines.size();

            for (size_t index = 0; index < numUnderlines; ++index) {
                const CompositionUnderline& underline = underlines[index];

                if (underline.endOffset <= start())
                    // underline is completely before this run.  This might be an underline that sits
                    // before the first run we draw, or underlines that were within runs we skipped 
                    // due to truncation.
                    continue;
                
                if (underline.startOffset <= end()) {
                    // underline intersects this run.  Paint it.
                    paintCompositionUnderline(context, boxOrigin, underline);
                    if (underline.endOffset > end() + 1)
                        // underline also runs into the next run. Bail now, no more marker advancement.
                        break;
                } else
                    // underline is completely after this run, bail.  A later run will paint it.
                    break;
            }
        }
    }
    
    if (shouldRotate)
        context.concatCTM(rotation(boxRect, Counterclockwise));
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos)
{
    int startPos, endPos;
    if (renderer().selectionState() == RenderObject::SelectionInside) {
        startPos = 0;
        endPos = renderer().textLength();
    } else {
        renderer().selectionStartEnd(startPos, endPos);
        if (renderer().selectionState() == RenderObject::SelectionStart)
            endPos = renderer().textLength();
        else if (renderer().selectionState() == RenderObject::SelectionEnd)
            startPos = 0;
    }

    sPos = std::max(startPos - m_start, 0);
    ePos = std::min(endPos - m_start, (int)m_len);
}

void InlineTextBox::paintSelection(GraphicsContext& context, const FloatPoint& boxOrigin, const RenderStyle& style, const FontCascade& font, Color textColor)
{
#if ENABLE(TEXT_SELECTION)
    if (context.paintingDisabled())
        return;

    // See if we have a selection to paint at all.
    int sPos, ePos;
    selectionStartEnd(sPos, ePos);
    if (sPos >= ePos)
        return;

    Color c = renderer().selectionBackgroundColor();
    if (!c.isValid() || c.alpha() == 0)
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    GraphicsContextStateSaver stateSaver(context);
    updateGraphicsContext(context, TextPaintStyle(c)); // Don't draw text at all!
    
    // If the text is truncated, let the thing being painted in the truncation
    // draw its own highlight.
    int length = m_truncation != cNoTruncation ? m_truncation : m_len;
    String string = renderer().text();

    if (string.length() != static_cast<unsigned>(length) || m_start) {
        ASSERT_WITH_SECURITY_IMPLICATION(static_cast<unsigned>(m_start + length) <= string.length());
        string = string.substringSharingImpl(m_start, length);
    }

    String hyphenatedStringBuffer;
    bool respectHyphen = ePos == length && hasHyphen();
    TextRun textRun = constructTextRun(style, string, renderer().textLength() - m_start, respectHyphen ? &hyphenatedStringBuffer : nullptr);
    if (respectHyphen)
        ePos = textRun.length();

    const RootInlineBox& rootBox = root();
    LayoutUnit selectionBottom = rootBox.selectionBottom();
    LayoutUnit selectionTop = rootBox.selectionTopAdjustedForPrecedingBlock();

    LayoutUnit deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom - logicalBottom() : logicalTop() - selectionTop;
    LayoutUnit selectionHeight = std::max<LayoutUnit>(0, selectionBottom - selectionTop);

    LayoutRect selectionRect = LayoutRect(boxOrigin.x(), boxOrigin.y() - deltaY, m_logicalWidth, selectionHeight);
    font.adjustSelectionRectForText(textRun, selectionRect, sPos, ePos);
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()), c);
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(boxOrigin);
    UNUSED_PARAM(style);
    UNUSED_PARAM(font);
    UNUSED_PARAM(textColor);
#endif
}

void InlineTextBox::paintCompositionBackground(GraphicsContext& context, const FloatPoint& boxOrigin, const RenderStyle& style, const FontCascade& font, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = std::max(startPos - offset, 0);
    int ePos = std::min(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    GraphicsContextStateSaver stateSaver(context);
    Color compositionColor = Color::compositionFill;
    updateGraphicsContext(context, TextPaintStyle(compositionColor)); // Don't draw text at all!

    LayoutUnit deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
    LayoutRect selectionRect = LayoutRect(boxOrigin.x(), boxOrigin.y() - deltaY, 0, selectionHeight());
    TextRun textRun = constructTextRun(style);
    font.adjustSelectionRectForText(textRun, selectionRect, sPos, ePos);
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), textRun.ltr()), compositionColor);
}

void InlineTextBox::paintDecoration(GraphicsContext& context, const FontCascade& font, RenderCombineText* combinedText, const TextRun& textRun, const FloatPoint& textOrigin,
    const FloatRect& boxRect, TextDecoration decoration, TextPaintStyle textPaintStyle, const ShadowData* shadow)
{
    if (m_truncation == cFullTruncation)
        return;

    FloatPoint localOrigin = boxRect.location();
    updateGraphicsContext(context, textPaintStyle);
    if (combinedText)
        context.concatCTM(rotation(boxRect, Clockwise));

    float width = m_logicalWidth;
    if (m_truncation != cNoTruncation) {
        width = renderer().width(m_start, m_truncation, textPos(), isFirstLine());
        if (!isLeftToRightDirection())
            localOrigin.move(m_logicalWidth - width, 0);
    }
    
    int baseline = lineStyle().fontMetrics().ascent();
    TextDecorationPainter decorationPainter(context, decoration, renderer(), isFirstLine());
    decorationPainter.setInlineTextBox(this);
    decorationPainter.setFont(font);
    decorationPainter.setWidth(width);
    decorationPainter.setBaseline(baseline);
    decorationPainter.setIsHorizontal(isHorizontal());
    decorationPainter.addTextShadow(shadow);

    decorationPainter.paintTextDecoration(textRun, textOrigin, localOrigin);

    if (combinedText)
        context.concatCTM(rotation(boxRect, Counterclockwise));
}

static GraphicsContext::DocumentMarkerLineStyle lineStyleForMarkerType(DocumentMarker::MarkerType markerType)
{
    switch (markerType) {
    case DocumentMarker::Spelling:
        return GraphicsContext::DocumentMarkerSpellingLineStyle;
    case DocumentMarker::Grammar:
        return GraphicsContext::DocumentMarkerGrammarLineStyle;
    case DocumentMarker::CorrectionIndicator:
        return GraphicsContext::DocumentMarkerAutocorrectionReplacementLineStyle;
    case DocumentMarker::DictationAlternatives:
        return GraphicsContext::DocumentMarkerDictationAlternativesLineStyle;
#if PLATFORM(IOS)
    case DocumentMarker::DictationPhraseWithAlternatives:
        // FIXME: Rename TextCheckingDictationPhraseWithAlternativesLineStyle and remove the PLATFORM(IOS)-guard.
        return GraphicsContext::TextCheckingDictationPhraseWithAlternativesLineStyle;
#endif
    default:
        ASSERT_NOT_REACHED();
        return GraphicsContext::DocumentMarkerSpellingLineStyle;
    }
}

void InlineTextBox::paintDocumentMarker(GraphicsContext& context, const FloatPoint& boxOrigin, RenderedDocumentMarker& marker, const RenderStyle& style, const FontCascade& font, bool grammar)
{
    // Never print spelling/grammar markers (5327887)
    if (renderer().document().printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    float start = 0; // start of line to draw, relative to tx
    float width = m_logicalWidth; // how much line to draw

    // Determine whether we need to measure text
    bool markerSpansWholeBox = true;
    if (m_start <= (int)marker.startOffset())
        markerSpansWholeBox = false;
    if ((end() + 1) != marker.endOffset()) // end points at the last char, not past it
        markerSpansWholeBox = false;
    if (m_truncation != cNoTruncation)
        markerSpansWholeBox = false;

    bool isDictationMarker = marker.type() == DocumentMarker::DictationAlternatives;
    if (!markerSpansWholeBox || grammar || isDictationMarker) {
        int startPosition = std::max<int>(marker.startOffset() - m_start, 0);
        int endPosition = std::min<int>(marker.endOffset() - m_start, m_len);
        
        if (m_truncation != cNoTruncation)
            endPosition = std::min<int>(endPosition, m_truncation);

        // Calculate start & width
        int deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
        int selHeight = selectionHeight();
        FloatPoint startPoint(boxOrigin.x(), boxOrigin.y() - deltaY);
        TextRun run = constructTextRun(style);

        LayoutRect selectionRect = LayoutRect(startPoint, FloatSize(0, selHeight));
        font.adjustSelectionRectForText(run, selectionRect, startPosition, endPosition);
        IntRect markerRect = enclosingIntRect(selectionRect);
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();
    }
    
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
    context.drawLineForDocumentMarker(FloatPoint(boxOrigin.x() + start, boxOrigin.y() + underlineOffset), width, lineStyleForMarkerType(marker.type()));
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext& context, const FloatPoint& boxOrigin, RenderedDocumentMarker& marker, const RenderStyle& style, const FontCascade& font)
{
    if (!renderer().frame().editor().markedTextMatchesAreHighlighted())
        return;

    Color color = marker.activeMatch() ? renderer().theme().platformActiveTextSearchHighlightColor() : renderer().theme().platformInactiveTextSearchHighlightColor();
    GraphicsContextStateSaver stateSaver(context);
    updateGraphicsContext(context, TextPaintStyle(color)); // Don't draw text at all!

    // Use same y positioning and height as for selection, so that when the selection and this highlight are on
    // the same word there are no pieces sticking out.
    LayoutUnit deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
    LayoutRect selectionRect = LayoutRect(boxOrigin.x(), boxOrigin.y() - deltaY, 0, this->selectionHeight());

    int sPos = std::max<int>(marker.startOffset() - m_start, 0);
    int ePos = std::min<int>(marker.endOffset() - m_start, m_len);
    TextRun run = constructTextRun(style);
    font.adjustSelectionRectForText(run, selectionRect, sPos, ePos);

    if (selectionRect.isEmpty())
        return;

    context.fillRect(snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), run.ltr()), color);
}
    
void InlineTextBox::paintDocumentMarkers(GraphicsContext& context, const FloatPoint& boxOrigin, const RenderStyle& style, const FontCascade& font, bool background)
{
    if (!renderer().textNode())
        return;

    Vector<RenderedDocumentMarker*> markers = renderer().document().markers().markersFor(renderer().textNode());

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for (auto* marker : markers) {
        // Paint either the background markers or the foreground markers, but not both
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
                if (background)
                    continue;
                break;
            case DocumentMarker::TextMatch:
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
            case DocumentMarker::TelephoneNumber:
#endif
                if (!background)
                    continue;
                break;
            default:
                continue;
        }

        if (marker->endOffset() <= start())
            // marker is completely before this run.  This might be a marker that sits before the
            // first run we draw, or markers that were within runs we skipped due to truncation.
            continue;
        
        if (marker->startOffset() > end())
            // marker is completely after this run, bail.  A later run will paint it.
            break;
        
        // marker intersects this run.  Paint it.
        switch (marker->type()) {
            case DocumentMarker::Spelling:
            case DocumentMarker::CorrectionIndicator:
            case DocumentMarker::DictationAlternatives:
                paintDocumentMarker(context, boxOrigin, *marker, style, font, false);
                break;
            case DocumentMarker::Grammar:
                paintDocumentMarker(context, boxOrigin, *marker, style, font, true);
                break;
#if PLATFORM(IOS)
            // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS)-guard.
            case DocumentMarker::DictationPhraseWithAlternatives:
                paintDocumentMarker(context, boxOrigin, *marker, style, font, true);
                break;
#endif
            case DocumentMarker::TextMatch:
                paintTextMatchMarker(context, boxOrigin, *marker, style, font);
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
}

void InlineTextBox::paintCompositionUnderline(GraphicsContext& context, const FloatPoint& boxOrigin, const CompositionUnderline& underline)
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

    context.setStrokeColor(underline.color);
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

    const RenderStyle& lineStyle = this->lineStyle();
    const FontCascade& font = fontToUse(lineStyle, renderer());
    return font.offsetForPosition(constructTextRun(lineStyle), lineOffset - logicalLeft(), includePartialGlyphs);
}

float InlineTextBox::positionForOffset(int offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + m_len);

    if (isLineBreak())
        return logicalLeft();

    const RenderStyle& lineStyle = this->lineStyle();
    const FontCascade& font = fontToUse(lineStyle, renderer());
    int from = !isLeftToRightDirection() ? offset - m_start : 0;
    int to = !isLeftToRightDirection() ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    LayoutRect selectionRect = LayoutRect(logicalLeft(), 0, 0, 0);
    TextRun run = constructTextRun(lineStyle);
    font.adjustSelectionRectForText(run, selectionRect, from, to);
    return snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), run.ltr()).maxX();
}

TextRun InlineTextBox::constructTextRun(const RenderStyle& style, String* hyphenatedStringBuffer) const
{
    ASSERT(renderer().text());

    String string = renderer().text();
    unsigned startPos = start();
    unsigned length = len();

    if (string.length() != length || startPos)
        string = string.substringSharingImpl(startPos, length);

    return constructTextRun(style, string, renderer().textLength() - startPos, hyphenatedStringBuffer);
}

TextRun InlineTextBox::constructTextRun(const RenderStyle& style, String string, unsigned maximumLength, String* hyphenatedStringBuffer) const
{
    unsigned length = string.length();

    if (hyphenatedStringBuffer) {
        const AtomicString& hyphenString = style.hyphenString();
        *hyphenatedStringBuffer = string + hyphenString;
        string = *hyphenatedStringBuffer;
        maximumLength = length + hyphenString.length();
    }

    ASSERT(maximumLength >= length);

    TextRun run(string, textPos(), expansion(), expansionBehavior(), direction(), dirOverride() || style.rtlOrdering() == VisualOrder, !renderer().canUseSimpleFontCodePath());
    run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(maximumLength);
    ASSERT(run.charactersLength() >= run.length());
    return run;
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

void InlineTextBox::showLineBox(bool mark, int depth) const
{
    fprintf(stderr, "------- --");

    int printedCharacters = 0;
    if (mark) {
        fprintf(stderr, "*");
        ++printedCharacters;
    }
    while (++printedCharacters <= depth * 2)
        fputc(' ', stderr);

    String value = renderer().text();
    value = value.substring(start(), len());
    value.replaceWithLiteral('\\', "\\\\");
    value.replaceWithLiteral('\n', "\\n");
    fprintf(stderr, "%s  (%.2f, %.2f) (%.2f, %.2f) (%p) run(%d, %d) \"%s\"\n", boxName(), x(), y(), width(), height(), this, start(), start() + len(), value.utf8().data());
}

#endif

} // namespace WebCore
