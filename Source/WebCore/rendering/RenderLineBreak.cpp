/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
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
#include "RenderLineBreak.h"

#include "Document.h"
#include "FontMetrics.h"
#include "HTMLElement.h"
#include "HTMLWBRElement.h"
#include "InlineElementBox.h"
#include "LogicalSelectionOffsetCaches.h"
#include "RenderBlock.h"
#include "RenderView.h"
#include "RootInlineBox.h"
#include "SimpleLineLayoutFunctions.h"
#include "VisiblePosition.h"
#include <wtf/IsoMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "SelectionRect.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderLineBreak);

static const int invalidLineHeight = -1;

static const SimpleLineLayout::Layout* simpleLineLayout(const RenderLineBreak& renderer)
{
    if (!is<RenderBlockFlow>(*renderer.parent()))
        return nullptr;
    return downcast<RenderBlockFlow>(*renderer.parent()).simpleLineLayout();
}

RenderLineBreak::RenderLineBreak(HTMLElement& element, RenderStyle&& style)
    : RenderBoxModelObject(element, WTFMove(style), 0)
    , m_inlineBoxWrapper(nullptr)
    , m_cachedLineHeight(invalidLineHeight)
    , m_isWBR(is<HTMLWBRElement>(element))
{
    setIsLineBreak();
}

RenderLineBreak::~RenderLineBreak()
{
    delete m_inlineBoxWrapper;
}

LayoutUnit RenderLineBreak::lineHeight(bool firstLine, LineDirectionMode /*direction*/, LinePositionMode /*linePositionMode*/) const
{
    if (firstLine && view().usesFirstLineRules()) {
        const RenderStyle& firstLineStyle = this->firstLineStyle();
        if (&firstLineStyle != &style())
            return firstLineStyle.computedLineHeight();
    }

    if (m_cachedLineHeight == invalidLineHeight)
        m_cachedLineHeight = style().computedLineHeight();
    
    return m_cachedLineHeight;
}

int RenderLineBreak::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    const RenderStyle& style = firstLine ? firstLineStyle() : this->style();
    const FontMetrics& fontMetrics = style.fontMetrics();
    return fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2;
}

std::unique_ptr<InlineElementBox> RenderLineBreak::createInlineBox()
{
    return std::make_unique<InlineElementBox>(*this);
}

void RenderLineBreak::setInlineBoxWrapper(InlineElementBox* inlineBox)
{
    ASSERT(!inlineBox || !m_inlineBoxWrapper);
    m_inlineBoxWrapper = inlineBox;
}

void RenderLineBreak::replaceInlineBoxWrapper(InlineElementBox& inlineBox)
{
    deleteInlineBoxWrapper();
    setInlineBoxWrapper(&inlineBox);
}

void RenderLineBreak::deleteInlineBoxWrapper()
{
    if (!m_inlineBoxWrapper)
        return;
    if (!renderTreeBeingDestroyed())
        m_inlineBoxWrapper->removeFromParent();
    delete m_inlineBoxWrapper;
    m_inlineBoxWrapper = nullptr;
}

void RenderLineBreak::dirtyLineBoxes(bool fullLayout)
{
    if (!m_inlineBoxWrapper)
        return;
    if (fullLayout) {
        delete m_inlineBoxWrapper;
        m_inlineBoxWrapper = nullptr;
        return;
    }
    m_inlineBoxWrapper->dirtyLineBoxes();
}

void RenderLineBreak::ensureLineBoxes()
{
    if (!is<RenderBlockFlow>(*parent()))
        return;
    downcast<RenderBlockFlow>(*parent()).ensureLineBoxes();
}

void RenderLineBreak::deleteLineBoxesBeforeSimpleLineLayout()
{
    delete m_inlineBoxWrapper;
    m_inlineBoxWrapper = nullptr;
}

int RenderLineBreak::caretMinOffset() const
{
    return 0;
}

int RenderLineBreak::caretMaxOffset() const
{ 
    return 1;
}

bool RenderLineBreak::canBeSelectionLeaf() const
{
    return true;
}

VisiblePosition RenderLineBreak::positionForPoint(const LayoutPoint&, const RenderFragmentContainer*)
{
    ensureLineBoxes();
    return createVisiblePosition(0, DOWNSTREAM);
}

void RenderLineBreak::setSelectionState(SelectionState state)
{
    if (state != SelectionNone)
        ensureLineBoxes();
    RenderBoxModelObject::setSelectionState(state);
    if (!m_inlineBoxWrapper)
        return;
    m_inlineBoxWrapper->root().setHasSelectedChildren(state != SelectionNone);
}

LayoutRect RenderLineBreak::localCaretRect(InlineBox* inlineBox, unsigned caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    ASSERT_UNUSED(caretOffset, !caretOffset);
    ASSERT_UNUSED(inlineBox, inlineBox == m_inlineBoxWrapper);
    if (!inlineBox)
        return LayoutRect();

    const RootInlineBox& rootBox = inlineBox->root();
    return rootBox.computeCaretRect(inlineBox->logicalLeft(), caretWidth, extraWidthToEndOfLine);
}

IntRect RenderLineBreak::linesBoundingBox() const
{
    if (auto* layout = simpleLineLayout(*this))
        return SimpleLineLayout::computeBoundingBox(*this, *layout);

    if (!m_inlineBoxWrapper)
        return IntRect();

    float logicalLeftSide = m_inlineBoxWrapper->logicalLeft();
    float logicalRightSide = m_inlineBoxWrapper->logicalRight();

    bool isHorizontal = style().isHorizontalWritingMode();

    float x = isHorizontal ? logicalLeftSide : m_inlineBoxWrapper->x();
    float y = isHorizontal ? m_inlineBoxWrapper->y() : logicalLeftSide;
    float width = isHorizontal ? logicalRightSide - logicalLeftSide : m_inlineBoxWrapper->logicalBottom() - x;
    float height = isHorizontal ? m_inlineBoxWrapper->logicalBottom() - y : logicalRightSide - logicalLeftSide;
    return enclosingIntRect(FloatRect(x, y, width, height));
}

void RenderLineBreak::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    if (auto* layout = simpleLineLayout(*this)) {
        rects.appendVector(SimpleLineLayout::collectAbsoluteRects(*this, *layout, accumulatedOffset));
        return;
    }

    if (!m_inlineBoxWrapper)
        return;
    rects.append(enclosingIntRect(FloatRect(accumulatedOffset + m_inlineBoxWrapper->topLeft(), m_inlineBoxWrapper->size())));
}

void RenderLineBreak::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (auto* layout = simpleLineLayout(*this)) {
        quads.appendVector(SimpleLineLayout::collectAbsoluteQuads(*this, *layout, wasFixed));
        return;
    }
    if (!m_inlineBoxWrapper)
        return;
    quads.append(localToAbsoluteQuad(FloatRect(m_inlineBoxWrapper->topLeft(), m_inlineBoxWrapper->size()), UseTransforms, wasFixed));
}

void RenderLineBreak::updateFromStyle()
{
    m_cachedLineHeight = invalidLineHeight;
}

#if PLATFORM(IOS_FAMILY)
void RenderLineBreak::collectSelectionRects(Vector<SelectionRect>& rects, unsigned, unsigned)
{
    ensureLineBoxes();
    InlineElementBox* box = m_inlineBoxWrapper;
    if (!box)
        return;
    const RootInlineBox& rootBox = box->root();
    LayoutRect rect = rootBox.computeCaretRect(box->logicalLeft(), 0, nullptr);
    if (rootBox.isFirstAfterPageBreak()) {
        if (box->isHorizontal())
            rect.shiftYEdgeTo(rootBox.lineTopWithLeading());
        else
            rect.shiftXEdgeTo(rootBox.lineTopWithLeading());
    }

    auto* containingBlock = containingBlockForObjectInFlow();
    // Map rect, extended left to leftOffset, and right to rightOffset, through transforms to get minX and maxX.
    LogicalSelectionOffsetCaches cache(*containingBlock);
    LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, box->logicalTop(), cache);
    LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, box->logicalTop(), cache);
    LayoutRect extentsRect = rect;
    if (box->isHorizontal()) {
        extentsRect.setX(leftOffset);
        extentsRect.setWidth(rightOffset - leftOffset);
    } else {
        extentsRect.setY(leftOffset);
        extentsRect.setHeight(rightOffset - leftOffset);
    }
    extentsRect = localToAbsoluteQuad(FloatRect(extentsRect)).enclosingBoundingBox();
    if (!box->isHorizontal())
        extentsRect = extentsRect.transposedRect();
    bool isFirstOnLine = !box->previousOnLineExists();
    bool isLastOnLine = !box->nextOnLineExists();
    if (containingBlock->isRubyBase() || containingBlock->isRubyText())
        isLastOnLine = !containingBlock->containingBlock()->inlineBoxWrapper()->nextOnLineExists();

    bool isFixed = false;
    IntRect absRect = localToAbsoluteQuad(FloatRect(rect), UseTransforms, &isFixed).enclosingBoundingBox();
    bool boxIsHorizontal = !box->isSVGInlineTextBox() ? box->isHorizontal() : !style().isVerticalWritingMode();
    // If the containing block is an inline element, we want to check the inlineBoxWrapper orientation
    // to determine the orientation of the block. In this case we also use the inlineBoxWrapper to
    // determine if the element is the last on the line.
    if (containingBlock->inlineBoxWrapper()) {
        if (containingBlock->inlineBoxWrapper()->isHorizontal() != boxIsHorizontal) {
            boxIsHorizontal = containingBlock->inlineBoxWrapper()->isHorizontal();
            isLastOnLine = !containingBlock->inlineBoxWrapper()->nextOnLineExists();
        }
    }

    rects.append(SelectionRect(absRect, box->direction(), extentsRect.x(), extentsRect.maxX(), extentsRect.maxY(), 0, box->isLineBreak(), isFirstOnLine, isLastOnLine, false, false, boxIsHorizontal, isFixed, containingBlock->isRubyText(), view().pageNumberForBlockProgressionOffset(absRect.x())));
}
#endif

} // namespace WebCore
