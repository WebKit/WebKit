/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LineWidth.h"

#include "RenderBlockFlow.h"
#include "RenderRubyRun.h"

namespace WebCore {

LineWidth::LineWidth(RenderBlockFlow& block, bool isFirstLine, IndentTextOrNot shouldIndentText)
    : m_block(block)
    , m_isFirstLine(isFirstLine)
    , m_shouldIndentText(shouldIndentText)
{
    updateAvailableWidth();
}

bool LineWidth::fitsOnLine(bool ignoringTrailingSpace) const
{
    return ignoringTrailingSpace ? fitsOnLineExcludingTrailingCollapsedWhitespace() : fitsOnLineIncludingExtraWidth(0);
}

bool LineWidth::fitsOnLineIncludingExtraWidth(float extra) const
{
    return currentWidth() + extra <= m_availableWidth;
}

bool LineWidth::fitsOnLineExcludingTrailingWhitespace(float extra) const
{
    return currentWidth() - m_trailingWhitespaceWidth + extra <= m_availableWidth;
}

void LineWidth::updateAvailableWidth(LayoutUnit replacedHeight)
{
    LayoutUnit height = m_block.logicalHeight();
    LayoutUnit logicalHeight = m_block.minLineHeightForReplacedRenderer(m_isFirstLine, replacedHeight);
    m_left = m_block.logicalLeftOffsetForLine(height, shouldIndentText(), logicalHeight);
    m_right = m_block.logicalRightOffsetForLine(height, shouldIndentText(), logicalHeight);

    computeAvailableWidthFromLeftAndRight();
}

static bool newFloatShrinksLine(const FloatingObject& newFloat, const RenderBlockFlow& block, bool isFirstLine)
{
    LayoutUnit blockOffset = block.logicalHeight();
    if (blockOffset >= block.logicalTopForFloat(newFloat) && blockOffset < block.logicalBottomForFloat(newFloat))
        return true;

    // initial-letter float always shrinks the first line.
    const auto& style = newFloat.renderer().style();
    if (isFirstLine && style.styleType() == PseudoId::FirstLetter && !style.initialLetter().isEmpty())
        return true;
    return false;
}

void LineWidth::shrinkAvailableWidthForNewFloatIfNeeded(const FloatingObject& newFloat)
{
    if (!newFloatShrinksLine(newFloat, m_block, m_isFirstLine))
        return;
    ShapeOutsideDeltas shapeDeltas;
    if (ShapeOutsideInfo* shapeOutsideInfo = newFloat.renderer().shapeOutsideInfo()) {
        LayoutUnit lineHeight = m_block.lineHeight(m_isFirstLine, m_block.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
        shapeDeltas = shapeOutsideInfo->computeDeltasForContainingBlockLine(m_block, newFloat, m_block.logicalHeight(), lineHeight);
    }

    if (newFloat.type() == FloatingObject::FloatLeft) {
        float newLeft = m_block.logicalRightForFloat(newFloat);
        if (shouldIndentText() == IndentText && m_block.style().isLeftToRightDirection())
            newLeft += floorToInt(m_block.textIndentOffset());
        if (shapeDeltas.isValid()) {
            if (shapeDeltas.lineOverlapsShape())
                newLeft += shapeDeltas.rightMarginBoxDelta();
            else // If the line doesn't overlap the shape, then we need to act as if this float didn't exist.
                newLeft = m_left;
        }
        m_left = std::max<float>(m_left, newLeft);
    } else {
        float newRight = m_block.logicalLeftForFloat(newFloat);
        if (shouldIndentText() == IndentText && !m_block.style().isLeftToRightDirection())
            newRight -= floorToInt(m_block.textIndentOffset());
        if (shapeDeltas.isValid()) {
            if (shapeDeltas.lineOverlapsShape())
                newRight += shapeDeltas.leftMarginBoxDelta();
            else // If the line doesn't overlap the shape, then we need to act as if this float didn't exist.
                newRight = m_right;
        }
        m_right = std::min<float>(m_right, newRight);
    }

    computeAvailableWidthFromLeftAndRight();
}

void LineWidth::commit()
{
    m_committedWidth += m_uncommittedWidth;
    m_uncommittedWidth = 0;
    if (m_hasUncommittedReplaced) {
        m_hasCommittedReplaced = true;
        m_hasUncommittedReplaced = false;
    }
    m_hasCommitted = true;
}

void LineWidth::applyOverhang(const RenderRubyRun& rubyRun, RenderObject* startRenderer, RenderObject* endRenderer)
{
    float startOverhang;
    float endOverhang;
    rubyRun.getOverhang(m_isFirstLine, startRenderer, endRenderer, startOverhang, endOverhang);

    startOverhang = std::min(startOverhang, m_committedWidth);
    m_availableWidth += startOverhang;

    endOverhang = std::max(std::min(endOverhang, m_availableWidth - currentWidth()), 0.0f);
    m_availableWidth += endOverhang;
    m_overhangWidth += startOverhang + endOverhang;
}

inline static float availableWidthAtOffset(const RenderBlockFlow& block, const LayoutUnit& offset, IndentTextOrNot shouldIndentText,
    float& newLineLeft, float& newLineRight, const LayoutUnit& lineHeight = 0)
{
    newLineLeft = block.logicalLeftOffsetForLine(offset, shouldIndentText, lineHeight);
    newLineRight = block.logicalRightOffsetForLine(offset, shouldIndentText, lineHeight);
    return std::max(0.0f, newLineRight - newLineLeft);
}

void LineWidth::updateLineDimension(LayoutUnit newLineTop, LayoutUnit newLineWidth, float newLineLeft, float newLineRight)
{
    if (newLineWidth <= m_availableWidth)
        return;

    m_block.setLogicalHeight(newLineTop);
    m_availableWidth = newLineWidth + m_overhangWidth;
    m_left = newLineLeft;
    m_right = newLineRight;
}

void LineWidth::wrapNextToShapeOutside(bool isFirstLine)
{
    LayoutUnit lineHeight = m_block.lineHeight(isFirstLine, m_block.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    LayoutUnit lineLogicalTop = m_block.logicalHeight();
    LayoutUnit newLineTop = lineLogicalTop;
    LayoutUnit floatLogicalBottom = m_block.nextFloatLogicalBottomBelow(lineLogicalTop);

    float newLineWidth;
    float newLineLeft = m_left;
    float newLineRight = m_right;
    while (true) {
        newLineWidth = availableWidthAtOffset(m_block, newLineTop, shouldIndentText(), newLineLeft, newLineRight, lineHeight);
        if (newLineWidth >= m_uncommittedWidth)
            break;

        if (newLineTop >= floatLogicalBottom)
            break;

        ++newLineTop;
    }
    updateLineDimension(newLineTop, LayoutUnit(newLineWidth), LayoutUnit(newLineLeft), LayoutUnit(newLineRight));
}

void LineWidth::fitBelowFloats(bool isFirstLine)
{
    ASSERT(!m_committedWidth);
    ASSERT(!fitsOnLine());

    LayoutUnit floatLogicalBottom;
    LayoutUnit lastFloatLogicalBottom = m_block.logicalHeight();
    float newLineWidth = m_availableWidth;
    float newLineLeft = m_left;
    float newLineRight = m_right;

    FloatingObject* lastFloatFromPreviousLine = (m_block.containsFloats() ? m_block.m_floatingObjects->set().last().get() : 0);
    if (lastFloatFromPreviousLine && lastFloatFromPreviousLine->renderer().shapeOutsideInfo())
        return wrapNextToShapeOutside(isFirstLine);

    while (true) {
        floatLogicalBottom = m_block.nextFloatLogicalBottomBelow(lastFloatLogicalBottom);
        if (floatLogicalBottom <= lastFloatLogicalBottom)
            break;

        newLineWidth = availableWidthAtOffset(m_block, floatLogicalBottom, shouldIndentText(), newLineLeft, newLineRight);
        lastFloatLogicalBottom = floatLogicalBottom;

        if (newLineWidth >= m_uncommittedWidth)
            break;
    }

    updateLineDimension(lastFloatLogicalBottom, LayoutUnit(newLineWidth), LayoutUnit(newLineLeft), LayoutUnit(newLineRight));
}

void LineWidth::setTrailingWhitespaceWidth(float collapsedWhitespace, float borderPaddingMargin)
{
    m_trailingCollapsedWhitespaceWidth = collapsedWhitespace;
    m_trailingWhitespaceWidth = collapsedWhitespace + borderPaddingMargin;
}

void LineWidth::computeAvailableWidthFromLeftAndRight()
{
    m_availableWidth = std::max<float>(0, m_right - m_left) + m_overhangWidth;
}

bool LineWidth::fitsOnLineExcludingTrailingCollapsedWhitespace() const
{
    return currentWidth() - m_trailingCollapsedWhitespaceWidth <= m_availableWidth;
}

IndentTextOrNot requiresIndent(bool isFirstLine, bool isAfterHardLineBreak, const RenderStyle& style)
{
    IndentTextOrNot shouldIndentText = DoNotIndentText;
    if (isFirstLine)
        shouldIndentText = IndentText;
#if ENABLE(CSS3_TEXT)
    else if (isAfterHardLineBreak && style.textIndentLine() == TextIndentLine::EachLine)
        shouldIndentText = IndentText;

    if (style.textIndentType() == TextIndentType::Hanging)
        shouldIndentText = shouldIndentText == IndentText ? DoNotIndentText : IndentText;
#else
    UNUSED_PARAM(isAfterHardLineBreak);
    UNUSED_PARAM(style);
#endif
    return shouldIndentText;
}

}
