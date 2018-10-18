/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 *
 */

#include "config.h"
#include "RenderInline.h"

#include "Chrome.h"
#include "FloatQuad.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "InlineElementBox.h"
#include "InlineTextBox.h"
#include "LayoutState.h"
#include "RenderBlock.h"
#include "RenderChildIterator.h"
#include "RenderFragmentedFlow.h"
#include "RenderFullScreen.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
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

#if ENABLE(DASHBOARD_SUPPORT)
#include "Frame.h"
#endif

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
#if !ASSERT_DISABLED
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
#endif

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
        auto* container = containingBlockForAbsolutePosition();
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

    if (!alwaysCreateLineBoxes()) {
        bool alwaysCreateLineBoxes = hasSelfPaintingLayer() || hasVisibleBoxDecorations() || newStyle.hasBorder() || newStyle.hasPadding() || newStyle.hasMargin() || hasOutline();
        if (oldStyle && alwaysCreateLineBoxes) {
            dirtyLineBoxes(false);
            setNeedsLayout();
        }
        setRenderInlineAlwaysCreatesLineBoxes(alwaysCreateLineBoxes);
    }
}

void RenderInline::updateAlwaysCreateLineBoxes(bool fullLayout)
{
    // Once we have been tainted once, just assume it will happen again. This way effects like hover highlighting that change the
    // background color will only cause a layout on the first rollover.
    if (alwaysCreateLineBoxes())
        return;

    auto* parentStyle = &parent()->style();
    RenderInline* parentRenderInline = is<RenderInline>(*parent()) ? downcast<RenderInline>(parent()) : nullptr;
    bool checkFonts = document().inNoQuirksMode();
    bool alwaysCreateLineBoxes = (parentRenderInline && parentRenderInline->alwaysCreateLineBoxes())
        || (parentRenderInline && parentStyle->verticalAlign() != VerticalAlign::Baseline)
        || style().verticalAlign() != VerticalAlign::Baseline
        || style().textEmphasisMark() != TextEmphasisMark::None
        || (checkFonts && (!parentStyle->fontCascade().fontMetrics().hasIdenticalAscentDescentAndLineGap(style().fontCascade().fontMetrics())
        || parentStyle->lineHeight() != style().lineHeight()));

    if (!alwaysCreateLineBoxes && checkFonts && view().usesFirstLineRules()) {
        // Have to check the first line style as well.
        parentStyle = &parent()->firstLineStyle();
        auto& childStyle = firstLineStyle();
        alwaysCreateLineBoxes = !parentStyle->fontCascade().fontMetrics().hasIdenticalAscentDescentAndLineGap(childStyle.fontCascade().fontMetrics())
            || childStyle.verticalAlign() != VerticalAlign::Baseline
            || parentStyle->lineHeight() != childStyle.lineHeight();
    }

    if (alwaysCreateLineBoxes) {
        if (!fullLayout)
            dirtyLineBoxes(false);
        setAlwaysCreateLineBoxes();
    }
}

LayoutRect RenderInline::localCaretRect(InlineBox* inlineBox, unsigned, LayoutUnit* extraWidthToEndOfLine)
{
    if (firstChild()) {
        // This condition is possible if the RenderInline is at an editing boundary,
        // i.e. the VisiblePosition is:
        //   <RenderInline editingBoundary=true>|<RenderText> </RenderText></RenderInline>
        // FIXME: need to figure out how to make this return a valid rect, note that
        // there are no line boxes created in the above case.
        return LayoutRect();
    }

    ASSERT_UNUSED(inlineBox, !inlineBox);

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = 0;

    LayoutRect caretRect = localCaretRectForEmptyElement(horizontalBorderAndPaddingExtent(), 0);

    if (InlineBox* firstBox = firstLineBox())
        caretRect.moveBy(LayoutPoint(firstBox->topLeft()));

    return caretRect;
}

void RenderInline::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    m_lineBoxes.paint(this, paintInfo, paintOffset);
}

template<typename GeneratorContext>
void RenderInline::generateLineBoxRects(GeneratorContext& context) const
{
    if (!alwaysCreateLineBoxes())
        generateCulledLineBoxRects(context, this);
    else if (InlineFlowBox* curr = firstLineBox()) {
        for (; curr; curr = curr->nextLineBox())
            context.addRect(FloatRect(curr->topLeft(), curr->size()));
    } else
        context.addRect(FloatRect());
}

template<typename GeneratorContext>
void RenderInline::generateCulledLineBoxRects(GeneratorContext& context, const RenderInline* container) const
{
    if (!culledInlineFirstLineBox()) {
        context.addRect(FloatRect());
        return;
    }

    bool isHorizontal = style().isHorizontalWritingMode();

    for (auto& current : childrenOfType<RenderObject>(*this)) {
        if (current.isFloatingOrOutOfFlowPositioned())
            continue;

        // We want to get the margin box in the inline direction, and then use our font ascent/descent in the block
        // direction (aligned to the root box's baseline).
        if (is<RenderBox>(current)) {
            auto& renderBox = downcast<RenderBox>(current);
            if (renderBox.inlineBoxWrapper()) {
                const RootInlineBox& rootBox = renderBox.inlineBoxWrapper()->root();
                const RenderStyle& containerStyle = rootBox.isFirstLine() ? container->firstLineStyle() : container->style();
                int logicalTop = rootBox.logicalTop() + (rootBox.lineStyle().fontCascade().fontMetrics().ascent() - containerStyle.fontCascade().fontMetrics().ascent());
                int logicalHeight = containerStyle.fontCascade().fontMetrics().height();
                if (isHorizontal)
                    context.addRect(FloatRect(renderBox.inlineBoxWrapper()->x() - renderBox.marginLeft(), logicalTop, renderBox.width() + renderBox.horizontalMarginExtent(), logicalHeight));
                else
                    context.addRect(FloatRect(logicalTop, renderBox.inlineBoxWrapper()->y() - renderBox.marginTop(), logicalHeight, renderBox.height() + renderBox.verticalMarginExtent()));
            }
        } else if (is<RenderInline>(current)) {
            // If the child doesn't need line boxes either, then we can recur.
            auto& renderInline = downcast<RenderInline>(current);
            if (!renderInline.alwaysCreateLineBoxes())
                renderInline.generateCulledLineBoxRects(context, container);
            else {
                for (InlineFlowBox* childLine = renderInline.firstLineBox(); childLine; childLine = childLine->nextLineBox()) {
                    const RootInlineBox& rootBox = childLine->root();
                    const RenderStyle& containerStyle = rootBox.isFirstLine() ? container->firstLineStyle() : container->style();
                    int logicalTop = rootBox.logicalTop() + (rootBox.lineStyle().fontCascade().fontMetrics().ascent() - containerStyle.fontCascade().fontMetrics().ascent());
                    int logicalHeight = containerStyle.fontMetrics().height();
                    if (isHorizontal) {
                        context.addRect(FloatRect(childLine->x() - childLine->marginLogicalLeft(),
                            logicalTop,
                            childLine->logicalWidth() + childLine->marginLogicalLeft() + childLine->marginLogicalRight(),
                            logicalHeight));
                    } else {
                        context.addRect(FloatRect(logicalTop,
                            childLine->y() - childLine->marginLogicalLeft(),
                            logicalHeight,
                            childLine->logicalWidth() + childLine->marginLogicalLeft() + childLine->marginLogicalRight()));
                    }
                }
            }
        } else if (is<RenderText>(current)) {
            auto& currText = downcast<RenderText>(current);
            for (InlineTextBox* childText = currText.firstTextBox(); childText; childText = childText->nextTextBox()) {
                const RootInlineBox& rootBox = childText->root();
                const RenderStyle& containerStyle = rootBox.isFirstLine() ? container->firstLineStyle() : container->style();
                int logicalTop = rootBox.logicalTop() + (rootBox.lineStyle().fontCascade().fontMetrics().ascent() - containerStyle.fontCascade().fontMetrics().ascent());
                int logicalHeight = containerStyle.fontCascade().fontMetrics().height();
                if (isHorizontal)
                    context.addRect(FloatRect(childText->x(), logicalTop, childText->logicalWidth(), logicalHeight));
                else
                    context.addRect(FloatRect(logicalTop, childText->y(), logicalHeight, childText->logicalWidth()));
            }
        } else if (is<RenderLineBreak>(current)) {
            if (auto* inlineBox = downcast<RenderLineBreak>(current).inlineBoxWrapper()) {
                // FIXME: This could use a helper to share these with text path.
                const RootInlineBox& rootBox = inlineBox->root();
                const RenderStyle& containerStyle = rootBox.isFirstLine() ? container->firstLineStyle() : container->style();
                int logicalTop = rootBox.logicalTop() + (rootBox.lineStyle().fontCascade().fontMetrics().ascent() - containerStyle.fontCascade().fontMetrics().ascent());
                int logicalHeight = containerStyle.fontMetrics().height();
                if (isHorizontal)
                    context.addRect(FloatRect(inlineBox->x(), logicalTop, inlineBox->logicalWidth(), logicalHeight));
                else
                    context.addRect(FloatRect(logicalTop, inlineBox->y(), logicalHeight, inlineBox->logicalWidth()));
            }
        }
    }
}

namespace {

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

} // unnamed namespace

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
    AbsoluteQuadsGeneratorContext context(this, quads);
    generateLineBoxRects(context);

    if (RenderBoxModelObject* continuation = this->continuation())
        continuation->absoluteQuads(quads, wasFixed);
}

#if PLATFORM(IOS)
void RenderInline::absoluteQuadsForSelection(Vector<FloatQuad>& quads) const
{
    AbsoluteQuadsGeneratorContext context(this, quads);
    generateLineBoxRects(context);
}
#endif

LayoutUnit RenderInline::offsetLeft() const
{
    LayoutPoint topLeft;
    if (InlineBox* firstBox = firstLineBoxIncludingCulling())
        topLeft = flooredLayoutPoint(firstBox->topLeft());
    return adjustedPositionRelativeToOffsetParent(topLeft).x();
}

LayoutUnit RenderInline::offsetTop() const
{
    LayoutPoint topLeft;
    if (InlineBox* firstBox = firstLineBoxIncludingCulling())
        topLeft = flooredLayoutPoint(firstBox->topLeft());
    return adjustedPositionRelativeToOffsetParent(topLeft).y();
}

static LayoutUnit computeMargin(const RenderInline* renderer, const Length& margin)
{
    if (margin.isAuto())
        return 0;
    if (margin.isFixed())
        return margin.value();
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

const char* RenderInline::renderName() const
{
    if (isRelativelyPositioned())
        return "RenderInline (relative positioned)";
    if (isStickilyPositioned())
        return "RenderInline (sticky positioned)";
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderInline (generated)";
    if (isAnonymous())
        return "RenderInline (generated)";
    return "RenderInline";
}

bool RenderInline::nodeAtPoint(const HitTestRequest& request, HitTestResult& result,
                                const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    return m_lineBoxes.hitTest(this, request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

namespace {

class HitTestCulledInlinesGeneratorContext {
public:
    HitTestCulledInlinesGeneratorContext(Region& region, const HitTestLocation& location)
        : m_intersected(false)
        , m_region(region)
        , m_location(location)
    { }

    void addRect(const FloatRect& rect)
    {
        m_intersected = m_intersected || m_location.intersects(rect);
        m_region.unite(enclosingIntRect(rect));
    }

    bool intersected() const { return m_intersected; }

private:
    bool m_intersected;
    Region& m_region;
    const HitTestLocation& m_location;
};

} // unnamed namespace

bool RenderInline::hitTestCulledInline(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    ASSERT(result.isRectBasedTest() && !alwaysCreateLineBoxes());
    if (!visibleToHitTesting())
        return false;

    HitTestLocation tmpLocation(locationInContainer, -toLayoutSize(accumulatedOffset));

    Region regionResult;
    HitTestCulledInlinesGeneratorContext context(regionResult, tmpLocation);
    generateCulledLineBoxRects(context, this);

    if (context.intersected()) {
        updateHitTestResult(result, tmpLocation.point());
        // We cannot use addNodeToListBasedTestResult to determine if we fully enclose the hit-test area
        // because it can only handle rectangular targets.
        result.addNodeToListBasedTestResult(element(), request, locationInContainer);
        return regionResult.contains(tmpLocation.boundingBox());
    }
    return false;
}

VisiblePosition RenderInline::positionForPoint(const LayoutPoint& point, const RenderFragmentContainer* fragment)
{
    // FIXME: Does not deal with relative or sticky positioned inlines (should it?)
    RenderBlock& containingBlock = *this->containingBlock();
    if (firstLineBox()) {
        // This inline actually has a line box.  We must have clicked in the border/padding of one of these boxes.  We
        // should try to find a result by asking our containing block.
        return containingBlock.positionForPoint(point, fragment);
    }

    // Translate the coords from the pre-anonymous block to the post-anonymous block.
    LayoutPoint parentBlockPoint = containingBlock.location() + point;
    RenderBoxModelObject* continuation = this->continuation();
    while (continuation) {
        RenderBlock* currentBlock = continuation->isInline() ? continuation->containingBlock() : downcast<RenderBlock>(continuation);
        if (continuation->isInline() || continuation->firstChild())
            return continuation->positionForPoint(parentBlockPoint - currentBlock->locationOffset(), fragment);
        continuation = continuation->inlineContinuation();
    }
    
    return RenderBoxModelObject::positionForPoint(point, fragment);
}

namespace {

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

} // unnamed namespace

IntRect RenderInline::linesBoundingBox() const
{
    if (!alwaysCreateLineBoxes()) {
        ASSERT(!firstLineBox());
        FloatRect floatResult;
        LinesBoundingBoxGeneratorContext context(floatResult);
        generateCulledLineBoxRects(context, this);
        return enclosingIntRect(floatResult);
    }

    IntRect result;
    
    // See <rdar://problem/5289721>, for an unknown reason the linked list here is sometimes inconsistent, first is non-zero and last is zero.  We have been
    // unable to reproduce this at all (and consequently unable to figure ot why this is happening).  The assert will hopefully catch the problem in debug
    // builds and help us someday figure out why.  We also put in a redundant check of lastLineBox() to avoid the crash for now.
    ASSERT(!firstLineBox() == !lastLineBox());  // Either both are null or both exist.
    if (firstLineBox() && lastLineBox()) {
        // Return the width of the minimal left side and the maximal right side.
        float logicalLeftSide = 0;
        float logicalRightSide = 0;
        for (InlineFlowBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
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

InlineBox* RenderInline::culledInlineFirstLineBox() const
{
    for (auto& current : childrenOfType<RenderObject>(*this)) {
        if (current.isFloatingOrOutOfFlowPositioned())
            continue;

        // We want to get the margin box in the inline direction, and then use our font ascent/descent in the block
        // direction (aligned to the root box's baseline).
        if (is<RenderBox>(current)) {
            auto& renderBox = downcast<RenderBox>(current);
            if (renderBox.inlineBoxWrapper())
                return renderBox.inlineBoxWrapper();
        } else if (is<RenderLineBreak>(current)) {
            auto& renderBR = downcast<RenderLineBreak>(current);
            if (renderBR.inlineBoxWrapper())
                return renderBR.inlineBoxWrapper();
        } else if (is<RenderInline>(current)) {
            auto& renderInline = downcast<RenderInline>(current);
            if (InlineBox* result = renderInline.firstLineBoxIncludingCulling())
                return result;
        } else if (is<RenderText>(current)) {
            auto& renderText = downcast<RenderText>(current);
            if (renderText.firstTextBox())
                return renderText.firstTextBox();
        }
    }
    return nullptr;
}

InlineBox* RenderInline::culledInlineLastLineBox() const
{
    for (RenderObject* current = lastChild(); current; current = current->previousSibling()) {
        if (current->isFloatingOrOutOfFlowPositioned())
            continue;
            
        // We want to get the margin box in the inline direction, and then use our font ascent/descent in the block
        // direction (aligned to the root box's baseline).
        if (is<RenderBox>(*current)) {
            const auto& renderBox = downcast<RenderBox>(*current);
            if (renderBox.inlineBoxWrapper())
                return renderBox.inlineBoxWrapper();
        } else if (is<RenderLineBreak>(*current)) {
            RenderLineBreak& renderBR = downcast<RenderLineBreak>(*current);
            if (renderBR.inlineBoxWrapper())
                return renderBR.inlineBoxWrapper();
        } else if (is<RenderInline>(*current)) {
            RenderInline& renderInline = downcast<RenderInline>(*current);
            if (InlineBox* result = renderInline.lastLineBoxIncludingCulling())
                return result;
        } else if (is<RenderText>(*current)) {
            RenderText& renderText = downcast<RenderText>(*current);
            if (renderText.lastTextBox())
                return renderText.lastTextBox();
        }
    }
    return nullptr;
}

LayoutRect RenderInline::culledInlineVisualOverflowBoundingBox() const
{
    FloatRect floatResult;
    LinesBoundingBoxGeneratorContext context(floatResult);
    generateCulledLineBoxRects(context, this);
    LayoutRect result(enclosingLayoutRect(floatResult));
    bool isHorizontal = style().isHorizontalWritingMode();
    for (auto& current : childrenOfType<RenderObject>(*this)) {
        if (current.isFloatingOrOutOfFlowPositioned())
            continue;

        // For overflow we just have to propagate by hand and recompute it all.
        if (is<RenderBox>(current)) {
            auto& renderBox = downcast<RenderBox>(current);
            if (!renderBox.hasSelfPaintingLayer() && renderBox.inlineBoxWrapper()) {
                LayoutRect logicalRect = renderBox.logicalVisualOverflowRectForPropagation(&style());
                if (isHorizontal) {
                    logicalRect.moveBy(renderBox.location());
                    result.uniteIfNonZero(logicalRect);
                } else {
                    logicalRect.moveBy(renderBox.location());
                    result.uniteIfNonZero(logicalRect.transposedRect());
                }
            }
        } else if (is<RenderInline>(current)) {
            // If the child doesn't need line boxes either, then we can recur.
            auto& renderInline = downcast<RenderInline>(current);
            if (!renderInline.alwaysCreateLineBoxes())
                result.uniteIfNonZero(renderInline.culledInlineVisualOverflowBoundingBox());
            else if (!renderInline.hasSelfPaintingLayer())
                result.uniteIfNonZero(renderInline.linesVisualOverflowBoundingBox());
        } else if (is<RenderText>(current)) {
            // FIXME; Overflow from text boxes is lost. We will need to cache this information in
            // InlineTextBoxes.
            auto& renderText = downcast<RenderText>(current);
            result.uniteIfNonZero(renderText.linesVisualOverflowBoundingBox());
        }
    }
    return result;
}

LayoutRect RenderInline::linesVisualOverflowBoundingBox() const
{
    if (!alwaysCreateLineBoxes())
        return culledInlineVisualOverflowBoundingBox();

    if (!firstLineBox() || !lastLineBox())
        return LayoutRect();

    // Return the width of the minimal left side and the maximal right side.
    LayoutUnit logicalLeftSide = LayoutUnit::max();
    LayoutUnit logicalRightSide = LayoutUnit::min();
    for (InlineFlowBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        logicalLeftSide = std::min(logicalLeftSide, curr->logicalLeftVisualOverflow());
        logicalRightSide = std::max(logicalRightSide, curr->logicalRightVisualOverflow());
    }

    const RootInlineBox& firstRootBox = firstLineBox()->root();
    const RootInlineBox& lastRootBox = lastLineBox()->root();
    
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
    ASSERT(alwaysCreateLineBoxes());
    ASSERT(fragment);

    if (!firstLineBox() || !lastLineBox())
        return LayoutRect();

    // Return the width of the minimal left side and the maximal right side.
    LayoutUnit logicalLeftSide = LayoutUnit::max();
    LayoutUnit logicalRightSide = LayoutUnit::min();
    LayoutUnit logicalTop;
    LayoutUnit logicalHeight;
    InlineFlowBox* lastInlineInFragment = 0;
    for (InlineFlowBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        const RootInlineBox& root = curr->root();
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

LayoutRect RenderInline::clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const
{
    // Only first-letter renderers are allowed in here during layout. They mutate the tree triggering repaints.
    ASSERT(!view().frameView().layoutContext().isPaintOffsetCacheEnabled() || style().styleType() == PseudoId::FirstLetter || hasSelfPaintingLayer());

    if (!firstLineBoxIncludingCulling() && !continuation())
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

    LayoutUnit outlineSize = style().outlineSize();
    repaintRect.inflate(outlineSize);

    if (hitRepaintContainer || !containingBlock)
        return repaintRect;

    if (containingBlock->hasOverflowClip())
        containingBlock->applyCachedClipAndScrollPosition(repaintRect, repaintContainer, visibleRectContextForRepaint());

    repaintRect = containingBlock->computeRectForRepaint(repaintRect, repaintContainer);

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
    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !container && !context.m_options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
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
    if (localContainer->hasOverflowClip()) {
        // FIXME: Respect the value of context.m_options.
        SetForScope<OptionSet<VisibleRectContextOption>> change(context.m_options, context.m_options | VisibleRectContextOption::ApplyCompositedContainerScrolls);
        bool isEmpty = !downcast<RenderBox>(*localContainer).applyCachedClipAndScrollPosition(adjustedRect, container, context);
        if (isEmpty) {
            if (context.m_options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection))
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

void RenderInline::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    if (repaintContainer == this)
        return;

    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !repaintContainer) {
        auto* layoutState = view().frameView().layoutContext().layoutState();
        LayoutSize offset = layoutState->paintOffset();
        if (style().hasInFlowPosition() && layer())
            offset += layer()->offsetForInFlowPosition();
        transformState.move(offset);
        return;
    }

    bool containerSkipped;
    RenderElement* container = this->container(repaintContainer, containerSkipped);
    if (!container)
        return;

    if (mode & ApplyContainerFlip && is<RenderBox>(*container)) {
        if (container->style().isFlippedBlocksWritingMode()) {
            LayoutPoint centerPoint(transformState.mappedPoint());
            transformState.move(downcast<RenderBox>(*container).flipForWritingMode(centerPoint) - centerPoint);
        }
        mode &= ~ApplyContainerFlip;
    }

    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    bool preserve3D = mode & UseTransforms && (container->style().preserves3D() || style().preserves3D());
    if (mode & UseTransforms && shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);

    if (containerSkipped) {
        // There can't be a transform between repaintContainer and o, because transforms create containers, so it should be safe
        // to just subtract the delta between the repaintContainer and o.
        LayoutSize containerOffset = repaintContainer->offsetFromAncestorContainer(*container);
        transformState.move(-containerOffset.width(), -containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
        return;
    }

    container->mapLocalToContainer(repaintContainer, transformState, mode, wasFixed);
}

const RenderObject* RenderInline::pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    ASSERT(ancestorToStopAt != this);

    bool ancestorSkipped;
    RenderElement* container = this->container(ancestorToStopAt, ancestorSkipped);
    if (!container)
        return nullptr;

    LayoutSize adjustmentForSkippedAncestor;
    if (ancestorSkipped) {
        // There can't be a transform between repaintContainer and o, because transforms create containers, so it should be safe
        // to just subtract the delta between the ancestor and o.
        adjustmentForSkippedAncestor = -ancestorToStopAt->offsetFromAncestorContainer(*container);
    }

    bool offsetDependsOnPoint = false;
    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint(), &offsetDependsOnPoint);

    bool preserve3D = container->style().preserves3D() || style().preserves3D();
    if (shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);
        t.translateRight(adjustmentForSkippedAncestor.width(), adjustmentForSkippedAncestor.height()); // FIXME: right?
        geometryMap.push(this, t, preserve3D, offsetDependsOnPoint);
    } else {
        containerOffset += adjustmentForSkippedAncestor;
        geometryMap.push(this, containerOffset, preserve3D, offsetDependsOnPoint);
    }
    
    return ancestorSkipped ? ancestorToStopAt : container;
}

void RenderInline::updateDragState(bool dragOn)
{
    RenderBoxModelObject::updateDragState(dragOn);
    if (RenderBoxModelObject* continuation = this->continuation())
        continuation->updateDragState(dragOn);
}

void RenderInline::updateHitTestResult(HitTestResult& result, const LayoutPoint& point)
{
    if (result.innerNode())
        return;

    LayoutPoint localPoint(point);
    if (Element* element = this->element()) {
        if (isContinuation()) {
            // We're in the continuation of a split inline.  Adjust our local point to be in the coordinate space
            // of the principal renderer's containing block.  This will end up being the innerNonSharedNode.
            RenderBlock* firstBlock = element->renderer()->containingBlock();
            
            // Get our containing block.
            RenderBox* block = containingBlock();
            localPoint.moveBy(block->location() - firstBlock->locationOffset());
        }

        result.setInnerNode(element);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(element);
        result.setLocalPoint(localPoint);
    }
}

void RenderInline::dirtyLineBoxes(bool fullLayout)
{
    if (fullLayout) {
        m_lineBoxes.deleteLineBoxes();
        return;
    }

    if (!alwaysCreateLineBoxes()) {
        // We have to grovel into our children in order to dirty the appropriate lines.
        for (auto& current : childrenOfType<RenderObject>(*this)) {
            if (current.isFloatingOrOutOfFlowPositioned())
                continue;
            if (is<RenderBox>(current) && !current.needsLayout()) {
                auto& renderBox = downcast<RenderBox>(current);
                if (renderBox.inlineBoxWrapper())
                    renderBox.inlineBoxWrapper()->root().markDirty();
            } else if (!current.selfNeedsLayout()) {
                if (is<RenderInline>(current)) {
                    auto& renderInline = downcast<RenderInline>(current);
                    for (InlineFlowBox* childLine = renderInline.firstLineBox(); childLine; childLine = childLine->nextLineBox())
                        childLine->root().markDirty();
                } else if (is<RenderText>(current)) {
                    auto& renderText = downcast<RenderText>(current);
                    for (InlineTextBox* childText = renderText.firstTextBox(); childText; childText = childText->nextTextBox())
                        childText->root().markDirty();
                } else if (is<RenderLineBreak>(current)) {
                    auto& renderBR = downcast<RenderLineBreak>(current);
                    if (renderBR.inlineBoxWrapper())
                        renderBR.inlineBoxWrapper()->root().markDirty();
                }
            }
        }
    } else
        m_lineBoxes.dirtyLineBoxes();
}

void RenderInline::deleteLines()
{
    m_lineBoxes.deleteLineBoxTree();
}

std::unique_ptr<InlineFlowBox> RenderInline::createInlineFlowBox()
{
    return std::make_unique<InlineFlowBox>(*this);
}

InlineFlowBox* RenderInline::createAndAppendInlineFlowBox()
{
    setAlwaysCreateLineBoxes();
    auto newFlowBox = createInlineFlowBox();
    auto flowBox = newFlowBox.get();
    m_lineBoxes.appendLineBox(WTFMove(newFlowBox));
    return flowBox;
}

LayoutUnit RenderInline::lineHeight(bool firstLine, LineDirectionMode /*direction*/, LinePositionMode /*linePositionMode*/) const
{
    if (firstLine && view().usesFirstLineRules()) {
        const RenderStyle& firstLineStyle = this->firstLineStyle();
        if (&firstLineStyle != &style())
            return firstLineStyle.computedLineHeight();
    }

    return style().computedLineHeight();
}

int RenderInline::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    const RenderStyle& style = firstLine ? firstLineStyle() : this->style();
    const FontMetrics& fontMetrics = style.fontMetrics();
    return fontMetrics.ascent(baselineType) + (lineHeight(firstLine, direction, linePositionMode) - fontMetrics.height()) / 2;
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

    LayoutSize logicalOffset;
    LayoutUnit inlinePosition;
    LayoutUnit blockPosition;
    if (firstLineBox()) {
        inlinePosition = LayoutUnit::fromFloatRound(firstLineBox()->logicalLeft());
        blockPosition = firstLineBox()->logicalTop();
    } else {
        inlinePosition = layer()->staticInlinePosition();
        blockPosition = layer()->staticBlockPosition();
    }

    if (!child->style().hasStaticInlinePosition(style().isHorizontalWritingMode()))
        logicalOffset.setWidth(inlinePosition);

    // This is not terribly intuitive, but we have to match other browsers.  Despite being a block display type inside
    // an inline, we still keep our x locked to the left of the relative positioned inline.  Arguably the correct
    // behavior would be to go flush left to the block that contains the inline, but that isn't what other browsers
    // do.
    else if (!child->style().isOriginalDisplayInlineType())
        // Avoid adding in the left border/padding of the containing block twice.  Subtract it out.
        logicalOffset.setWidth(inlinePosition - child->containingBlock()->borderAndPaddingLogicalLeft());

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

void RenderInline::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
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
    rects.append(LayoutRect());
    for (InlineFlowBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        const RootInlineBox& rootBox = curr->root();
        LayoutUnit top = std::max<LayoutUnit>(rootBox.lineTop(), curr->logicalTop());
        LayoutUnit bottom = std::min<LayoutUnit>(rootBox.lineBottom(), curr->logicalBottom());
        rects.append(LayoutRect(curr->x(), top, curr->logicalWidth(), bottom - top));
    }
    rects.append(LayoutRect());

    Color outlineColor = styleToUse.visitedDependentColorWithColorFilter(CSSPropertyOutlineColor);
    bool useTransparencyLayer = !outlineColor.isOpaque();
    if (useTransparencyLayer) {
        graphicsContext.beginTransparencyLayer(outlineColor.alphaAsFloat());
        outlineColor = outlineColor.opaqueColor();
    }

    for (unsigned i = 1; i < rects.size() - 1; i++)
        paintOutlineForLine(graphicsContext, paintOffset, rects.at(i - 1), rects.at(i), rects.at(i + 1), outlineColor);

    if (useTransparencyLayer)
        graphicsContext.endTransparencyLayer();
}

void RenderInline::paintOutlineForLine(GraphicsContext& graphicsContext, const LayoutPoint& paintOffset,
    const LayoutRect& previousLine, const LayoutRect& thisLine, const LayoutRect& nextLine, const Color& outlineColor)
{
    const auto& styleToUse = style();
    float outlineOffset = styleToUse.outlineOffset();
    LayoutRect outlineBoxRect = thisLine;
    outlineBoxRect.inflate(outlineOffset);
    outlineBoxRect.moveBy(paintOffset);
    if (outlineBoxRect.isEmpty())
        return;

    float outlineWidth = styleToUse.outlineWidth();
    BorderStyle outlineStyle = styleToUse.outlineStyle();
    bool antialias = shouldAntialiasLines(graphicsContext);

    auto adjustedPreviousLine = previousLine;
    adjustedPreviousLine.moveBy(paintOffset);
    auto adjustedNextLine = nextLine;
    adjustedNextLine.moveBy(paintOffset);
    
    float adjacentWidth1 = 0;
    float adjacentWidth2 = 0;
    // left edge
    auto topLeft = outlineBoxRect.minXMinYCorner();
    if (previousLine.isEmpty() || thisLine.x() < previousLine.x() || (previousLine.maxX()) <= thisLine.x()) {
        topLeft.move(-outlineWidth, -outlineWidth);
        adjacentWidth1 = outlineWidth;
    } else {
        topLeft.move(-outlineWidth, 2 * outlineOffset);
        adjacentWidth1 = -outlineWidth;
    }
    auto bottomRight = outlineBoxRect.minXMaxYCorner();
    if (nextLine.isEmpty() || thisLine.x() <= nextLine.x() || (nextLine.maxX()) <= thisLine.x()) {
        bottomRight.move(0, outlineWidth);
        adjacentWidth2 = outlineWidth;
    } else {
        bottomRight.move(0, -2 * outlineOffset);
        adjacentWidth2 = -outlineWidth;
    }
    drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSLeft, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);
    
    // right edge
    topLeft = outlineBoxRect.maxXMinYCorner();
    if (previousLine.isEmpty() || previousLine.maxX() < thisLine.maxX() || thisLine.maxX() <= previousLine.x()) {
        topLeft.move(0, -outlineWidth);
        adjacentWidth1 = outlineWidth;
    } else {
        topLeft.move(0, 2 * outlineOffset);
        adjacentWidth1 = -outlineWidth;
    }
    bottomRight = outlineBoxRect.maxXMaxYCorner();
    if (nextLine.isEmpty() || nextLine.maxX() <= thisLine.maxX() || thisLine.maxX() <= nextLine.x()) {
        bottomRight.move(outlineWidth, outlineWidth);
        adjacentWidth2 = outlineWidth;
    } else {
        bottomRight.move(outlineWidth, -2 * outlineOffset);
        adjacentWidth2 = -outlineWidth;
    }
    drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSRight, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);

    // upper edge
    if (thisLine.x() < previousLine.x()) {
        topLeft = outlineBoxRect.minXMinYCorner();
        topLeft.move(-outlineWidth, -outlineWidth);
        adjacentWidth1 = outlineWidth;
        bottomRight = outlineBoxRect.maxXMinYCorner();
        bottomRight.move(outlineWidth, 0);
        if (!previousLine.isEmpty() && adjustedPreviousLine.x() < bottomRight.x()) {
            bottomRight.setX(adjustedPreviousLine.x() - outlineOffset);
            adjacentWidth2 = -outlineWidth;
        } else
            adjacentWidth2 = outlineWidth;
        drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSTop, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);
    }
    
    if (previousLine.maxX() < thisLine.maxX()) {
        topLeft = outlineBoxRect.minXMinYCorner();
        topLeft.move(-outlineWidth, -outlineWidth);
        if (!previousLine.isEmpty() && adjustedPreviousLine.maxX() > topLeft.x()) {
            topLeft.setX(adjustedPreviousLine.maxX() + outlineOffset);
            adjacentWidth1 = -outlineWidth;
        } else
            adjacentWidth1 = outlineWidth;
        bottomRight = outlineBoxRect.maxXMinYCorner();
        bottomRight.move(outlineWidth, 0);
        adjacentWidth2 = outlineWidth;
        drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSTop, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);
    }

    if (thisLine.x() == thisLine.maxX()) {
        topLeft = outlineBoxRect.minXMinYCorner();
        topLeft.move(-outlineWidth, -outlineWidth);
        adjacentWidth1 = outlineWidth;
        bottomRight = outlineBoxRect.maxXMinYCorner();
        bottomRight.move(outlineWidth, 0);
        adjacentWidth2 = outlineWidth;
        drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSTop, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);
    }

    // lower edge
    if (thisLine.x() < nextLine.x()) {
        topLeft = outlineBoxRect.minXMaxYCorner();
        topLeft.move(-outlineWidth, 0);
        adjacentWidth1 = outlineWidth;
        bottomRight = outlineBoxRect.maxXMaxYCorner();
        bottomRight.move(outlineWidth, outlineWidth);
        if (!nextLine.isEmpty() && (adjustedNextLine.x() < bottomRight.x())) {
            bottomRight.setX(adjustedNextLine.x() - outlineOffset);
            adjacentWidth2 = -outlineWidth;
        } else
            adjacentWidth2 = outlineWidth;
        drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSBottom, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);
    }
    
    if (nextLine.maxX() < thisLine.maxX()) {
        topLeft = outlineBoxRect.minXMaxYCorner();
        topLeft.move(-outlineWidth, 0);
        if (!nextLine.isEmpty() && adjustedNextLine.maxX() > topLeft.x()) {
            topLeft.setX(adjustedNextLine.maxX() + outlineOffset);
            adjacentWidth1 = -outlineWidth;
        } else
            adjacentWidth1 = outlineWidth;
        bottomRight = outlineBoxRect.maxXMaxYCorner();
        bottomRight.move(outlineWidth, outlineWidth);
        adjacentWidth2 = outlineWidth;
        drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSBottom, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);
    }

    if (thisLine.x() == thisLine.maxX()) {
        topLeft = outlineBoxRect.minXMaxYCorner();
        topLeft.move(-outlineWidth, 0);
        adjacentWidth1 = outlineWidth;
        bottomRight = outlineBoxRect.maxXMaxYCorner();
        bottomRight.move(outlineWidth, outlineWidth);
        adjacentWidth2 = outlineWidth;
        drawLineForBoxSide(graphicsContext, FloatRect(topLeft, bottomRight), BSBottom, outlineColor, outlineStyle, adjacentWidth1, adjacentWidth2, antialias);
    }
}

#if ENABLE(DASHBOARD_SUPPORT)
void RenderInline::addAnnotatedRegions(Vector<AnnotatedRegionValue>& regions)
{
    // Convert the style regions to absolute coordinates.
    if (style().visibility() != Visibility::Visible)
        return;

    const Vector<StyleDashboardRegion>& styleRegions = style().dashboardRegions();
    unsigned i, count = styleRegions.size();
    for (i = 0; i < count; i++) {
        StyleDashboardRegion styleRegion = styleRegions[i];

        LayoutRect linesBoundingBox = this->linesBoundingBox();
        LayoutUnit w = linesBoundingBox.width();
        LayoutUnit h = linesBoundingBox.height();

        AnnotatedRegionValue region;
        region.label = styleRegion.label;
        region.bounds = LayoutRect(linesBoundingBox.x() + styleRegion.offset.left().value(),
                                linesBoundingBox.y() + styleRegion.offset.top().value(),
                                w - styleRegion.offset.left().value() - styleRegion.offset.right().value(),
                                h - styleRegion.offset.top().value() - styleRegion.offset.bottom().value());
        region.type = styleRegion.type;

        RenderObject* container = containingBlock();
        if (!container)
            container = this;

        region.clip = container->computeAbsoluteRepaintRect(region.bounds);
        if (region.clip.height() < 0) {
            region.clip.setHeight(0);
            region.clip.setWidth(0);
        }

        FloatPoint absPos = container->localToAbsolute();
        region.bounds.setX(absPos.x() + region.bounds.x());
        region.bounds.setY(absPos.y() + region.bounds.y());

        regions.append(region);
    }
}
#endif

} // namespace WebCore
