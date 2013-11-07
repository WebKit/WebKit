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
#include "HTMLElement.h"
#include "InlineElementBox.h"
#include "RenderBlock.h"
#include "RootInlineBox.h"
#include "VisiblePosition.h"

namespace WebCore {

static const int invalidLineHeight = -1;

RenderLineBreak::RenderLineBreak(HTMLElement& element, PassRef<RenderStyle> style)
    : RenderBoxModelObject(element, std::move(style), 0)
    , m_inlineBoxWrapper(nullptr)
    , m_cachedLineHeight(invalidLineHeight)
    , m_isWBR(element.hasTagName(HTMLNames::wbrTag))
{
    setIsLineBreak();
}

RenderLineBreak::~RenderLineBreak()
{
    delete m_inlineBoxWrapper;
}

LayoutUnit RenderLineBreak::lineHeight(bool firstLine, LineDirectionMode /*direction*/, LinePositionMode /*linePositionMode*/) const
{
    if (firstLine && document().styleSheetCollection().usesFirstLineRules()) {
        const RenderStyle& firstLineStyle = this->firstLineStyle();
        if (&firstLineStyle != &style())
            return firstLineStyle.computedLineHeight(&view());
    }

    if (m_cachedLineHeight == invalidLineHeight)
        m_cachedLineHeight = style().computedLineHeight(&view());
    
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
    if (!documentBeingDestroyed())
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

VisiblePosition RenderLineBreak::positionForPoint(const LayoutPoint&)
{
    return createVisiblePosition(0, DOWNSTREAM);
}

void RenderLineBreak::setSelectionState(SelectionState state)
{
    RenderBoxModelObject::setSelectionState(state);
    if (!m_inlineBoxWrapper)
        return;
    m_inlineBoxWrapper->root().setHasSelectedChildren(state != SelectionNone);
}

LayoutRect RenderLineBreak::localCaretRect(InlineBox* inlineBox, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    ASSERT_UNUSED(caretOffset, !caretOffset);
    ASSERT_UNUSED(inlineBox, inlineBox == m_inlineBoxWrapper);
    if (!inlineBox)
        return LayoutRect();

    static const unsigned caretWidth = 1;
    const RootInlineBox& rootBox = inlineBox->root();
    return rootBox.computeCaretRect(inlineBox->logicalLeft(), caretWidth, extraWidthToEndOfLine);
}

IntRect RenderLineBreak::linesBoundingBox() const
{
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
    if (!m_inlineBoxWrapper)
        return;
    rects.append(enclosingIntRect(FloatRect(accumulatedOffset + m_inlineBoxWrapper->topLeft(), m_inlineBoxWrapper->size())));
}

void RenderLineBreak::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (!m_inlineBoxWrapper)
        return;
    quads.append(localToAbsoluteQuad(FloatRect(m_inlineBoxWrapper->topLeft(), m_inlineBoxWrapper->size()), 0 /* mode */, wasFixed));
}

void RenderLineBreak::updateFromStyle()
{
    m_cachedLineHeight = invalidLineHeight;
}

IntRect RenderLineBreak::borderBoundingBox() const
{
    return IntRect(IntPoint(), linesBoundingBox().size());
}

} // namespace WebCore
