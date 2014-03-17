/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include "FontCache.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
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
#include "SVGTextRunRenderingContext.h"
#include "Text.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"
#include "break_lines.h"
#include <wtf/text/CString.h>

#ifndef NDEBUG
#include <cstdio>
#endif

namespace WebCore {

struct SameSizeAsInlineTextBox : public InlineBox {
    unsigned variables[1];
    unsigned short variables2[2];
    void* pointers[2];
};

COMPILE_ASSERT(sizeof(InlineTextBox) == sizeof(SameSizeAsInlineTextBox), InlineTextBox_should_stay_small);

typedef WTF::HashMap<const InlineTextBox*, LayoutRect> InlineTextBoxOverflowMap;
static InlineTextBoxOverflowMap* gTextBoxesWithOverflow;

#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
static bool compareTuples(std::pair<float, float> l, std::pair<float, float> r)
{
    return l.first < r.first;
}

static DashArray translateIntersectionPointsToSkipInkBoundaries(const DashArray& intersections, float dilationAmount, float totalWidth)
{
    ASSERT(!(intersections.size() % 2));
    
    // Step 1: Make pairs so we can sort based on range starting-point. We dilate the ranges in this step as well.
    Vector<std::pair<float, float>> tuples;
    for (auto i = intersections.begin(); i != intersections.end(); i++, i++)
        tuples.append(std::make_pair(*i - dilationAmount, *(i + 1) + dilationAmount));
    std::sort(tuples.begin(), tuples.end(), &compareTuples);

    // Step 2: Deal with intersecting ranges.
    Vector<std::pair<float, float>> intermediateTuples;
    if (tuples.size() >= 2) {
        intermediateTuples.append(*tuples.begin());
        for (auto i = tuples.begin() + 1; i != tuples.end(); i++) {
            float& firstEnd = intermediateTuples.last().second;
            float secondStart = i->first;
            float secondEnd = i->second;
            if (secondStart <= firstEnd && secondEnd <= firstEnd) {
                // Ignore this range completely
            } else if (secondStart <= firstEnd)
                firstEnd = secondEnd;
            else
                intermediateTuples.append(*i);
        }
    } else
        intermediateTuples = tuples;

    // Step 3: Output the space between the ranges, but only if the space warrants an underline.
    float previous = 0;
    DashArray result;
    for (const auto& tuple : intermediateTuples) {
        if (tuple.first - previous > dilationAmount) {
            result.append(previous);
            result.append(tuple.first);
        }
        previous = tuple.second;
    }
    if (totalWidth - previous > dilationAmount) {
        result.append(previous);
        result.append(totalWidth);
    }
    
    return result;
}

static void drawSkipInkUnderline(TextPainter& textPainter, GraphicsContext& context, FloatPoint localOrigin, float underlineOffset, float width, bool isPrinting, bool doubleLines)
{
    FloatPoint adjustedLocalOrigin = localOrigin;
    adjustedLocalOrigin.move(0, underlineOffset);
    FloatRect underlineBoundingBox = context.computeLineBoundsForText(adjustedLocalOrigin, width, isPrinting);
    DashArray intersections = textPainter.dashesForIntersectionsWithRect(underlineBoundingBox);
    DashArray a = translateIntersectionPointsToSkipInkBoundaries(intersections, underlineBoundingBox.height(), width);

    ASSERT(!(a.size() % 2));
    context.drawLinesForText(adjustedLocalOrigin, a, isPrinting, doubleLines);
}
#endif

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
    return toRenderBoxModelObject(renderer().parent())->baselinePosition(baselineType, isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineTextBox::lineHeight() const
{
    if (!renderer().parent())
        return 0;
    if (&parent()->renderer() == renderer().parent())
        return parent()->lineHeight();
    return toRenderBoxModelObject(renderer().parent())->lineHeight(isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
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

static void adjustCharactersAndLengthForHyphen(BufferForAppendingHyphen& charactersWithHyphen, const RenderStyle& style, String& string, int& length)
{
    const AtomicString& hyphenString = style.hyphenString();
    charactersWithHyphen.reserveCapacity(length + hyphenString.length());
    charactersWithHyphen.append(string);
    charactersWithHyphen.append(hyphenString);
    string = charactersWithHyphen.toString();
    length += hyphenString.length();
}

static const Font& fontToUse(const RenderStyle& style, const RenderText& renderer)
{
    if (style.hasTextCombine() && renderer.isCombineText()) {
        const RenderCombineText& textCombineRenderer = toRenderCombineText(renderer);
        if (textCombineRenderer.isCombined())
            return textCombineRenderer.textCombineFont();
    }
    return style.font();
}

LayoutRect InlineTextBox::localSelectionRect(int startPos, int endPos) const
{
    int sPos = std::max(startPos - m_start, 0);
    int ePos = std::min(endPos - m_start, (int)m_len);
    
    if (sPos > ePos)
        return LayoutRect();

    FontCachePurgePreventer fontCachePurgePreventer;

    LayoutUnit selTop = selectionTop();
    LayoutUnit selHeight = selectionHeight();
    const RenderStyle& lineStyle = this->lineStyle();
    const Font& font = fontToUse(lineStyle, renderer());

    BufferForAppendingHyphen charactersWithHyphen;
    bool respectHyphen = ePos == m_len && hasHyphen();
    TextRun textRun = constructTextRun(lineStyle, font, respectHyphen ? &charactersWithHyphen : 0);
    if (respectHyphen)
        endPos = textRun.length();

    FloatPoint startingPoint = FloatPoint(logicalLeft(), selTop);
    LayoutRect r;
    if (sPos || ePos != static_cast<int>(m_len))
        r = enclosingIntRect(font.selectionRectForText(textRun, startingPoint, selHeight, sPos, ePos));
    else // Avoid computing the font width when the entire line box is selected as an optimization.
        r = enclosingIntRect(FloatRect(startingPoint, FloatSize(m_logicalWidth, selHeight)));

    LayoutUnit logicalWidth = r.width();
    if (r.x() > logicalRight())
        logicalWidth  = 0;
    else if (r.maxX() > logicalRight())
        logicalWidth = logicalRight() - r.x();

    LayoutPoint topPoint = isHorizontal() ? LayoutPoint(r.x(), selTop) : LayoutPoint(selTop, r.x());
    LayoutUnit width = isHorizontal() ? logicalWidth : selHeight;
    LayoutUnit height = isHorizontal() ? selHeight : logicalWidth;

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

bool InlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
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

FloatSize InlineTextBox::applyShadowToGraphicsContext(GraphicsContext* context, const ShadowData* shadow, const FloatRect& textRect, bool stroked, bool opaque, bool horizontal)
{
    if (!shadow)
        return FloatSize();

    FloatSize extraOffset;
    int shadowX = horizontal ? shadow->x() : shadow->y();
    int shadowY = horizontal ? shadow->y() : -shadow->x();
    FloatSize shadowOffset(shadowX, shadowY);
    int shadowRadius = shadow->radius();
    const Color& shadowColor = shadow->color();

    if (shadow->next() || stroked || !opaque) {
        FloatRect shadowRect(textRect);
        shadowRect.inflate(shadow->paintingExtent());
        shadowRect.move(shadowOffset);
        context->save();
        context->clip(shadowRect);

        extraOffset = FloatSize(0, 2 * textRect.height() + std::max(0.0f, shadowOffset.height()) + shadowRadius);
        shadowOffset -= extraOffset;
    }

    context->setShadow(shadowOffset, shadowRadius, shadowColor, context->fillColorSpace());
    return extraOffset;
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

    if (!containingBlock->parent()->isRubyRun())
        return true; // Cannot get the ruby text.

    RenderRubyText* rubyText = toRenderRubyRun(containingBlock->parent())->rubyText();

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
    
    LayoutPoint adjustedPaintOffset = roundedIntPoint(paintOffset);
    
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
            adjustedPaintOffset.move(isHorizontal() ? truncationOffset : truncationOffset.transposedSize());
        }
    }

    GraphicsContext* context = paintInfo.context;

    const RenderStyle& lineStyle = this->lineStyle();
    
    adjustedPaintOffset.move(0, lineStyle.isHorizontalWritingMode() ? 0 : -logicalHeight());

    FloatPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.move(adjustedPaintOffset.x(), adjustedPaintOffset.y());
    FloatRect boxRect(boxOrigin, FloatSize(logicalWidth(), logicalHeight()));

    RenderCombineText* combinedText = lineStyle.hasTextCombine() && renderer().isCombineText() && toRenderCombineText(renderer()).isCombined() ? &toRenderCombineText(renderer()) : 0;

    bool shouldRotate = !isHorizontal() && !combinedText;
    if (shouldRotate)
        context->concatCTM(rotation(boxRect, Clockwise));

    // Determine whether or not we have composition underlines to draw.
    bool containsComposition = renderer().textNode() && renderer().frame().editor().compositionNode() == renderer().textNode();
    bool useCustomUnderlines = containsComposition && renderer().frame().editor().compositionUsesCustomUnderlines();

    // Determine the text colors and selection colors.
    TextPaintStyle textPaintStyle = computeTextPaintStyle(renderer(), lineStyle, paintInfo);

    bool paintSelectedTextOnly;
    bool paintSelectedTextSeparately;
    const ShadowData* selectionShadow;
    TextPaintStyle selectionPaintStyle = computeTextSelectionPaintStyle(textPaintStyle, renderer(), lineStyle, paintInfo, paintSelectedTextOnly, paintSelectedTextSeparately, selectionShadow);

    // Set our font.
    const Font& font = fontToUse(lineStyle, renderer());

    FloatPoint textOrigin = FloatPoint(boxOrigin.x(), boxOrigin.y() + font.fontMetrics().ascent());

    if (combinedText)
        combinedText->adjustTextOrigin(textOrigin, boxRect);

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

    BufferForAppendingHyphen charactersWithHyphen;
    TextRun textRun = constructTextRun(lineStyle, font, string, maximumLength, hasHyphen() ? &charactersWithHyphen : 0);
    if (hasHyphen())
        length = textRun.length();

    int sPos = 0;
    int ePos = 0;
    if (haveSelection && (paintSelectedTextOnly || paintSelectedTextSeparately))
        selectionStartEnd(sPos, ePos);

    if (m_truncation != cNoTruncation) {
        sPos = std::min<int>(sPos, m_truncation);
        ePos = std::min<int>(ePos, m_truncation);
        length = m_truncation;
    }

    int emphasisMarkOffset = 0;
    bool emphasisMarkAbove;
    bool hasTextEmphasis = emphasisMarkExistsAndIsAbove(lineStyle, emphasisMarkAbove);
    const AtomicString& emphasisMark = hasTextEmphasis ? lineStyle.textEmphasisMarkString() : nullAtom;
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = emphasisMarkAbove ? -font.fontMetrics().ascent() - font.emphasisMarkDescent(emphasisMark) : font.fontMetrics().descent() + font.emphasisMarkAscent(emphasisMark);

    const ShadowData* textShadow = paintInfo.forceBlackText() ? 0 : lineStyle.textShadow();

    TextPainter textPainter(*context, paintSelectedTextOnly, paintSelectedTextSeparately, font, sPos, ePos, length, emphasisMark, combinedText, textRun, boxRect, textOrigin, emphasisMarkOffset, textShadow, selectionShadow, isHorizontal(), textPaintStyle, selectionPaintStyle);
    textPainter.paintText();

    // Paint decorations
    TextDecoration textDecorations = lineStyle.textDecorationsInEffect();
    if (textDecorations != TextDecorationNone && paintInfo.phase != PaintPhaseSelection) {
        updateGraphicsContext(*context, textPaintStyle);
        if (combinedText)
            context->concatCTM(rotation(boxRect, Clockwise));
        paintDecoration(*context, boxOrigin, textDecorations, lineStyle.textDecorationStyle(), textShadow, textPainter);
        if (combinedText)
            context->concatCTM(rotation(boxRect, Counterclockwise));
    }

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
        context->concatCTM(rotation(boxRect, Counterclockwise));
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

void alignSelectionRectToDevicePixels(FloatRect& rect)
{
    float maxX = floorf(rect.maxX());
    rect.setX(floorf(rect.x()));
    rect.setWidth(roundf(maxX - rect.x()));
}

void InlineTextBox::paintSelection(GraphicsContext* context, const FloatPoint& boxOrigin, const RenderStyle& style, const Font& font, Color textColor)
{
#if ENABLE(TEXT_SELECTION)
    if (context->paintingDisabled())
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

    GraphicsContextStateSaver stateSaver(*context);
    updateGraphicsContext(*context, TextPaintStyle(c, style.colorSpace())); // Don't draw text at all!
    
    // If the text is truncated, let the thing being painted in the truncation
    // draw its own highlight.
    int length = m_truncation != cNoTruncation ? m_truncation : m_len;
    String string = renderer().text();

    if (string.length() != static_cast<unsigned>(length) || m_start) {
        ASSERT_WITH_SECURITY_IMPLICATION(static_cast<unsigned>(m_start + length) <= string.length());
        string = string.substringSharingImpl(m_start, length);
    }

    BufferForAppendingHyphen charactersWithHyphen;
    bool respectHyphen = ePos == length && hasHyphen();
    TextRun textRun = constructTextRun(style, font, string, renderer().textLength() - m_start, respectHyphen ? &charactersWithHyphen : 0);
    if (respectHyphen)
        ePos = textRun.length();

    const RootInlineBox& rootBox = root();
    LayoutUnit selectionBottom = rootBox.selectionBottom();
    LayoutUnit selectionTop = rootBox.selectionTopAdjustedForPrecedingBlock();

    int deltaY = roundToInt(renderer().style().isFlippedLinesWritingMode() ? selectionBottom - logicalBottom() : logicalTop() - selectionTop);
    int selHeight = std::max(0, roundToInt(selectionBottom - selectionTop));

    FloatPoint localOrigin(boxOrigin.x(), boxOrigin.y() - deltaY);
    FloatRect clipRect(localOrigin, FloatSize(m_logicalWidth, selHeight));
    alignSelectionRectToDevicePixels(clipRect);

    context->clip(clipRect);

    context->drawHighlightForText(font, textRun, localOrigin, selHeight, c, style.colorSpace(), sPos, ePos);
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(boxOrigin);
    UNUSED_PARAM(style);
    UNUSED_PARAM(font);
    UNUSED_PARAM(textColor);
#endif
}

void InlineTextBox::paintCompositionBackground(GraphicsContext* context, const FloatPoint& boxOrigin, const RenderStyle& style, const Font& font, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = std::max(startPos - offset, 0);
    int ePos = std::min(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    GraphicsContextStateSaver stateSaver(*context);

#if !PLATFORM(IOS)
    // FIXME: Is this color still applicable as of Mavericks? for non-Mac ports? We should
    // be able to move this color information to RenderStyle.
    Color c = Color(225, 221, 85);
#else
    Color c = style.compositionFillColor();
#endif
    
    updateGraphicsContext(*context, TextPaintStyle(c, style.colorSpace())); // Don't draw text at all!

    int deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
    int selHeight = selectionHeight();
    FloatPoint localOrigin(boxOrigin.x(), boxOrigin.y() - deltaY);
    context->drawHighlightForText(font, constructTextRun(style, font), localOrigin, selHeight, c, style.colorSpace(), sPos, ePos);
}

static StrokeStyle textDecorationStyleToStrokeStyle(TextDecorationStyle decorationStyle)
{
    StrokeStyle strokeStyle = SolidStroke;
    switch (decorationStyle) {
    case TextDecorationStyleSolid:
        strokeStyle = SolidStroke;
        break;
    case TextDecorationStyleDouble:
        strokeStyle = DoubleStroke;
        break;
    case TextDecorationStyleDotted:
        strokeStyle = DottedStroke;
        break;
    case TextDecorationStyleDashed:
        strokeStyle = DashedStroke;
        break;
    case TextDecorationStyleWavy:
        strokeStyle = WavyStroke;
        break;
    }

    return strokeStyle;
}

static int computeUnderlineOffset(const TextUnderlinePosition underlinePosition, const FontMetrics& fontMetrics, const InlineTextBox* inlineTextBox, const int textDecorationThickness)
{
    // Compute the gap between the font and the underline. Use at least one
    // pixel gap, if underline is thick then use a bigger gap.
    const int gap = std::max<int>(1, ceilf(textDecorationThickness / 2.0));

    // According to the specification TextUnderlinePositionAuto should default to 'alphabetic' for horizontal text
    // and to 'under Left' for vertical text (e.g. japanese). We support only horizontal text for now.
    switch (underlinePosition) {
    case TextUnderlinePositionAlphabetic:
    case TextUnderlinePositionAuto:
        return fontMetrics.ascent() + gap; // Position underline near the alphabetic baseline.
    case TextUnderlinePositionUnder: {
        // Position underline relative to the under edge of the lowest element's content box.
        const float offset = inlineTextBox->root().maxLogicalTop() - inlineTextBox->logicalTop();
        return inlineTextBox->logicalHeight() + gap + std::max<float>(offset, 0);
    }
    }

    ASSERT_NOT_REACHED();
    return fontMetrics.ascent() + gap;
}

static void adjustStepToDecorationLength(float& step, float& controlPointDistance, float length)
{
    ASSERT(step > 0);

    if (length <= 0)
        return;

    unsigned stepCount = static_cast<unsigned>(length / step);

    // Each Bezier curve starts at the same pixel that the previous one
    // ended. We need to subtract (stepCount - 1) pixels when calculating the
    // length covered to account for that.
    float uncoveredLength = length - (stepCount * step - (stepCount - 1));
    float adjustment = uncoveredLength / stepCount;
    step += adjustment;
    controlPointDistance += adjustment;
}

static void getWavyStrokeParameters(float strokeThickness, float& controlPointDistance, float& step)
{
    // Distance between decoration's axis and Bezier curve's control points.
    // The height of the curve is based on this distance. Use a minimum of 6 pixels distance since
    // the actual curve passes approximately at half of that distance, that is 3 pixels.
    // The minimum height of the curve is also approximately 3 pixels. Increases the curve's height
    // as strockThickness increases to make the curve looks better.
    controlPointDistance = 3 * std::max<float>(2, strokeThickness);

    // Increment used to form the diamond shape between start point (p1), control
    // points and end point (p2) along the axis of the decoration. Makes the
    // curve wider as strockThickness increases to make the curve looks better.
    step = 2 * std::max<float>(2, strokeThickness);
}

/*
 * Draw one cubic Bezier curve and repeat the same pattern long the the decoration's axis.
 * The start point (p1), controlPoint1, controlPoint2 and end point (p2) of the Bezier curve
 * form a diamond shape:
 *
 *                              step
 *                         |-----------|
 *
 *                   controlPoint1
 *                         +
 *
 *
 *                  . .
 *                .     .
 *              .         .
 * (x1, y1) p1 +           .            + p2 (x2, y2) - <--- Decoration's axis
 *                          .         .               |
 *                            .     .                 |
 *                              . .                   | controlPointDistance
 *                                                    |
 *                                                    |
 *                         +                          -
 *                   controlPoint2
 *
 *             |-----------|
 *                 step
 */
static void strokeWavyTextDecoration(GraphicsContext& context, FloatPoint& p1, FloatPoint& p2, float strokeThickness)
{
    context.adjustLineToPixelBoundaries(p1, p2, strokeThickness, context.strokeStyle());

    Path path;
    path.moveTo(p1);

    float controlPointDistance;
    float step;
    getWavyStrokeParameters(strokeThickness, controlPointDistance, step);

    bool isVerticalLine = (p1.x() == p2.x());

    if (isVerticalLine) {
        ASSERT(p1.x() == p2.x());

        float xAxis = p1.x();
        float y1;
        float y2;

        if (p1.y() < p2.y()) {
            y1 = p1.y();
            y2 = p2.y();
        } else {
            y1 = p2.y();
            y2 = p1.y();
        }

        adjustStepToDecorationLength(step, controlPointDistance, y2 - y1);
        FloatPoint controlPoint1(xAxis + controlPointDistance, 0);
        FloatPoint controlPoint2(xAxis - controlPointDistance, 0);

        for (float y = y1; y + 2 * step <= y2;) {
            controlPoint1.setY(y + step);
            controlPoint2.setY(y + step);
            y += 2 * step;
            path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(xAxis, y));
        }
    } else {
        ASSERT(p1.y() == p2.y());

        float yAxis = p1.y();
        float x1;
        float x2;

        if (p1.x() < p2.x()) {
            x1 = p1.x();
            x2 = p2.x();
        } else {
            x1 = p2.x();
            x2 = p1.x();
        }

        adjustStepToDecorationLength(step, controlPointDistance, x2 - x1);
        FloatPoint controlPoint1(0, yAxis + controlPointDistance);
        FloatPoint controlPoint2(0, yAxis - controlPointDistance);

        for (float x = x1; x + 2 * step <= x2;) {
            controlPoint1.setX(x + step);
            controlPoint2.setX(x + step);
            x += 2 * step;
            path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(x, yAxis));
        }
    }

    context.setShouldAntialias(true);
    context.strokePath(path);
}

void InlineTextBox::paintDecoration(GraphicsContext& context, const FloatPoint& boxOrigin, TextDecoration decoration, TextDecorationStyle decorationStyle, const ShadowData* shadow, TextPainter& textPainter)
{
#if !ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
    UNUSED_PARAM(textPainter);
#endif

    // FIXME: We should improve this rule and not always just assume 1.
    const float textDecorationThickness = 1.f;

    if (m_truncation == cFullTruncation)
        return;

    FloatPoint localOrigin = boxOrigin;

    float width = m_logicalWidth;
    if (m_truncation != cNoTruncation) {
        width = renderer().width(m_start, m_truncation, textPos(), isFirstLine());
        if (!isLeftToRightDirection())
            localOrigin.move(m_logicalWidth - width, 0);
    }
    
    // Get the text decoration colors.
    Color underline, overline, linethrough;
    renderer().getTextDecorationColors(decoration, underline, overline, linethrough, true);
    if (isFirstLine())
        renderer().getTextDecorationColors(decoration, underline, overline, linethrough, true, true);
    
    // Use a special function for underlines to get the positioning exactly right.
    bool isPrinting = renderer().document().printing();

    const float textDecorationBaseFontSize = 16;
    float fontSizeScaling = renderer().style().fontSize() / textDecorationBaseFontSize;
    float strokeThickness = roundf(textDecorationThickness * fontSizeScaling);
    context.setStrokeThickness(strokeThickness);

    bool linesAreOpaque = !isPrinting && (!(decoration & TextDecorationUnderline) || underline.alpha() == 255) && (!(decoration & TextDecorationOverline) || overline.alpha() == 255) && (!(decoration & TextDecorationLineThrough) || linethrough.alpha() == 255);

    const RenderStyle& lineStyle = this->lineStyle();
    int baseline = lineStyle.fontMetrics().ascent();

    bool setClip = false;
    int extraOffset = 0;
    if (!linesAreOpaque && shadow && shadow->next()) {
        FloatRect clipRect(localOrigin, FloatSize(width, baseline + 2));
        for (const ShadowData* s = shadow; s; s = s->next()) {
            int shadowExtent = s->paintingExtent();
            FloatRect shadowRect(localOrigin, FloatSize(width, baseline + 2));
            shadowRect.inflate(shadowExtent);
            int shadowX = isHorizontal() ? s->x() : s->y();
            int shadowY = isHorizontal() ? s->y() : -s->x();
            shadowRect.move(shadowX, shadowY);
            clipRect.unite(shadowRect);
            extraOffset = std::max(extraOffset, std::max(0, shadowY) + shadowExtent);
        }
        context.save();
        context.clip(clipRect);
        extraOffset += baseline + 2;
        localOrigin.move(0, extraOffset);
        setClip = true;
    }

    ColorSpace colorSpace = renderer().style().colorSpace();
    bool setShadow = false;

    do {
        if (shadow) {
            if (!shadow->next()) {
                // The last set of lines paints normally inside the clip.
                localOrigin.move(0, -extraOffset);
                extraOffset = 0;
            }
            int shadowX = isHorizontal() ? shadow->x() : shadow->y();
            int shadowY = isHorizontal() ? shadow->y() : -shadow->x();
            context.setShadow(FloatSize(shadowX, shadowY - extraOffset), shadow->radius(), shadow->color(), colorSpace);
            setShadow = true;
            shadow = shadow->next();
        }
        
        float wavyOffset = 2.f;

        context.setStrokeStyle(textDecorationStyleToStrokeStyle(decorationStyle));
        if (decoration & TextDecorationUnderline) {
            context.setStrokeColor(underline, colorSpace);
            TextUnderlinePosition underlinePosition = lineStyle.textUnderlinePosition();
            const int underlineOffset = computeUnderlineOffset(underlinePosition, lineStyle.fontMetrics(), this, textDecorationThickness);

            switch (decorationStyle) {
            case TextDecorationStyleWavy: {
                FloatPoint start(localOrigin.x(), localOrigin.y() + underlineOffset + wavyOffset);
                FloatPoint end(localOrigin.x() + width, localOrigin.y() + underlineOffset + wavyOffset);
                strokeWavyTextDecoration(context, start, end, textDecorationThickness);
                break;
            }
            default:
#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
                if ((lineStyle.textDecorationSkip() == TextDecorationSkipInk || lineStyle.textDecorationSkip() == TextDecorationSkipAuto) && isHorizontal()) {
                    if (!context.paintingDisabled()) {
                        drawSkipInkUnderline(textPainter, context, localOrigin, underlineOffset, width, isPrinting, decorationStyle == TextDecorationStyleDouble);
                    }
                } else
                    // FIXME: Need to support text-decoration-skip: none.
#endif // CSS3_TEXT_DECORATION_SKIP_INK
                    context.drawLineForText(FloatPoint(localOrigin.x(), localOrigin.y() + underlineOffset), width, isPrinting, decorationStyle == TextDecorationStyleDouble);
            }
        }
        if (decoration & TextDecorationOverline) {
            context.setStrokeColor(overline, colorSpace);
            switch (decorationStyle) {
            case TextDecorationStyleWavy: {
                FloatPoint start(localOrigin.x(), localOrigin.y() - wavyOffset);
                FloatPoint end(localOrigin.x() + width, localOrigin.y() - wavyOffset);
                strokeWavyTextDecoration(context, start, end, textDecorationThickness);
                break;
            }
            default:
#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
                if ((lineStyle.textDecorationSkip() == TextDecorationSkipInk || lineStyle.textDecorationSkip() == TextDecorationSkipAuto) && isHorizontal()) {
                    if (!context.paintingDisabled())
                        drawSkipInkUnderline(textPainter, context, localOrigin, 0, width, isPrinting, decorationStyle == TextDecorationStyleDouble);
                } else
                    // FIXME: Need to support text-decoration-skip: none.
#endif // CSS3_TEXT_DECORATION_SKIP_INK
                    context.drawLineForText(localOrigin, width, isPrinting, decorationStyle == TextDecorationStyleDouble);
            }
        }
        if (decoration & TextDecorationLineThrough) {
            context.setStrokeColor(linethrough, colorSpace);
            switch (decorationStyle) {
            case TextDecorationStyleWavy: {
                FloatPoint start(localOrigin.x(), localOrigin.y() + 2 * baseline / 3);
                FloatPoint end(localOrigin.x() + width, localOrigin.y() + 2 * baseline / 3);
                strokeWavyTextDecoration(context, start, end, textDecorationThickness);
                break;
            }
            default:
                context.drawLineForText(FloatPoint(localOrigin.x(), localOrigin.y() + 2 * baseline / 3), width, isPrinting, decorationStyle == TextDecorationStyleDouble);
            }
        }
    } while (shadow);

    if (setClip)
        context.restore();
    else if (setShadow)
        context.clearShadow();
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

void InlineTextBox::paintDocumentMarker(GraphicsContext* pt, const FloatPoint& boxOrigin, DocumentMarker* marker, const RenderStyle& style, const Font& font, bool grammar)
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
    if (m_start <= (int)marker->startOffset())
        markerSpansWholeBox = false;
    if ((end() + 1) != marker->endOffset()) // end points at the last char, not past it
        markerSpansWholeBox = false;
    if (m_truncation != cNoTruncation)
        markerSpansWholeBox = false;

    bool isDictationMarker = marker->type() == DocumentMarker::DictationAlternatives;
    if (!markerSpansWholeBox || grammar || isDictationMarker) {
        int startPosition = std::max<int>(marker->startOffset() - m_start, 0);
        int endPosition = std::min<int>(marker->endOffset() - m_start, m_len);
        
        if (m_truncation != cNoTruncation)
            endPosition = std::min<int>(endPosition, m_truncation);

        // Calculate start & width
        int deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
        int selHeight = selectionHeight();
        FloatPoint startPoint(boxOrigin.x(), boxOrigin.y() - deltaY);
        TextRun run = constructTextRun(style, font);

        // FIXME: Convert the document markers to float rects.
        IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, selHeight, startPosition, endPosition));
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();
        
        // Store rendered rects for bad grammar markers, so we can hit-test against it elsewhere in order to
        // display a toolTip. We don't do this for misspelling markers.
        if (grammar || isDictationMarker) {
            markerRect.move(-boxOrigin.x(), -boxOrigin.y());
            markerRect = renderer().localToAbsoluteQuad(FloatRect(markerRect)).enclosingBoundingBox();
            toRenderedDocumentMarker(marker)->setRenderedRect(markerRect);
        }
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
    pt->drawLineForDocumentMarker(FloatPoint(boxOrigin.x() + start, boxOrigin.y() + underlineOffset), width, lineStyleForMarkerType(marker->type()));
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext* pt, const FloatPoint& boxOrigin, DocumentMarker* marker, const RenderStyle& style, const Font& font)
{
    // Use same y positioning and height as for selection, so that when the selection and this highlight are on
    // the same word there are no pieces sticking out.
    int deltaY = renderer().style().isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
    int selHeight = selectionHeight();

    int sPos = std::max(marker->startOffset() - m_start, (unsigned)0);
    int ePos = std::min(marker->endOffset() - m_start, (unsigned)m_len);
    TextRun run = constructTextRun(style, font);

    // Always compute and store the rect associated with this marker. The computed rect is in absolute coordinates.
    IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, IntPoint(x(), selectionTop()), selHeight, sPos, ePos));
    markerRect = renderer().localToAbsoluteQuad(FloatRect(markerRect)).enclosingBoundingBox();
    toRenderedDocumentMarker(marker)->setRenderedRect(markerRect);
    
    // Optionally highlight the text
    if (renderer().frame().editor().markedTextMatchesAreHighlighted()) {
        Color color = marker->activeMatch() ? renderer().theme().platformActiveTextSearchHighlightColor() : renderer().theme().platformInactiveTextSearchHighlightColor();
        GraphicsContextStateSaver stateSaver(*pt);
        updateGraphicsContext(*pt, TextPaintStyle(color, style.colorSpace())); // Don't draw text at all!
        pt->clip(FloatRect(boxOrigin.x(), boxOrigin.y() - deltaY, m_logicalWidth, selHeight));
        pt->drawHighlightForText(font, run, FloatPoint(boxOrigin.x(), boxOrigin.y() - deltaY), selHeight, color, style.colorSpace(), sPos, ePos);
    }
}

void InlineTextBox::computeRectForReplacementMarker(DocumentMarker* marker, const RenderStyle& style, const Font& font)
{
    // Replacement markers are not actually drawn, but their rects need to be computed for hit testing.
    int top = selectionTop();
    int h = selectionHeight();
    
    int sPos = std::max(marker->startOffset() - m_start, (unsigned)0);
    int ePos = std::min(marker->endOffset() - m_start, (unsigned)m_len);
    TextRun run = constructTextRun(style, font);
    IntPoint startPoint = IntPoint(x(), top);
    
    // Compute and store the rect associated with this marker.
    IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, h, sPos, ePos));
    markerRect = renderer().localToAbsoluteQuad(FloatRect(markerRect)).enclosingBoundingBox();
    toRenderedDocumentMarker(marker)->setRenderedRect(markerRect);
}
    
void InlineTextBox::paintDocumentMarkers(GraphicsContext* pt, const FloatPoint& boxOrigin, const RenderStyle& style, const Font& font, bool background)
{
    if (!renderer().textNode())
        return;

    Vector<DocumentMarker*> markers = renderer().document().markers().markersFor(renderer().textNode());
    Vector<DocumentMarker*>::const_iterator markerIt = markers.begin();

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for ( ; markerIt != markers.end(); ++markerIt) {
        DocumentMarker* marker = *markerIt;
        
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
                paintDocumentMarker(pt, boxOrigin, marker, style, font, false);
                break;
            case DocumentMarker::Grammar:
                paintDocumentMarker(pt, boxOrigin, marker, style, font, true);
                break;
#if PLATFORM(IOS)
            // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS)-guard.
            case DocumentMarker::DictationPhraseWithAlternatives:
                paintDocumentMarker(pt, boxOrigin, marker, style, font, true);
                break;
#endif
            case DocumentMarker::TextMatch:
                paintTextMatchMarker(pt, boxOrigin, marker, style, font);
                break;
            case DocumentMarker::Replacement:
                computeRectForReplacementMarker(marker, style, font);
                break;
            default:
                ASSERT_NOT_REACHED();
        }

    }
}

void InlineTextBox::paintCompositionUnderline(GraphicsContext* ctx, const FloatPoint& boxOrigin, const CompositionUnderline& underline)
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

    ctx->setStrokeColor(underline.color, renderer().style().colorSpace());
    ctx->setStrokeThickness(lineThickness);
    ctx->drawLineForText(FloatPoint(boxOrigin.x() + start, boxOrigin.y() + logicalHeight() - lineThickness), width, renderer().document().printing());
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

    FontCachePurgePreventer fontCachePurgePreventer;

    const RenderStyle& lineStyle = this->lineStyle();
    const Font& font = fontToUse(lineStyle, renderer());
    return font.offsetForPosition(constructTextRun(lineStyle, font), lineOffset - logicalLeft(), includePartialGlyphs);
}

float InlineTextBox::positionForOffset(int offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + m_len);

    if (isLineBreak())
        return logicalLeft();

    FontCachePurgePreventer fontCachePurgePreventer;

    const RenderStyle& lineStyle = this->lineStyle();
    const Font& font = fontToUse(lineStyle, renderer());
    int from = !isLeftToRightDirection() ? offset - m_start : 0;
    int to = !isLeftToRightDirection() ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return font.selectionRectForText(constructTextRun(lineStyle, font), IntPoint(logicalLeft(), 0), 0, from, to).maxX();
}

TextRun InlineTextBox::constructTextRun(const RenderStyle& style, const Font& font, BufferForAppendingHyphen* charactersWithHyphen) const
{
    ASSERT(renderer().text());

    String string = renderer().text();
    unsigned startPos = start();
    unsigned length = len();

    if (string.length() != length || startPos)
        string = string.substringSharingImpl(startPos, length);

    return constructTextRun(style, font, string, renderer().textLength() - startPos, charactersWithHyphen);
}

TextRun InlineTextBox::constructTextRun(const RenderStyle& style, const Font& font, String string, int maximumLength, BufferForAppendingHyphen* charactersWithHyphen) const
{
    int length = string.length();

    if (charactersWithHyphen) {
        adjustCharactersAndLengthForHyphen(*charactersWithHyphen, style, string, length);
        maximumLength = length;
    }

    ASSERT(maximumLength >= length);

    TextRun run(string, textPos(), expansion(), expansionBehavior(), direction(), dirOverride() || style.rtlOrdering() == VisualOrder, !renderer().canUseSimpleFontCodePath());
    run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    if (font.isSVGFont())
        run.setRenderingContext(SVGTextRunRenderingContext::create(renderer()));

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(maximumLength);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

#ifndef NDEBUG

const char* InlineTextBox::boxName() const
{
    return "InlineTextBox";
}

void InlineTextBox::showBox(int printedCharacters) const
{
    String value = renderer().text();
    value = value.substring(start(), len());
    value.replaceWithLiteral('\\', "\\\\");
    value.replaceWithLiteral('\n', "\\n");
    printedCharacters += fprintf(stderr, "%s\t%p", boxName(), this);
    for (; printedCharacters < showTreeCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    printedCharacters = fprintf(stderr, "\t%s %p", renderer().renderName(), &renderer());
    const int rendererCharacterOffset = 24;
    for (; printedCharacters < rendererCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    fprintf(stderr, "(%d,%d) \"%s\"\n", start(), start() + len(), value.utf8().data());
}

#endif

} // namespace WebCore
