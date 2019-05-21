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
#include "InlineBox.h"

#include "FontMetrics.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "InlineFlowBox.h"
#include "RenderBlockFlow.h"
#include "RenderLineBreak.h"
#include "RootInlineBox.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

#if ENABLE(TREE_DEBUGGING)
#include <stdio.h>
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineBox);

struct SameSizeAsInlineBox {
    virtual ~SameSizeAsInlineBox() = default;
    void* a[4];
    FloatPoint b;
    float c[2];
    unsigned d : 23;
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    unsigned s;
    bool f;
    bool i;
#endif
};

COMPILE_ASSERT(sizeof(InlineBox) == sizeof(SameSizeAsInlineBox), InlineBox_size_guard);

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED

void InlineBox::assertNotDeleted() const
{
    ASSERT(m_deletionSentinel == deletionSentinelNotDeletedValue);
}

InlineBox::~InlineBox()
{
    invalidateParentChildList();
    m_deletionSentinel = deletionSentinelDeletedValue;
}

void InlineBox::setHasBadParent()
{
    assertNotDeleted();
    m_hasBadParent = true;
}

void InlineBox::invalidateParentChildList()
{
    assertNotDeleted();
    if (!m_hasBadParent && m_parent && m_isEverInChildList)
        m_parent->setHasBadChildList();
}

#endif

void InlineBox::removeFromParent()
{ 
    if (parent())
        parent()->removeChild(this);
}

#if ENABLE(TREE_DEBUGGING)

const char* InlineBox::boxName() const
{
    return "InlineBox";
}

void InlineBox::showNodeTreeForThis() const
{
    m_renderer.showNodeTreeForThis();
}

void InlineBox::showLineTreeForThis() const
{
    m_renderer.containingBlock()->showLineTreeForThis();
}

void InlineBox::outputLineTreeAndMark(TextStream& stream, const InlineBox* markedBox, int depth) const
{
    outputLineBox(stream, markedBox == this, depth);
}

void InlineBox::outputLineBox(TextStream& stream, bool mark, int depth) const
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

float InlineBox::logicalHeight() const
{
    if (hasVirtualLogicalHeight())
        return virtualLogicalHeight();

    const RenderStyle& lineStyle = this->lineStyle();
    if (renderer().isTextOrLineBreak())
        return behavesLikeText() ? lineStyle.fontMetrics().height() : 0;
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

int InlineBox::baselinePosition(FontBaseline baselineType) const
{
    if (renderer().isLineBreak() && !behavesLikeText())
        return 0;
    return boxModelObject()->baselinePosition(baselineType, m_bitfields.firstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineBox::lineHeight() const
{
    if (renderer().isLineBreak() && !behavesLikeText())
        return 0;
    return boxModelObject()->lineHeight(m_bitfields.firstLine(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

int InlineBox::caretMinOffset() const 
{ 
    return m_renderer.caretMinOffset();
}

int InlineBox::caretMaxOffset() const 
{ 
    return m_renderer.caretMaxOffset();
}

void InlineBox::dirtyLineBoxes()
{
    markDirty();
    for (InlineFlowBox* curr = parent(); curr && !curr->isDirty(); curr = curr->parent())
        curr->markDirty();
}

void InlineBox::adjustPosition(float dx, float dy)
{
    m_topLeft.move(dx, dy);

    if (m_renderer.isOutOfFlowPositioned())
        return;

    if (m_renderer.isReplaced())
        downcast<RenderBox>(renderer()).move(LayoutUnit(dx), LayoutUnit(dy));
}

const RootInlineBox& InlineBox::root() const
{ 
    if (parent())
        return parent()->root();
    return downcast<RootInlineBox>(*this);
}

RootInlineBox& InlineBox::root()
{ 
    if (parent())
        return parent()->root();
    return downcast<RootInlineBox>(*this);
}

bool InlineBox::nextOnLineExists() const
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

bool InlineBox::previousOnLineExists() const
{
    if (!parent())
        return false;
    if (prevOnLine())
        return true;
    return parent()->previousOnLineExists();
}

InlineBox* InlineBox::nextLeafChild() const
{
    InlineBox* leaf = nullptr;
    for (InlineBox* box = nextOnLine(); box && !leaf; box = box->nextOnLine())
        leaf = box->isLeaf() ? box : downcast<InlineFlowBox>(*box).firstLeafChild();
    if (!leaf && parent())
        leaf = parent()->nextLeafChild();
    return leaf;
}
    
InlineBox* InlineBox::prevLeafChild() const
{
    InlineBox* leaf = nullptr;
    for (InlineBox* box = prevOnLine(); box && !leaf; box = box->prevOnLine())
        leaf = box->isLeaf() ? box : downcast<InlineFlowBox>(*box).lastLeafChild();
    if (!leaf && parent())
        leaf = parent()->prevLeafChild();
    return leaf;
}

InlineBox* InlineBox::nextLeafChildIgnoringLineBreak() const
{
    InlineBox* leaf = nextLeafChild();
    if (leaf && leaf->isLineBreak())
        return nullptr;
    return leaf;
}

InlineBox* InlineBox::prevLeafChildIgnoringLineBreak() const
{
    InlineBox* leaf = prevLeafChild();
    if (leaf && leaf->isLineBreak())
        return nullptr;
    return leaf;
}

RenderObject::SelectionState InlineBox::selectionState()
{
    return m_renderer.selectionState();
}

bool InlineBox::canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth) const
{
    // Non-replaced elements can always accommodate an ellipsis.
    if (!m_renderer.isReplaced())
        return true;
    
    IntRect boxRect(left(), 0, m_logicalWidth, 10);
    IntRect ellipsisRect(ltr ? blockEdge - ellipsisWidth : blockEdge, 0, ellipsisWidth, 10);
    return !(boxRect.intersects(ellipsisRect));
}

float InlineBox::placeEllipsisBox(bool, float, float, float, float& truncatedWidth, bool&)
{
    // Use -1 to mean "we didn't set the position."
    truncatedWidth += logicalWidth();
    return -1;
}

void InlineBox::clearKnownToHaveNoOverflow()
{ 
    m_bitfields.setKnownToHaveNoOverflow(false);
    if (parent() && parent()->knownToHaveNoOverflow())
        parent()->clearKnownToHaveNoOverflow();
}

FloatPoint InlineBox::locationIncludingFlipping() const
{
    if (!m_renderer.style().isFlippedBlocksWritingMode())
        return topLeft();
    RenderBlockFlow& block = root().blockFlow();
    if (block.style().isHorizontalWritingMode())
        return { x(), block.height() - height() - y() };
    return { block.width() - width() - x(), y() };
}

void InlineBox::flipForWritingMode(FloatRect& rect) const
{
    if (!m_renderer.style().isFlippedBlocksWritingMode())
        return;
    root().blockFlow().flipForWritingMode(rect);
}

FloatPoint InlineBox::flipForWritingMode(const FloatPoint& point) const
{
    if (!m_renderer.style().isFlippedBlocksWritingMode())
        return point;
    return root().blockFlow().flipForWritingMode(point);
}

void InlineBox::flipForWritingMode(LayoutRect& rect) const
{
    if (!m_renderer.style().isFlippedBlocksWritingMode())
        return;
    root().blockFlow().flipForWritingMode(rect);
}

LayoutPoint InlineBox::flipForWritingMode(const LayoutPoint& point) const
{
    if (!m_renderer.style().isFlippedBlocksWritingMode())
        return point;
    return root().blockFlow().flipForWritingMode(point);
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showNodeTree(const WebCore::InlineBox* inlineBox)
{
    if (!inlineBox)
        return;
    inlineBox->showNodeTreeForThis();
}

void showLineTree(const WebCore::InlineBox* inlineBox)
{
    if (!inlineBox)
        return;
    inlineBox->showLineTreeForThis();
}

#endif
