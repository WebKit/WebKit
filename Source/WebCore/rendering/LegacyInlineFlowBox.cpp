/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "LegacyInlineFlowBox.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "InlineBoxPainter.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "Text.h"
#include "TextBoxPainter.h"
#include "TextDecorationThickness.h"
#include "TextUnderlineOffset.h"
#include <math.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(LegacyInlineFlowBox);

struct SameSizeAsLegacyInlineFlowBox : public LegacyInlineBox {
    uint32_t bitfields : 23;
    void* pointers[5];
};

static_assert(sizeof(LegacyInlineFlowBox) == sizeof(SameSizeAsLegacyInlineFlowBox), "LegacyInlineFlowBox should stay small");

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED

LegacyInlineFlowBox::~LegacyInlineFlowBox()
{
    setHasBadChildList();
}

void LegacyInlineFlowBox::setHasBadChildList()
{
    assertNotDeleted();
    if (m_hasBadChildList)
        return;
    for (auto* child = firstChild(); child; child = child->nextOnLine())
        child->setHasBadParent();
    m_hasBadChildList = true;
}

#endif

static void setHasTextDescendantsOnAncestors(LegacyInlineFlowBox* box)
{
    while (box && !box->hasTextDescendants()) {
        box->setHasTextDescendants();
        box = box->parent();
    }
}

void LegacyInlineFlowBox::addToLine(LegacyInlineBox* child) 
{
    ASSERT(!child->parent());
    ASSERT(!child->nextOnLine());
    ASSERT(!child->previousOnLine());
    checkConsistency();

    child->setParent(this);
    if (!m_firstChild) {
        m_firstChild = child;
        m_lastChild = child;
    } else {
        m_lastChild->setNextOnLine(child);
        child->setPreviousOnLine(m_lastChild);
        m_lastChild = child;
    }
    child->setIsFirstLine(isFirstLine());
    child->setIsHorizontal(isHorizontal());
    if (child->isInlineTextBox()) {
        if (child->renderer().parent() == &renderer())
            m_hasTextChildren = true;
        setHasTextDescendantsOnAncestors(this);
    } else if (auto* blockFlow = dynamicDowncast<LegacyInlineFlowBox>(*child)) {
        if (blockFlow->hasTextDescendants())
            setHasTextDescendantsOnAncestors(this);
    }

    const RenderStyle& childStyle = child->lineStyle();
    if (child->isInlineTextBox()) {
        const RenderStyle* childStyle = &child->lineStyle();
        bool hasMarkers = false;
        if (auto* textBox = dynamicDowncast<LegacyInlineTextBox>(*child))
            hasMarkers = textBox->hasMarkers();
        if (childStyle->letterSpacing() < 0 || childStyle->textShadow() || childStyle->textEmphasisMark() != TextEmphasisMark::None || childStyle->hasPositiveStrokeWidth() || hasMarkers || !childStyle->textUnderlineOffset().isAuto() || !childStyle->textDecorationThickness().isAuto() || !childStyle->textUnderlinePosition().isEmpty())
            child->clearKnownToHaveNoOverflow();
    } else if (child->boxModelObject()->hasSelfPaintingLayer())
        child->clearKnownToHaveNoOverflow();
    else if (childStyle.hasOutlineInVisualOverflow())
        child->clearKnownToHaveNoOverflow();

    if (lineStyle().hasOutlineInVisualOverflow())
        clearKnownToHaveNoOverflow();

    if (knownToHaveNoOverflow()) {
        auto* flowBox = dynamicDowncast<LegacyInlineFlowBox>(*child);
        if (flowBox && !flowBox->knownToHaveNoOverflow())
            clearKnownToHaveNoOverflow();
    }

    if (auto* renderInline = dynamicDowncast<RenderInline>(child->renderer()); renderInline && renderInline->hasSelfPaintingLayer())
        m_hasSelfPaintInlineBox = true;

    checkConsistency();
}

void LegacyInlineFlowBox::removeChild(LegacyInlineBox* child)
{
    checkConsistency();

    if (!isDirty())
        dirtyLineBoxes();

    if (child == m_firstChild)
        m_firstChild = child->nextOnLine();
    if (child == m_lastChild)
        m_lastChild = child->previousOnLine();
    if (child->nextOnLine())
        child->nextOnLine()->setPreviousOnLine(child->previousOnLine());
    if (child->previousOnLine())
        child->previousOnLine()->setNextOnLine(child->nextOnLine());
    
    child->setParent(nullptr);

    checkConsistency();
}

void LegacyInlineFlowBox::deleteLine()
{
    LegacyInlineBox* child = firstChild();
    LegacyInlineBox* next = nullptr;
    while (child) {
        ASSERT(this == child->parent());
        next = child->nextOnLine();
#ifndef NDEBUG
        child->setParent(nullptr);
#endif
        child->deleteLine();
        child = next;
    }
#ifndef NDEBUG
    m_firstChild = nullptr;
    m_lastChild = nullptr;
#endif

    removeLineBoxFromRenderObject();
    delete this;
}

void LegacyInlineFlowBox::removeLineBoxFromRenderObject()
{
    downcast<RenderInline>(renderer()).lineBoxes().removeLineBox(this);
}

void LegacyInlineFlowBox::extractLine()
{
    if (!extracted())
        extractLineBoxFromRenderObject();
    for (auto* child = firstChild(); child; child = child->nextOnLine())
        child->extractLine();
}

void LegacyInlineFlowBox::extractLineBoxFromRenderObject()
{
    downcast<RenderInline>(renderer()).lineBoxes().extractLineBox(this);
}

void LegacyInlineFlowBox::attachLine()
{
    if (extracted())
        attachLineBoxToRenderObject();
    for (auto* child = firstChild(); child; child = child->nextOnLine())
        child->attachLine();
}

void LegacyInlineFlowBox::attachLineBoxToRenderObject()
{
    downcast<RenderInline>(renderer()).lineBoxes().attachLineBox(this);
}

void LegacyInlineFlowBox::adjustPosition(float dx, float dy)
{
    LegacyInlineBox::adjustPosition(dx, dy);
    for (auto* child = firstChild(); child; child = child->nextOnLine())
        child->adjustPosition(dx, dy);
    if (m_overflow)
        m_overflow->move(LayoutUnit(dx), LayoutUnit(dy));
}

inline void LegacyInlineFlowBox::addTextBoxVisualOverflow(LegacyInlineTextBox& textBox, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, LayoutRect& logicalVisualOverflow)
{
    if (textBox.knownToHaveNoOverflow())
        return;

    const RenderStyle& lineStyle = this->lineStyle();
    
    GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.find(&textBox);
    GlyphOverflow* glyphOverflow = it == textBoxDataMap.end() ? nullptr : &it->value.second;
    bool isFlippedLine = lineStyle.isFlippedLinesWritingMode();

    auto topGlyphEdge = glyphOverflow ? (isFlippedLine ? glyphOverflow->bottom : glyphOverflow->top) : 0_lu;
    auto bottomGlyphEdge = glyphOverflow ? (isFlippedLine ? glyphOverflow->top : glyphOverflow->bottom) : 0_lu;
    auto leftGlyphEdge = glyphOverflow ? glyphOverflow->left : 0_lu;
    auto rightGlyphEdge = glyphOverflow ? glyphOverflow->right : 0_lu;

    auto viewportSize = textBox.renderer().frame().view() ? textBox.renderer().frame().view()->size() : IntSize();
    LayoutUnit strokeOverflow(std::ceil(lineStyle.computedStrokeWidth(viewportSize) / 2.0f));
    auto topGlyphOverflow = -strokeOverflow - topGlyphEdge;
    auto bottomGlyphOverflow = strokeOverflow + bottomGlyphEdge;
    auto leftGlyphOverflow = -strokeOverflow - leftGlyphEdge;
    auto rightGlyphOverflow = strokeOverflow + rightGlyphEdge;

    if (auto markExistsAndIsAbove = RenderText::emphasisMarkExistsAndIsAbove(textBox.renderer(), lineStyle)) {
        LayoutUnit emphasisMarkHeight = lineStyle.fontCascade().emphasisMarkHeight(lineStyle.textEmphasisMarkString());
        if (*markExistsAndIsAbove == !lineStyle.isFlippedLinesWritingMode())
            topGlyphOverflow = std::min(topGlyphOverflow, -emphasisMarkHeight);
        else
            bottomGlyphOverflow = std::max(bottomGlyphOverflow, emphasisMarkHeight);
    }

    // If letter-spacing is negative, we should factor that into right layout overflow. (Even in RTL, letter-spacing is
    // applied to the right, so this is not an issue with left overflow.
    rightGlyphOverflow -= std::min(0, (int)lineStyle.fontCascade().letterSpacing());

    LayoutUnit textShadowLogicalTop;
    LayoutUnit textShadowLogicalBottom;
    lineStyle.getTextShadowBlockDirectionExtent(textShadowLogicalTop, textShadowLogicalBottom);
    
    LayoutUnit childOverflowLogicalTop = std::min<LayoutUnit>(textShadowLogicalTop + topGlyphOverflow, topGlyphOverflow);
    LayoutUnit childOverflowLogicalBottom = std::max<LayoutUnit>(textShadowLogicalBottom + bottomGlyphOverflow, bottomGlyphOverflow);

    LayoutUnit textShadowLogicalLeft;
    LayoutUnit textShadowLogicalRight;
    lineStyle.getTextShadowInlineDirectionExtent(textShadowLogicalLeft, textShadowLogicalRight);

    LayoutUnit childOverflowLogicalLeft = std::min<LayoutUnit>(textShadowLogicalLeft + leftGlyphOverflow, leftGlyphOverflow);
    LayoutUnit childOverflowLogicalRight = std::max<LayoutUnit>(textShadowLogicalRight + rightGlyphOverflow, rightGlyphOverflow);

    LayoutUnit logicalTopVisualOverflow = std::min(LayoutUnit(textBox.logicalTop() + childOverflowLogicalTop), logicalVisualOverflow.y());
    LayoutUnit logicalBottomVisualOverflow = std::max(LayoutUnit(textBox.logicalBottom() + childOverflowLogicalBottom), logicalVisualOverflow.maxY());
    LayoutUnit logicalLeftVisualOverflow = std::min(LayoutUnit(textBox.logicalLeft() + childOverflowLogicalLeft), logicalVisualOverflow.x());
    LayoutUnit logicalRightVisualOverflow = std::max(LayoutUnit(textBox.logicalRight() + childOverflowLogicalRight), logicalVisualOverflow.maxX());
    
    logicalVisualOverflow = LayoutRect(logicalLeftVisualOverflow, logicalTopVisualOverflow, logicalRightVisualOverflow - logicalLeftVisualOverflow, logicalBottomVisualOverflow - logicalTopVisualOverflow);

    auto documentMarkerBounds = LegacyTextBoxPainter::calculateUnionOfAllDocumentMarkerBounds(textBox);
    documentMarkerBounds.move(textBox.logicalLeft(), textBox.logicalTop());
    logicalVisualOverflow = unionRect(logicalVisualOverflow, LayoutRect(documentMarkerBounds));

    textBox.setLogicalOverflowRect(logicalVisualOverflow);
}

void LegacyInlineFlowBox::computeOverflow(LayoutUnit lineTop, LayoutUnit lineBottom, GlyphOverflowAndFallbackFontsMap& textBoxDataMap)
{
    // If we know we have no overflow, we can just bail.
    if (knownToHaveNoOverflow())
        return;

    if (m_overflow)
        m_overflow = nullptr;

    // Visual overflow just includes overflow for stuff we need to repaint ourselves. Self-painting layers are ignored.
    // Layout overflow is used to determine scrolling extent, so it still includes child layers and also factors in
    // transforms, relative positioning, etc.
    LayoutRect logicalVisualOverflow(enclosingLayoutRect(logicalFrameRectIncludingLineHeight(lineTop, lineBottom)));

    for (auto* child = firstChild(); child; child = child->nextOnLine()) {
        if (is<RenderText>(child->renderer())) {
            auto& textBox = downcast<LegacyInlineTextBox>(*child);
            LayoutRect textBoxOverflow(enclosingLayoutRect(textBox.logicalFrameRect()));
            addTextBoxVisualOverflow(textBox, textBoxDataMap, textBoxOverflow);
            logicalVisualOverflow.unite(textBoxOverflow);
        } else if (is<RenderInline>(child->renderer())) {
            auto& flow = downcast<LegacyInlineFlowBox>(*child);
            flow.computeOverflow(lineTop, lineBottom, textBoxDataMap);
            if (!flow.renderer().hasSelfPaintingLayer())
                logicalVisualOverflow.unite(flow.logicalVisualOverflowRect(lineTop, lineBottom));
        }
    }
    setOverflowFromLogicalRects(logicalVisualOverflow, lineTop, lineBottom);
}

void LegacyInlineFlowBox::setVisualOverflow(const LayoutRect& rect, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LayoutRect frameBox = enclosingLayoutRect(frameRectIncludingLineHeight(lineTop, lineBottom));
    if (frameBox.contains(rect) || rect.isEmpty())
        return;

    if (!m_overflow)
        m_overflow = adoptRef(new RenderOverflow(frameBox, frameBox));

    m_overflow->setVisualOverflow(rect);
}

void LegacyInlineFlowBox::setOverflowFromLogicalRects(const LayoutRect& logicalVisualOverflow, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LayoutRect visualOverflow(isHorizontal() ? logicalVisualOverflow : logicalVisualOverflow.transposedRect());
    setVisualOverflow(visualOverflow, lineTop, lineBottom);
}

bool LegacyInlineFlowBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    LayoutRect overflowRect(visualOverflowRect(lineTop, lineBottom));
    flipForWritingMode(overflowRect);
    overflowRect.moveBy(accumulatedOffset);
    if (!locationInContainer.intersects(overflowRect))
        return false;

    // Check children first.
    for (auto* child = lastChild(); child; child = child->previousOnLine()) {
        if (is<RenderText>(child->renderer()) || !child->boxModelObject()->hasSelfPaintingLayer()) {
            if (child->nodeAtPoint(request, result, locationInContainer, accumulatedOffset, lineTop, lineBottom, hitTestAction)) {
                renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                return true;
            }
        }
    }

    // Now check ourselves. Pixel snap hit testing.
    if (!renderer().visibleToHitTesting(request))
        return false;

    // Move x/y to our coordinates.
    FloatRect rect(frameRect());
    flipForWritingMode(rect);
    rect.moveBy(accumulatedOffset);

    if (locationInContainer.intersects(rect)) {
        renderer().updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset))); // Don't add in m_x or m_y here, we want coords in the containing block's space.
        if (result.addNodeToListBasedTestResult(renderer().protectedNodeForHitTest().get(), request, locationInContainer, rect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

void LegacyInlineFlowBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection && paintInfo.phase != PaintPhase::Outline && paintInfo.phase != PaintPhase::SelfOutline && paintInfo.phase != PaintPhase::ChildOutlines && paintInfo.phase != PaintPhase::TextClip && paintInfo.phase != PaintPhase::Mask && paintInfo.phase != PaintPhase::EventRegion && paintInfo.phase != PaintPhase::Accessibility)
        return;

    LayoutRect overflowRect(visualOverflowRect(lineTop, lineBottom));
    flipForWritingMode(overflowRect);
    overflowRect.moveBy(paintOffset);
    
    if (!paintInfo.rect.intersects(snappedIntRect(overflowRect)))
        return;

    if (paintInfo.phase != PaintPhase::ChildOutlines) {
        InlineBoxPainter painter(*this, paintInfo, paintOffset);
        painter.paint();
    }

    if (paintInfo.phase == PaintPhase::Mask)
        return;

    PaintPhase paintPhase = paintInfo.phase == PaintPhase::ChildOutlines ? PaintPhase::Outline : paintInfo.phase;
    PaintInfo childInfo(paintInfo);
    childInfo.phase = paintPhase;
    childInfo.updateSubtreePaintRootForChildren(&renderer());
    
    // Paint our children.
    if (paintPhase != PaintPhase::SelfOutline) {
        for (auto* curr = firstChild(); curr; curr = curr->nextOnLine()) {
            if (curr->renderer().isRenderText() || !curr->boxModelObject()->hasSelfPaintingLayer())
                curr->paint(childInfo, paintOffset, lineTop, lineBottom);
        }
    }
}

LegacyInlineBox* LegacyInlineFlowBox::firstLeafDescendant() const
{
    LegacyInlineBox* leaf = nullptr;
    for (auto* child = firstChild(); child && !leaf; child = child->nextOnLine())
        leaf = child->isLeaf() ? child : downcast<LegacyInlineFlowBox>(*child).firstLeafDescendant();
    return leaf;
}

LegacyInlineBox* LegacyInlineFlowBox::lastLeafDescendant() const
{
    LegacyInlineBox* leaf = nullptr;
    for (auto* child = lastChild(); child && !leaf; child = child->previousOnLine())
        leaf = child->isLeaf() ? child : downcast<LegacyInlineFlowBox>(*child).lastLeafDescendant();
    return leaf;
}

RenderObject::HighlightState LegacyInlineFlowBox::selectionState() const
{
    return RenderObject::HighlightState::None;
}

#if ENABLE(TREE_DEBUGGING)

ASCIILiteral LegacyInlineFlowBox::boxName() const
{
    return "InlineFlowBox"_s;
}

void LegacyInlineFlowBox::outputLineTreeAndMark(WTF::TextStream& stream, const LegacyInlineBox* markedBox, int depth) const
{
    LegacyInlineBox::outputLineTreeAndMark(stream, markedBox, depth);
    for (const LegacyInlineBox* box = firstChild(); box; box = box->nextOnLine())
        box->outputLineTreeAndMark(stream, markedBox, depth + 1);
}

#endif

#ifndef NDEBUG

void LegacyInlineFlowBox::checkConsistency() const
{
    assertNotDeleted();
    ASSERT_WITH_SECURITY_IMPLICATION(!m_hasBadChildList);
#ifdef CHECK_CONSISTENCY
    const LegacyInlineBox* previousChild = nullptr;
    for (const LegacyInlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        ASSERT(child->parent() == this);
        ASSERT(child->previousOnLine() == previousChild);
        previousChild = child;
    }
    ASSERT(previousChild == m_lastChild);
#endif
}

#endif

} // namespace WebCore
