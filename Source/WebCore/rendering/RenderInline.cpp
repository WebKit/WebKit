/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyInlineTextBox.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
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
#include "WillChangeData.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderInline);

RenderInline::RenderInline(Type type, Element& element, RenderStyle&& style)
    : RenderBoxModelObject(type, element, WTFMove(style), TypeFlag::IsRenderInline, { })
{
    setChildrenInline(true);
    ASSERT(isRenderInline());
}

RenderInline::RenderInline(Type type, Document& document, RenderStyle&& style)
    : RenderBoxModelObject(type, document, WTFMove(style), TypeFlag::IsRenderInline, { })
{
    setChildrenInline(true);
    ASSERT(isRenderInline());
}

void RenderInline::willBeDestroyed()
{
#if ASSERT_ENABLED
    // Make sure we do not retain "this" in the continuation outline table map of our containing blocks.
    if (parent() && style().usedVisibility() == Visibility::Visible && hasOutline()) {
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

        CheckedPtr block = dynamicDowncast<RenderBlock>(*box);
        if (!block)
            continue;

        if (!block->isContinuation())
            continue;
        
        // If we are no longer in-flow positioned but our descendant block(s) still have an in-flow positioned ancestor then
        // their containing anonymous block should keep its in-flow positioning. 
        RenderInline* continuation = block->inlineContinuation();
        if (oldStyle->hasInFlowPosition() && inFlowPositionedInlineAncestor(continuation))
            continue;
        auto blockStyle = RenderStyle::createAnonymousStyleWithDisplay(block->style(), DisplayType::Block);
        blockStyle.setPosition(newStyle->position());
        block->setStyle(WTFMove(blockStyle));
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

    if (diff >= StyleDifference::Repaint && selfNeedsLayout()) {
        if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
            lineLayout->flow().invalidateLineLayoutPath(RenderBlockFlow::InvalidationReason::StyleChange);
    }

    propagateStyleToAnonymousChildren(StylePropagationType::AllChildren);
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

void RenderInline::boundingRects(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    AbsoluteRectsGeneratorContext context(rects, accumulatedOffset);
    generateLineBoxRects(context);

    if (auto* continuation = this->continuation()) {
        if (auto* box = dynamicDowncast<RenderBox>(*continuation)) {
            continuation->boundingRects(rects, toLayoutPoint(accumulatedOffset - containingBlock()->location() + box->locationOffset()));
        } else
            continuation->boundingRects(rects, toLayoutPoint(accumulatedOffset - containingBlock()->location()));
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

VisiblePosition RenderInline::positionForPoint(const LayoutPoint& point, HitTestSource source, const RenderFragmentContainer* fragment)
{
    auto& containingBlock = *this->containingBlock();

    if (auto* continuation = this->continuation()) {
        // Translate the coords from the pre-anonymous block to the post-anonymous block.
        LayoutPoint parentBlockPoint = containingBlock.location() + point;
        while (continuation) {
            RenderBlock* currentBlock = continuation->isInline() ? continuation->containingBlock() : downcast<RenderBlock>(continuation);
            if (continuation->isInline() || continuation->firstChild())
                return continuation->positionForPoint(parentBlockPoint - currentBlock->locationOffset(), source, fragment);
            continuation = continuation->inlineContinuation();
        }
        return RenderBoxModelObject::positionForPoint(point, source, fragment);
    }

    return containingBlock.positionForPoint(point, source, fragment);
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
            if (style().isLeftToRightDirection()) {
                firstInlineBoxPaddingBoxLeft = inlineBox->logicalLeftIgnoringInlineDirection() + borderStart();
                for (; inlineBox->nextInlineBox(); inlineBox.traverseNextInlineBox()) { }
                ASSERT(inlineBox);
                lastInlineBoxPaddingBoxRight = inlineBox->logicalRightIgnoringInlineDirection() - borderEnd();
            } else {
                lastInlineBoxPaddingBoxRight = inlineBox->logicalRightIgnoringInlineDirection() - borderStart();
                for (; inlineBox->nextInlineBox(); inlineBox.traverseNextInlineBox()) { }
                ASSERT(inlineBox);
                firstInlineBoxPaddingBoxLeft = inlineBox->logicalLeftIgnoringInlineDirection() + borderEnd();
            }
            return std::max(0_lu, lastInlineBoxPaddingBoxRight - firstInlineBoxPaddingBoxLeft);
        }
        return { };
    }

    auto* firstInlineBox = firstLineBox();
    auto* lastInlineBox = lastLineBox();
    if (!firstInlineBox || !lastInlineBox)
        return { };

    if (style().isLeftToRightDirection()) {
        firstInlineBoxPaddingBoxLeft = firstInlineBox->logicalLeft();
        lastInlineBoxPaddingBoxRight = lastInlineBox->logicalRight();
    } else {
        lastInlineBoxPaddingBoxRight = firstInlineBox->logicalRight();
        firstInlineBoxPaddingBoxLeft = lastInlineBox->logicalLeft();
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
    if (auto* layout = LayoutIntegration::LineLayout::containing(*this)) {
        if (!layoutBox() || !layout->contains(*this)) {
            // Repaint may be issued on subtrees during content mutation with newly inserted renderers
            // (or we just forgot to initiate layout before querying geometry on stale content after moving inline boxes between blocks).
            ASSERT(needsLayout());
            return { };
        }
        return enclosingIntRect(layout->enclosingBorderBoxRectFor(*this));
    }

    // See <rdar://problem/5289721>, for an unknown reason the linked list here is sometimes inconsistent, first is non-zero and last is zero.  We have been
    // unable to reproduce this at all (and consequently unable to figure ot why this is happening).  The assert will hopefully catch the problem in debug
    // builds and help us someday figure out why.  We also put in a redundant check of lastLineBox() to avoid the crash for now.
    ASSERT(!firstLineBox() == !lastLineBox());  // Either both are null or both exist.
    IntRect result;
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
    if (auto* layout = LayoutIntegration::LineLayout::containing(*this)) {
        if (!layoutBox()) {
            // Repaint may be issued on subtrees during content mutation with newly inserted renderers. 
            ASSERT(needsLayout());
            return { };
        }
        return layout->visualOverflowBoundingBoxRectFor(*this);
    }

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
    ASSERT_UNUSED(insideSelfPaintingInlineBox, !view().frameView().layoutContext().isPaintOffsetCacheEnabled() || style().pseudoElementType() == PseudoId::FirstLetter || insideSelfPaintingInlineBox());
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

    auto repaintRect = linesVisualOverflowBoundingBox();
    bool hitRepaintContainer = false;

    // We need to add in the in-flow position offsets of any inlines (including us) up to our
    // containing block.
    RenderBlock* containingBlock = this->containingBlock();
    for (const RenderElement* inlineFlow = this; inlineFlow; inlineFlow = inlineFlow->parent()) {
        auto* renderInline = dynamicDowncast<RenderInline>(*inlineFlow);
        if (!renderInline || inlineFlow == containingBlock)
            break;
        if (inlineFlow == repaintContainer) {
            hitRepaintContainer = true;
            break;
        }
        if (inlineFlow->style().hasInFlowPosition() && inlineFlow->hasLayer())
            repaintRect.move(renderInline->layer()->offsetForInFlowPosition());
    }

    LayoutUnit outlineSize { style().outlineSize() };
    repaintRect.inflate(outlineSize);

    if (hitRepaintContainer || !containingBlock)
        return repaintRect;

    auto rects = RepaintRects { repaintRect };

    if (containingBlock->hasNonVisibleOverflow())
        containingBlock->applyCachedClipAndScrollPosition(rects, repaintContainer, context);

    rects = containingBlock->computeRects(rects, repaintContainer, context);
    repaintRect = rects.clippedOverflowRect;

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

auto RenderInline::rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds) const -> RepaintRects
{
    // RepaintOutlineBounds is unused for inlines.
    return { clippedOverflowRect(repaintContainer, visibleRectContextForRepaint()) };
}

LayoutRect RenderInline::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBoxModelObject::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    for (auto& child : childrenOfType<RenderElement>(*this))
        r.unite(child.rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    return r;
}

auto RenderInline::computeVisibleRectsUsingPaintOffset(const RepaintRects& rects) const -> RepaintRects
{
    auto adjustedRects = rects;
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (style().hasInFlowPosition() && layer())
        adjustedRects.move(layer()->offsetForInFlowPosition());
    adjustedRects.move(layoutState->paintOffset());
    if (layoutState->isClipped())
        adjustedRects.clippedOverflowRect.intersect(layoutState->clipRect());
    return adjustedRects;
}

auto RenderInline::computeVisibleRectsInContainer(const RepaintRects& rects, const RenderLayerModelObject* container, VisibleRectContext context) const -> std::optional<RepaintRects>
{
    // Repaint offset cache is only valid for root-relative repainting
    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !container && !context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
        return computeVisibleRectsUsingPaintOffset(rects);

    if (container == this)
        return rects;

    bool containerSkipped;
    RenderElement* localContainer = this->container(container, containerSkipped);
    if (!localContainer)
        return rects;

    auto adjustedRects = rects;
    if (style().hasInFlowPosition() && layer()) {
        // Apply the in-flow position offset when invalidating a rectangle. The layer
        // is translated, but the render box isn't, so we need to do this to get the
        // right dirty rect. Since this is called from RenderObject::setStyle, the relative or sticky position
        // flag on the RenderObject has been cleared, so use the one on the style().
        auto offsetForInFlowPosition = layer()->offsetForInFlowPosition();
        adjustedRects.move(offsetForInFlowPosition);
    }

    if (localContainer->hasNonVisibleOverflow()) {
        // FIXME: Respect the value of context.options.
        SetForScope change(context.options, context.options | VisibleRectContextOption::ApplyCompositedContainerScrolls);
        bool isEmpty = !downcast<RenderLayerModelObject>(*localContainer).applyCachedClipAndScrollPosition(adjustedRects, container, context);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRects;
        }
    }

    if (containerSkipped) {
        // If the repaintContainer is below o, then we need to map the rect into repaintContainer's coordinates.
        auto containerOffset = container->offsetFromAncestorContainer(*localContainer);
        adjustedRects.move(-containerOffset);
        return adjustedRects;
    }

    return localContainer->computeVisibleRectsInContainer(adjustedRects, container, context);
}

LayoutSize RenderInline::offsetFromContainer(RenderElement& container, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    ASSERT(&container == this->container());
    
    LayoutSize offset;    
    if (isInFlowPositioned())
        offset += offsetForInFlowPosition();

    if (auto* box = dynamicDowncast<RenderBox>(container))
        offset -= toLayoutSize(box->scrollPosition());

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

    if (mode.contains(ApplyContainerFlip)) {
        if (CheckedPtr box = dynamicDowncast<RenderBox>(*container)) {
            if (container->style().isFlippedBlocksWritingMode()) {
                LayoutPoint centerPoint(transformState.mappedPoint());
                transformState.move(box->flipForWritingMode(centerPoint) - centerPoint);
            }
            mode.remove(ApplyContainerFlip);
        }
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
    if (RefPtr node = nodeForHitTest()) {
        if (isContinuation()) {
            // We're in the continuation of a split inline. Adjust our local point to be in the coordinate space
            // of the principal renderer's containing block. This will end up being the innerNonSharedNode.
            auto* firstBlock = node->renderer()->containingBlock();
            localPoint.moveBy(containingBlock()->location() - firstBlock->locationOffset());
        }

        result.setInnerNode(node.get());
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node.get());
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
    return LayoutUnit::fromFloatCeil(lineStyle.computedLineHeight());
}

LayoutUnit RenderInline::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    auto& style = firstLine ? firstLineStyle() : this->style();
    auto& fontMetrics = style.metricsOfPrimaryFont();
    return LayoutUnit { fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2 };
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
        if (!layoutBox()) {
            // Repaint may be issued on subtrees during content mutation with newly inserted renderers.
            ASSERT(needsLayout());
            return LayoutSize();
        }
        if (auto inlineBox = InlineIterator::firstInlineBoxFor(*this)) {
            inlinePosition = LayoutUnit::fromFloatRound(inlineBox->logicalLeftIgnoringInlineDirection());
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

namespace {
    class AbsoluteRectsIgnoringEmptyGeneratorContext : public AbsoluteRectsGeneratorContext {
        public:
            AbsoluteRectsIgnoringEmptyGeneratorContext(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset)
                : AbsoluteRectsGeneratorContext(rects, accumulatedOffset) { }

                void addRect(const FloatRect& rect)
                {
                    if (!rect.isEmpty())
                        AbsoluteRectsGeneratorContext::addRect(rect);
                }
    };
} // unnamed namespace

void RenderInline::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer) const
{
    AbsoluteRectsIgnoringEmptyGeneratorContext context(rects, additionalOffset);
    generateLineBoxRects(context);

    for (auto& child : childrenOfType<RenderElement>(*this)) {
        if (is<RenderListMarker>(child))
            continue;
        FloatPoint pos(additionalOffset);
        // FIXME: This doesn't work correctly with transforms.
        if (child.hasLayer())
            pos = child.localToContainerPoint(FloatPoint(), paintContainer);
        else if (auto* box = dynamicDowncast<RenderBox>(child))
            pos.move(box->locationOffset());
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

    if (!containingBlock()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto isHorizontalWritingMode = this->isHorizontalWritingMode();
    auto& containingBlock = *this->containingBlock();
    auto isFlippedBlocksWritingMode = containingBlock.style().isFlippedBlocksWritingMode();
    Vector<LayoutRect> rects;
    for (auto box = InlineIterator::firstInlineBoxFor(*this); box; box.traverseNextInlineBox()) {
        auto lineBox = box->lineBox();
        auto logicalTop = std::max(lineBox->contentLogicalTop(), box->logicalTop());
        auto logicalBottom = std::min(lineBox->contentLogicalBottom(), box->logicalBottom());
        auto enclosingVisualRect = FloatRect { box->logicalLeftIgnoringInlineDirection(), logicalTop, box->logicalWidth(), logicalBottom - logicalTop };

        if (!isHorizontalWritingMode)
            enclosingVisualRect = enclosingVisualRect.transposedRect();

        if (isFlippedBlocksWritingMode)
            containingBlock.flipForWritingMode(enclosingVisualRect);

        rects.append(LayoutRect { enclosingVisualRect });
    }
    BorderPainter { *this, paintInfo }.paintOutline(paintOffset, rects);
}

bool isEmptyInline(const RenderInline& renderer)
{
    for (auto& current : childrenOfType<RenderObject>(renderer)) {
        if (current.isFloatingOrOutOfFlowPositioned())
            continue;
        if (auto* text = dynamicDowncast<RenderText>(current)) {
            if (!text->containsOnlyCollapsibleWhitespace())
                return false;
            continue;
        }
        auto* renderInline = dynamicDowncast<RenderInline>(current);
        if (!renderInline || !isEmptyInline(*renderInline))
            return false;
    }
    return true;
}

inline bool RenderInline::willChangeCreatesStackingContext() const
{
    return style().willChange() && style().willChange()->canCreateStackingContext();
}

bool RenderInline::requiresLayer() const
{
    return isInFlowPositioned() || createsGroup() || hasClipPath() || shouldApplyPaintContainment() || willChangeCreatesStackingContext() || hasRunningAcceleratedAnimations() || hasViewTransitionName();
}

} // namespace WebCore
