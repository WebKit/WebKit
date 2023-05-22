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
#include "LegacyInlineTextBox.h"

#include "BreakLines.h"
#include "CompositionHighlight.h"
#include "DashArray.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "ElementRuleCollector.h"
#include "EventRegion.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "HighlightData.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorTextBox.h"
#include "InlineIteratorTextBoxInlines.h"
#include "InlineTextBoxStyle.h"
#include "LegacyEllipsisBox.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBlock.h"
#include "RenderCombineText.h"
#include "RenderElementInlines.h"
#include "RenderLineBreak.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"
#include "RenderStyleInlines.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "Settings.h"
#include "StyledMarkedText.h"
#include "Text.h"
#include "TextBoxPainter.h"
#include "TextBoxSelectableRange.h"
#include "TextDecorationPainter.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"
#include <stdio.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyInlineTextBox);

struct SameSizeAsLegacyInlineTextBox : public LegacyInlineBox {
    void* pointers[2];
    unsigned variables[2];
    unsigned short variables2;
};

static_assert(sizeof(LegacyInlineTextBox) == sizeof(SameSizeAsLegacyInlineTextBox), "LegacyInlineTextBox should stay small");

typedef HashMap<const LegacyInlineTextBox*, LayoutRect> LegacyInlineTextBoxOverflowMap;
static LegacyInlineTextBoxOverflowMap* gTextBoxesWithOverflow;

LegacyInlineTextBox::~LegacyInlineTextBox()
{
    if (!knownToHaveNoOverflow() && gTextBoxesWithOverflow)
        gTextBoxesWithOverflow->remove(this);
    TextPainter::removeGlyphDisplayList(*this);
}

bool LegacyInlineTextBox::hasTextContent() const
{
    if (m_len > 1)
        return true;
    if (auto* combinedText = this->combinedText()) {
        ASSERT(m_len == 1);
        return !combinedText->combinedStringForRendering().isEmpty();
    }
    return m_len;
}

void LegacyInlineTextBox::markDirty(bool dirty)
{
    if (dirty) {
        m_len = 0;
        m_start = 0;
    }
    LegacyInlineBox::markDirty(dirty);
}

LayoutRect LegacyInlineTextBox::logicalOverflowRect() const
{
    if (knownToHaveNoOverflow() || !gTextBoxesWithOverflow)
        return enclosingIntRect(logicalFrameRect());
    return gTextBoxesWithOverflow->get(this);
}

void LegacyInlineTextBox::setLogicalOverflowRect(const LayoutRect& rect)
{
    ASSERT(!knownToHaveNoOverflow());
    if (!gTextBoxesWithOverflow)
        gTextBoxesWithOverflow = new LegacyInlineTextBoxOverflowMap;
    gTextBoxesWithOverflow->add(this, rect);
}

LayoutUnit LegacyInlineTextBox::baselinePosition(FontBaseline baselineType) const
{
    if (!parent())
        return 0;
    if (&parent()->renderer() == renderer().parent())
        return parent()->baselinePosition(baselineType);
    return downcast<RenderBoxModelObject>(*renderer().parent()).baselinePosition(baselineType, isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit LegacyInlineTextBox::lineHeight() const
{
    if (!renderer().parent())
        return 0;
    if (&parent()->renderer() == renderer().parent())
        return parent()->lineHeight();
    return downcast<RenderBoxModelObject>(*renderer().parent()).lineHeight(isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit LegacyInlineTextBox::selectionTop() const
{
    return root().selectionTop();
}

LayoutUnit LegacyInlineTextBox::selectionBottom() const
{
    return root().selectionBottom();
}

LayoutUnit LegacyInlineTextBox::selectionHeight() const
{
    return root().selectionHeight();
}

RenderObject::HighlightState LegacyInlineTextBox::selectionState() const
{
    return renderer().view().selection().highlightStateForTextBox(renderer(), selectableRange());
}

const FontCascade& LegacyInlineTextBox::lineFont() const
{
    return combinedText() ? combinedText()->textCombineFont() : lineStyle().fontCascade();
}

LayoutRect snappedSelectionRect(const LayoutRect& selectionRect, float logicalRight, float selectionTop, float selectionHeight, bool isHorizontal)
{
    auto snappedSelectionRect = enclosingIntRect(selectionRect);
    LayoutUnit logicalWidth = snappedSelectionRect.width();
    if (snappedSelectionRect.x() > logicalRight)
        logicalWidth = 0;
    else if (snappedSelectionRect.maxX() > logicalRight)
        logicalWidth = logicalRight - snappedSelectionRect.x();

    LayoutPoint topPoint;
    LayoutUnit width;
    LayoutUnit height;
    if (isHorizontal) {
        topPoint = LayoutPoint { snappedSelectionRect.x(), selectionTop };
        width = logicalWidth;
        height = selectionHeight;
    } else {
        topPoint = LayoutPoint { selectionTop, snappedSelectionRect.x() };
        width = selectionHeight;
        height = logicalWidth;
    }
    return LayoutRect { topPoint, LayoutSize { width, height } };
}

LayoutRect LegacyInlineTextBox::localSelectionRect(unsigned startPos, unsigned endPos) const
{
    auto [clampedStart, clampedEnd] = selectableRange().clamp(startPos, endPos);

    if (clampedStart >= clampedEnd && !(startPos == endPos && startPos >= start() && startPos <= (start() + len())))
        return { };

    LayoutUnit selectionTop = this->selectionTop();
    LayoutUnit selectionHeight = this->selectionHeight();

    TextRun textRun = createTextRun();

    LayoutRect selectionRect { LayoutUnit(logicalLeft()), selectionTop, LayoutUnit(logicalWidth()), selectionHeight };
    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (clampedStart || clampedEnd != textRun.length())
        lineFont().adjustSelectionRectForText(textRun, selectionRect, clampedStart, clampedEnd);
    // FIXME: The computation of the snapped selection rect differs from the computation of this rect
    // in paintMarkedTextBackground(). See <https://bugs.webkit.org/show_bug.cgi?id=138913>.
    return snappedSelectionRect(selectionRect, logicalRight(), selectionTop, selectionHeight, isHorizontal());
}

void LegacyInlineTextBox::deleteLine()
{
    renderer().removeTextBox(*this);
    delete this;
}

void LegacyInlineTextBox::extractLine()
{
    if (extracted())
        return;

    renderer().extractTextBox(*this);
}

void LegacyInlineTextBox::attachLine()
{
    if (!extracted())
        return;
    
    renderer().attachTextBox(*this);
}

float LegacyInlineTextBox::placeEllipsisBox(bool flowIsLTR, float visibleLeftEdge, float visibleRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox)
{
    if (foundBox) {
        m_truncation = 0;
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
        // Too far. Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
        m_truncation = 0;
        foundBox = true;
        return -1;
    }

    bool ltrEllipsisWithinBox = flowIsLTR && (ellipsisX < right());
    bool rtlEllipsisWithinBox = !flowIsLTR && (ellipsisX > left());
    if (ltrEllipsisWithinBox || rtlEllipsisWithinBox) {
        foundBox = true;

        // The inline box may have different directionality than it's parent. Since truncation
        // behavior depends both on both the parent and the inline block's directionality, we
        // must keep track of these separately.
        bool ltr = isLeftToRightDirection();
        if (ltr != flowIsLTR) {
            // Width in pixels of the visible portion of the box, excluding the ellipsis.
            int visibleBoxWidth = visibleRightEdge - visibleLeftEdge  - ellipsisWidth;
            ellipsisX = ltr ? left() + visibleBoxWidth : right() - visibleBoxWidth;
        }

        auto textBox = InlineIterator::textBoxFor(this);
        auto offset = lineFont().offsetForPosition(textBox->textRun(InlineIterator::TextRunMode::Editing), ellipsisX - textBox->logicalLeftIgnoringInlineDirection(), false);
        if (!offset) {
            // No characters should be rendered. Set ourselves to full truncation and place the ellipsis at the min of our start
            // and the ellipsis edge.
            m_truncation = 0;
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

        return right() - widthOfVisibleText - ellipsisWidth;
    }
    truncatedWidth += logicalWidth();
    return -1;
}

bool LegacyInlineTextBox::isLineBreak() const
{
    return renderer().style().preserveNewline() && len() == 1 && renderer().text()[start()] == '\n';
}

bool LegacyInlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/,
    HitTestAction /*hitTestAction*/)
{
    if (!renderer().parent()->visibleToHitTesting(request))
        return false;

    if (isLineBreak())
        return false;

    if (m_truncation && !*m_truncation)
        return false;

    FloatRect rect(locationIncludingFlipping(), size());
    // Make sure truncated text is ignored while hittesting.
    if (m_truncation) {
        LayoutUnit widthOfVisibleText { renderer().width(m_start, *m_truncation, textPos(), isFirstLine()) };

        if (isHorizontal())
            renderer().style().isLeftToRightDirection() ? rect.setWidth(widthOfVisibleText) : rect.shiftXEdgeTo(right() - widthOfVisibleText);
        else
            rect.setHeight(widthOfVisibleText);
    }

    rect.moveBy(accumulatedOffset);

    if (locationInContainer.intersects(rect)) {
        renderer().updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (result.addNodeToListBasedTestResult(renderer().nodeForHitTest(), request, locationInContainer, rect) == HitTestProgress::Stop)
            return true;
    }
    return false;
}

void LegacyInlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /*lineTop*/, LayoutUnit /*lineBottom*/)
{
    if (isLineBreak() || !paintInfo.shouldPaintWithinRoot(renderer()) || renderer().style().visibility() != Visibility::Visible
        || (m_truncation && !*m_truncation) || paintInfo.phase == PaintPhase::Outline || !hasTextContent())
        return;

    ASSERT(paintInfo.phase != PaintPhase::SelfOutline && paintInfo.phase != PaintPhase::ChildOutlines);

    LayoutUnit logicalLeftSide = logicalLeftVisualOverflow();
    LayoutUnit logicalRightSide = logicalRightVisualOverflow();
    LayoutUnit logicalStart = logicalLeftSide + (isHorizontal() ? paintOffset.x() : paintOffset.y());
    LayoutUnit logicalExtent = logicalRightSide - logicalLeftSide;
    
    LayoutUnit paintEnd = isHorizontal() ? paintInfo.rect.maxX() : paintInfo.rect.maxY();
    LayoutUnit paintStart = isHorizontal() ? paintInfo.rect.x() : paintInfo.rect.y();

    if (logicalStart >= paintEnd || logicalStart + logicalExtent <= paintStart)
        return;

    LegacyTextBoxPainter textBoxPainter(*this, paintInfo, paintOffset);
    textBoxPainter.paint();
}

TextBoxSelectableRange LegacyInlineTextBox::selectableRange() const
{
    // Fix up the offset if we are combined text or have a hyphen because we manage these embellishments.
    // That is, they are not reflected in renderer().text(). We treat combined text as a single unit.
    // We also treat the last codepoint in this box and the hyphen as a single unit.
    auto additionalLengthAtEnd = [&] {
        if (auto* combinedText = this->combinedText())
            return combinedText->combinedStringForRendering().length() - m_len;
        if (hasHyphen())
            return lineStyle().hyphenString().length();
        return 0u;
    }();

    return {
        m_start,
        m_len,
        additionalLengthAtEnd,
        isLineBreak(),
        m_truncation
    };
}

std::pair<unsigned, unsigned> LegacyInlineTextBox::selectionStartEnd() const
{
    return renderer().view().selection().rangeForTextBox(renderer(), selectableRange());
}

bool LegacyInlineTextBox::hasMarkers() const
{
    return MarkedText::collectForDocumentMarkers(renderer(), selectableRange(), MarkedText::PaintPhase::Decoration).size();
}

int LegacyInlineTextBox::caretMinOffset() const
{
    return m_start;
}

int LegacyInlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

float LegacyInlineTextBox::textPos() const
{
    // When computing the width of a text run, RenderBlock::computeInlineDirectionPositionsForLine() doesn't include the actual offset
    // from the containing block edge in its measurement. textPos() should be consistent so the text are rendered in the same width.
    if (!logicalLeft())
        return 0;
    return logicalLeft() - root().logicalLeft();
}

TextRun LegacyInlineTextBox::createTextRun(bool ignoreCombinedText, bool ignoreHyphen) const
{
    const auto& style = lineStyle();
    TextRun textRun { text(ignoreCombinedText, ignoreHyphen), textPos(), expansion(), expansionBehavior(), direction(), style.rtlOrdering() == Order::Visual, !renderer().canUseSimpleFontCodePath() };
    textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    return textRun;
}

String LegacyInlineTextBox::text(bool ignoreCombinedText, bool ignoreHyphen) const
{
    String result;
    if (auto* combinedText = this->combinedText()) {
        if (ignoreCombinedText)
            result = renderer().text().substring(m_start, m_len);
        else
            result = combinedText->combinedStringForRendering();
    } else if (hasHyphen()) {
        if (ignoreHyphen)
            result = renderer().text().substring(m_start, m_len);
        else
            result = makeString(StringView(renderer().text()).substring(m_start, m_len), lineStyle().hyphenString());
    } else
        result = renderer().text().substring(m_start, m_len);

    // This works because this replacement doesn't affect string indices. We're replacing a single Unicode code unit with another Unicode code unit.
    // How convenient.
    return RenderBlock::updateSecurityDiscCharacters(lineStyle(), WTFMove(result));
}

const RenderCombineText* LegacyInlineTextBox::combinedText() const
{
    return lineStyle().hasTextCombine() && is<RenderCombineText>(renderer()) && downcast<RenderCombineText>(renderer()).isCombined() ? &downcast<RenderCombineText>(renderer()) : nullptr;
}

ExpansionBehavior LegacyInlineTextBox::expansionBehavior() const
{
    ExpansionBehavior behavior;

    if (forceLeftExpansion())
        behavior.left = ExpansionBehavior::Behavior::Force;
    else if (canHaveLeftExpansion())
        behavior.left = ExpansionBehavior::Behavior::Allow;
    else
        behavior.left = ExpansionBehavior::Behavior::Forbid;

    if (forceRightExpansion())
        behavior.right = ExpansionBehavior::Behavior::Force;
    else if (expansion() && nextLeafOnLine() && !nextLeafOnLine()->isLineBreak())
        behavior.right = ExpansionBehavior::Behavior::Allow;
    else
        behavior.right = ExpansionBehavior::Behavior::Forbid;

    return behavior;
}

#if ENABLE(TREE_DEBUGGING)

const char* LegacyInlineTextBox::boxName() const
{
    return "InlineTextBox";
}

void LegacyInlineTextBox::outputLineBox(TextStream& stream, bool mark, int depth) const
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
    value = makeStringByReplacingAll(value, '\\', "\\\\"_s);
    value = makeStringByReplacingAll(value, '\n', "\\n"_s);
    stream << boxName() << " " << FloatRect(x(), y(), width(), height()) << " (" << this << ") renderer->(" << &renderer() << ") run(" << start() << ", " << start() + len() << ") \"" << value.utf8().data() << "\"";
    stream.nextLine();
}

#endif

} // namespace WebCore
