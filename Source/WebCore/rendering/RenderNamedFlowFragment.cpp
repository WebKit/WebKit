/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "RenderNamedFlowFragment.h"

#include "FlowThreadController.h"
#include "PaintInfo.h"
#include "RenderBoxRegionInfo.h"
#include "RenderFlowThread.h"
#include "RenderNamedFlowThread.h"
#include "RenderView.h"
#include "StyleResolver.h"

#include <wtf/StackStats.h>

namespace WebCore {

RenderNamedFlowFragment::RenderNamedFlowFragment(Document& document, PassRef<RenderStyle> style)
    : RenderRegion(document, std::move(style), nullptr)
    , m_hasCustomRegionStyle(false)
    , m_hasAutoLogicalHeight(false)
    , m_hasComputedAutoHeight(false)
    , m_computedAutoHeight(0)
{
}

RenderNamedFlowFragment::~RenderNamedFlowFragment()
{
}

PassRef<RenderStyle> RenderNamedFlowFragment::createStyle(const RenderStyle& parentStyle)
{
    auto style = RenderStyle::createAnonymousStyleWithDisplay(&parentStyle, BLOCK);

    style.get().setFlowThread(parentStyle.flowThread());
    style.get().setRegionThread(parentStyle.regionThread());
    style.get().setRegionFragment(parentStyle.regionFragment());
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    style.get().setShapeInside(parentStyle.shapeInside());
#endif

    return style;
}

void RenderNamedFlowFragment::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderRegion::styleDidChange(diff, oldStyle);

    // If the region is not attached to any thread, there is no need to check
    // whether the region has region styling since no content will be displayed
    // into the region.
    if (!m_flowThread) {
        setHasCustomRegionStyle(false);
        return;
    }

    updateRegionHasAutoLogicalHeightFlag();

    checkRegionStyle();

    if (parent() && parent()->needsLayout())
        setNeedsLayout(MarkOnlyThis);
}

void RenderNamedFlowFragment::getRanges(Vector<RefPtr<Range>>& rangeObjects) const
{
    const RenderNamedFlowThread& namedFlow = view().flowThreadController().ensureRenderFlowThreadWithName(style().regionThread());
    namedFlow.getRanges(rangeObjects, this);
}

bool RenderNamedFlowFragment::shouldHaveAutoLogicalHeight() const
{
    ASSERT(parent());

    const RenderStyle& styleToUse = parent()->style();
    bool hasSpecifiedEndpointsForHeight = styleToUse.logicalTop().isSpecified() && styleToUse.logicalBottom().isSpecified();
    bool hasAnchoredEndpointsForHeight = parent()->isOutOfFlowPositioned() && hasSpecifiedEndpointsForHeight;
    return styleToUse.logicalHeight().isAuto() && !hasAnchoredEndpointsForHeight;
}

void RenderNamedFlowFragment::incrementAutoLogicalHeightCount()
{
    ASSERT(isValid());
    ASSERT(m_hasAutoLogicalHeight);

    m_flowThread->incrementAutoLogicalHeightRegions();
}

void RenderNamedFlowFragment::decrementAutoLogicalHeightCount()
{
    ASSERT(isValid());

    m_flowThread->decrementAutoLogicalHeightRegions();
}

void RenderNamedFlowFragment::updateRegionHasAutoLogicalHeightFlag()
{
    ASSERT(m_flowThread);

    if (!isValid())
        return;

    bool didHaveAutoLogicalHeight = m_hasAutoLogicalHeight;
    m_hasAutoLogicalHeight = shouldHaveAutoLogicalHeight();
    if (m_hasAutoLogicalHeight != didHaveAutoLogicalHeight) {
        if (m_hasAutoLogicalHeight)
            incrementAutoLogicalHeightCount();
        else {
            clearComputedAutoHeight();
            decrementAutoLogicalHeightCount();
        }
    }
}

void RenderNamedFlowFragment::updateLogicalHeight()
{
    RenderRegion::updateLogicalHeight();

    if (!hasAutoLogicalHeight())
        return;

    // We want to update the logical height based on the computed auto-height
    // only after the measure cotnent layout phase when all the
    // auto logical height regions have a computed auto-height.
    if (m_flowThread->inMeasureContentLayoutPhase())
        return;

    // There may be regions with auto logical height that during the prerequisite layout phase
    // did not have the chance to layout flow thread content. Because of that, these regions do not
    // have a computedAutoHeight and they will not be able to fragment any flow
    // thread content.
    if (!hasComputedAutoHeight())
        return;

    LayoutUnit newLogicalHeight = computedAutoHeight() + borderAndPaddingLogicalHeight();
    ASSERT(newLogicalHeight < RenderFlowThread::maxLogicalHeight());
    if (newLogicalHeight > logicalHeight()) {
        setLogicalHeight(newLogicalHeight);
        // Recalculate position of the render block after new logical height is set.
        // (needed in absolute positioning case with bottom alignment for example)
        RenderRegion::updateLogicalHeight();
    }
}

LayoutUnit RenderNamedFlowFragment::pageLogicalHeight() const
{
    ASSERT(m_flowThread);
    if (hasComputedAutoHeight() && m_flowThread->inMeasureContentLayoutPhase()) {
        ASSERT(hasAutoLogicalHeight());
        return computedAutoHeight();
    }
    return m_flowThread->isHorizontalWritingMode() ? contentHeight() : contentWidth();
}

// This method returns the maximum page size of a region with auto-height. This is the initial
// height value for auto-height regions in the first layout phase of the parent named flow.
LayoutUnit RenderNamedFlowFragment::maxPageLogicalHeight() const
{
    ASSERT(m_flowThread);
    ASSERT(hasAutoLogicalHeight() && m_flowThread->inMeasureContentLayoutPhase());
    ASSERT(isAnonymous());
    ASSERT(parent());

    const RenderStyle& styleToUse = parent()->style();
    return styleToUse.logicalMaxHeight().isUndefined() ? RenderFlowThread::maxLogicalHeight() : toRenderBlock(parent())->computeReplacedLogicalHeightUsing(styleToUse.logicalMaxHeight());
}

LayoutRect RenderNamedFlowFragment::flowThreadPortionRectForClipping(bool isFirstRegionInRange, bool isLastRegionInRange) const
{
    // Elements flowed into a region should not be painted past the region's content box
    // if they continue to flow into another region in that direction.
    // If they do not continue into another region in that direction, they should be
    // painted all the way to the region's border box.
    // Regions with overflow:hidden will apply clip at the border box, not the content box.
    
    LayoutRect clippingRect = flowThreadPortionRect();
    RenderBlockFlow& container = fragmentContainer();
    if (container.style().hasPadding()) {
        if (isFirstRegionInRange) {
            if (flowThread()->isHorizontalWritingMode()) {
                clippingRect.move(0, -container.paddingBefore());
                clippingRect.expand(0, container.paddingBefore());
            } else {
                clippingRect.move(-container.paddingBefore(), 0);
                clippingRect.expand(container.paddingBefore(), 0);
            }
        }
        
        if (isLastRegionInRange) {
            if (flowThread()->isHorizontalWritingMode())
                clippingRect.expand(0, container.paddingAfter());
            else
                clippingRect.expand(container.paddingAfter(), 0);
        }
        
        if (flowThread()->isHorizontalWritingMode()) {
            clippingRect.move(-container.paddingStart(), 0);
            clippingRect.expand(container.paddingStart() + container.paddingEnd(), 0);
        } else {
            clippingRect.move(0, -container.paddingStart());
            clippingRect.expand(0, container.paddingStart() + container.paddingEnd());
        }
    }
    
    return clippingRect;
}

RenderBlockFlow& RenderNamedFlowFragment::fragmentContainer() const
{
    ASSERT(parent());
    ASSERT(parent()->isRenderNamedFlowFragmentContainer());
    return *toRenderBlockFlow(parent());
}

RenderLayer& RenderNamedFlowFragment::fragmentContainerLayer() const
{
    ASSERT(fragmentContainer().layer());
    return *fragmentContainer().layer();
}

bool RenderNamedFlowFragment::shouldClipFlowThreadContent() const
{
    if (fragmentContainer().hasOverflowClip())
        return true;
    
    return isLastRegion() && (style().regionFragment() == BreakRegionFragment);
}

void RenderNamedFlowFragment::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    RenderRegion::layoutBlock(relayoutChildren);

    if (isValid()) {
        LayoutRect oldRegionRect(flowThreadPortionRect());
        if (!isHorizontalWritingMode())
            oldRegionRect = oldRegionRect.transposedRect();

        if (m_flowThread->inOverflowLayoutPhase() || m_flowThread->inFinalLayoutPhase()) {
            computeOverflowFromFlowThread();
            updateOversetState();
        }

        if (hasAutoLogicalHeight() && m_flowThread->inMeasureContentLayoutPhase()) {
            m_flowThread->invalidateRegions();
            clearComputedAutoHeight();
            return;
        }

        if ((oldRegionRect.width() != pageLogicalWidth() || oldRegionRect.height() != pageLogicalHeight()) && !m_flowThread->inFinalLayoutPhase())
            // This can happen even if we are in the inConstrainedLayoutPhase and it will trigger a pathological layout of the flow thread.
            m_flowThread->invalidateRegions();
    }
}

void RenderNamedFlowFragment::setRegionOversetState(RegionOversetState state)
{
    ASSERT(generatingElement());

    generatingElement()->setRegionOversetState(state);
}

RegionOversetState RenderNamedFlowFragment::regionOversetState() const
{
    ASSERT(generatingElement());

    if (!isValid())
        return RegionUndefined;

    return generatingElement()->regionOversetState();
}

void RenderNamedFlowFragment::updateOversetState()
{
    ASSERT(isValid());

    RenderNamedFlowThread* flowThread = namedFlowThread();
    ASSERT(flowThread && (flowThread->inOverflowLayoutPhase() || flowThread->inFinalLayoutPhase()));

    LayoutUnit flowContentBottom = flowThread->flowContentBottom();
    bool isHorizontalWritingMode = flowThread->isHorizontalWritingMode();

    LayoutUnit flowMin = flowContentBottom - (isHorizontalWritingMode ? flowThreadPortionRect().y() : flowThreadPortionRect().x());
    LayoutUnit flowMax = flowContentBottom - (isHorizontalWritingMode ? flowThreadPortionRect().maxY() : flowThreadPortionRect().maxX());

    RegionOversetState previousState = regionOversetState();
    RegionOversetState state = RegionFit;
    if (flowMin <= 0)
        state = RegionEmpty;
    if (flowMax > 0 && isLastRegion())
        state = RegionOverset;
    
    setRegionOversetState(state);

    // Determine whether the NamedFlow object should dispatch a regionLayoutUpdate event
    if (previousState != state
        || state == RegionFit
        || state == RegionOverset)
        flowThread->setDispatchRegionLayoutUpdateEvent(true);
    
    if (previousState != state)
        flowThread->setDispatchRegionOversetChangeEvent(true);
}

void RenderNamedFlowFragment::checkRegionStyle()
{
    ASSERT(m_flowThread);
    bool customRegionStyle = false;

    // FIXME: Region styling doesn't work for pseudo elements.
    if (!isPseudoElement())
        customRegionStyle = view().document().ensureStyleResolver().checkRegionStyle(generatingElement());
    setHasCustomRegionStyle(customRegionStyle);
    toRenderNamedFlowThread(m_flowThread)->checkRegionsWithStyling();
}

PassRefPtr<RenderStyle> RenderNamedFlowFragment::computeStyleInRegion(const RenderObject* object)
{
    ASSERT(object);
    ASSERT(!object->isAnonymous());
    ASSERT(object->node() && object->node()->isElementNode());

    // FIXME: Region styling fails for pseudo-elements because the renderers don't have a node.
    Element* element = toElement(object->node());
    RefPtr<RenderStyle> renderObjectRegionStyle = object->view().document().ensureStyleResolver().styleForElement(element, 0, DisallowStyleSharing, MatchAllRules, this);

    return renderObjectRegionStyle.release();
}

void RenderNamedFlowFragment::computeChildrenStyleInRegion(const RenderElement* object)
{
    for (RenderObject* child = object->firstChild(); child; child = child->nextSibling()) {

        auto it = m_renderObjectRegionStyle.find(child);

        RefPtr<RenderStyle> childStyleInRegion;
        bool objectRegionStyleCached = false;
        if (it != m_renderObjectRegionStyle.end()) {
            childStyleInRegion = it->value.style;
            objectRegionStyleCached = true;
        } else {
            if (child->isAnonymous() || child->isInFlowRenderFlowThread())
                childStyleInRegion = RenderStyle::createAnonymousStyleWithDisplay(&object->style(), child->style().display());
            else if (child->isText())
                childStyleInRegion = RenderStyle::clone(&object->style());
            else
                childStyleInRegion = computeStyleInRegion(child);
        }

        setObjectStyleInRegion(child, childStyleInRegion, objectRegionStyleCached);

        if (child->isRenderElement())
            computeChildrenStyleInRegion(toRenderElement(child));
    }
}

void RenderNamedFlowFragment::setObjectStyleInRegion(RenderObject* object, PassRefPtr<RenderStyle> styleInRegion, bool objectRegionStyleCached)
{
    ASSERT(object->flowThreadContainingBlock());

    RefPtr<RenderStyle> objectOriginalStyle = &object->style();
    if (object->isRenderElement())
        toRenderElement(object)->setStyleInternal(*styleInRegion);

    if (object->isBoxModelObject() && !object->hasBoxDecorations()) {
        bool hasBoxDecorations = object->isTableCell()
        || object->style().hasBackground()
        || object->style().hasBorder()
        || object->style().hasAppearance()
        || object->style().boxShadow();
        object->setHasBoxDecorations(hasBoxDecorations);
    }

    ObjectRegionStyleInfo styleInfo;
    styleInfo.style = objectOriginalStyle;
    styleInfo.cached = objectRegionStyleCached;
    m_renderObjectRegionStyle.set(object, styleInfo);
}

void RenderNamedFlowFragment::clearObjectStyleInRegion(const RenderObject* object)
{
    ASSERT(object);
    m_renderObjectRegionStyle.remove(object);

    // Clear the style for the children of this object.
    for (RenderObject* child = object->firstChildSlow(); child; child = child->nextSibling())
        clearObjectStyleInRegion(child);
}

void RenderNamedFlowFragment::setRegionObjectsRegionStyle()
{
    if (!hasCustomRegionStyle())
        return;

    // Start from content nodes and recursively compute the style in region for the render objects below.
    // If the style in region was already computed, used that style instead of computing a new one.
    const RenderNamedFlowThread& namedFlow = view().flowThreadController().ensureRenderFlowThreadWithName(style().regionThread());
    const NamedFlowContentElements& contentElements = namedFlow.contentElements();

    for (const auto& element : contentElements) {
        // The list of content nodes contains also the nodes with display:none.
        if (!element->renderer())
            continue;

        RenderElement* object = element->renderer();
        // If the content node does not flow any of its children in this region,
        // we do not compute any style for them in this region.
        if (!flowThread()->objectInFlowRegion(object, this))
            continue;

        // If the object has style in region, use that instead of computing a new one.
        auto it = m_renderObjectRegionStyle.find(object);
        RefPtr<RenderStyle> objectStyleInRegion;
        bool objectRegionStyleCached = false;
        if (it != m_renderObjectRegionStyle.end()) {
            objectStyleInRegion = it->value.style;
            ASSERT(it->value.cached);
            objectRegionStyleCached = true;
        } else
            objectStyleInRegion = computeStyleInRegion(object);

        setObjectStyleInRegion(object, objectStyleInRegion, objectRegionStyleCached);

        computeChildrenStyleInRegion(object);
    }
}

void RenderNamedFlowFragment::restoreRegionObjectsOriginalStyle()
{
    if (!hasCustomRegionStyle())
        return;

    RenderObjectRegionStyleMap temp;
    for (auto& objectPair : m_renderObjectRegionStyle) {
        RenderObject* object = const_cast<RenderObject*>(objectPair.key);
        RefPtr<RenderStyle> objectRegionStyle = &object->style();
        RefPtr<RenderStyle> objectOriginalStyle = objectPair.value.style;
        if (object->isRenderElement())
            toRenderElement(object)->setStyleInternal(*objectOriginalStyle);

        bool shouldCacheRegionStyle = objectPair.value.cached;
        if (!shouldCacheRegionStyle) {
            // Check whether we should cache the computed style in region.
            unsigned changedContextSensitiveProperties = ContextSensitivePropertyNone;
            StyleDifference styleDiff = objectOriginalStyle->diff(objectRegionStyle.get(), changedContextSensitiveProperties);
            if (styleDiff < StyleDifferenceLayoutPositionedMovementOnly)
                shouldCacheRegionStyle = true;
        }
        if (shouldCacheRegionStyle) {
            ObjectRegionStyleInfo styleInfo;
            styleInfo.style = objectRegionStyle;
            styleInfo.cached = true;
            temp.set(object, styleInfo);
        }
    }

    m_renderObjectRegionStyle.swap(temp);
}

RenderNamedFlowThread* RenderNamedFlowFragment::namedFlowThread() const
{
    return toRenderNamedFlowThread(flowThread());
}

LayoutRect RenderNamedFlowFragment::visualOverflowRect() const
{
    if (isValid()) {
        RenderBoxRegionInfo* boxInfo = renderBoxRegionInfo(flowThread());
        if (boxInfo && boxInfo->overflow())
            return boxInfo->overflow()->visualOverflowRect();
    }
    
    return RenderRegion::visualOverflowRect();
}

void RenderNamedFlowFragment::attachRegion()
{
    RenderRegion::attachRegion();

    if (documentBeingDestroyed() || !m_flowThread)
        return;

    // The region just got attached to the flow thread, lets check whether
    // it has region styling rules associated.
    checkRegionStyle();

    if (!isValid())
        return;

    m_hasAutoLogicalHeight = shouldHaveAutoLogicalHeight();
    if (hasAutoLogicalHeight())
        incrementAutoLogicalHeightCount();
}

void RenderNamedFlowFragment::detachRegion()
{
    if (m_flowThread && hasAutoLogicalHeight())
        decrementAutoLogicalHeightCount();
    
    RenderRegion::detachRegion();
}

} // namespace WebCore
