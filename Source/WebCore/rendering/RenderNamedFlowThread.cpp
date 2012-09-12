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

#include "FlowThreadController.h"
#include "InspectorInstrumentation.h"
#include "RenderRegion.h"
#include "RenderView.h"
#include "WebKitNamedFlow.h"

namespace WebCore {

RenderNamedFlowThread::RenderNamedFlowThread(Node* node, PassRefPtr<WebKitNamedFlow> namedFlow)
    : RenderFlowThread(node)
    , m_namedFlow(namedFlow)
    , m_regionLayoutUpdateEventTimer(this, &RenderNamedFlowThread::regionLayoutUpdateEventTimerFired)
{
}

RenderNamedFlowThread::~RenderNamedFlowThread()
{
    // The flow thread can be destroyed without unregistering the content nodes if the document is destroyed.
    // This can lead to problems because the nodes are still marked as belonging to a flow thread.
    clearContentNodes();

    // Also leave the NamedFlow object in a consistent state by calling mark for destruction.
    setMarkForDestruction();
}

const char* RenderNamedFlowThread::renderName() const
{    
    return "RenderNamedFlowThread";
}
    
void RenderNamedFlowThread::clearContentNodes()
{
    for (NamedFlowContentNodes::iterator it = m_contentNodes.begin(); it != m_contentNodes.end(); ++it) {
        Node* contentNode = *it;
        
        ASSERT(contentNode && contentNode->isElementNode());
        ASSERT(contentNode->inNamedFlow());
        ASSERT(contentNode->document() == document());
        
        contentNode->clearInNamedFlow();
    }
    
    m_contentNodes.clear();
}

RenderObject* RenderNamedFlowThread::nextRendererForNode(Node* node) const
{
    FlowThreadChildList::const_iterator it = m_flowThreadChildList.begin();
    FlowThreadChildList::const_iterator end = m_flowThreadChildList.end();

    for (; it != end; ++it) {
        RenderObject* child = *it;
        ASSERT(child->node());
        unsigned short position = node->compareDocumentPosition(child->node());
        if (position & Node::DOCUMENT_POSITION_FOLLOWING)
            return child;
    }

    return 0;
}

RenderObject* RenderNamedFlowThread::previousRendererForNode(Node* node) const
{
    if (m_flowThreadChildList.isEmpty())
        return 0;

    FlowThreadChildList::const_iterator begin = m_flowThreadChildList.begin();
    FlowThreadChildList::const_iterator end = m_flowThreadChildList.end();
    FlowThreadChildList::const_iterator it = end;

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

void RenderNamedFlowThread::addFlowChild(RenderObject* newChild, RenderObject* beforeChild)
{
    // The child list is used to sort the flow thread's children render objects 
    // based on their corresponding nodes DOM order. The list is needed to avoid searching the whole DOM.

    // Do not add anonymous objects.
    if (!newChild->node())
        return;

    if (beforeChild)
        m_flowThreadChildList.insertBefore(beforeChild, newChild);
    else
        m_flowThreadChildList.add(newChild);
}

void RenderNamedFlowThread::removeFlowChild(RenderObject* child)
{
    m_flowThreadChildList.remove(child);
}

bool RenderNamedFlowThread::dependsOn(RenderNamedFlowThread* otherRenderFlowThread) const
{
    if (m_layoutBeforeThreadsSet.contains(otherRenderFlowThread))
        return true;

    // Recursively traverse the m_layoutBeforeThreadsSet.
    RenderNamedFlowThreadCountedSet::const_iterator iterator = m_layoutBeforeThreadsSet.begin();
    RenderNamedFlowThreadCountedSet::const_iterator end = m_layoutBeforeThreadsSet.end();
    for (; iterator != end; ++iterator) {
        const RenderNamedFlowThread* beforeFlowThread = (*iterator).first;
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

    ASSERT(firstRegion->generatingNode());
    ASSERT(secondRegion->generatingNode());

    // If the regions belong to different nodes, compare their position in the DOM.
    if (firstRegion->generatingNode() != secondRegion->generatingNode()) {
        unsigned short position = firstRegion->generatingNode()->compareDocumentPosition(secondRegion->generatingNode());

        // If the second region is contained in the first one, the first region is "less" if it's :before.
        if (position & Node::DOCUMENT_POSITION_CONTAINED_BY) {
            ASSERT(secondRegion->style()->styleType() == NOPSEUDO);
            return firstRegion->style()->styleType() == BEFORE;
        }

        // If the second region contains the first region, the first region is "less" if the second is :after.
        if (position & Node::DOCUMENT_POSITION_CONTAINS) {
            ASSERT(firstRegion->style()->styleType() == NOPSEUDO);
            return secondRegion->style()->styleType() == AFTER;
        }

        return (position & Node::DOCUMENT_POSITION_FOLLOWING);
    }

    // FIXME: Currently it's not possible for an element to be both a region and have pseudo-children. The case is covered anyway.
    switch (firstRegion->style()->styleType()) {
    case BEFORE:
        // The second region can be the node or the after pseudo-element (before is smaller than any of those).
        return true;
    case AFTER:
        // The second region can be the node or the before pseudo-element (after is greater than any of those).
        return false;
    case NOPSEUDO:
        // The second region can either be the before or the after pseudo-element (the node is only smaller than the after pseudo-element).
        return firstRegion->style()->styleType() == AFTER;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return true;
}

void RenderNamedFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    if (m_regionList.isEmpty())
        m_regionList.add(renderRegion);
    else {
        // Find the first region "greater" than renderRegion.
        RenderRegionList::iterator it = m_regionList.begin();
        while (it != m_regionList.end() && !compareRenderRegions(renderRegion, *it))
            ++it;
        m_regionList.insertBefore(it, renderRegion);
    }

    resetMarkForDestruction();

    ASSERT(!renderRegion->isValid());
    if (renderRegion->parentNamedFlowThread()) {
        if (renderRegion->parentNamedFlowThread()->dependsOn(this)) {
            // Register ourself to get a notification when the state changes.
            renderRegion->parentNamedFlowThread()->m_observerThreadsSet.add(this);
            return;
        }

        addDependencyOnFlowThread(renderRegion->parentNamedFlowThread());
    }

    renderRegion->setIsValid(true);

    invalidateRegions();
}

void RenderNamedFlowThread::removeRegionFromThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionRangeMap.clear();
    m_regionList.remove(renderRegion);

    if (renderRegion->parentNamedFlowThread()) {
        if (!renderRegion->isValid()) {
            renderRegion->parentNamedFlowThread()->m_observerThreadsSet.remove(this);
            // No need to invalidate the regions rectangles. The removed region
            // was not taken into account. Just return here.
            return;
        }
        removeDependencyOnFlowThread(renderRegion->parentNamedFlowThread());
    }

    if (canBeDestroyed())
        setMarkForDestruction();

    // After removing all the regions in the flow the following layout needs to dispatch the regionLayoutUpdate event
    if (m_regionList.isEmpty())
        setDispatchRegionLayoutUpdateEvent(true);

    invalidateRegions();
}

void RenderNamedFlowThread::checkInvalidRegions()
{
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        // The only reason a region would be invalid is because it has a parent flow thread.
        ASSERT(region->isValid() || region->parentNamedFlowThread());
        if (region->isValid() || region->parentNamedFlowThread()->dependsOn(this))
            continue;

        region->parentNamedFlowThread()->m_observerThreadsSet.remove(this);
        addDependencyOnFlowThread(region->parentNamedFlowThread());
        region->setIsValid(true);
        invalidateRegions();
    }

    if (m_observerThreadsSet.isEmpty())
        return;

    // Notify all the flow threads that were dependent on this flow.

    // Create a copy of the list first. That's because observers might change the list when calling checkInvalidRegions.
    Vector<RenderNamedFlowThread*> observers;
    copyToVector(m_observerThreadsSet, observers);

    for (size_t i = 0; i < observers.size(); ++i) {
        RenderNamedFlowThread* flowThread = observers.at(i);
        flowThread->checkInvalidRegions();
    }
}

void RenderNamedFlowThread::addDependencyOnFlowThread(RenderNamedFlowThread* otherFlowThread)
{
    RenderNamedFlowThreadCountedSet::AddResult result = m_layoutBeforeThreadsSet.add(otherFlowThread);
    if (result.isNewEntry) {
        // This is the first time we see this dependency. Make sure we recalculate all the dependencies.
        view()->flowThreadController()->setIsRenderNamedFlowThreadOrderDirty(true);
    }
}

void RenderNamedFlowThread::removeDependencyOnFlowThread(RenderNamedFlowThread* otherFlowThread)
{
    bool removed = m_layoutBeforeThreadsSet.remove(otherFlowThread);
    if (removed) {
        checkInvalidRegions();
        view()->flowThreadController()->setIsRenderNamedFlowThreadOrderDirty(true);
    }
}

void RenderNamedFlowThread::pushDependencies(RenderNamedFlowThreadList& list)
{
    for (RenderNamedFlowThreadCountedSet::iterator iter = m_layoutBeforeThreadsSet.begin(); iter != m_layoutBeforeThreadsSet.end(); ++iter) {
        RenderNamedFlowThread* flowThread = (*iter).first;
        if (list.contains(flowThread))
            continue;
        flowThread->pushDependencies(list);
        list.add(flowThread);
    }
}

// The content nodes list contains those nodes with -webkit-flow-into: flow.
// An element with display:none should also be listed among those nodes.
// The list of nodes is ordered.
void RenderNamedFlowThread::registerNamedFlowContentNode(Node* contentNode)
{
    ASSERT(contentNode && contentNode->isElementNode());
    ASSERT(contentNode->document() == document());

    contentNode->setInNamedFlow();

    resetMarkForDestruction();

    // Find the first content node following the new content node.
    for (NamedFlowContentNodes::iterator it = m_contentNodes.begin(); it != m_contentNodes.end(); ++it) {
        Node* node = *it;
        unsigned short position = contentNode->compareDocumentPosition(node);
        if (position & Node::DOCUMENT_POSITION_FOLLOWING) {
            m_contentNodes.insertBefore(node, contentNode);
            return;
        }
    }
    m_contentNodes.add(contentNode);
}

void RenderNamedFlowThread::unregisterNamedFlowContentNode(Node* contentNode)
{
    ASSERT(contentNode && contentNode->isElementNode());
    ASSERT(m_contentNodes.contains(contentNode));
    ASSERT(contentNode->inNamedFlow());
    ASSERT(contentNode->document() == document());

    contentNode->clearInNamedFlow();
    m_contentNodes.remove(contentNode);

    if (canBeDestroyed())
        setMarkForDestruction();
}

const AtomicString& RenderNamedFlowThread::flowThreadName() const
{
    return m_namedFlow->name();
}

void RenderNamedFlowThread::dispatchRegionLayoutUpdateEvent()
{
    RenderFlowThread::dispatchRegionLayoutUpdateEvent();
    InspectorInstrumentation::didUpdateRegionLayout(document(), m_namedFlow.get());

    if (!m_regionLayoutUpdateEventTimer.isActive() && m_namedFlow->hasEventListeners())
        m_regionLayoutUpdateEventTimer.startOneShot(0);
}

void RenderNamedFlowThread::regionLayoutUpdateEventTimerFired(Timer<RenderNamedFlowThread>*)
{
    ASSERT(m_namedFlow);

    m_namedFlow->dispatchRegionLayoutUpdateEvent();
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

}
