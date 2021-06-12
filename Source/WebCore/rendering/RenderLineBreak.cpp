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
#include "InlineRunAndOffset.h"
#include "LayoutIntegrationLineIterator.h"
#include "LayoutIntegrationRunIterator.h"
#include "LegacyInlineElementBox.h"
#include "LegacyRootInlineBox.h"
#include "LogicalSelectionOffsetCaches.h"
#include "RenderBlock.h"
#include "RenderView.h"
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
    if (firstLine && view().usesFirstLineRules()) {
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
    const FontMetrics& fontMetrics = style.fontMetrics();
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
    auto run = LayoutIntegration::runFor(*this);
    if (!run)
        return { };

    return enclosingIntRect(run->rect());
}

void RenderLineBreak::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    auto box = LayoutIntegration::runFor(*this);
    if (!box)
        return;

    auto rect = box->rect();
    rects.append(enclosingIntRect(FloatRect(accumulatedOffset + rect.location(), rect.size())));
}

void RenderLineBreak::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    auto box = LayoutIntegration::runFor(*this);
    if (!box)
        return;

    auto rect = box->rect();
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
    auto run = LayoutIntegration::runFor(*this);

    if (!run)
        return;
    auto line = run.line();

    auto lineSelectionRect = line->selectionRect();
    LayoutRect rect = IntRect(run->logicalLeft(), lineSelectionRect.y(), 0, lineSelectionRect.height());
    if (!line->isHorizontal())
        rect = rect.transposedRect();

    if (line->legacyRootInlineBox() && line->legacyRootInlineBox()->isFirstAfterPageBreak()) {
        if (run->isHorizontal())
            rect.shiftYEdgeTo(line->lineBoxTop());
        else
            rect.shiftXEdgeTo(line->lineBoxTop());
    }

    auto* containingBlock = containingBlockForObjectInFlow();
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
    bool isFirstOnLine = !run.previousOnLine();
    bool isLastOnLine = !run.nextOnLine();
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
