/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "LegacyRootInlineBox.h"

#include "BidiResolver.h"
#include "CSSLineBoxContainValue.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LegacyInlineTextBox.h"
#include "LocalFrame.h"
#include "LogicalSelectionOffsetCaches.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderLayoutState.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRootInlineBox);

struct SameSizeAsLegacyRootInlineBox : LegacyInlineFlowBox, CanMakeWeakPtr<LegacyRootInlineBox>, CanMakeCheckedPtr {
    unsigned lineBreakPos;
    SingleThreadWeakPtr<RenderObject> lineBreakObj;
    void* lineBreakContext;
    int layoutUnits[4];
};

static_assert(sizeof(LegacyRootInlineBox) == sizeof(SameSizeAsLegacyRootInlineBox), "LegacyRootInlineBox should stay small");
#if !ASSERT_ENABLED
static_assert(sizeof(SingleThreadWeakPtr<RenderObject>) == sizeof(void*), "WeakPtr should be same size as raw pointer");
#endif

LegacyRootInlineBox::LegacyRootInlineBox(RenderBlockFlow& block)
    : LegacyInlineFlowBox(block)
{
    setIsHorizontal(block.isHorizontalWritingMode());
}

LegacyRootInlineBox::~LegacyRootInlineBox()
{
}

LayoutUnit LegacyRootInlineBox::baselinePosition(FontBaseline baselineType) const
{
    return renderer().baselinePosition(baselineType, isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
}

LayoutUnit LegacyRootInlineBox::lineHeight() const
{
    return renderer().lineHeight(isFirstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
}

void LegacyRootInlineBox::adjustPosition(float dx, float dy)
{
    LegacyInlineFlowBox::adjustPosition(dx, dy);
    LayoutUnit blockDirectionDelta { isHorizontal() ? dy : dx }; // The block direction delta is a LayoutUnit.
    m_lineTop += blockDirectionDelta;
    m_lineBottom += blockDirectionDelta;
    m_lineBoxTop += blockDirectionDelta;
    m_lineBoxBottom += blockDirectionDelta;
}

void LegacyRootInlineBox::childRemoved(LegacyInlineBox* box)
{
    if (&box->renderer() == m_lineBreakObj)
        setLineBreakInfo(nullptr, 0, BidiStatus());

    for (auto* prev = prevRootBox(); prev && prev->lineBreakObj() == &box->renderer(); prev = prev->prevRootBox()) {
        prev->setLineBreakInfo(nullptr, 0, BidiStatus());
        prev->markDirty();
    }
}

RenderObject::HighlightState LegacyRootInlineBox::selectionState() const
{
    // Walk over all of the selected boxes.
    RenderObject::HighlightState state = RenderObject::HighlightState::None;
    for (auto* box = firstLeafDescendant(); box; box = box->nextLeafOnLine()) {
        RenderObject::HighlightState boxState = box->selectionState();
        if ((boxState == RenderObject::HighlightState::Start && state == RenderObject::HighlightState::End)
            || (boxState == RenderObject::HighlightState::End && state == RenderObject::HighlightState::Start))
            state = RenderObject::HighlightState::Both;
        else if (state == RenderObject::HighlightState::None || ((boxState == RenderObject::HighlightState::Start || boxState == RenderObject::HighlightState::End)
            && (state == RenderObject::HighlightState::None || state == RenderObject::HighlightState::Inside)))
            state = boxState;
        else if (boxState == RenderObject::HighlightState::None && state == RenderObject::HighlightState::Start) {
            // We are past the end of the selection.
            state = RenderObject::HighlightState::Both;
        }
        if (state == RenderObject::HighlightState::Both)
            break;
    }

    return state;
}

const LegacyInlineBox* LegacyRootInlineBox::firstSelectedBox() const
{
    for (auto* box = firstLeafDescendant(); box; box = box->nextLeafOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return nullptr;
}

const LegacyInlineBox* LegacyRootInlineBox::lastSelectedBox() const
{
    for (auto* box = lastLeafDescendant(); box; box = box->previousLeafOnLine()) {
        if (box->selectionState() != RenderObject::HighlightState::None)
            return box;
    }
    return nullptr;
}

LayoutUnit LegacyRootInlineBox::selectionTop() const
{
    LayoutUnit selectionTop = m_lineTop;

    if (renderer().style().isFlippedLinesWritingMode())
        return selectionTop;

    LayoutUnit prevBottom;
    if (auto* previousBox = prevRootBox())
        prevBottom = previousBox->selectionBottom();
    else
        prevBottom = selectionTop;

    return prevBottom;
}

LayoutUnit LegacyRootInlineBox::selectionTopAdjustedForPrecedingBlock() const
{
    return blockFlow().adjustEnclosingTopForPrecedingBlock(selectionTop());
}

LayoutUnit LegacyRootInlineBox::selectionBottom() const
{
    LayoutUnit selectionBottom = m_lineBottom;

    if (!renderer().style().isFlippedLinesWritingMode() || !nextRootBox())
        return selectionBottom;

    return nextRootBox()->selectionTop();
}

RenderBlockFlow& LegacyRootInlineBox::blockFlow() const
{
    return downcast<RenderBlockFlow>(renderer());
}

BidiStatus LegacyRootInlineBox::lineBreakBidiStatus() const
{ 
    return { static_cast<UCharDirection>(m_lineBreakBidiStatusEor), static_cast<UCharDirection>(m_lineBreakBidiStatusLastStrong), static_cast<UCharDirection>(m_lineBreakBidiStatusLast), m_lineBreakContext.copyRef() };
}

void LegacyRootInlineBox::setLineBreakInfo(RenderObject* object, unsigned breakPosition, const BidiStatus& status)
{
    m_lineBreakObj = object;
    m_lineBreakPos = breakPosition;
    m_lineBreakBidiStatusEor = status.eor;
    m_lineBreakBidiStatusLastStrong = status.lastStrong;
    m_lineBreakBidiStatusLast = status.last;
    m_lineBreakContext = status.context;
}

void LegacyRootInlineBox::removeLineBoxFromRenderObject()
{
    // Null if we are destroying LegacyLineLayout.
    if (auto* legacyLineLayout = blockFlow().legacyLineLayout())
        legacyLineLayout->lineBoxes().removeLineBox(this);
}

void LegacyRootInlineBox::extractLineBoxFromRenderObject()
{
    blockFlow().legacyLineLayout()->lineBoxes().extractLineBox(this);
}

void LegacyRootInlineBox::attachLineBoxToRenderObject()
{
    blockFlow().legacyLineLayout()->lineBoxes().attachLineBox(this);
}

LayoutRect LegacyRootInlineBox::paddedLayoutOverflowRect(LayoutUnit endPadding) const
{
    LayoutRect lineLayoutOverflow = layoutOverflowRect(lineTop(), lineBottom());
    if (!endPadding)
        return lineLayoutOverflow;
    
    if (isHorizontal()) {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxXEdgeTo(std::max(lineLayoutOverflow.maxX(), LayoutUnit(logicalRight() + endPadding)));
        else
            lineLayoutOverflow.shiftXEdgeTo(std::min(lineLayoutOverflow.x(), LayoutUnit(logicalLeft() - endPadding)));
    } else {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxYEdgeTo(std::max(lineLayoutOverflow.maxY(), LayoutUnit(logicalRight() + endPadding)));
        else
            lineLayoutOverflow.shiftYEdgeTo(std::min(lineLayoutOverflow.y(), LayoutUnit(logicalLeft() - endPadding)));
    }
    
    return lineLayoutOverflow;
}

LayoutUnit LegacyRootInlineBox::lineBoxWidth() const
{
    return blockFlow().availableLogicalWidthForLine(lineBoxTop(), isFirstLine() ? IndentText : DoNotIndentText, lineBoxHeight());
}

#if ENABLE(TREE_DEBUGGING)

void LegacyRootInlineBox::outputLineBox(WTF::TextStream& stream, bool mark, int depth) const
{
    stream << "-------- --";
    int printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";
    stream << "Line: (top: " << lineTop() << " bottom: " << lineBottom() << ") with leading (top: " << lineBoxTop() << " bottom: " << lineBoxBottom() << ")";
    stream.nextLine();
    LegacyInlineBox::outputLineBox(stream, mark, depth);
}

const char* LegacyRootInlineBox::boxName() const
{
    return "RootInlineBox";
}
#endif

} // namespace WebCore
