/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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
#include "Range.h"

#include "ClientRect.h"
#include "ClientRectList.h"
#include "DocumentFragment.h"
#include "Event.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "NodeTraversal.h"
#include "NodeWithIndex.h"
#include "Page.h"
#include "ProcessingInstruction.h"
#include "RenderBoxModelObject.h"
#include "RenderText.h"
#include "ScopedEventQueue.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "VisibleUnits.h"
#include "htmlediting.h"
#include "markup.h"
#include <stdio.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(IOS)
#include "SelectionRect.h"
#endif

namespace WebCore {

using namespace HTMLNames;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, rangeCounter, ("Range"));

inline Range::Range(Document& ownerDocument)
    : m_ownerDocument(ownerDocument)
    , m_start(&ownerDocument)
    , m_end(&ownerDocument)
{
#ifndef NDEBUG
    rangeCounter.increment();
#endif

    m_ownerDocument->attachRange(this);
}

Ref<Range> Range::create(Document& ownerDocument)
{
    return adoptRef(*new Range(ownerDocument));
}

inline Range::Range(Document& ownerDocument, PassRefPtr<Node> startContainer, int startOffset, PassRefPtr<Node> endContainer, int endOffset)
    : m_ownerDocument(ownerDocument)
    , m_start(&ownerDocument)
    , m_end(&ownerDocument)
{
#ifndef NDEBUG
    rangeCounter.increment();
#endif

    m_ownerDocument->attachRange(this);

    // Simply setting the containers and offsets directly would not do any of the checking
    // that setStart and setEnd do, so we call those functions.
    setStart(startContainer, startOffset);
    setEnd(endContainer, endOffset);
}

Ref<Range> Range::create(Document& ownerDocument, PassRefPtr<Node> startContainer, int startOffset, PassRefPtr<Node> endContainer, int endOffset)
{
    return adoptRef(*new Range(ownerDocument, startContainer, startOffset, endContainer, endOffset));
}

Ref<Range> Range::create(Document& ownerDocument, const Position& start, const Position& end)
{
    return adoptRef(*new Range(ownerDocument, start.containerNode(), start.computeOffsetInContainerNode(), end.containerNode(), end.computeOffsetInContainerNode()));
}

Ref<Range> Range::create(Document& ownerDocument, const VisiblePosition& visibleStart, const VisiblePosition& visibleEnd)
{
    Position start = visibleStart.deepEquivalent().parentAnchoredEquivalent();
    Position end = visibleEnd.deepEquivalent().parentAnchoredEquivalent();
    return adoptRef(*new Range(ownerDocument, start.anchorNode(), start.deprecatedEditingOffset(), end.anchorNode(), end.deprecatedEditingOffset()));
}

Range::~Range()
{
    // Always detach (even if we've already detached) to fix https://bugs.webkit.org/show_bug.cgi?id=26044
    m_ownerDocument->detachRange(this);

#ifndef NDEBUG
    rangeCounter.decrement();
#endif
}

void Range::setDocument(Document& document)
{
    ASSERT(m_ownerDocument.ptr() != &document);
    m_ownerDocument->detachRange(this);
    m_ownerDocument = document;
    m_start.setToStartOfNode(&document);
    m_end.setToStartOfNode(&document);
    m_ownerDocument->attachRange(this);
}

Node* Range::commonAncestorContainer(Node* containerA, Node* containerB)
{
    for (Node* parentA = containerA; parentA; parentA = parentA->parentNode()) {
        for (Node* parentB = containerB; parentB; parentB = parentB->parentNode()) {
            if (parentA == parentB)
                return parentA;
        }
    }
    return nullptr;
}

static inline bool checkForDifferentRootContainer(const RangeBoundaryPoint& start, const RangeBoundaryPoint& end)
{
    Node* endRootContainer = end.container();
    while (endRootContainer->parentNode())
        endRootContainer = endRootContainer->parentNode();
    Node* startRootContainer = start.container();
    while (startRootContainer->parentNode())
        startRootContainer = startRootContainer->parentNode();

    return startRootContainer != endRootContainer || (Range::compareBoundaryPoints(start, end, ASSERT_NO_EXCEPTION) > 0);
}

void Range::setStart(PassRefPtr<Node> refNode, int offset, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    bool didMoveDocument = false;
    if (&refNode->document() != &ownerDocument()) {
        setDocument(refNode->document());
        didMoveDocument = true;
    }

    ec = 0;
    Node* childNode = checkNodeWOffset(refNode.get(), offset, ec);
    if (ec)
        return;

    m_start.set(refNode, offset, childNode);

    if (didMoveDocument || checkForDifferentRootContainer(m_start, m_end))
        collapse(true);
}

void Range::setEnd(PassRefPtr<Node> refNode, int offset, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    bool didMoveDocument = false;
    if (&refNode->document() != &ownerDocument()) {
        setDocument(refNode->document());
        didMoveDocument = true;
    }

    ec = 0;
    Node* childNode = checkNodeWOffset(refNode.get(), offset, ec);
    if (ec)
        return;

    m_end.set(refNode, offset, childNode);

    if (didMoveDocument || checkForDifferentRootContainer(m_start, m_end))
        collapse(false);
}

void Range::setStart(const Position& start, ExceptionCode& ec)
{
    Position parentAnchored = start.parentAnchoredEquivalent();
    setStart(parentAnchored.containerNode(), parentAnchored.offsetInContainerNode(), ec);
}

void Range::setEnd(const Position& end, ExceptionCode& ec)
{
    Position parentAnchored = end.parentAnchoredEquivalent();
    setEnd(parentAnchored.containerNode(), parentAnchored.offsetInContainerNode(), ec);
}

void Range::collapse(bool toStart)
{
    if (toStart)
        m_end = m_start;
    else
        m_start = m_end;
}

bool Range::isPointInRange(Node* refNode, int offset, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return false;
    }

    if (&refNode->document() != &ownerDocument()) {
        return false;
    }

    ec = 0;
    checkNodeWOffset(refNode, offset, ec);
    if (ec) {
        // DOM4 spec requires us to check whether refNode and start container have the same root first
        // but we do it in the reverse order to avoid O(n) operation here in common case.
        if (!commonAncestorContainer(refNode, &startContainer()))
            ec = 0;
        return false;
    }

    bool result = compareBoundaryPoints(refNode, offset, &startContainer(), m_start.offset(), ec) >= 0 && !ec
        && compareBoundaryPoints(refNode, offset, &endContainer(), m_end.offset(), ec) <= 0 && !ec;
    ASSERT(!ec || ec == WRONG_DOCUMENT_ERR);
    ec = 0;
    return result;
}

short Range::comparePoint(Node* refNode, int offset, ExceptionCode& ec) const
{
    // http://developer.mozilla.org/en/docs/DOM:range.comparePoint
    // This method returns -1, 0 or 1 depending on if the point described by the 
    // refNode node and an offset within the node is before, same as, or after the range respectively.

    if (!refNode) {
        ec = TypeError;
        return 0;
    }

    if (&refNode->document() != &ownerDocument()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    ec = 0;
    checkNodeWOffset(refNode, offset, ec);
    if (ec) {
        // DOM4 spec requires us to check whether refNode and start container have the same root first
        // but we do it in the reverse order to avoid O(n) operation here in common case.
        if (!refNode->inDocument() && !commonAncestorContainer(refNode, &startContainer()))
            ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    // compare to start, and point comes before
    if (compareBoundaryPoints(refNode, offset, &startContainer(), m_start.offset(), ec) < 0)
        return -1;

    if (ec)
        return 0;

    // compare to end, and point comes after
    if (compareBoundaryPoints(refNode, offset, &endContainer(), m_end.offset(), ec) > 0 && !ec)
        return 1;

    // point is in the middle of this range, or on the boundary points
    return 0;
}

Range::CompareResults Range::compareNode(Node* refNode, ExceptionCode& ec) const
{
    // http://developer.mozilla.org/en/docs/DOM:range.compareNode
    // This method returns 0, 1, 2, or 3 based on if the node is before, after,
    // before and after(surrounds), or inside the range, respectively

    if (!refNode) {
        ec = TypeError;
        return NODE_BEFORE;
    }

    if (!refNode->inDocument()) {
        // Firefox doesn't throw an exception for this case; it returns 0.
        return NODE_BEFORE;
    }

    if (&refNode->document() != &ownerDocument()) {
        // Firefox doesn't throw an exception for this case; it returns 0.
        return NODE_BEFORE;
    }

    ContainerNode* parentNode = refNode->parentNode();
    unsigned nodeIndex = refNode->computeNodeIndex();
    
    if (!parentNode) {
        // if the node is the top document we should return NODE_BEFORE_AND_AFTER
        // but we throw to match firefox behavior
        ec = NOT_FOUND_ERR;
        return NODE_BEFORE;
    }

    if (comparePoint(parentNode, nodeIndex, ec) < 0) { // starts before
        if (comparePoint(parentNode, nodeIndex + 1, ec) > 0) // ends after the range
            return NODE_BEFORE_AND_AFTER;
        return NODE_BEFORE; // ends before or in the range
    } else { // starts at or after the range start
        if (comparePoint(parentNode, nodeIndex + 1, ec) > 0) // ends after the range
            return NODE_AFTER;
        return NODE_INSIDE; // ends inside the range
    }
}

short Range::compareBoundaryPoints(CompareHow how, const Range* sourceRange, ExceptionCode& ec) const
{
    if (!sourceRange) {
        ec = TypeError;
        return 0;
    }

    Node* thisCont = commonAncestorContainer();
    Node* sourceCont = sourceRange->commonAncestorContainer();

    if (&thisCont->document() != &sourceCont->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    Node* thisTop = thisCont;
    Node* sourceTop = sourceCont;
    while (thisTop->parentNode())
        thisTop = thisTop->parentNode();
    while (sourceTop->parentNode())
        sourceTop = sourceTop->parentNode();
    if (thisTop != sourceTop) { // in different DocumentFragments
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    switch (how) {
        case START_TO_START:
            return compareBoundaryPoints(m_start, sourceRange->m_start, ec);
        case START_TO_END:
            return compareBoundaryPoints(m_end, sourceRange->m_start, ec);
        case END_TO_END:
            return compareBoundaryPoints(m_end, sourceRange->m_end, ec);
        case END_TO_START:
            return compareBoundaryPoints(m_start, sourceRange->m_end, ec);
    }

    ec = SYNTAX_ERR;
    return 0;
}

short Range::compareBoundaryPointsForBindings(unsigned short compareHow, const Range* sourceRange, ExceptionCode& ec) const
{
    if (compareHow > END_TO_START) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    return compareBoundaryPoints(static_cast<CompareHow>(compareHow), sourceRange, ec);
}

short Range::compareBoundaryPoints(Node* containerA, int offsetA, Node* containerB, int offsetB, ExceptionCode& ec)
{
    ASSERT(containerA);
    ASSERT(containerB);

    if (!containerA)
        return -1;
    if (!containerB)
        return 1;

    // see DOM2 traversal & range section 2.5

    // case 1: both points have the same container
    if (containerA == containerB) {
        if (offsetA == offsetB)
            return 0;           // A is equal to B
        if (offsetA < offsetB)
            return -1;          // A is before B
        else
            return 1;           // A is after B
    }

    // case 2: node C (container B or an ancestor) is a child node of A
    Node* c = containerB;
    while (c && c->parentNode() != containerA)
        c = c->parentNode();
    if (c) {
        int offsetC = 0;
        Node* n = containerA->firstChild();
        while (n != c && offsetC < offsetA) {
            offsetC++;
            n = n->nextSibling();
        }

        if (offsetA <= offsetC)
            return -1;              // A is before B
        else
            return 1;               // A is after B
    }

    // case 3: node C (container A or an ancestor) is a child node of B
    c = containerA;
    while (c && c->parentNode() != containerB)
        c = c->parentNode();
    if (c) {
        int offsetC = 0;
        Node* n = containerB->firstChild();
        while (n != c && offsetC < offsetB) {
            offsetC++;
            n = n->nextSibling();
        }

        if (offsetC < offsetB)
            return -1;              // A is before B
        else
            return 1;               // A is after B
    }

    // case 4: containers A & B are siblings, or children of siblings
    // ### we need to do a traversal here instead
    Node* commonAncestor = commonAncestorContainer(containerA, containerB);
    if (!commonAncestor) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }
    Node* childA = containerA;
    while (childA && childA->parentNode() != commonAncestor)
        childA = childA->parentNode();
    if (!childA)
        childA = commonAncestor;
    Node* childB = containerB;
    while (childB && childB->parentNode() != commonAncestor)
        childB = childB->parentNode();
    if (!childB)
        childB = commonAncestor;

    if (childA == childB)
        return 0; // A is equal to B

    Node* n = commonAncestor->firstChild();
    while (n) {
        if (n == childA)
            return -1; // A is before B
        if (n == childB)
            return 1; // A is after B
        n = n->nextSibling();
    }

    // Should never reach this point.
    ASSERT_NOT_REACHED();
    return 0;
}

short Range::compareBoundaryPoints(const RangeBoundaryPoint& boundaryA, const RangeBoundaryPoint& boundaryB, ExceptionCode& ec)
{
    return compareBoundaryPoints(boundaryA.container(), boundaryA.offset(), boundaryB.container(), boundaryB.offset(), ec);
}

bool Range::boundaryPointsValid() const
{
    ExceptionCode ec = 0;
    return compareBoundaryPoints(m_start, m_end, ec) <= 0 && !ec;
}

void Range::deleteContents(ExceptionCode& ec)
{
    processContents(Delete, ec);
}

bool Range::intersectsNode(Node* refNode, ExceptionCode& ec) const
{
    if (!refNode) {
        ec = TypeError;
        return false;
    }

    if (!refNode->inDocument() || &refNode->document() != &ownerDocument())
        return false;

    ContainerNode* parentNode = refNode->parentNode();
    if (!parentNode)
        return true;

    unsigned nodeIndex = refNode->computeNodeIndex();

    // If (parent, offset) is before end and (parent, offset + 1) is after start, return true.
    // Otherwise, return false.
    short compareFirst = comparePoint(parentNode, nodeIndex, ec);
    short compareSecond = comparePoint(parentNode, nodeIndex + 1, ec);

    bool isFirstBeforeEnd = m_start == m_end ? compareFirst < 0 : compareFirst <= 0;
    bool isSecondAfterStart = m_start == m_end ? compareSecond > 0 : compareSecond >= 0;

    return isFirstBeforeEnd && isSecondAfterStart;
}

static inline Node* highestAncestorUnderCommonRoot(Node* node, Node* commonRoot)
{
    if (node == commonRoot)
        return 0;

    ASSERT(commonRoot->contains(node));

    while (node->parentNode() != commonRoot)
        node = node->parentNode();

    return node;
}

static inline Node* childOfCommonRootBeforeOffset(Node* container, unsigned offset, Node* commonRoot)
{
    ASSERT(container);
    ASSERT(commonRoot);
    
    if (!commonRoot->contains(container))
        return 0;

    if (container == commonRoot) {
        container = container->firstChild();
        for (unsigned i = 0; container && i < offset; i++)
            container = container->nextSibling();
    } else {
        while (container->parentNode() != commonRoot)
            container = container->parentNode();
    }

    return container;
}

static inline unsigned lengthOfContentsInNode(Node& node)
{
    // This switch statement must be consistent with that of Range::processContentsBetweenOffsets.
    switch (node.nodeType()) {
    case Node::DOCUMENT_TYPE_NODE:
        return 0;
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
    case Node::PROCESSING_INSTRUCTION_NODE:
        return downcast<CharacterData>(node).length();
    case Node::ELEMENT_NODE:
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        return downcast<ContainerNode>(node).countChildNodes();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

RefPtr<DocumentFragment> Range::processContents(ActionType action, ExceptionCode& ec)
{
    typedef Vector<RefPtr<Node>> NodeVector;

    RefPtr<DocumentFragment> fragment;
    if (action == Extract || action == Clone)
        fragment = DocumentFragment::create(ownerDocument());

    if (collapsed())
        return fragment;

    RefPtr<Node> commonRoot = commonAncestorContainer();
    ASSERT(commonRoot);

    if (&startContainer() == &endContainer()) {
        processContentsBetweenOffsets(action, fragment, &startContainer(), m_start.offset(), m_end.offset(), ec);
        return fragment;
    }

    // Since mutation events can modify the range during the process, the boundary points need to be saved.
    RangeBoundaryPoint originalStart(m_start);
    RangeBoundaryPoint originalEnd(m_end);

    // what is the highest node that partially selects the start / end of the range?
    RefPtr<Node> partialStart = highestAncestorUnderCommonRoot(originalStart.container(), commonRoot.get());
    RefPtr<Node> partialEnd = highestAncestorUnderCommonRoot(originalEnd.container(), commonRoot.get());

    // Start and end containers are different.
    // There are three possibilities here:
    // 1. Start container == commonRoot (End container must be a descendant)
    // 2. End container == commonRoot (Start container must be a descendant)
    // 3. Neither is commonRoot, they are both descendants
    //
    // In case 3, we grab everything after the start (up until a direct child
    // of commonRoot) into leftContents, and everything before the end (up until
    // a direct child of commonRoot) into rightContents. Then we process all
    // commonRoot children between leftContents and rightContents
    //
    // In case 1 or 2, we skip either processing of leftContents or rightContents,
    // in which case the last lot of nodes either goes from the first or last
    // child of commonRoot.
    //
    // These are deleted, cloned, or extracted (i.e. both) depending on action.

    // Note that we are verifying that our common root hierarchy is still intact
    // after any DOM mutation event, at various stages below. See webkit bug 60350.

    RefPtr<Node> leftContents;
    if (originalStart.container() != commonRoot && commonRoot->contains(originalStart.container())) {
        leftContents = processContentsBetweenOffsets(action, 0, originalStart.container(), originalStart.offset(), lengthOfContentsInNode(*originalStart.container()), ec);
        leftContents = processAncestorsAndTheirSiblings(action, originalStart.container(), ProcessContentsForward, leftContents, commonRoot.get(), ec);
    }

    RefPtr<Node> rightContents;
    if (&endContainer() != commonRoot && commonRoot->contains(originalEnd.container())) {
        rightContents = processContentsBetweenOffsets(action, 0, originalEnd.container(), 0, originalEnd.offset(), ec);
        rightContents = processAncestorsAndTheirSiblings(action, originalEnd.container(), ProcessContentsBackward, rightContents, commonRoot.get(), ec);
    }

    // delete all children of commonRoot between the start and end container
    RefPtr<Node> processStart = childOfCommonRootBeforeOffset(originalStart.container(), originalStart.offset(), commonRoot.get());
    if (processStart && originalStart.container() != commonRoot) // processStart contains nodes before m_start.
        processStart = processStart->nextSibling();
    RefPtr<Node> processEnd = childOfCommonRootBeforeOffset(originalEnd.container(), originalEnd.offset(), commonRoot.get());

    // Collapse the range, making sure that the result is not within a node that was partially selected.
    ec = 0;
    if (action == Extract || action == Delete) {
        if (partialStart && commonRoot->contains(partialStart.get()))
            setStart(partialStart->parentNode(), partialStart->computeNodeIndex() + 1, ec);
        else if (partialEnd && commonRoot->contains(partialEnd.get()))
            setStart(partialEnd->parentNode(), partialEnd->computeNodeIndex(), ec);
        if (ec)
            return nullptr;
        m_end = m_start;
    }

    // Now add leftContents, stuff in between, and rightContents to the fragment
    // (or just delete the stuff in between)

    if ((action == Extract || action == Clone) && leftContents)
        fragment->appendChild(*leftContents, ec);

    if (processStart) {
        NodeVector nodes;
        for (Node* n = processStart.get(); n && n != processEnd; n = n->nextSibling())
            nodes.append(n);
        processNodes(action, nodes, commonRoot, fragment, ec);
    }

    if ((action == Extract || action == Clone) && rightContents)
        fragment->appendChild(*rightContents, ec);

    return fragment;
}

static inline void deleteCharacterData(PassRefPtr<CharacterData> data, unsigned startOffset, unsigned endOffset, ExceptionCode& ec)
{
    if (data->length() - endOffset)
        data->deleteData(endOffset, data->length() - endOffset, ec);
    if (startOffset)
        data->deleteData(0, startOffset, ec);
}

RefPtr<Node> Range::processContentsBetweenOffsets(ActionType action, PassRefPtr<DocumentFragment> fragment, Node* container, unsigned startOffset, unsigned endOffset, ExceptionCode& ec)
{
    ASSERT(container);
    ASSERT(startOffset <= endOffset);

    // This switch statement must be consistent with that of lengthOfContentsInNode.
    RefPtr<Node> result;
    switch (container->nodeType()) {
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
        endOffset = std::min(endOffset, static_cast<CharacterData*>(container)->length());
        startOffset = std::min(startOffset, endOffset);
        if (action == Extract || action == Clone) {
            RefPtr<CharacterData> c = static_cast<CharacterData*>(container->cloneNode(true).ptr());
            deleteCharacterData(c, startOffset, endOffset, ec);
            if (fragment) {
                result = fragment;
                result->appendChild(c.release(), ec);
            } else
                result = c.release();
        }
        if (action == Extract || action == Delete)
            downcast<CharacterData>(*container).deleteData(startOffset, endOffset - startOffset, ec);
        break;
    case Node::PROCESSING_INSTRUCTION_NODE:
        endOffset = std::min(endOffset, static_cast<ProcessingInstruction*>(container)->data().length());
        startOffset = std::min(startOffset, endOffset);
        if (action == Extract || action == Clone) {
            RefPtr<ProcessingInstruction> c = static_cast<ProcessingInstruction*>(container->cloneNode(true).ptr());
            c->setData(c->data().substring(startOffset, endOffset - startOffset));
            if (fragment) {
                result = fragment;
                result->appendChild(c.release(), ec);
            } else
                result = c.release();
        }
        if (action == Extract || action == Delete) {
            ProcessingInstruction& pi = downcast<ProcessingInstruction>(*container);
            String data(pi.data());
            data.remove(startOffset, endOffset - startOffset);
            pi.setData(data);
        }
        break;
    case Node::ELEMENT_NODE:
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_TYPE_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        // FIXME: Should we assert that some nodes never appear here?
        if (action == Extract || action == Clone) {
            if (fragment)
                result = fragment;
            else
                result = container->cloneNode(false);
        }

        Node* n = container->firstChild();
        Vector<RefPtr<Node>> nodes;
        for (unsigned i = startOffset; n && i; i--)
            n = n->nextSibling();
        for (unsigned i = startOffset; n && i < endOffset; i++, n = n->nextSibling()) {
            if (action != Delete && n->isDocumentTypeNode()) {
                ec = HIERARCHY_REQUEST_ERR;
                return nullptr;
            }
            nodes.append(n);
        }

        processNodes(action, nodes, container, result, ec);
        break;
    }

    return result;
}

void Range::processNodes(ActionType action, Vector<RefPtr<Node>>& nodes, PassRefPtr<Node> oldContainer, PassRefPtr<Node> newContainer, ExceptionCode& ec)
{
    for (auto& node : nodes) {
        switch (action) {
        case Delete:
            oldContainer->removeChild(node.get(), ec);
            break;
        case Extract:
            newContainer->appendChild(node.release(), ec); // will remove n from its parent
            break;
        case Clone:
            newContainer->appendChild(node->cloneNode(true), ec);
            break;
        }
    }
}

RefPtr<Node> Range::processAncestorsAndTheirSiblings(ActionType action, Node* container, ContentsProcessDirection direction, PassRefPtr<Node> passedClonedContainer, Node* commonRoot, ExceptionCode& ec)
{
    typedef Vector<RefPtr<Node>> NodeVector;

    RefPtr<Node> clonedContainer = passedClonedContainer;
    Vector<RefPtr<Node>> ancestors;
    for (ContainerNode* n = container->parentNode(); n && n != commonRoot; n = n->parentNode())
        ancestors.append(n);

    RefPtr<Node> firstChildInAncestorToProcess = direction == ProcessContentsForward ? container->nextSibling() : container->previousSibling();
    for (auto& ancestor : ancestors) {
        if (action == Extract || action == Clone) {
            if (RefPtr<Node> clonedAncestor = ancestor->cloneNode(false)) { // Might have been removed already during mutation event.
                clonedAncestor->appendChild(clonedContainer, ec);
                clonedContainer = clonedAncestor;
            }
        }

        // Copy siblings of an ancestor of start/end containers
        // FIXME: This assertion may fail if DOM is modified during mutation event
        // FIXME: Share code with Range::processNodes
        ASSERT(!firstChildInAncestorToProcess || firstChildInAncestorToProcess->parentNode() == ancestor);
        
        NodeVector nodes;
        for (Node* child = firstChildInAncestorToProcess.get(); child;
            child = (direction == ProcessContentsForward) ? child->nextSibling() : child->previousSibling())
            nodes.append(child);

        for (auto& child : nodes) {
            switch (action) {
            case Delete:
                ancestor->removeChild(child.get(), ec);
                break;
            case Extract: // will remove child from ancestor
                if (direction == ProcessContentsForward)
                    clonedContainer->appendChild(child.get(), ec);
                else
                    clonedContainer->insertBefore(child.get(), clonedContainer->firstChild(), ec);
                break;
            case Clone:
                if (direction == ProcessContentsForward)
                    clonedContainer->appendChild(child->cloneNode(true), ec);
                else
                    clonedContainer->insertBefore(child->cloneNode(true), clonedContainer->firstChild(), ec);
                break;
            }
        }
        firstChildInAncestorToProcess = direction == ProcessContentsForward ? ancestor->nextSibling() : ancestor->previousSibling();
    }

    return clonedContainer;
}

RefPtr<DocumentFragment> Range::extractContents(ExceptionCode& ec)
{
    return processContents(Extract, ec);
}

RefPtr<DocumentFragment> Range::cloneContents(ExceptionCode& ec)
{
    return processContents(Clone, ec);
}

void Range::insertNode(RefPtr<Node>&& node, ExceptionCode& ec)
{
    if (!node) {
        ec = TypeError;
        return;
    }

    bool startIsCharacterData = is<CharacterData>(startContainer());
    if (startIsCharacterData && !startContainer().parentNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }
    bool startIsText = startIsCharacterData && startContainer().nodeType() == Node::TEXT_NODE;
    RefPtr<Node> referenceNode = startIsText ? &startContainer() : startContainer().traverseToChildAt(startOffset());
    Node* parentNode = referenceNode ? referenceNode->parentNode() : &startContainer();
    if (!is<ContainerNode>(parentNode)) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }

    Ref<ContainerNode> parent = downcast<ContainerNode>(*parentNode);

    ec = 0;
    if (!parent->ensurePreInsertionValidity(*node, referenceNode.get(), ec))
        return;

    EventQueueScope scope;
    if (startIsText) {
        referenceNode = downcast<Text>(startContainer()).splitText(startOffset(), ec);
        if (ec)
            return;
    }

    if (referenceNode == node)
        referenceNode = referenceNode->nextSibling();

    node->remove(ec);
    if (ec)
        return;

    unsigned newOffset = referenceNode ? referenceNode->computeNodeIndex() : parent->countChildNodes();
    if (is<DocumentFragment>(*node))
        newOffset += downcast<DocumentFragment>(*node).countChildNodes();
    else
        ++newOffset;

    parent->insertBefore(node.releaseNonNull(), referenceNode.get(), ec);
    if (ec)
        return;

    if (collapsed())
        setEnd(parent.ptr(), newOffset, ec);
}

String Range::toString() const
{
    StringBuilder builder;

    Node* pastLast = pastLastNode();
    for (Node* n = firstNode(); n != pastLast; n = NodeTraversal::next(*n)) {
        if (n->nodeType() == Node::TEXT_NODE || n->nodeType() == Node::CDATA_SECTION_NODE) {
            const String& data = static_cast<CharacterData*>(n)->data();
            int length = data.length();
            int start = n == &startContainer() ? std::min(std::max(0, m_start.offset()), length) : 0;
            int end = n == &endContainer() ? std::min(std::max(start, m_end.offset()), length) : length;
            builder.append(data, start, end - start);
        }
    }

    return builder.toString();
}

String Range::toHTML() const
{
    return createMarkup(*this);
}

String Range::text() const
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    // FIXME: As with innerText, we'd like this to work even if there are no render objects.
    startContainer().document().updateLayout();

    return plainText(this);
}

RefPtr<DocumentFragment> Range::createContextualFragment(const String& markup, ExceptionCode& ec)
{
    Node* element = startContainer().isElementNode() ? &startContainer() : startContainer().parentNode();
    if (!element || !element->isHTMLElement()) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    return WebCore::createContextualFragment(markup, downcast<HTMLElement>(element), AllowScriptingContentAndDoNotMarkAlreadyStarted, ec);
}


void Range::detach()
{
    // This is now a no-op as per the DOM specification.
}

Node* Range::checkNodeWOffset(Node* n, int offset, ExceptionCode& ec) const
{
    switch (n->nodeType()) {
        case Node::DOCUMENT_TYPE_NODE:
            ec = INVALID_NODE_TYPE_ERR;
            return nullptr;
        case Node::CDATA_SECTION_NODE:
        case Node::COMMENT_NODE:
        case Node::TEXT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
            if (static_cast<unsigned>(offset) > downcast<CharacterData>(*n).length())
                ec = INDEX_SIZE_ERR;
            return nullptr;
        case Node::ATTRIBUTE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
        case Node::DOCUMENT_NODE:
        case Node::ELEMENT_NODE: {
            if (!offset)
                return nullptr;
            Node* childBefore = n->traverseToChildAt(offset - 1);
            if (!childBefore)
                ec = INDEX_SIZE_ERR;
            return childBefore;
        }
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

Ref<Range> Range::cloneRange() const
{
    return Range::create(ownerDocument(), &startContainer(), m_start.offset(), &endContainer(), m_end.offset());
}

void Range::setStartAfter(Node* refNode, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    if (!refNode->parentNode()) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    setStart(refNode->parentNode(), refNode->computeNodeIndex() + 1, ec);
}

void Range::setEndBefore(Node* refNode, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    if (!refNode->parentNode()) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    setEnd(refNode->parentNode(), refNode->computeNodeIndex(), ec);
}

void Range::setEndAfter(Node* refNode, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    if (!refNode->parentNode()) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    setEnd(refNode->parentNode(), refNode->computeNodeIndex() + 1, ec);
}

void Range::selectNode(Node* refNode, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    if (!refNode->parentNode()) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    if (&ownerDocument() != &refNode->document())
        setDocument(refNode->document());

    unsigned index = refNode->computeNodeIndex();
    ec = 0;
    setStart(refNode->parentNode(), index, ec);
    if (ec)
        return;
    setEnd(refNode->parentNode(), index + 1, ec);
}

void Range::selectNodeContents(Node* refNode, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    if (refNode->isDocumentTypeNode()) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    if (&ownerDocument() != &refNode->document())
        setDocument(refNode->document());

    m_start.setToStartOfNode(refNode);
    m_end.setToEndOfNode(refNode);
}

void Range::surroundContents(PassRefPtr<Node> passNewParent, ExceptionCode& ec)
{
    RefPtr<Node> newParent = passNewParent;

    if (!newParent) {
        ec = TypeError;
        return;
    }

    // INVALID_STATE_ERR: Raised if the Range partially selects a non-Text node.
    // https://dom.spec.whatwg.org/#dom-range-surroundcontents (step 1).
    Node* startNonTextContainer = &startContainer();
    if (startNonTextContainer->nodeType() == Node::TEXT_NODE)
        startNonTextContainer = startNonTextContainer->parentNode();
    Node* endNonTextContainer = &endContainer();
    if (endNonTextContainer->nodeType() == Node::TEXT_NODE)
        endNonTextContainer = endNonTextContainer->parentNode();
    if (startNonTextContainer != endNonTextContainer) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // INVALID_NODE_TYPE_ERR: Raised if node is an Attr, Entity, DocumentType,
    // Document, or DocumentFragment node.
    switch (newParent->nodeType()) {
        case Node::ATTRIBUTE_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_TYPE_NODE:
            ec = INVALID_NODE_TYPE_ERR;
            return;
        case Node::CDATA_SECTION_NODE:
        case Node::COMMENT_NODE:
        case Node::ELEMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::TEXT_NODE:
            break;
    }

    // Raise a HIERARCHY_REQUEST_ERR if startContainer() doesn't accept children like newParent.
    Node* parentOfNewParent = &startContainer();

    // If startContainer() is a character data node, it will be split and it will be its parent that will
    // need to accept newParent (or in the case of a comment, it logically "would" be inserted into the parent,
    // although this will fail below for another reason).
    if (parentOfNewParent->isCharacterDataNode())
        parentOfNewParent = parentOfNewParent->parentNode();
    if (!parentOfNewParent || !parentOfNewParent->childTypeAllowed(newParent->nodeType())) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }
    
    if (newParent->contains(&startContainer())) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }

    // FIXME: Do we need a check if the node would end up with a child node of a type not
    // allowed by the type of node?

    ec = 0;
    while (Node* n = newParent->firstChild()) {
        downcast<ContainerNode>(*newParent).removeChild(*n, ec);
        if (ec)
            return;
    }
    RefPtr<DocumentFragment> fragment = extractContents(ec);
    if (ec)
        return;
    insertNode(newParent.copyRef(), ec);
    if (ec)
        return;
    newParent->appendChild(fragment.release(), ec);
    if (ec)
        return;
    selectNode(newParent.get(), ec);
}

void Range::setStartBefore(Node* refNode, ExceptionCode& ec)
{
    if (!refNode) {
        ec = TypeError;
        return;
    }

    if (!refNode->parentNode()) {
        ec = INVALID_NODE_TYPE_ERR;
        return;
    }

    setStart(refNode->parentNode(), refNode->computeNodeIndex(), ec);
}

Node* Range::firstNode() const
{
    if (startContainer().offsetInCharacters())
        return &startContainer();
    if (Node* child = startContainer().traverseToChildAt(m_start.offset()))
        return child;
    if (!m_start.offset())
        return &startContainer();
    return NodeTraversal::nextSkippingChildren(startContainer());
}

ShadowRoot* Range::shadowRoot() const
{
    return startContainer().containingShadowRoot();
}

Node* Range::pastLastNode() const
{
    if (endContainer().offsetInCharacters())
        return NodeTraversal::nextSkippingChildren(endContainer());
    if (Node* child = endContainer().traverseToChildAt(m_end.offset()))
        return child;
    return NodeTraversal::nextSkippingChildren(endContainer());
}

IntRect Range::absoluteBoundingBox() const
{
    IntRect result;
    Vector<IntRect> rects;
    absoluteTextRects(rects);
    for (auto& rect : rects)
        result.unite(rect);
    return result;
}

void Range::absoluteTextRects(Vector<IntRect>& rects, bool useSelectionHeight, RangeInFixedPosition* inFixed) const
{
    bool allFixed = true;
    bool someFixed = false;

    Node* stopNode = pastLastNode();
    for (Node* node = firstNode(); node != stopNode; node = NodeTraversal::next(*node)) {
        RenderObject* renderer = node->renderer();
        if (!renderer)
            continue;
        bool isFixed = false;
        if (renderer->isBR())
            renderer->absoluteRects(rects, flooredLayoutPoint(renderer->localToAbsolute()));
        else if (is<RenderText>(*renderer)) {
            int startOffset = node == &startContainer() ? m_start.offset() : 0;
            int endOffset = node == &endContainer() ? m_end.offset() : std::numeric_limits<int>::max();
            rects.appendVector(downcast<RenderText>(*renderer).absoluteRectsForRange(startOffset, endOffset, useSelectionHeight, &isFixed));
        } else
            continue;
        allFixed &= isFixed;
        someFixed |= isFixed;
    }
    
    if (inFixed)
        *inFixed = allFixed ? EntirelyFixedPosition : (someFixed ? PartiallyFixedPosition : NotFixedPosition);
}

void Range::absoluteTextQuads(Vector<FloatQuad>& quads, bool useSelectionHeight, RangeInFixedPosition* inFixed) const
{
    bool allFixed = true;
    bool someFixed = false;

    Node* stopNode = pastLastNode();
    for (Node* node = firstNode(); node != stopNode; node = NodeTraversal::next(*node)) {
        RenderObject* renderer = node->renderer();
        if (!renderer)
            continue;
        bool isFixed = false;
        if (renderer->isBR())
            renderer->absoluteQuads(quads, &isFixed);
        else if (is<RenderText>(*renderer)) {
            int startOffset = node == &startContainer() ? m_start.offset() : 0;
            int endOffset = node == &endContainer() ? m_end.offset() : std::numeric_limits<int>::max();
            quads.appendVector(downcast<RenderText>(*renderer).absoluteQuadsForRange(startOffset, endOffset, useSelectionHeight, &isFixed));
        } else
            continue;
        allFixed &= isFixed;
        someFixed |= isFixed;
    }

    if (inFixed)
        *inFixed = allFixed ? EntirelyFixedPosition : (someFixed ? PartiallyFixedPosition : NotFixedPosition);
}

#if PLATFORM(IOS)
static bool intervalsSufficientlyOverlap(int startA, int endA, int startB, int endB)
{
    if (endA <= startA || endB <= startB)
        return false;

    const float sufficientOverlap = .75;

    int lengthA = endA - startA;
    int lengthB = endB - startB;

    int maxStart = std::max(startA, startB);
    int minEnd = std::min(endA, endB);

    if (maxStart > minEnd)
        return false;

    return minEnd - maxStart >= sufficientOverlap * std::min(lengthA, lengthB);
}

static inline void adjustLineHeightOfSelectionRects(Vector<SelectionRect>& rects, size_t numberOfRects, int lineNumber, int lineTop, int lineHeight)
{
    ASSERT(rects.size() >= numberOfRects);
    for (size_t i = numberOfRects; i; ) {
        --i;
        if (rects[i].lineNumber())
            break;
        rects[i].setLineNumber(lineNumber);
        rects[i].setLogicalTop(lineTop);
        rects[i].setLogicalHeight(lineHeight);
    }
}

static SelectionRect coalesceSelectionRects(const SelectionRect& original, const SelectionRect& previous)
{
    SelectionRect result(unionRect(previous.rect(), original.rect()), original.isHorizontal(), original.pageNumber());
    result.setDirection(original.containsStart() || original.containsEnd() ? original.direction() : previous.direction());
    result.setContainsStart(previous.containsStart() || original.containsStart());
    result.setContainsEnd(previous.containsEnd() || original.containsEnd());
    result.setIsFirstOnLine(previous.isFirstOnLine() || original.isFirstOnLine());
    result.setIsLastOnLine(previous.isLastOnLine() || original.isLastOnLine());
    return result;
}

// This function is similar in spirit to addLineBoxRects, but annotates the returned rectangles
// with additional state which helps iOS draw selections in its unique way.
void Range::collectSelectionRects(Vector<SelectionRect>& rects)
{
    auto& startContainer = this->startContainer();
    auto& endContainer = this->endContainer();
    int startOffset = m_start.offset();
    int endOffset = m_end.offset();

    Vector<SelectionRect> newRects;
    Node* stopNode = pastLastNode();
    bool hasFlippedWritingMode = startContainer.renderer() && startContainer.renderer()->style().isFlippedBlocksWritingMode();
    bool containsDifferentWritingModes = false;
    for (Node* node = firstNode(); node && node != stopNode; node = NodeTraversal::next(*node)) {
        RenderObject* renderer = node->renderer();
        // Only ask leaf render objects for their line box rects.
        if (renderer && !renderer->firstChildSlow() && renderer->style().userSelect() != SELECT_NONE) {
            bool isStartNode = renderer->node() == &startContainer;
            bool isEndNode = renderer->node() == &endContainer;
            if (hasFlippedWritingMode != renderer->style().isFlippedBlocksWritingMode())
                containsDifferentWritingModes = true;
            // FIXME: Sending 0 for the startOffset is a weird way of telling the renderer that the selection
            // doesn't start inside it, since we'll also send 0 if the selection *does* start in it, at offset 0.
            //
            // FIXME: Selection endpoints aren't always inside leaves, and we only build SelectionRects for leaves,
            // so we can't accurately determine which SelectionRects contain the selection start and end using
            // only the offsets of the start and end. We need to pass the whole Range.
            int beginSelectionOffset = isStartNode ? startOffset : 0;
            int endSelectionOffset = isEndNode ? endOffset : std::numeric_limits<int>::max();
            renderer->collectSelectionRects(newRects, beginSelectionOffset, endSelectionOffset);
            for (auto& selectionRect : newRects) {
                if (selectionRect.containsStart() && !isStartNode)
                    selectionRect.setContainsStart(false);
                if (selectionRect.containsEnd() && !isEndNode)
                    selectionRect.setContainsEnd(false);
                if (selectionRect.logicalWidth() || selectionRect.logicalHeight())
                    rects.append(selectionRect);
            }
            newRects.shrink(0);
        }
    }

    // The range could span over nodes with different writing modes.
    // If this is the case, we use the writing mode of the common ancestor.
    if (containsDifferentWritingModes) {
        if (Node* ancestor = commonAncestorContainer(&startContainer, &endContainer))
            hasFlippedWritingMode = ancestor->renderer()->style().isFlippedBlocksWritingMode();
    }

    const size_t numberOfRects = rects.size();

    // If the selection ends in a BR, then add the line break bit to the last
    // rect we have. This will cause its selection rect to extend to the
    // end of the line.
    if (stopNode && stopNode->hasTagName(HTMLNames::brTag) && numberOfRects) {
        // Only set the line break bit if the end of the range actually
        // extends all the way to include the <br>. VisiblePosition helps to
        // figure this out.
        VisiblePosition endPosition(createLegacyEditingPosition(&endContainer, endOffset), VP_DEFAULT_AFFINITY);
        VisiblePosition brPosition(createLegacyEditingPosition(stopNode, 0), VP_DEFAULT_AFFINITY);
        if (endPosition == brPosition)
            rects.last().setIsLineBreak(true);    
    }

    int lineTop = std::numeric_limits<int>::max();
    int lineBottom = std::numeric_limits<int>::min();
    int lastLineTop = lineTop;
    int lastLineBottom = lineBottom;
    int lineNumber = 0;

    for (size_t i = 0; i < numberOfRects; ++i) {
        int currentRectTop = rects[i].logicalTop();
        int currentRectBottom = currentRectTop + rects[i].logicalHeight();

        // We don't want to count the ruby text as a separate line.
        if (intervalsSufficientlyOverlap(currentRectTop, currentRectBottom, lineTop, lineBottom) || (i && rects[i].isRubyText())) {
            // Grow the current line bounds.
            lineTop = std::min(lineTop, currentRectTop);
            lineBottom = std::max(lineBottom, currentRectBottom);
            // Avoid overlap with the previous line.
            if (!hasFlippedWritingMode)
                lineTop = std::max(lastLineBottom, lineTop);
            else
                lineBottom = std::min(lastLineTop, lineBottom);
        } else {
            adjustLineHeightOfSelectionRects(rects, i, lineNumber, lineTop, lineBottom - lineTop);
            if (!hasFlippedWritingMode) {
                lastLineTop = lineTop;
                if (currentRectBottom >= lastLineTop) {
                    lastLineBottom = lineBottom;
                    lineTop = lastLineBottom;
                } else {
                    lineTop = currentRectTop;
                    lastLineBottom = std::numeric_limits<int>::min();
                }
                lineBottom = currentRectBottom;
            } else {
                lastLineBottom = lineBottom;
                if (currentRectTop <= lastLineBottom && i && rects[i].pageNumber() == rects[i - 1].pageNumber()) {
                    lastLineTop = lineTop;
                    lineBottom = lastLineTop;
                } else {
                    lastLineTop = std::numeric_limits<int>::max();
                    lineBottom = currentRectBottom;
                }
                lineTop = currentRectTop;
            }
            ++lineNumber;
        }
    }

    // Adjust line height.
    adjustLineHeightOfSelectionRects(rects, numberOfRects, lineNumber, lineTop, lineBottom - lineTop);

    // Sort the rectangles and make sure there are no gaps. The rectangles could be unsorted when
    // there is ruby text and we could have gaps on the line when adjacent elements on the line
    // have a different orientation.
    size_t firstRectWithCurrentLineNumber = 0;
    for (size_t currentRect = 1; currentRect < numberOfRects; ++currentRect) {
        if (rects[currentRect].lineNumber() != rects[currentRect - 1].lineNumber()) {
            firstRectWithCurrentLineNumber = currentRect;
            continue;
        }
        if (rects[currentRect].logicalLeft() >= rects[currentRect - 1].logicalLeft())
            continue;

        SelectionRect selectionRect = rects[currentRect];
        size_t i;
        for (i = currentRect; i > firstRectWithCurrentLineNumber && selectionRect.logicalLeft() < rects[i - 1].logicalLeft(); --i)
            rects[i] = rects[i - 1];
        rects[i] = selectionRect;
    }

    for (size_t j = 1; j < numberOfRects; ++j) {
        if (rects[j].lineNumber() != rects[j - 1].lineNumber())
            continue;
        SelectionRect& previousRect = rects[j - 1];
        bool previousRectMayNotReachRightEdge = (previousRect.direction() == LTR && previousRect.containsEnd()) || (previousRect.direction() == RTL && previousRect.containsStart());
        if (previousRectMayNotReachRightEdge)
            continue;
        int adjustedWidth = rects[j].logicalLeft() - previousRect.logicalLeft();
        if (adjustedWidth > previousRect.logicalWidth())
            previousRect.setLogicalWidth(adjustedWidth);
    }

    int maxLineNumber = lineNumber;

    // Extend rects out to edges as needed.
    for (size_t i = 0; i < numberOfRects; ++i) {
        SelectionRect& selectionRect = rects[i];
        if (!selectionRect.isLineBreak() && selectionRect.lineNumber() >= maxLineNumber)
            continue;
        if (selectionRect.direction() == RTL && selectionRect.isFirstOnLine()) {
            selectionRect.setLogicalWidth(selectionRect.logicalWidth() + selectionRect.logicalLeft() - selectionRect.minX());
            selectionRect.setLogicalLeft(selectionRect.minX());
        } else if (selectionRect.direction() == LTR && selectionRect.isLastOnLine())
            selectionRect.setLogicalWidth(selectionRect.maxX() - selectionRect.logicalLeft());
    }

    // Union all the rectangles on interior lines (i.e. not first or last).
    // On first and last lines, just avoid having overlaps by merging intersecting rectangles.
    Vector<SelectionRect> unionedRects;
    IntRect interiorUnionRect;
    for (size_t i = 0; i < numberOfRects; ++i) {
        SelectionRect& currentRect = rects[i];
        if (currentRect.lineNumber() == 1) {
            ASSERT(interiorUnionRect.isEmpty());
            if (!unionedRects.isEmpty()) {
                SelectionRect& previousRect = unionedRects.last();
                if (previousRect.rect().intersects(currentRect.rect())) {
                    previousRect = coalesceSelectionRects(currentRect, previousRect);
                    continue;
                }
            }
            // Couldn't merge with previous rect, so just appending.
            unionedRects.append(currentRect);
        } else if (currentRect.lineNumber() < maxLineNumber) {
            if (interiorUnionRect.isEmpty()) {
                // Start collecting interior rects.
                interiorUnionRect = currentRect.rect();         
            } else if (interiorUnionRect.intersects(currentRect.rect())
                || interiorUnionRect.maxX() == currentRect.rect().x()
                || interiorUnionRect.maxY() == currentRect.rect().y()
                || interiorUnionRect.x() == currentRect.rect().maxX()
                || interiorUnionRect.y() == currentRect.rect().maxY()) {
                // Only union the lines that are attached.
                // For iBooks, the interior lines may cross multiple horizontal pages.
                interiorUnionRect.unite(currentRect.rect());
            } else {
                unionedRects.append(SelectionRect(interiorUnionRect, currentRect.isHorizontal(), currentRect.pageNumber()));
                interiorUnionRect = currentRect.rect();
            }
        } else {
            // Processing last line.
            if (!interiorUnionRect.isEmpty()) {
                unionedRects.append(SelectionRect(interiorUnionRect, currentRect.isHorizontal(), currentRect.pageNumber()));
                interiorUnionRect = IntRect();
            }

            ASSERT(!unionedRects.isEmpty());
            SelectionRect& previousRect = unionedRects.last();
            if (previousRect.logicalTop() == currentRect.logicalTop() && previousRect.rect().intersects(currentRect.rect())) {
                // previousRect is also on the last line, and intersects the current one.
                previousRect = coalesceSelectionRects(currentRect, previousRect);
                continue;
            }
            // Couldn't merge with previous rect, so just appending.
            unionedRects.append(currentRect);
        }
    }

    rects.swap(unionedRects);
}
#endif

#if ENABLE(TREE_DEBUGGING)
void Range::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;

    const int FormatBufferSize = 1024;
    char s[FormatBufferSize];
    result.appendLiteral("from offset ");
    result.appendNumber(m_start.offset());
    result.appendLiteral(" of ");
    startContainer().formatForDebugger(s, FormatBufferSize);
    result.append(s);
    result.appendLiteral(" to offset ");
    result.appendNumber(m_end.offset());
    result.appendLiteral(" of ");
    endContainer().formatForDebugger(s, FormatBufferSize);
    result.append(s);

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}
#endif

bool Range::contains(const Range& other) const
{
    if (commonAncestorContainer()->ownerDocument() != other.commonAncestorContainer()->ownerDocument())
        return false;

    short startToStart = compareBoundaryPoints(Range::START_TO_START, &other, ASSERT_NO_EXCEPTION);
    if (startToStart > 0)
        return false;

    short endToEnd = compareBoundaryPoints(Range::END_TO_END, &other, ASSERT_NO_EXCEPTION);
    return endToEnd >= 0;
}

bool Range::contains(const VisiblePosition& position) const
{
    RefPtr<Range> positionRange = makeRange(position, position);
    if (!positionRange)
        return false;
    return contains(*positionRange);
}

bool areRangesEqual(const Range* a, const Range* b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    return a->startPosition() == b->startPosition() && a->endPosition() == b->endPosition();
}

bool rangesOverlap(const Range* a, const Range* b)
{
    if (!a || !b)
        return false;

    if (a == b)
        return true;

    if (a->commonAncestorContainer()->ownerDocument() != b->commonAncestorContainer()->ownerDocument())
        return false;

    short startToStart = a->compareBoundaryPoints(Range::START_TO_START, b, ASSERT_NO_EXCEPTION);
    short endToEnd = a->compareBoundaryPoints(Range::END_TO_END, b, ASSERT_NO_EXCEPTION);

    // First range contains the second range.
    if (startToStart <= 0 && endToEnd >= 0)
        return true;

    // End of first range is inside second range.
    if (a->compareBoundaryPoints(Range::START_TO_END, b, ASSERT_NO_EXCEPTION) >= 0 && endToEnd <= 0)
        return true;

    // Start of first range is inside second range.
    if (startToStart >= 0 && a->compareBoundaryPoints(Range::END_TO_START, b, ASSERT_NO_EXCEPTION) <= 0)
        return true;

    return false;
}

Ref<Range> rangeOfContents(Node& node)
{
    Ref<Range> range = Range::create(node.document());
    int exception = 0;
    range->selectNodeContents(&node, exception);
    return range;
}

static inline void boundaryNodeChildrenChanged(RangeBoundaryPoint& boundary, ContainerNode& container)
{
    if (!boundary.childBefore())
        return;
    if (boundary.container() != &container)
        return;
    boundary.invalidateOffset();
}

void Range::nodeChildrenChanged(ContainerNode& container)
{
    ASSERT(&container.document() == &ownerDocument());
    boundaryNodeChildrenChanged(m_start, container);
    boundaryNodeChildrenChanged(m_end, container);
}

static inline void boundaryNodeChildrenWillBeRemoved(RangeBoundaryPoint& boundary, ContainerNode& container)
{
    for (Node* nodeToBeRemoved = container.firstChild(); nodeToBeRemoved; nodeToBeRemoved = nodeToBeRemoved->nextSibling()) {
        if (boundary.childBefore() == nodeToBeRemoved) {
            boundary.setToStartOfNode(&container);
            return;
        }

        for (Node* n = boundary.container(); n; n = n->parentNode()) {
            if (n == nodeToBeRemoved) {
                boundary.setToStartOfNode(&container);
                return;
            }
        }
    }
}

void Range::nodeChildrenWillBeRemoved(ContainerNode& container)
{
    ASSERT(&container.document() == &ownerDocument());
    boundaryNodeChildrenWillBeRemoved(m_start, container);
    boundaryNodeChildrenWillBeRemoved(m_end, container);
}

static inline void boundaryNodeWillBeRemoved(RangeBoundaryPoint& boundary, Node& nodeToBeRemoved)
{
    if (boundary.childBefore() == &nodeToBeRemoved) {
        boundary.childBeforeWillBeRemoved();
        return;
    }

    for (Node* n = boundary.container(); n; n = n->parentNode()) {
        if (n == &nodeToBeRemoved) {
            boundary.setToBeforeChild(nodeToBeRemoved);
            return;
        }
    }
}

void Range::nodeWillBeRemoved(Node& node)
{
    ASSERT(&node.document() == &ownerDocument());
    ASSERT(&node != &ownerDocument());
    ASSERT(node.parentNode());
    boundaryNodeWillBeRemoved(m_start, node);
    boundaryNodeWillBeRemoved(m_end, node);
}

static inline void boundaryTextInserted(RangeBoundaryPoint& boundary, Node* text, unsigned offset, unsigned length)
{
    if (boundary.container() != text)
        return;
    unsigned boundaryOffset = boundary.offset();
    if (offset >= boundaryOffset)
        return;
    boundary.setOffset(boundaryOffset + length);
}

void Range::textInserted(Node* text, unsigned offset, unsigned length)
{
    ASSERT(text);
    ASSERT(&text->document() == &ownerDocument());
    boundaryTextInserted(m_start, text, offset, length);
    boundaryTextInserted(m_end, text, offset, length);
}

static inline void boundaryTextRemoved(RangeBoundaryPoint& boundary, Node* text, unsigned offset, unsigned length)
{
    if (boundary.container() != text)
        return;
    unsigned boundaryOffset = boundary.offset();
    if (offset >= boundaryOffset)
        return;
    if (offset + length >= boundaryOffset)
        boundary.setOffset(offset);
    else
        boundary.setOffset(boundaryOffset - length);
}

void Range::textRemoved(Node* text, unsigned offset, unsigned length)
{
    ASSERT(text);
    ASSERT(&text->document() == &ownerDocument());
    boundaryTextRemoved(m_start, text, offset, length);
    boundaryTextRemoved(m_end, text, offset, length);
}

static inline void boundaryTextNodesMerged(RangeBoundaryPoint& boundary, NodeWithIndex& oldNode, unsigned offset)
{
    if (boundary.container() == oldNode.node())
        boundary.set(oldNode.node()->previousSibling(), boundary.offset() + offset, 0);
    else if (boundary.container() == oldNode.node()->parentNode() && boundary.offset() == oldNode.index())
        boundary.set(oldNode.node()->previousSibling(), offset, 0);
}

void Range::textNodesMerged(NodeWithIndex& oldNode, unsigned offset)
{
    ASSERT(oldNode.node());
    ASSERT(&oldNode.node()->document() == &ownerDocument());
    ASSERT(oldNode.node()->parentNode());
    ASSERT(oldNode.node()->isTextNode());
    ASSERT(oldNode.node()->previousSibling());
    ASSERT(oldNode.node()->previousSibling()->isTextNode());
    boundaryTextNodesMerged(m_start, oldNode, offset);
    boundaryTextNodesMerged(m_end, oldNode, offset);
}

static inline void boundaryTextNodesSplit(RangeBoundaryPoint& boundary, Text* oldNode)
{
    if (boundary.container() == oldNode) {
        unsigned splitOffset = oldNode->length();
        unsigned boundaryOffset = boundary.offset();
        if (boundaryOffset > splitOffset)
            boundary.set(oldNode->nextSibling(), boundaryOffset - splitOffset, 0);
        return;
    }
    auto* parent = oldNode->parentNode();
    if (boundary.container() == parent && boundary.childBefore() == oldNode) {
        auto* newChild = oldNode->nextSibling();
        ASSERT(newChild);
        boundary.setToAfterChild(*newChild);
    }
}

void Range::textNodeSplit(Text* oldNode)
{
    ASSERT(oldNode);
    ASSERT(&oldNode->document() == &ownerDocument());
    ASSERT(oldNode->parentNode());
    ASSERT(oldNode->isTextNode());
    ASSERT(oldNode->nextSibling());
    ASSERT(oldNode->nextSibling()->isTextNode());
    boundaryTextNodesSplit(m_start, oldNode);
    boundaryTextNodesSplit(m_end, oldNode);
}

void Range::expand(const String& unit, ExceptionCode& ec)
{
    VisiblePosition start(startPosition());
    VisiblePosition end(endPosition());
    if (unit == "word") {
        start = startOfWord(start);
        end = endOfWord(end);
    } else if (unit == "sentence") {
        start = startOfSentence(start);
        end = endOfSentence(end);
    } else if (unit == "block") {
        start = startOfParagraph(start);
        end = endOfParagraph(end);
    } else if (unit == "document") {
        start = startOfDocument(start);
        end = endOfDocument(end);
    } else
        return;
    setStart(start.deepEquivalent().containerNode(), start.deepEquivalent().computeOffsetInContainerNode(), ec);
    setEnd(end.deepEquivalent().containerNode(), end.deepEquivalent().computeOffsetInContainerNode(), ec);
}

Ref<ClientRectList> Range::getClientRects() const
{
    ownerDocument().updateLayoutIgnorePendingStylesheets();

    Vector<FloatQuad> quads;
    getBorderAndTextQuads(quads, CoordinateSpace::Client);

    return ClientRectList::create(quads);
}

Ref<ClientRect> Range::getBoundingClientRect() const
{
    return ClientRect::create(boundingRectInternal(CoordinateSpace::Client));
}

void Range::getBorderAndTextQuads(Vector<FloatQuad>& quads, CoordinateSpace space) const
{
    Node* stopNode = pastLastNode();

    HashSet<Node*> selectedElementsSet;
    for (Node* node = firstNode(); node != stopNode; node = NodeTraversal::next(*node)) {
        if (node->isElementNode())
            selectedElementsSet.add(node);
    }

    // Don't include elements that are only partially selected.
    Node* lastNode = m_end.childBefore() ? m_end.childBefore() : &endContainer();
    for (Node* parent = lastNode->parentNode(); parent; parent = parent->parentNode())
        selectedElementsSet.remove(parent);

    for (Node* node = firstNode(); node != stopNode; node = NodeTraversal::next(*node)) {
        if (is<Element>(*node) && selectedElementsSet.contains(node) && !selectedElementsSet.contains(node->parentNode())) {
            if (RenderBoxModelObject* renderBoxModelObject = downcast<Element>(*node).renderBoxModelObject()) {
                Vector<FloatQuad> elementQuads;
                renderBoxModelObject->absoluteQuads(elementQuads);

                if (space == CoordinateSpace::Client)
                    ownerDocument().adjustFloatQuadsForScrollAndAbsoluteZoomAndFrameScale(elementQuads, renderBoxModelObject->style());

                quads.appendVector(elementQuads);
            }
        } else if (is<Text>(*node)) {
            if (RenderText* renderText = downcast<Text>(*node).renderer()) {
                int startOffset = node == &startContainer() ? m_start.offset() : 0;
                int endOffset = node == &endContainer() ? m_end.offset() : INT_MAX;
                
                auto textQuads = renderText->absoluteQuadsForRange(startOffset, endOffset);

                if (space == CoordinateSpace::Client)
                    ownerDocument().adjustFloatQuadsForScrollAndAbsoluteZoomAndFrameScale(textQuads, renderText->style());

                quads.appendVector(textQuads);
            }
        }
    }
}

FloatRect Range::boundingRectInternal(CoordinateSpace space) const
{
    ownerDocument().updateLayoutIgnorePendingStylesheets();

    Vector<FloatQuad> quads;
    getBorderAndTextQuads(quads, space);

    FloatRect result;
    for (auto& quad : quads)
        result.unite(quad.boundingBox());

    return result;
}

FloatRect Range::absoluteBoundingRect() const
{
    return boundingRectInternal(CoordinateSpace::Absolute);
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showTree(const WebCore::Range* range)
{
    if (range && range->boundaryPointsValid()) {
        range->startContainer().showTreeAndMark(&range->startContainer(), "S", &range->endContainer(), "E");
        fprintf(stderr, "start offset: %d, end offset: %d\n", range->startOffset(), range->endOffset());
    }
}

#endif
