/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "LegacyInlineBox.h"

#include "FontMetrics.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "LegacyInlineFlowBox.h"
#include "LegacyRootInlineBox.h"
#include "RenderBlockFlow.h"
#include "RenderLineBreak.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

#if ENABLE(TREE_DEBUGGING)
#include <stdio.h>
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyInlineBox);

struct SameSizeAsLegacyInlineBox {
    virtual ~SameSizeAsLegacyInlineBox() = default;
    void* a[3];
    WeakPtr<RenderObject> r;
    FloatPoint b;
    float c[2];
    unsigned d : 23;
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    unsigned s;
    bool f;
    bool i;
#endif
};

COMPILE_ASSERT(sizeof(LegacyInlineBox) == sizeof(SameSizeAsLegacyInlineBox), LegacyInlineBox_size_guard);

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED

void LegacyInlineBox::assertNotDeleted() const
{
    ASSERT(m_deletionSentinel == deletionSentinelNotDeletedValue);
}

LegacyInlineBox::~LegacyInlineBox()
{
    invalidateParentChildList();
    m_deletionSentinel = deletionSentinelDeletedValue;
}

void LegacyInlineBox::setHasBadParent()
{
    assertNotDeleted();
    m_hasBadParent = true;
}

void LegacyInlineBox::invalidateParentChildList()
{
    assertNotDeleted();
    if (!m_hasBadParent && m_parent && m_isEverInChildList)
        m_parent->setHasBadChildList();
}

#endif

void LegacyInlineBox::removeFromParent()
{ 
    if (parent())
        parent()->removeChild(this);
}

#if ENABLE(TREE_DEBUGGING)

const char* LegacyInlineBox::boxName() const
{
    return "InlineBox";
}

void LegacyInlineBox::showNodeTreeForThis() const
{
    renderer().showNodeTreeForThis();
}

void LegacyInlineBox::showLineTreeForThis() const
{
    renderer().containingBlock()->showLineTreeForThis();
}

void LegacyInlineBox::outputLineTreeAndMark(TextStream& stream, const LegacyInlineBox* markedBox, int depth) const
{
    outputLineBox(stream, markedBox == this, depth);
}

void LegacyInlineBox::outputLineBox(TextStream& stream, bool mark, int depth) const
{
    stream << "-------- " << (isDirty() ? "D" : "-") << "-";
    int printedCharacters = 0;
    if (mark) {
        stream << "*";
        ++printedCharacters;
    }
    while (++printedCharacters <= depth * 2)
        stream << " ";
    stream << boxName() << " " << FloatRect(x(), y(), width(), height()) << " (" << this << ") renderer->(" << &renderer() << ")";
    stream.nextLine();
}

#endif // ENABLE(TREE_DEBUGGING)

float LegacyInlineBox::logicalHeight() const
{
    if (hasVirtualLogicalHeight())
        return virtualLogicalHeight();

    if (is<LegacyRootInlineBox>(*this) && downcast<LegacyRootInlineBox>(*this).isForTrailingFloats())
        return 0;

    const RenderStyle& lineStyle = this->lineStyle();
    if (renderer().isTextOrLineBreak())
        return lineStyle.fontMetrics().height();
    if (is<RenderBox>(renderer()) && parent())
        return isHorizontal() ? downcast<RenderBox>(renderer()).height() : downcast<RenderBox>(renderer()).width();

    ASSERT(isInlineFlowBox());
    RenderBoxModelObject* flowObject = boxModelObject();
    const FontMetrics& fontMetrics = lineStyle.fontMetrics();
    float result = fontMetrics.height();
    if (parent())
        result += flowObject->borderAndPaddingLogicalHeight();
    return result;
}

LayoutUnit LegacyInlineBox::baselinePosition(FontBaseline baselineType) const
{
    return boxModelObject()->baselinePosition(baselineType, m_bitfields.firstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit LegacyInlineBox::lineHeight() const
{
    return boxModelObject()->lineHeight(m_bitfields.firstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

int LegacyInlineBox::caretMinOffset() const
{ 
    return renderer().caretMinOffset();
}

int LegacyInlineBox::caretMaxOffset() const
{ 
    return renderer().caretMaxOffset();
}

void LegacyInlineBox::dirtyLineBoxes()
{
    markDirty();
    for (auto* curr = parent(); curr && !curr->isDirty(); curr = curr->parent())
        curr->markDirty();
}

void LegacyInlineBox::adjustPosition(float dx, float dy)
{
    m_topLeft.move(dx, dy);

    if (renderer().isOutOfFlowPositioned())
        return;

    if (renderer().isReplaced())
        downcast<RenderBox>(renderer()).move(LayoutUnit(dx), LayoutUnit(dy));
}

const LegacyRootInlineBox& LegacyInlineBox::root() const
{ 
    if (parent())
        return parent()->root();
    return downcast<LegacyRootInlineBox>(*this);
}

LegacyRootInlineBox& LegacyInlineBox::root()
{ 
    if (parent())
        return parent()->root();
    return downcast<LegacyRootInlineBox>(*this);
}

bool LegacyInlineBox::nextOnLineExists() const
{
    if (!m_bitfields.determinedIfNextOnLineExists()) {
        m_bitfields.setDeterminedIfNextOnLineExists(true);

        if (!parent())
            m_bitfields.setNextOnLineExists(false);
        else if (nextOnLine())
            m_bitfields.setNextOnLineExists(true);
        else
            m_bitfields.setNextOnLineExists(parent()->nextOnLineExists());
    }
    return m_bitfields.nextOnLineExists();
}

bool LegacyInlineBox::previousOnLineExists() const
{
    if (!parent())
        return false;
    if (previousOnLine())
        return true;
    return parent()->previousOnLineExists();
}

LegacyInlineBox* LegacyInlineBox::nextLeafOnLine() const
{
    LegacyInlineBox* leaf = nullptr;
    for (auto* box = nextOnLine(); box && !leaf; box = box->nextOnLine())
        leaf = box->isLeaf() ? box : downcast<LegacyInlineFlowBox>(*box).firstLeafDescendant();
    if (!leaf && parent())
        leaf = parent()->nextLeafOnLine();
    return leaf;
}
    
LegacyInlineBox* LegacyInlineBox::previousLeafOnLine() const
{
    LegacyInlineBox* leaf = nullptr;
    for (auto* box = previousOnLine(); box && !leaf; box = box->previousOnLine())
        leaf = box->isLeaf() ? box : downcast<LegacyInlineFlowBox>(*box).lastLeafDescendant();
    if (!leaf && parent())
        leaf = parent()->previousLeafOnLine();
    return leaf;
}

RenderObject::HighlightState LegacyInlineBox::selectionState()
{
    return renderer().selectionState();
}

bool LegacyInlineBox::canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth) const
{
    // Non-replaced elements can always accommodate an ellipsis.
    if (!renderer().isReplaced())
        return true;
    
    IntRect boxRect(left(), 0, m_logicalWidth, 10);
    IntRect ellipsisRect(ltr ? blockEdge - ellipsisWidth : blockEdge, 0, ellipsisWidth, 10);
    return !(boxRect.intersects(ellipsisRect));
}

float LegacyInlineBox::placeEllipsisBox(bool, float, float, float, float& truncatedWidth, bool&)
{
    // Use -1 to mean "we didn't set the position."
    truncatedWidth += logicalWidth();
    return -1;
}

void LegacyInlineBox::clearKnownToHaveNoOverflow()
{ 
    m_bitfields.setKnownToHaveNoOverflow(false);
    if (parent() && parent()->knownToHaveNoOverflow())
        parent()->clearKnownToHaveNoOverflow();
}

FloatPoint LegacyInlineBox::locationIncludingFlipping() const
{
    if (!renderer().style().isFlippedBlocksWritingMode())
        return topLeft();
    RenderBlockFlow& block = root().blockFlow();
    if (block.style().isHorizontalWritingMode())
        return { x(), block.height() - height() - y() };
    return { block.width() - width() - x(), y() };
}

void LegacyInlineBox::flipForWritingMode(FloatRect& rect) const
{
    if (!renderer().style().isFlippedBlocksWritingMode())
        return;
    root().blockFlow().flipForWritingMode(rect);
}

FloatPoint LegacyInlineBox::flipForWritingMode(const FloatPoint& point) const
{
    if (!renderer().style().isFlippedBlocksWritingMode())
        return point;
    return root().blockFlow().flipForWritingMode(point);
}

void LegacyInlineBox::flipForWritingMode(LayoutRect& rect) const
{
    if (!renderer().style().isFlippedBlocksWritingMode())
        return;
    root().blockFlow().flipForWritingMode(rect);
}

LayoutPoint LegacyInlineBox::flipForWritingMode(const LayoutPoint& point) const
{
    if (!renderer().style().isFlippedBlocksWritingMode())
        return point;
    return root().blockFlow().flipForWritingMode(point);
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showNodeTree(const WebCore::LegacyInlineBox* inlineBox)
{
    if (!inlineBox)
        return;
    inlineBox->showNodeTreeForThis();
}

void showLineTree(const WebCore::LegacyInlineBox* inlineBox)
{
    if (!inlineBox)
        return;
    inlineBox->showLineTreeForThis();
}

#endif
