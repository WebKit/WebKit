/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderNamedFlowThread.h"

#include "ExceptionCodePlaceholder.h"
#include "FlowThreadController.h"
#include "InlineTextBox.h"
#include "InspectorInstrumentation.h"
#include "NodeRenderingTraversal.h"
#include "NodeTraversal.h"
#include "Position.h"
#include "Range.h"
#include "RenderInline.h"
#include "RenderNamedFlowFragment.h"
#include "RenderRegion.h"
#include "RenderText.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "WebKitNamedFlow.h"

namespace WebCore {

RenderNamedFlowThread::RenderNamedFlowThread(Document& document, PassRef<RenderStyle> style, PassRefPtr<WebKitNamedFlow> namedFlow)
    : RenderFlowThread(document, std::move(style))
    , m_flowThreadChildList(adoptPtr(new FlowThreadChildList()))
    , m_overset(true)
    , m_hasRegionsWithStyling(false)
    , m_namedFlow(namedFlow)
    , m_regionLayoutUpdateEventTimer(this, &RenderNamedFlowThread::regionLayoutUpdateEventTimerFired)
    , m_regionOversetChangeEventTimer(this, &RenderNamedFlowThread::regionOversetChangeEventTimerFired)
{
}

RenderNamedFlowThread::~RenderNamedFlowThread()
{
    // The flow thread can be destroyed without unregistering the content nodes if the document is destroyed.
    // This can lead to problems because the nodes are still marked as belonging to a flow thread.
    clearContentElements();

    // Also leave the NamedFlow object in a consistent state by calling mark for destruction.
    setMarkForDestruction();
}

const char* RenderNamedFlowThread::renderName() const
{    
    return "RenderNamedFlowThread";
}
    
void RenderNamedFlowThread::clearContentElements()
{
    for (auto& contentElement : m_contentElements) {
        ASSERT(contentElement);
        ASSERT(contentElement->inNamedFlow());
        ASSERT(&contentElement->document() == &document());
        
        contentElement->clearInNamedFlow();
    }
    
    m_contentElements.clear();
}

void RenderNamedFlowThread::updateWritingMode()
{
    RenderRegion* firstRegion = m_regionList.first();
    if (!firstRegion)
        return;
    if (style().writingMode() == firstRegion->style().writingMode())
        return;

    // The first region defines the principal writing mode for the entire flow.
    auto newStyle = RenderStyle::clone(&style());
    newStyle.get().setWritingMode(firstRegion->style().writingMode());
    setStyle(std::move(newStyle));
}

RenderObject* RenderNamedFlowThread::nextRendererForNode(Node* node) const
{
    const FlowThreadChildList& childList = *(m_flowThreadChildList.get());
    for (auto& child : childList) {
        ASSERT(child->node());
        unsigned short position = node->compareDocumentPosition(child->node());
        if (position & Node::DOCUMENT_POSITION_FOLLOWING)
            return child;
    }

    return 0;
}

RenderObject* RenderNamedFlowThread::previousRendererForNode(Node* node) const
{
    if (m_flowThreadChildList->isEmpty())
        return 0;

    auto begin = m_flowThreadChildList->begin();
    auto end = m_flowThreadChildList->end();
    auto it = end;

    do {
        --it;
        RenderObject* child = *it;
        ASSERT(child->node());
        unsigned short position = node->compareDocumentPosition(child->node());
        if (position & Node::DOCUMENT_POSITION_PRECEDING)
            return child;
    } while (it != begin);

    return 0;
}

void RenderNamedFlowThread::addFlowChild(RenderObject* newChild)
{
    // The child list is used to sort the flow thread's children render objects 
    // based on their corresponding nodes DOM order. The list is needed to avoid searching the whole DOM.

    Node* childNode = newChild->node();

    // Do not add anonymous objects.
    if (!childNode)
        return;

    ASSERT(childNode->isElementNode());

    RenderObject* beforeChild = nextRendererForNode(childNode);
    if (beforeChild)
        m_flowThreadChildList->insertBefore(beforeChild, newChild);
    else
        m_flowThreadChildList->add(newChild);
}

void RenderNamedFlowThread::removeFlowChild(RenderObject* child)
{
    m_flowThreadChildList->remove(child);
}

bool RenderNamedFlowThread::dependsOn(RenderNamedFlowThread* otherRenderFlowThread) const
{
    if (m_layoutBeforeThreadsSet.contains(otherRenderFlowThread))
        return true;

    // Recursively traverse the m_layoutBeforeThreadsSet.
    for (const auto& beforeFlowThreadPair : m_layoutBeforeThreadsSet) {
        const auto& beforeFlowThread = beforeFlowThreadPair.key;
        if (beforeFlowThread->dependsOn(otherRenderFlowThread))
            return true;
    }

    return false;
}

// Compare two regions to determine in which one the content should flow first.
// The function returns true if the first passed region is "less" than the second passed region.
// If the first region appears before second region in DOM,
// the first region is "less" than the second region.
// If the first region is "less" than the second region, the first region receives content before second region.
static bool compareRenderRegions(const RenderRegion* firstRegion, const RenderRegion* secondRegion)
{
    ASSERT(firstRegion);
    ASSERT(secondRegion);

    ASSERT(firstRegion->generatingElement());
    ASSERT(secondRegion->generatingElement());

    // If the regions belong to different nodes, compare their position in the DOM.
    if (firstRegion->generatingElement() != secondRegion->generatingElement()) {
        unsigned short position = firstRegion->generatingElement()->compareDocumentPosition(secondRegion->generatingElement());

        // If the second region is contained in the first one, the first region is "less" if it's :before.
        if (position & Node::DOCUMENT_POSITION_CONTAINED_BY) {
            ASSERT(secondRegion->style().styleType() == NOPSEUDO);
            return firstRegion->style().styleType() == BEFORE;
        }

        // If the second region contains the first region, the first region is "less" if the second is :after.
        if (position & Node::DOCUMENT_POSITION_CONTAINS) {
            ASSERT(firstRegion->style().styleType() == NOPSEUDO);
            return secondRegion->style().styleType() == AFTER;
        }

        return (position & Node::DOCUMENT_POSITION_FOLLOWING);
    }

    // FIXME: Currently it's not possible for an element to be both a region and have pseudo-children. The case is covered anyway.
    switch (firstRegion->style().styleType()) {
    case BEFORE:
        // The second region can be the node or the after pseudo-element (before is smaller than any of those).
        return true;
    case AFTER:
        // The second region can be the node or the before pseudo-element (after is greater than any of those).
        return false;
    case NOPSEUDO:
        // The second region can either be the before or the after pseudo-element (the node is only smaller than the after pseudo-element).
        return firstRegion->style().styleType() == AFTER;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return true;
}

// This helper function adds a region to a list preserving the order property of the list.
static void addRegionToList(RenderRegionList& regionList, RenderRegion* renderRegion)
{
    if (regionList.isEmpty())
        regionList.add(renderRegion);
    else {
        // Find the first region "greater" than renderRegion.
        auto it = regionList.begin();
        while (it != regionList.end() && !compareRenderRegions(renderRegion, *it))
            ++it;
        regionList.insertBefore(it, renderRegion);
    }
}

void RenderNamedFlowThread::addRegionToNamedFlowThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    ASSERT(!renderRegion->isValid());

    if (renderRegion->parentNamedFlowThread())
        addDependencyOnFlowThread(renderRegion->parentNamedFlowThread());

    renderRegion->setIsValid(true);
    addRegionToList(m_regionList, renderRegion);

    if (m_regionList.first() == renderRegion)
        updateWritingMode();
}

void RenderNamedFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    ASSERT(!renderRegion->isValid());

    resetMarkForDestruction();

    if (renderRegion->parentNamedFlowThread() && renderRegion->parentNamedFlowThread()->dependsOn(this)) {
        // The order of invalid regions is irrelevant.
        m_invalidRegionList.add(renderRegion);
        // Register ourself to get a notification when the state changes.
        renderRegion->parentNamedFlowThread()->m_observerThreadsSet.add(this);
        return;
    }

    addRegionToNamedFlowThread(renderRegion);

    invalidateRegions();
}

void RenderNamedFlowThread::removeRegionFromThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);

    if (renderRegion->parentNamedFlowThread()) {
        if (!renderRegion->isValid()) {
            ASSERT(m_invalidRegionList.contains(renderRegion));
            m_invalidRegionList.remove(renderRegion);
            renderRegion->parentNamedFlowThread()->m_observerThreadsSet.remove(this);
            // No need to invalidate the regions rectangles. The removed region
            // was not taken into account. Just return here.
            return;
        }
        removeDependencyOnFlowThread(renderRegion->parentNamedFlowThread());
    }

    ASSERT(m_regionList.contains(renderRegion));
    bool wasFirst = m_regionList.first() == renderRegion;
    m_regionList.remove(renderRegion);

    if (canBeDestroyed())
        setMarkForDestruction();

    // After removing all the regions in the flow the following layout needs to dispatch the regionLayoutUpdate event
    if (m_regionList.isEmpty())
        setDispatchRegionLayoutUpdateEvent(true);
    else if (wasFirst)
        updateWritingMode();

    invalidateRegions();
}

void RenderNamedFlowThread::regionChangedWritingMode(RenderRegion* region)
{
    if (m_regionList.first() == region)
        updateWritingMode();
}

void RenderNamedFlowThread::computeOversetStateForRegions(LayoutUnit oldClientAfterEdge)
{
    LayoutUnit height = oldClientAfterEdge;

    // FIXME: the visual overflow of middle region (if it is the last one to contain any content in a render flow thread)
    // might not be taken into account because the render flow thread height is greater that that regions height + its visual overflow
    // because of how computeLogicalHeight is implemented for RenderNamedFlowThread (as a sum of all regions height).
    // This means that the middle region will be marked as fit (even if it has visual overflow flowing into the next region)
    if (hasRenderOverflow()
        && ( (isHorizontalWritingMode() && visualOverflowRect().maxY() > clientBoxRect().maxY())
            || (!isHorizontalWritingMode() && visualOverflowRect().maxX() > clientBoxRect().maxX())))
        height = isHorizontalWritingMode() ? visualOverflowRect().maxY() : visualOverflowRect().maxX();

    RenderRegion* lastReg = lastRegion();
    for (auto& region : m_regionList) {
        LayoutUnit flowMin = height - (isHorizontalWritingMode() ? region->flowThreadPortionRect().y() : region->flowThreadPortionRect().x());
        LayoutUnit flowMax = height - (isHorizontalWritingMode() ? region->flowThreadPortionRect().maxY() : region->flowThreadPortionRect().maxX());
        RegionOversetState previousState = region->regionOversetState();
        RegionOversetState state = RegionFit;
        if (flowMin <= 0)
            state = RegionEmpty;
        if (flowMax > 0 && region == lastReg)
            state = RegionOverset;
        region->setRegionOversetState(state);
        // determine whether the NamedFlow object should dispatch a regionLayoutUpdate event
        // FIXME: currently it cannot determine whether a region whose regionOverset state remained either "fit" or "overset" has actually
        // changed, so it just assumes that the NamedFlow should dispatch the event
        if (previousState != state
            || state == RegionFit
            || state == RegionOverset)
            setDispatchRegionLayoutUpdateEvent(true);
        
        if (previousState != state)
            setDispatchRegionOversetChangeEvent(true);
    }
    
    // If the number of regions has changed since we last computed the overset property, schedule the regionOversetChange event.
    if (previousRegionCountChanged()) {
        setDispatchRegionOversetChangeEvent(true);
        updatePreviousRegionCount();
    }

    // With the regions overflow state computed we can also set the overset flag for the named flow.
    // If there are no valid regions in the chain, overset is true.
    m_overset = lastReg ? lastReg->regionOversetState() == RegionOverset : true;
}

void RenderNamedFlowThread::checkInvalidRegions()
{
    Vector<RenderRegion*> newValidRegions;
    for (auto& region : m_invalidRegionList) {
        // The only reason a region would be invalid is because it has a parent flow thread.
        ASSERT(!region->isValid() && region->parentNamedFlowThread());
        if (region->parentNamedFlowThread()->dependsOn(this))
            continue;

        newValidRegions.append(region);
    }

    for (auto& region : newValidRegions) {
        m_invalidRegionList.remove(region);
        region->parentNamedFlowThread()->m_observerThreadsSet.remove(this);
        addRegionToNamedFlowThread(region);
    }

    if (!newValidRegions.isEmpty())
        invalidateRegions();

    if (m_observerThreadsSet.isEmpty())
        return;

    // Notify all the flow threads that were dependent on this flow.

    // Create a copy of the list first. That's because observers might change the list when calling checkInvalidRegions.
    Vector<RenderNamedFlowThread*> observers;
    copyToVector(m_observerThreadsSet, observers);

    for (auto& flowThread : observers)
        flowThread->checkInvalidRegions();
}

void RenderNamedFlowThread::addDependencyOnFlowThread(RenderNamedFlowThread* otherFlowThread)
{
    RenderNamedFlowThreadCountedSet::AddResult result = m_layoutBeforeThreadsSet.add(otherFlowThread);
    if (result.isNewEntry) {
        // This is the first time we see this dependency. Make sure we recalculate all the dependencies.
        view().flowThreadController().setIsRenderNamedFlowThreadOrderDirty(true);
    }
}

void RenderNamedFlowThread::removeDependencyOnFlowThread(RenderNamedFlowThread* otherFlowThread)
{
    bool removed = m_layoutBeforeThreadsSet.remove(otherFlowThread);
    if (removed) {
        checkInvalidRegions();
        view().flowThreadController().setIsRenderNamedFlowThreadOrderDirty(true);
    }
}

void RenderNamedFlowThread::pushDependencies(RenderNamedFlowThreadList& list)
{
    for (auto& flowThreadPair : m_layoutBeforeThreadsSet) {
        auto& flowThread = flowThreadPair.key;
        if (list.contains(flowThread))
            continue;
        flowThread->pushDependencies(list);
        list.add(flowThread);
    }
}

// The content nodes list contains those nodes with -webkit-flow-into: flow.
// An element with display:none should also be listed among those nodes.
// The list of nodes is ordered.
void RenderNamedFlowThread::registerNamedFlowContentElement(Element& contentElement)
{
    ASSERT(&contentElement.document() == &document());

    contentElement.setInNamedFlow();

    resetMarkForDestruction();

    // Find the first content node following the new content node.
    for (auto& element : m_contentElements) {
        unsigned short position = contentElement.compareDocumentPosition(element);
        if (position & Node::DOCUMENT_POSITION_FOLLOWING) {
            m_contentElements.insertBefore(element, &contentElement);
            InspectorInstrumentation::didRegisterNamedFlowContentElement(&document(), m_namedFlow.get(), &contentElement, element);
            return;
        }
    }

    m_contentElements.add(&contentElement);
    InspectorInstrumentation::didRegisterNamedFlowContentElement(&document(), m_namedFlow.get(), &contentElement);
}

void RenderNamedFlowThread::unregisterNamedFlowContentElement(Element& contentElement)
{
    ASSERT(m_contentElements.contains(&contentElement));
    ASSERT(contentElement.inNamedFlow());
    ASSERT(&contentElement.document() == &document());

    contentElement.clearInNamedFlow();
    m_contentElements.remove(&contentElement);

    if (canBeDestroyed())
        setMarkForDestruction();

    InspectorInstrumentation::didUnregisterNamedFlowContentElement(&document(), m_namedFlow.get(), &contentElement);
}

bool RenderNamedFlowThread::hasContentElement(Element& contentElement) const
{
    return m_contentElements.contains(&contentElement);
}
    
const AtomicString& RenderNamedFlowThread::flowThreadName() const
{
    return m_namedFlow->name();
}

bool RenderNamedFlowThread::isChildAllowed(const RenderObject& child, const RenderStyle& style) const
{
    if (!child.node())
        return true;

    ASSERT(child.node()->isElementNode());

    Node* originalParent = NodeRenderingTraversal::parent(child.node());
    if (!originalParent || !originalParent->isElementNode() || !originalParent->renderer())
        return true;

    return toElement(originalParent)->renderer()->isChildAllowed(child, style);
}

void RenderNamedFlowThread::dispatchRegionLayoutUpdateEvent()
{
    RenderFlowThread::dispatchRegionLayoutUpdateEvent();
    InspectorInstrumentation::didUpdateRegionLayout(&document(), m_namedFlow.get());

    if (!m_regionLayoutUpdateEventTimer.isActive() && m_namedFlow->hasEventListeners())
        m_regionLayoutUpdateEventTimer.startOneShot(0);
}

void RenderNamedFlowThread::dispatchRegionOversetChangeEvent()
{
    RenderFlowThread::dispatchRegionOversetChangeEvent();
    InspectorInstrumentation::didChangeRegionOverset(&document(), m_namedFlow.get());
    
    if (!m_regionOversetChangeEventTimer.isActive() && m_namedFlow->hasEventListeners())
        m_regionOversetChangeEventTimer.startOneShot(0);
}

void RenderNamedFlowThread::regionLayoutUpdateEventTimerFired(Timer<RenderNamedFlowThread>&)
{
    ASSERT(m_namedFlow);

    m_namedFlow->dispatchRegionLayoutUpdateEvent();
}

void RenderNamedFlowThread::regionOversetChangeEventTimerFired(Timer<RenderNamedFlowThread>&)
{
    ASSERT(m_namedFlow);
    
    m_namedFlow->dispatchRegionOversetChangeEvent();
}

void RenderNamedFlowThread::setMarkForDestruction()
{
    if (m_namedFlow->flowState() == WebKitNamedFlow::FlowStateNull)
        return;

    m_namedFlow->setRenderer(0);
    // After this call ends, the renderer can be safely destroyed.
    // The NamedFlow object may outlive its renderer if it's referenced from a script and may be reatached to one if the named flow is recreated in the stylesheet.
}

void RenderNamedFlowThread::resetMarkForDestruction()
{
    if (m_namedFlow->flowState() == WebKitNamedFlow::FlowStateCreated)
        return;

    m_namedFlow->setRenderer(this);
}

bool RenderNamedFlowThread::isMarkedForDestruction() const
{
    // Flow threads in the "NULL" state can be destroyed.
    return m_namedFlow->flowState() == WebKitNamedFlow::FlowStateNull;
}

static bool isContainedInElements(const Vector<Element*>& others, Element* element)
{
    for (auto& other : others) {
        if (other->contains(element))
            return true;
    }
    return false;
}

static bool boxIntersectsRegion(LayoutUnit logicalTopForBox, LayoutUnit logicalBottomForBox, LayoutUnit logicalTopForRegion, LayoutUnit logicalBottomForRegion)
{
    bool regionIsEmpty = logicalBottomForRegion != LayoutUnit::max() && logicalTopForRegion != LayoutUnit::min()
        && (logicalBottomForRegion - logicalTopForRegion) <= 0;
    return  (logicalBottomForBox - logicalTopForBox) > 0
        && !regionIsEmpty
        && logicalTopForBox < logicalBottomForRegion && logicalTopForRegion < logicalBottomForBox;
}

// Retrieve the next node to be visited while computing the ranges inside a region.
static Node* nextNodeInsideContentElement(const Node* currNode, const Element* contentElement)
{
    ASSERT(currNode);
    ASSERT(contentElement && contentElement->inNamedFlow());

    if (currNode->renderer() && currNode->renderer()->isSVGRoot())
        return NodeTraversal::nextSkippingChildren(currNode, contentElement);

    return NodeTraversal::next(currNode, contentElement);
}

void RenderNamedFlowThread::getRanges(Vector<RefPtr<Range>>& rangeObjects, const RenderRegion* region) const
{
    LayoutUnit logicalTopForRegion;
    LayoutUnit logicalBottomForRegion;

    // extend the first region top to contain everything up to its logical height
    if (region->isFirstRegion())
        logicalTopForRegion = LayoutUnit::min();
    else
        logicalTopForRegion =  region->logicalTopForFlowThreadContent();

    // extend the last region to contain everything above its y()
    if (region->isLastRegion())
        logicalBottomForRegion = LayoutUnit::max();
    else
        logicalBottomForRegion = region->logicalBottomForFlowThreadContent();

    Vector<Element*> elements;
    // eliminate the contentElements that are descendants of other contentElements
    for (auto& element : contentElements()) {
        if (!isContainedInElements(elements, element))
            elements.append(element);
    }

    for (auto& contentElement : elements) {
        if (!contentElement->renderer())
            continue;

        RefPtr<Range> range = Range::create(contentElement->document());
        bool foundStartPosition = false;
        bool startsAboveRegion = true;
        bool endsBelowRegion = true;
        bool skipOverOutsideNodes = false;
        Node* lastEndNode = 0;

        for (Node* node = contentElement; node; node = nextNodeInsideContentElement(node, contentElement)) {
            RenderObject* renderer = node->renderer();
            if (!renderer)
                continue;

            LayoutRect boundingBox;
            if (renderer->isRenderInline())
                boundingBox = toRenderInline(renderer)->linesBoundingBox();
            else if (renderer->isText())
                boundingBox = toRenderText(renderer)->linesBoundingBox();
            else {
                boundingBox =  toRenderBox(renderer)->frameRect();
                if (toRenderBox(renderer)->isRelPositioned())
                    boundingBox.move(toRenderBox(renderer)->relativePositionLogicalOffset());
            }

            LayoutUnit offsetTop = renderer->containingBlock()->offsetFromLogicalTopOfFirstPage();
            const LayoutPoint logicalOffsetFromTop(isHorizontalWritingMode() ? LayoutUnit() :  offsetTop,
                isHorizontalWritingMode() ? offsetTop : LayoutUnit());

            boundingBox.moveBy(logicalOffsetFromTop);

            LayoutUnit logicalTopForRenderer = region->logicalTopOfFlowThreadContentRect(boundingBox);
            LayoutUnit logicalBottomForRenderer = region->logicalBottomOfFlowThreadContentRect(boundingBox);

            // if the bounding box of the current element doesn't intersect the region box
            // close the current range only if the start element began inside the region,
            // otherwise just move the start position after this node and keep skipping them until we found a proper start position.
            if (!boxIntersectsRegion(logicalTopForRenderer, logicalBottomForRenderer, logicalTopForRegion, logicalBottomForRegion)) {
                if (foundStartPosition) {
                    if (!startsAboveRegion) {
                        if (range->intersectsNode(node, IGNORE_EXCEPTION))
                            range->setEndBefore(node, IGNORE_EXCEPTION);
                        rangeObjects.append(range->cloneRange(IGNORE_EXCEPTION));
                        range = Range::create(contentElement->document());
                        startsAboveRegion = true;
                    } else
                        skipOverOutsideNodes = true;
                }
                if (skipOverOutsideNodes)
                    range->setStartAfter(node, IGNORE_EXCEPTION);
                foundStartPosition = false;
                continue;
            }

            // start position
            if (logicalTopForRenderer < logicalTopForRegion && startsAboveRegion) {
                if (renderer->isText()) { // Text crosses region top
                    // for Text elements, just find the last textbox that is contained inside the region and use its start() offset as start position
                    RenderText* textRenderer = toRenderText(renderer);
                    for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                        if (offsetTop + box->logicalBottom() < logicalTopForRegion)
                            continue;
                        range->setStart(Position(toText(node), box->start()));
                        startsAboveRegion = false;
                        break;
                    }
                } else { // node crosses region top
                    // for all elements, except Text, just set the start position to be before their children
                    startsAboveRegion = true;
                    range->setStart(Position(node, Position::PositionIsBeforeChildren));
                }
            } else { // node starts inside region
                // for elements that start inside the region, set the start position to be before them. If we found one, we will just skip the others until
                // the range is closed.
                if (startsAboveRegion) {
                    startsAboveRegion = false;
                    range->setStartBefore(node, IGNORE_EXCEPTION);
                }
            }
            skipOverOutsideNodes  = false;
            foundStartPosition = true;

            // end position
            if (logicalBottomForRegion < logicalBottomForRenderer && (endsBelowRegion || (!endsBelowRegion && !node->isDescendantOf(lastEndNode)))) {
                // for Text elements, just find just find the last textbox that is contained inside the region and use its start()+len() offset as end position
                if (renderer->isText()) { // Text crosses region bottom
                    RenderText* textRenderer = toRenderText(renderer);
                    InlineTextBox* lastBox = 0;
                    for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                        if ((offsetTop + box->logicalTop()) < logicalBottomForRegion) {
                            lastBox = box;
                            continue;
                        }
                        ASSERT(lastBox);
                        if (lastBox)
                            range->setEnd(Position(toText(node), lastBox->start() + lastBox->len()));
                        break;
                    }
                    endsBelowRegion = false;
                    lastEndNode = node;
                } else { // node crosses region bottom
                    // for all elements, except Text, just set the start position to be after their children
                    range->setEnd(Position(node, Position::PositionIsAfterChildren));
                    endsBelowRegion = true;
                    lastEndNode = node;
                }
            } else { // node ends inside region
                // for elements that ends inside the region, set the end position to be after them
                // allow this end position to be changed only by other elements that are not descendants of the current end node
                if (endsBelowRegion || (!endsBelowRegion && !node->isDescendantOf(lastEndNode))) {
                    range->setEndAfter(node, IGNORE_EXCEPTION);
                    endsBelowRegion = false;
                    lastEndNode = node;
                }
            }
        }
        if (foundStartPosition || skipOverOutsideNodes)
            rangeObjects.append(range);
    }
}

bool RenderNamedFlowThread::collectsGraphicsLayersUnderRegions() const
{
    // We only need to map layers to regions for named flow threads.
    // Multi-column threads are displayed on top of the regions and do not require
    // distributing the layers.

    return true;
}

// Check if the content is flown into at least a region with region styling rules.
void RenderNamedFlowThread::checkRegionsWithStyling()
{
    bool hasRegionsWithStyling = false;
    for (const auto& region : m_regionList) {
        const RenderNamedFlowFragment* namedFlowFragment = toRenderNamedFlowFragment(region);
        if (namedFlowFragment->hasCustomRegionStyle()) {
            hasRegionsWithStyling = true;
            break;
        }
    }
    m_hasRegionsWithStyling = hasRegionsWithStyling;
}

void RenderNamedFlowThread::clearRenderObjectCustomStyle(const RenderObject* object)
{
    // Clear the styles for the object in the regions.
    // FIXME: Region styling is not computed only for the region range of the object so this is why we need to walk the whole chain.
    for (auto& region : m_regionList) {
        RenderNamedFlowFragment* namedFlowFragment = toRenderNamedFlowFragment(region);
        namedFlowFragment->clearObjectStyleInRegion(object);
    }
}

void RenderNamedFlowThread::removeFlowChildInfo(RenderObject* child)
{
    RenderFlowThread::removeFlowChildInfo(child);
    clearRenderObjectCustomStyle(child);
}

}
