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
#include "InlineIteratorBox.h"
#include "InlineIteratorLineBox.h"
#include "InlineRunAndOffset.h"
#include "LegacyInlineElementBox.h"
#include "LegacyRootInlineBox.h"
#include "LineSelection.h"
#include "LogicalSelectionOffsetCaches.h"
#include "RenderBlock.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGInlineTextBox.h"
#include "VisiblePosition.h"
#include <wtf/IsoMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "SelectionGeometry.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderLineBreak);

static const int invalidLineHeight = -1;

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
    if (firstLine) {
        const RenderStyle& firstLineStyle = this->firstLineStyle();
        if (&firstLineStyle != &style())
            return firstLineStyle.computedLineHeight();
    }

    if (m_cachedLineHeight == invalidLineHeight)
        m_cachedLineHeight = style().computedLineHeight();
    
    return m_cachedLineHeight;
}

LayoutUnit RenderLineBreak::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    const RenderStyle& style = firstLine ? firstLineStyle() : this->style();
    const FontMetrics& fontMetrics = style.metricsOfPrimaryFont();
    return LayoutUnit { (fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2).toInt() };
}

std::unique_ptr<LegacyInlineElementBox> RenderLineBreak::createInlineBox()
{
    return makeUnique<LegacyInlineElementBox>(*this);
}

void RenderLineBreak::setInlineBoxWrapper(LegacyInlineElementBox* inlineBox)
{
    ASSERT(!inlineBox || !m_inlineBoxWrapper);
    m_inlineBoxWrapper = inlineBox;
}

void RenderLineBreak::replaceInlineBoxWrapper(LegacyInlineElementBox& inlineBox)
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
    return createVisiblePosition(0, Affinity::Downstream);
}

IntRect RenderLineBreak::linesBoundingBox() const
{
    auto run = InlineIterator::boxFor(*this);
    if (!run)
        return { };

    return enclosingIntRect(run->visualRectIgnoringBlockDirection());
}

void RenderLineBreak::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    auto box = InlineIterator::boxFor(*this);
    if (!box)
        return;

    auto rect = box->visualRectIgnoringBlockDirection();
    rects.append(enclosingIntRect(FloatRect(accumulatedOffset + rect.location(), rect.size())));
}

void RenderLineBreak::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    auto box = InlineIterator::boxFor(*this);
    if (!box)
        return;

    auto rect = box->visualRectIgnoringBlockDirection();
    quads.append(localToAbsoluteQuad(FloatRect(rect.location(), rect.size()), UseTransforms, wasFixed));
}

void RenderLineBreak::updateFromStyle()
{
    m_cachedLineHeight = invalidLineHeight;
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(isInline());
}

#if PLATFORM(IOS_FAMILY)
void RenderLineBreak::collectSelectionGeometries(Vector<SelectionGeometry>& rects, unsigned, unsigned)
{
    auto run = InlineIterator::boxFor(*this);

    if (!run)
        return;
    auto lineBox = run->lineBox();

    auto lineSelectionRect = LineSelection::logicalRect(*lineBox);
    LayoutRect rect = IntRect(run->logicalLeft(), lineSelectionRect.y(), 0, lineSelectionRect.height());
    if (!lineBox->isHorizontal())
        rect = rect.transposedRect();

    if (lineBox->isFirstAfterPageBreak()) {
        if (run->isHorizontal())
            rect.shiftYEdgeTo(lineBox->logicalTop());
        else
            rect.shiftXEdgeTo(lineBox->logicalTop());
    }

    // FIXME: Out-of-flow positioned line breaks do not follow normal containing block chain.
    auto* containingBlock = RenderObject::containingBlockForPositionType(PositionType::Static, *this);
    // Map rect, extended left to leftOffset, and right to rightOffset, through transforms to get minX and maxX.
    LogicalSelectionOffsetCaches cache(*containingBlock);
    LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
    LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
    LayoutRect extentsRect = rect;
    if (run->isHorizontal()) {
        extentsRect.setX(leftOffset);
        extentsRect.setWidth(rightOffset - leftOffset);
    } else {
        extentsRect.setY(leftOffset);
        extentsRect.setHeight(rightOffset - leftOffset);
    }
    extentsRect = localToAbsoluteQuad(FloatRect(extentsRect)).enclosingBoundingBox();
    if (!run->isHorizontal())
        extentsRect = extentsRect.transposedRect();
    bool isFirstOnLine = !run->previousOnLine();
    bool isLastOnLine = !run->nextOnLine();
    if (containingBlock->isRubyBase() || containingBlock->isRubyText())
        isLastOnLine = !containingBlock->containingBlock()->inlineBoxWrapper()->nextOnLineExists();

    bool isFixed = false;
    auto absoluteQuad = localToAbsoluteQuad(FloatRect(rect), UseTransforms, &isFixed);
    bool boxIsHorizontal = !is<SVGInlineTextBox>(run->legacyInlineBox()) ? run->isHorizontal() : !style().isVerticalWritingMode();
    // If the containing block is an inline element, we want to check the inlineBoxWrapper orientation
    // to determine the orientation of the block. In this case we also use the inlineBoxWrapper to
    // determine if the element is the last on the line.
    if (containingBlock->inlineBoxWrapper()) {
        if (containingBlock->inlineBoxWrapper()->isHorizontal() != boxIsHorizontal) {
            boxIsHorizontal = containingBlock->inlineBoxWrapper()->isHorizontal();
            isLastOnLine = !containingBlock->inlineBoxWrapper()->nextOnLineExists();
        }
    }

    rects.append(SelectionGeometry(absoluteQuad, HTMLElement::selectionRenderingBehavior(element()), run->direction(), extentsRect.x(), extentsRect.maxX(), extentsRect.maxY(), 0, run->isLineBreak(), isFirstOnLine, isLastOnLine, false, false, boxIsHorizontal, isFixed, containingBlock->isRubyText(), view().pageNumberForBlockProgressionOffset(absoluteQuad.enclosingBoundingBox().x())));
}
#endif

} // namespace WebCore
