/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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
#include "RenderInline.h"

#include "BorderPainter.h"
#include "Chrome.h"
#include "FloatQuad.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyInlineElementBox.h"
#include "LegacyInlineTextBox.h"
#include "RenderBlock.h"
#include "RenderChildIterator.h"
#include "RenderFragmentedFlow.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListMarker.h"
#include "RenderTable.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleInheritedData.h"
#include "TransformState.h"
#include "VisiblePosition.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderInline);

RenderInline::RenderInline(Element& element, RenderStyle&& style)
    : RenderBoxModelObject(element, WTFMove(style), RenderInlineFlag)
{
    setChildrenInline(true);
}

RenderInline::RenderInline(Document& document, RenderStyle&& style)
    : RenderBoxModelObject(document, WTFMove(style), RenderInlineFlag)
{
    setChildrenInline(true);
}

void RenderInline::willBeDestroyed()
{
#if ASSERT_ENABLED
    // Make sure we do not retain "this" in the continuation outline table map of our containing blocks.
    if (parent() && style().visibility() == Visibility::Visible && hasOutline()) {
        bool containingBlockPaintsContinuationOutline = continuation() || isContinuation();
        if (containingBlockPaintsContinuationOutline) {
            if (RenderBlock* cb = containingBlock()) {
                if (RenderBlock* cbCb = cb->containingBlock())
                    ASSERT(!cbCb->paintsContinuationOutline(this));
            }
        }
    }
#endif // ASSERT_ENABLED

    if (!renderTreeBeingDestroyed()) {
        if (firstLineBox()) {
            // We can't wait for RenderBoxModelObject::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            if (isSelectionBorder())
                frame().selection().setNeedsSelectionUpdate();

            // If line boxes are contained inside a root, that means we're an inline.
            // In that case, we need to remove all the line boxes so that the parent
            // lines aren't pointing to deleted children. If the first line box does
            // not have a parent that means they are either already disconnected or
            // root lines that can just be destroyed without disconnecting.
            if (firstLineBox()->parent()) {
                for (auto* box = firstLineBox(); box; box = box->nextLineBox())
                    box->removeFromParent();
            }
        } else if (parent())
            parent()->dirtyLinesFromChangedChild(*this);
    }

    m_lineBoxes.deleteLineBoxes();

    RenderBoxModelObject::willBeDestroyed();
}

void RenderInline::updateFromStyle()
{
    RenderBoxModelObject::updateFromStyle();

    // FIXME: Support transforms and reflections on inline flows someday.
    setHasTransformRelatedProperty(false);
    setHasReflection(false);    
}

static RenderElement* inFlowPositionedInlineAncestor(RenderElement* p)
{
    while (p && p->isRenderInline()) {
        if (p->isInFlowPositioned())
            return p;
        p = p->parent();
    }
    return nullptr;
}

static void updateStyleOfAnonymousBlockContinuations(const RenderBlock& block, const RenderStyle* newStyle, const RenderStyle* oldStyle)
{
    // If any descendant blocks exist then they will be in the next anonymous block and its siblings.
    for (RenderBox* box = block.nextSiblingBox(); box && box->isAnonymousBlock(); box = box->nextSiblingBox()) {
        if (box->style().position() == newStyle->position())
            continue;
        
        if (!is<RenderBlock>(*box))
            continue;

        RenderBlock& block = downcast<RenderBlock>(*box);
        if (!block.isContinuation())
            continue;
        
        // If we are no longer in-flow positioned but our descendant block(s) still have an in-flow positioned ancestor then
        // their containing anonymous block should keep its in-flow positioning. 
        RenderInline* continuation = block.inlineContinuation();
        if (oldStyle->hasInFlowPosition() && inFlowPositionedInlineAncestor(continuation))
            continue;
        auto blockStyle = RenderStyle::createAnonymousStyleWithDisplay(block.style(), DisplayType::Block);
        blockStyle.setPosition(newStyle->position());
        block.setStyle(WTFMove(blockStyle));
    }
}

void RenderInline::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    RenderBoxModelObject::styleWillChange(diff, newStyle);
    // RenderInlines forward their absolute positioned descendants to their (non-anonymous) containing block.
    // Check if this non-anonymous containing block can hold the absolute positioned elements when the inline is no longer positioned.
    if (canContainAbsolutelyPositionedObjects() && newStyle.position() == PositionType::Static) {
        auto* container = RenderObject::containingBlockForPositionType(PositionType::Absolute, *this);
        if (container && !container->canContainAbsolutelyPositionedObjects())
            container->removePositionedObjects(nullptr, NewContainingBlock);
    }
}

void RenderInline::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBoxModelObject::styleDidChange(diff, oldStyle);

    // Ensure that all of the split inlines pick up the new style. We
    // only do this if we're an inline, since we don't want to propagate
    // a block's style to the other inlines.
    // e.g., <font>foo <h4>goo</h4> moo</font>.  The <font> inlines before
    // and after the block share the same style, but the block doesn't
    // need to pass its style on to anyone else.
    auto& newStyle = style();
    RenderInline* continuation = inlineContinuation();
    if (continuation && !isContinuation()) {
        for (RenderInline* currCont = continuation; currCont; currCont = currCont->inlineContinuation())
            currCont->setStyle(RenderStyle::clone(newStyle));
        // If an inline's in-flow positioning has changed and it is part of an active continuation as a descendant of an anonymous containing block,
        // then any descendant blocks will need to change their in-flow positioning accordingly.
        // Do this by updating the position of the descendant blocks' containing anonymous blocks - there may be more than one.
        if (containingBlock()->isAnonymousBlock() && oldStyle && newStyle.position() != oldStyle->position() && (newStyle.hasInFlowPosition() || oldStyle->hasInFlowPosition()))
            updateStyleOfAnonymousBlockContinuations(*containingBlock(), &newStyle, oldStyle);
    }

    if (diff >= StyleDifference::Repaint) {
        if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this)) {
            auto shouldInvalidateLineLayoutPath = selfNeedsLayout() || !LayoutIntegration::LineLayout::canUseForAfterInlineBoxStyleChange(*this, diff);
            if (shouldInvalidateLineLayoutPath)
                lineLayout->flow().invalidateLineLayoutPath();
            else
                lineLayout->updateStyle(*this, *oldStyle);
        }
    }
}

bool RenderInline::mayAffectLayout() const
{
    auto* parentStyle = &parent()->style();
    auto* parentRenderInline = dynamicDowncast<RenderInline>(*parent());
    auto hasHardLineBreakChildOnly = firstChild() && firstChild() == lastChild() && firstChild()->isBR();
    bool checkFonts = document().inNoQuirksMode();
    auto mayAffectLayout = (parentRenderInline && parentRenderInline->mayAffectLayout())
        || (parentRenderInline && parentStyle->verticalAlign() != VerticalAlign::Baseline)
        || style().verticalAlign() != VerticalAlign::Baseline
        || style().textEmphasisMark() != TextEmphasisMark::None
        || (checkFonts && (!parentStyle->fontCascade().metricsOfPrimaryFont().hasIdenticalAscentDescentAndLineGap(style().fontCascade().metricsOfPrimaryFont())
        || parentStyle->lineHeight() != style().lineHeight()))
        || hasHardLineBreakChildOnly;

    if (!mayAffectLayout && checkFonts) {
        // Have to check the first line style as well.
        parentStyle = &parent()->firstLineStyle();
        auto& childStyle = firstLineStyle();
        mayAffectLayout = !parentStyle->fontCascade().metricsOfPrimaryFont().hasIdenticalAscentDescentAndLineGap(childStyle.fontCascade().metricsOfPrimaryFont())
            || childStyle.verticalAlign() != VerticalAlign::Baseline
            || parentStyle->lineHeight() != childStyle.lineHeight();
    }
    return mayAffectLayout;
}

void RenderInline::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this)) {
        lineLayout->paint(paintInfo, paintOffset, this);
        return;
    }
    m_lineBoxes.paint(this, paintInfo, paintOffset);
}

template<typename GeneratorContext>
void RenderInline::generateLineBoxRects(GeneratorContext& context) const
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this)) {
        auto inlineBoxRects = lineLayout->collectInlineBoxRects(*this);
        if (inlineBoxRects.isEmpty()) {
            context.addRect({ });
            return;
        }
        for (auto inlineBoxRect : inlineBoxRects)
            context.addRect(inlineBoxRect);
        return;
    }
    if (LegacyInlineFlowBox* curr = firstLineBox()) {
        for (; curr; curr = curr->nextLineBox())
            context.addRect(FloatRect(curr->topLeft(), curr->size()));
    } else
        context.addRect(FloatRect());
}

class AbsoluteRectsGeneratorContext {
public:
    AbsoluteRectsGeneratorContext(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset)
        : m_rects(rects)
        , m_accumulatedOffset(accumulatedOffset) { }

    void addRect(const FloatRect& rect)
    {
        LayoutRect adjustedRect = LayoutRect(rect);
        adjustedRect.moveBy(m_accumulatedOffset);
        m_rects.append(adjustedRect);
    }
private:
    Vector<LayoutRect>& m_rects;
    const LayoutPoint& m_accumulatedOffset;
};

void RenderInline::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    Vector<LayoutRect> lineboxRects;
    AbsoluteRectsGeneratorContext context(lineboxRects, accumulatedOffset);
    generateLineBoxRects(context);
    for (const auto& rect : lineboxRects)
        rects.append(snappedIntRect(rect));

    if (RenderBoxModelObject* continuation = this->continuation()) {
        if (is<RenderBox>(*continuation)) {
            auto& box = downcast<RenderBox>(*continuation);
            continuation->absoluteRects(rects, toLayoutPoint(accumulatedOffset - containingBlock()->location() + box.locationOffset()));
        } else
            continuation->absoluteRects(rects, toLayoutPoint(accumulatedOffset - containingBlock()->location()));
    }
}

namespace {

class AbsoluteQuadsGeneratorContext {
public:
    AbsoluteQuadsGeneratorContext(const RenderInline* renderer, Vector<FloatQuad>& quads)
        : m_quads(quads)
        , m_geometryMap()
    {
        m_geometryMap.pushMappingsToAncestor(renderer, nullptr);
    }

    void addRect(const FloatRect& rect)
    {
        m_quads.append(m_geometryMap.absoluteRect(rect));
    }
private:
    Vector<FloatQuad>& m_quads;
    RenderGeometryMap m_geometryMap;
};

} // unnamed namespace

void RenderInline::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    absoluteQuadsIgnoringContinuation({ }, quads, wasFixed);
    if (continuation())
        collectAbsoluteQuadsForContinuation(quads, wasFixed);
}

void RenderInline::absoluteQuadsIgnoringContinuation(const FloatRect&, Vector<FloatQuad>& quads, bool*) const
{
    AbsoluteQuadsGeneratorContext context(this, quads);
    generateLineBoxRects(context);
}

#if PLATFORM(IOS_FAMILY)
void RenderInline::absoluteQuadsForSelection(Vector<FloatQuad>& quads) const
{
    AbsoluteQuadsGeneratorContext context(this, quads);
    generateLineBoxRects(context);
}
#endif

LayoutUnit RenderInline::offsetLeft() const
{
    return adjustedPositionRelativeToOffsetParent(firstInlineBoxTopLeft()).x();
}

LayoutUnit RenderInline::offsetTop() const
{
    return adjustedPositionRelativeToOffsetParent(firstInlineBoxTopLeft()).y();
}

LayoutPoint RenderInline::firstInlineBoxTopLeft() const
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        return lineLayout->firstInlineBoxRect(*this).location();
    if (LegacyInlineBox* firstBox = firstLineBox())
        return flooredLayoutPoint(firstBox->locationIncludingFlipping());
    return { };
}

static LayoutUnit computeMargin(const RenderInline* renderer, const Length& margin)
{
    if (margin.isAuto())
        return 0;
    if (margin.isFixed())
        return LayoutUnit(margin.value());
    if (margin.isPercentOrCalculated())
        return minimumValueForLength(margin, std::max<LayoutUnit>(0, renderer->containingBlock()->availableLogicalWidth()));
    return 0;
}

LayoutUnit RenderInline::marginLeft() const
{
    return computeMargin(this, style().marginLeft());
}

LayoutUnit RenderInline::marginRight() const
{
    return computeMargin(this, style().marginRight());
}

LayoutUnit RenderInline::marginTop() const
{
    return computeMargin(this, style().marginTop());
}

LayoutUnit RenderInline::marginBottom() const
{
    return computeMargin(this, style().marginBottom());
}

LayoutUnit RenderInline::marginStart(const RenderStyle* otherStyle) const
{
    return computeMargin(this, style().marginStartUsing(otherStyle ? otherStyle : &style()));
}

LayoutUnit RenderInline::marginEnd(const RenderStyle* otherStyle) const
{
    return computeMargin(this, style().marginEndUsing(otherStyle ? otherStyle : &style()));
}

LayoutUnit RenderInline::marginBefore(const RenderStyle* otherStyle) const
{
    return computeMargin(this, style().marginBeforeUsing(otherStyle ? otherStyle : &style()));
}

LayoutUnit RenderInline::marginAfter(const RenderStyle* otherStyle) const
{
    return computeMargin(this, style().marginAfterUsing(otherStyle ? otherStyle : &style()));
}

ASCIILiteral RenderInline::renderName() const
{
    if (isRelativelyPositioned())
        return "RenderInline (relative positioned)"_s;
    if (isStickilyPositioned())
        return "RenderInline (sticky positioned)"_s;
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderInline (generated)"_s;
    if (isAnonymous())
        return "RenderInline (generated)"_s;
    return "RenderInline"_s;
}

bool RenderInline::nodeAtPoint(const HitTestRequest& request, HitTestResult& result,
                                const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    ASSERT(layer());
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        return lineLayout->hitTest(request, result, locationInContainer, accumulatedOffset, hitTestAction, this);
    return m_lineBoxes.hitTest(this, request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

VisiblePosition RenderInline::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer* fragment)
{
    auto& containingBlock = *this->containingBlock();

    if (auto* continuation = this->continuation()) {
        // Translate the coords from the pre-anonymous block to the post-anonymous block.
        LayoutPoint parentBlockPoint = containingBlock.location() + point;
        while (continuation) {
            RenderBlock* currentBlock = continuation->isInline() ? continuation->containingBlock() : downcast<RenderBlock>(continuation);
            if (continuation->isInline() || continuation->firstChild())
                return continuation->positionForPoint(parentBlockPoint - currentBlock->locationOffset(), fragment);
            continuation = continuation->inlineContinuation();
        }
        return RenderBoxModelObject::positionForPoint(point, fragment);
    }

    return containingBlock.positionForPoint(point, fragment);
}

class LinesBoundingBoxGeneratorContext {
public:
    LinesBoundingBoxGeneratorContext(FloatRect& rect) : m_rect(rect) { }

    void addRect(const FloatRect& rect)
    {
        m_rect.uniteIfNonZero(rect);
    }
private:
    FloatRect& m_rect;
};

LayoutUnit RenderInline::innerPaddingBoxWidth() const
{
    auto firstInlineBoxPaddingBoxLeft = LayoutUnit { };
    auto lastInlineBoxPaddingBoxRight = LayoutUnit { };

    if (LayoutIntegration::LineLayout::containing(*this)) {
        if (auto inlineBox = InlineIterator::firstInlineBoxFor(*this)) {
            firstInlineBoxPaddingBoxLeft = inlineBox->logicalLeft() + borderStart();
            for (; inlineBox->nextInlineBox(); inlineBox.traverseNextInlineBox()) { }
            ASSERT(inlineBox);
            lastInlineBoxPaddingBoxRight = inlineBox->logicalRight() - borderEnd();
            return std::max(0_lu, lastInlineBoxPaddingBoxRight - firstInlineBoxPaddingBoxLeft);
        }
        return { };
    }

    auto* firstInlineBox = firstLineBox();
    auto* lastInlineBox = lastLineBox();
    if (!firstInlineBox || !lastInlineBox)
        return { };

    if (style().isLeftToRightDirection()) {
        firstInlineBoxPaddingBoxLeft = firstInlineBox->logicalLeft() + firstInlineBox->borderLogicalLeft();
        lastInlineBoxPaddingBoxRight = lastInlineBox->logicalRight() - lastInlineBox->borderLogicalRight();
    } else {
        lastInlineBoxPaddingBoxRight = firstInlineBox->logicalRight() - firstInlineBox->borderLogicalRight();
        firstInlineBoxPaddingBoxLeft = lastInlineBox->logicalLeft() + lastInlineBox->borderLogicalLeft();
    }
    return std::max(0_lu, lastInlineBoxPaddingBoxRight - firstInlineBoxPaddingBoxLeft);
}

LayoutUnit RenderInline::innerPaddingBoxHeight() const
{
    auto innerPaddingBoxLogicalHeight = LayoutUnit { isHorizontalWritingMode() ? linesBoundingBox().height() : linesBoundingBox().width() };
    innerPaddingBoxLogicalHeight -= (borderBefore() + borderAfter());
    return innerPaddingBoxLogicalHeight;
}

IntRect RenderInline::linesBoundingBox() const
{
    if (auto* layout = LayoutIntegration::LineLayout::containing(*this))
        return enclosingIntRect(layout->enclosingBorderBoxRectFor(*this));

    IntRect result;
    
    // See <rdar://problem/5289721>, for an unknown reason the linked list here is sometimes inconsistent, first is non-zero and last is zero.  We have been
    // unable to reproduce this at all (and consequently unable to figure ot why this is happening).  The assert will hopefully catch the problem in debug
    // builds and help us someday figure out why.  We also put in a redundant check of lastLineBox() to avoid the crash for now.
    ASSERT(!firstLineBox() == !lastLineBox());  // Either both are null or both exist.
    if (firstLineBox() && lastLineBox()) {
        // Return the width of the minimal left side and the maximal right side.
        float logicalLeftSide = 0;
        float logicalRightSide = 0;
        for (auto* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
            if (curr == firstLineBox() || curr->logicalLeft() < logicalLeftSide)
                logicalLeftSide = curr->logicalLeft();
            if (curr == firstLineBox() || curr->logicalRight() > logicalRightSide)
                logicalRightSide = curr->logicalRight();
        }
        
        bool isHorizontal = style().isHorizontalWritingMode();
        
        float x = isHorizontal ? logicalLeftSide : firstLineBox()->x();
        float y = isHorizontal ? firstLineBox()->y() : logicalLeftSide;
        float width = isHorizontal ? logicalRightSide - logicalLeftSide : lastLineBox()->logicalBottom() - x;
        float height = isHorizontal ? lastLineBox()->logicalBottom() - y : logicalRightSide - logicalLeftSide;
        result = enclosingIntRect(FloatRect(x, y, width, height));
    }

    return result;
}

LayoutRect RenderInline::linesVisualOverflowBoundingBox() const
{
    if (auto* layout = LayoutIntegration::LineLayout::containing(*this))
        return layout->visualOverflowBoundingBoxRectFor(*this);

    if (!firstLineBox() || !lastLineBox())
        return LayoutRect();

    // Return the width of the minimal left side and the maximal right side.
    LayoutUnit logicalLeftSide = LayoutUnit::max();
    LayoutUnit logicalRightSide = LayoutUnit::min();
    for (auto* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        logicalLeftSide = std::min(logicalLeftSide, curr->logicalLeftVisualOverflow());
        logicalRightSide = std::max(logicalRightSide, curr->logicalRightVisualOverflow());
    }

    const LegacyRootInlineBox& firstRootBox = firstLineBox()->root();
    const LegacyRootInlineBox& lastRootBox = lastLineBox()->root();
    
    LayoutUnit logicalTop = firstLineBox()->logicalTopVisualOverflow(firstRootBox.lineTop());
    LayoutUnit logicalWidth = logicalRightSide - logicalLeftSide;
    LayoutUnit logicalHeight = lastLineBox()->logicalBottomVisualOverflow(lastRootBox.lineBottom()) - logicalTop;
    
    LayoutRect rect(logicalLeftSide, logicalTop, logicalWidth, logicalHeight);
    if (!style().isHorizontalWritingMode())
        rect = rect.transposedRect();
    return rect;
}

LayoutRect RenderInline::linesVisualOverflowBoundingBoxInFragment(const RenderFragmentContainer* fragment) const
{
    ASSERT(fragment);

    if (!firstLineBox() || !lastLineBox())
        return LayoutRect();

    // Return the width of the minimal left side and the maximal right side.
    LayoutUnit logicalLeftSide = LayoutUnit::max();
    LayoutUnit logicalRightSide = LayoutUnit::min();
    LayoutUnit logicalTop;
    LayoutUnit logicalHeight;
    LegacyInlineFlowBox* lastInlineInFragment = 0;
    for (auto* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        const LegacyRootInlineBox& root = curr->root();
        if (root.containingFragment() != fragment) {
            if (lastInlineInFragment)
                break;
            continue;
        }

        if (!lastInlineInFragment)
            logicalTop = curr->logicalTopVisualOverflow(root.lineTop());

        lastInlineInFragment = curr;

        logicalLeftSide = std::min(logicalLeftSide, curr->logicalLeftVisualOverflow());
        logicalRightSide = std::max(logicalRightSide, curr->logicalRightVisualOverflow());
    }

    if (!lastInlineInFragment)
        return LayoutRect();

    logicalHeight = lastInlineInFragment->logicalBottomVisualOverflow(lastInlineInFragment->root().lineBottom()) - logicalTop;
    
    LayoutUnit logicalWidth = logicalRightSide - logicalLeftSide;
    
    LayoutRect rect(logicalLeftSide, logicalTop, logicalWidth, logicalHeight);
    if (!style().isHorizontalWritingMode())
        rect = rect.transposedRect();
    return rect;
}

LayoutRect RenderInline::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    // Only first-letter renderers are allowed in here during layout. They mutate the tree triggering repaints.
#ifndef NDEBUG
    auto insideSelfPaintingInlineBox = [&] {
        if (hasSelfPaintingLayer())
            return true;
        auto* containingBlock = this->containingBlock();
        for (auto* ancestor = this->parent(); ancestor && ancestor != containingBlock; ancestor = ancestor->parent()) {
            if (ancestor->hasSelfPaintingLayer())
                return true;
        }
        return false;
    };
    ASSERT_UNUSED(insideSelfPaintingInlineBox, !view().frameView().layoutContext().isPaintOffsetCacheEnabled() || style().styleType() == PseudoId::FirstLetter || insideSelfPaintingInlineBox());
#endif

    auto knownEmpty = [&] {
        if (firstLineBox())
            return false;
        if (continuation())
            return false;
        if (LayoutIntegration::LineLayout::containing(*this))
            return false;
        return true;
    };

    if (knownEmpty())
        return LayoutRect();

    LayoutRect repaintRect(linesVisualOverflowBoundingBox());
    bool hitRepaintContainer = false;

    // We need to add in the in-flow position offsets of any inlines (including us) up to our
    // containing block.
    RenderBlock* containingBlock = this->containingBlock();
    for (const RenderElement* inlineFlow = this; is<RenderInline>(inlineFlow) && inlineFlow != containingBlock;
         inlineFlow = inlineFlow->parent()) {
         if (inlineFlow == repaintContainer) {
            hitRepaintContainer = true;
            break;
        }
        if (inlineFlow->style().hasInFlowPosition() && inlineFlow->hasLayer())
            repaintRect.move(downcast<RenderInline>(*inlineFlow).layer()->offsetForInFlowPosition());
    }

    LayoutUnit outlineSize { style().outlineSize() };
    repaintRect.inflate(outlineSize);

    if (hitRepaintContainer || !containingBlock)
        return repaintRect;

    if (containingBlock->hasNonVisibleOverflow())
        containingBlock->applyCachedClipAndScrollPosition(repaintRect, repaintContainer, context);

    repaintRect = containingBlock->computeRect(repaintRect, repaintContainer, context);

    if (outlineSize) {
        for (auto& child : childrenOfType<RenderElement>(*this))
            repaintRect.unite(child.rectWithOutlineForRepaint(repaintContainer, outlineSize));

        if (RenderBoxModelObject* continuation = this->continuation()) {
            if (!continuation->isInline() && continuation->parent())
                repaintRect.unite(continuation->rectWithOutlineForRepaint(repaintContainer, outlineSize));
        }
    }

    return repaintRect;
}

LayoutRect RenderInline::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBoxModelObject::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    for (auto& child : childrenOfType<RenderElement>(*this))
        r.unite(child.rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    return r;
}

LayoutRect RenderInline::computeVisibleRectUsingPaintOffset(const LayoutRect& rect) const
{
    LayoutRect adjustedRect = rect;
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (style().hasInFlowPosition() && layer())
        adjustedRect.move(layer()->offsetForInFlowPosition());
    adjustedRect.move(layoutState->paintOffset());
    if (layoutState->isClipped())
        adjustedRect.intersect(layoutState->clipRect());
    return adjustedRect;
}

std::optional<LayoutRect> RenderInline::computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    // Repaint offset cache is only valid for root-relative repainting
    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !container && !context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
        return computeVisibleRectUsingPaintOffset(rect);

    if (container == this)
        return rect;

    bool containerSkipped;
    RenderElement* localContainer = this->container(container, containerSkipped);
    if (!localContainer)
        return rect;

    LayoutRect adjustedRect = rect;
    LayoutPoint topLeft = adjustedRect.location();

    if (style().hasInFlowPosition() && layer()) {
        // Apply the in-flow position offset when invalidating a rectangle. The layer
        // is translated, but the render box isn't, so we need to do this to get the
        // right dirty rect. Since this is called from RenderObject::setStyle, the relative or sticky position
        // flag on the RenderObject has been cleared, so use the one on the style().
        topLeft += layer()->offsetForInFlowPosition();
    }
    
    // FIXME: We ignore the lightweight clipping rect that controls use, since if |o| is in mid-layout,
    // its controlClipRect will be wrong. For overflow clip we use the values cached by the layer.
    adjustedRect.setLocation(topLeft);
    if (localContainer->hasNonVisibleOverflow()) {
        // FIXME: Respect the value of context.options.
        SetForScope change(context.options, context.options | VisibleRectContextOption::ApplyCompositedContainerScrolls);
        bool isEmpty = !downcast<RenderLayerModelObject>(*localContainer).applyCachedClipAndScrollPosition(adjustedRect, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRect;
        }
    }

    if (containerSkipped) {
        // If the repaintContainer is below o, then we need to map the rect into repaintContainer's coordinates.
        LayoutSize containerOffset = container->offsetFromAncestorContainer(*localContainer);
        adjustedRect.move(-containerOffset);
        return adjustedRect;
    }
    return localContainer->computeVisibleRectInContainer(adjustedRect, container, context);
}

LayoutSize RenderInline::offsetFromContainer(RenderElement& container, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    ASSERT(&container == this->container());
    
    LayoutSize offset;    
    if (isInFlowPositioned())
        offset += offsetForInFlowPosition();

    if (is<RenderBox>(container))
        offset -= toLayoutSize(downcast<RenderBox>(container).scrollPosition());

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = (is<RenderBox>(container) && container.style().isFlippedBlocksWritingMode()) || is<RenderFragmentedFlow>(container);

    return offset;
}

void RenderInline::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (ancestorContainer == this)
        return;

    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !ancestorContainer) {
        auto* layoutState = view().frameView().layoutContext().layoutState();
        LayoutSize offset = layoutState->paintOffset();
        if (style().hasInFlowPosition() && layer())
            offset += layer()->offsetForInFlowPosition();
        transformState.move(offset);
        return;
    }

    bool containerSkipped;
    RenderElement* container = this->container(ancestorContainer, containerSkipped);
    if (!container)
        return;

    if (mode.contains(ApplyContainerFlip) && is<RenderBox>(*container)) {
        if (container->style().isFlippedBlocksWritingMode()) {
            LayoutPoint centerPoint(transformState.mappedPoint());
            transformState.move(downcast<RenderBox>(*container).flipForWritingMode(centerPoint) - centerPoint);
        }
        mode.remove(ApplyContainerFlip);
    }

    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    pushOntoTransformState(transformState, mode, ancestorContainer, container, containerOffset, containerSkipped);
    if (containerSkipped)
        return;

    container->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

const RenderObject* RenderInline::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT(ancestorToStopAt != this);

    bool ancestorSkipped;
    RenderElement* container = this->container(ancestorToStopAt, ancestorSkipped);
    if (!container)
        return nullptr;

    pushOntoGeometryMap(geometryMap, ancestorToStopAt, container, ancestorSkipped);

    return ancestorSkipped ? ancestorToStopAt : container;
}

void RenderInline::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    LayoutPoint localPoint(point);
    if (auto* node = nodeForHitTest()) {
        if (isContinuation()) {
            // We're in the continuation of a split inline. Adjust our local point to be in the coordinate space
            // of the principal renderer's containing block. This will end up being the innerNonSharedNode.
            auto* firstBlock = node->renderer()->containingBlock();
            localPoint.moveBy(containingBlock()->location() - firstBlock->locationOffset());
        }

        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);
        result.setLocalPoint(localPoint);
    }
}

void RenderInline::dirtyLineBoxes(bool fullLayout)
{
    if (fullLayout) {
        m_lineBoxes.deleteLineBoxes();
        return;
    }

    m_lineBoxes.dirtyLineBoxes();
}

void RenderInline::deleteLines()
{
    m_lineBoxes.deleteLineBoxTree();
}

std::unique_ptr<LegacyInlineFlowBox> RenderInline::createInlineFlowBox()
{
    return makeUnique<LegacyInlineFlowBox>(*this);
}

LegacyInlineFlowBox* RenderInline::createAndAppendInlineFlowBox()
{
    auto newFlowBox = createInlineFlowBox();
    auto flowBox = newFlowBox.get();
    m_lineBoxes.appendLineBox(WTFMove(newFlowBox));
    return flowBox;
}

LayoutUnit RenderInline::lineHeight(bool firstLine, LineDirectionMode /*direction*/, LinePositionMode /*linePositionMode*/) const
{
    auto& lineStyle = firstLine ? firstLineStyle() : style();
    return lineStyle.computedLineHeight();
}

LayoutUnit RenderInline::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    const RenderStyle& style = firstLine ? firstLineStyle() : this->style();
    const FontMetrics& fontMetrics = style.metricsOfPrimaryFont();
    return LayoutUnit { (fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2).toInt() };
}

LayoutSize RenderInline::offsetForInFlowPositionedInline(const RenderBox* child) const
{
    // FIXME: This function isn't right with mixed writing modes.

    ASSERT(isInFlowPositioned());
    if (!isInFlowPositioned())
        return LayoutSize();

    // When we have an enclosing relpositioned inline, we need to add in the offset of the first line
    // box from the rest of the content, but only in the cases where we know we're positioned
    // relative to the inline itself.
    auto inlinePosition = layer()->staticInlinePosition();
    auto blockPosition = layer()->staticBlockPosition();
    if (firstLineBox()) {
        inlinePosition = LayoutUnit::fromFloatRound(firstLineBox()->logicalLeft());
        blockPosition = firstLineBox()->logicalTop();
    } else if (LayoutIntegration::LineLayout::containing(*this)) {
        if (auto inlineBox = InlineIterator::firstInlineBoxFor(*this)) {
            inlinePosition = LayoutUnit::fromFloatRound(inlineBox->logicalLeft());
            blockPosition = inlineBox->logicalTop();
        }
    }

    // Per http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width an absolute positioned box with a static position
    // should locate itself as though it is a normal flow box in relation to its containing block.
    LayoutSize logicalOffset;
    if (!child->style().hasStaticInlinePosition(style().isHorizontalWritingMode()))
        logicalOffset.setWidth(inlinePosition);

    if (!child->style().hasStaticBlockPosition(style().isHorizontalWritingMode()))
        logicalOffset.setHeight(blockPosition);

    return style().isHorizontalWritingMode() ? logicalOffset : logicalOffset.transposedSize();
}

void RenderInline::imageChanged(WrappedImagePtr, const IntRect*)
{
    if (!parent())
        return;
        
    // FIXME: We can do better.
    repaint();
}

void RenderInline::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer) const
{
    AbsoluteRectsGeneratorContext context(rects, additionalOffset);
    generateLineBoxRects(context);

    for (auto& child : childrenOfType<RenderElement>(*this)) {
        if (is<RenderListMarker>(child))
            continue;
        FloatPoint pos(additionalOffset);
        // FIXME: This doesn't work correctly with transforms.
        if (child.hasLayer())
            pos = child.localToContainerPoint(FloatPoint(), paintContainer);
        else if (is<RenderBox>(child))
            pos.move(downcast<RenderBox>(child).locationOffset());
        child.addFocusRingRects(rects, flooredIntPoint(pos), paintContainer);
    }

    if (RenderBoxModelObject* continuation = this->continuation()) {
        if (continuation->isInline())
            continuation->addFocusRingRects(rects, flooredLayoutPoint(LayoutPoint(additionalOffset + continuation->containingBlock()->location() - containingBlock()->location())), paintContainer);
        else
            continuation->addFocusRingRects(rects, flooredLayoutPoint(LayoutPoint(additionalOffset + downcast<RenderBox>(*continuation).location() - containingBlock()->location())), paintContainer);
    }
}

void RenderInline::paintOutline(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!hasOutline())
        return;

    auto& styleToUse = style();
    // Only paint the focus ring by hand if the theme isn't able to draw it.
    if (styleToUse.outlineStyleIsAuto() == OutlineIsAuto::On && !theme().supportsFocusRing(styleToUse)) {
        Vector<LayoutRect> focusRingRects;
        addFocusRingRects(focusRingRects, paintOffset, paintInfo.paintContainer);
        paintFocusRing(paintInfo, styleToUse, focusRingRects);
    }

    if (hasOutlineAnnotation() && styleToUse.outlineStyleIsAuto() == OutlineIsAuto::Off && !theme().supportsFocusRing(styleToUse))
        addPDFURLRect(paintInfo, paintOffset);

    GraphicsContext& graphicsContext = paintInfo.context();
    if (graphicsContext.paintingDisabled())
        return;

    if (styleToUse.outlineStyleIsAuto() == OutlineIsAuto::On || !styleToUse.hasOutline())
        return;

    Vector<LayoutRect> rects;
    for (auto box = InlineIterator::firstInlineBoxFor(*this); box; box.traverseNextInlineBox()) {
        auto lineBox = box->lineBox();
        auto top = LayoutUnit { std::max(lineBox->contentLogicalTop(), box->logicalTop()) };
        auto bottom = LayoutUnit { std::min(lineBox->contentLogicalBottom(), box->logicalBottom()) };
        // FIXME: This is mixing physical and logical coordinates.
        rects.append({ LayoutUnit(box->visualRectIgnoringBlockDirection().x()), top, LayoutUnit(box->logicalWidth()), bottom - top });
    }
    BorderPainter { *this, paintInfo }.paintOutline(paintOffset, rects);
}

bool isEmptyInline(const RenderInline& renderer)
{
    for (auto& current : childrenOfType<RenderObject>(renderer)) {
        if (current.isFloatingOrOutOfFlowPositioned())
            continue;
        if (is<RenderText>(current)) {
            if (!downcast<RenderText>(current).isAllCollapsibleWhitespace())
                return false;
            continue;
        }
        if (!is<RenderInline>(current) || !isEmptyInline(downcast<RenderInline>(current)))
            return false;
    }
    return true;
}

} // namespace WebCore
