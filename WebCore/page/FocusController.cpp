/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FocusController.h"

#include "AXObjectCache.h"
#include "Chrome.h"
#include "Document.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SpatialNavigation.h"
#include "Widget.h"

namespace WebCore {

using namespace HTMLNames;
using namespace std;

static inline void dispatchEventsOnWindowAndFocusedNode(Document* document, bool focused)
{
    // If we have a focused node we should dispatch blur on it before we blur the window.
    // If we have a focused node we should dispatch focus on it after we focus the window.
    // https://bugs.webkit.org/show_bug.cgi?id=27105

    // Do not fire events while modal dialogs are up.  See https://bugs.webkit.org/show_bug.cgi?id=33962
    if (Page* page = document->page()) {
        if (page->defersLoading())
            return;
    }

    if (!focused && document->focusedNode())
        document->focusedNode()->dispatchBlurEvent();
    document->dispatchWindowEvent(Event::create(focused ? eventNames().focusEvent : eventNames().blurEvent, false, false));
    if (focused && document->focusedNode())
        document->focusedNode()->dispatchFocusEvent();
}

FocusController::FocusController(Page* page)
    : m_page(page)
    , m_isActive(false)
    , m_isFocused(false)
    , m_isChangingFocusedFrame(false)
{
}

void FocusController::setFocusedFrame(PassRefPtr<Frame> frame)
{
    if (m_focusedFrame == frame || m_isChangingFocusedFrame)
        return;

    m_isChangingFocusedFrame = true;

    RefPtr<Frame> oldFrame = m_focusedFrame;
    RefPtr<Frame> newFrame = frame;

    m_focusedFrame = newFrame;

    // Now that the frame is updated, fire events and update the selection focused states of both frames.
    if (oldFrame && oldFrame->view()) {
        oldFrame->selection()->setFocused(false);
        oldFrame->document()->dispatchWindowEvent(Event::create(eventNames().blurEvent, false, false));
    }

    if (newFrame && newFrame->view() && isFocused()) {
        newFrame->selection()->setFocused(true);
        newFrame->document()->dispatchWindowEvent(Event::create(eventNames().focusEvent, false, false));
    }

    m_isChangingFocusedFrame = false;
}

Frame* FocusController::focusedOrMainFrame()
{
    if (Frame* frame = focusedFrame())
        return frame;
    return m_page->mainFrame();
}

void FocusController::setFocused(bool focused)
{
    if (isFocused() == focused)
        return;
    
    m_isFocused = focused;

    if (!m_focusedFrame)
        setFocusedFrame(m_page->mainFrame());

    if (m_focusedFrame->view()) {
        m_focusedFrame->selection()->setFocused(focused);
        dispatchEventsOnWindowAndFocusedNode(m_focusedFrame->document(), focused);
    }
}

static Node* deepFocusableNode(FocusDirection direction, Node* node, KeyboardEvent* event)
{
    // The node we found might be a HTMLFrameOwnerElement, so descend down the frame tree until we find either:
    // 1) a focusable node, or
    // 2) the deepest-nested HTMLFrameOwnerElement
    while (node && node->isFrameOwnerElement()) {
        HTMLFrameOwnerElement* owner = static_cast<HTMLFrameOwnerElement*>(node);
        if (!owner->contentFrame())
            break;

        Document* document = owner->contentFrame()->document();

        node = (direction == FocusDirectionForward)
            ? document->nextFocusableNode(0, event)
            : document->previousFocusableNode(0, event);
        if (!node) {
            node = owner;
            break;
        }
    }

    return node;
}

bool FocusController::setInitialFocus(FocusDirection direction, KeyboardEvent* event)
{
    return advanceFocus(direction, event, true);
}

bool FocusController::advanceFocus(FocusDirection direction, KeyboardEvent* event, bool initialFocus)
{
    switch (direction) {
    case FocusDirectionForward:
    case FocusDirectionBackward:
        return advanceFocusInDocumentOrder(direction, event, initialFocus);
    case FocusDirectionLeft:
    case FocusDirectionRight:
    case FocusDirectionUp:
    case FocusDirectionDown:
        return advanceFocusDirectionally(direction, event);
    default:
        ASSERT_NOT_REACHED();
    }

    return false;
}

bool FocusController::advanceFocusInDocumentOrder(FocusDirection direction, KeyboardEvent* event, bool initialFocus)
{
    Frame* frame = focusedOrMainFrame();
    ASSERT(frame);
    Document* document = frame->document();

    Node* currentNode = document->focusedNode();
    // FIXME: Not quite correct when it comes to focus transitions leaving/entering the WebView itself
    bool caretBrowsing = focusedOrMainFrame()->settings()->caretBrowsingEnabled();

    if (caretBrowsing && !currentNode)
        currentNode = frame->selection()->start().node();

    document->updateLayoutIgnorePendingStylesheets();

    Node* node = (direction == FocusDirectionForward)
        ? document->nextFocusableNode(currentNode, event)
        : document->previousFocusableNode(currentNode, event);
            
    // If there's no focusable node to advance to, move up the frame tree until we find one.
    while (!node && frame) {
        Frame* parentFrame = frame->tree()->parent();
        if (!parentFrame)
            break;

        Document* parentDocument = parentFrame->document();

        HTMLFrameOwnerElement* owner = frame->ownerElement();
        if (!owner)
            break;

        node = (direction == FocusDirectionForward)
            ? parentDocument->nextFocusableNode(owner, event)
            : parentDocument->previousFocusableNode(owner, event);

        frame = parentFrame;
    }

    node = deepFocusableNode(direction, node, event);

    if (!node) {
        // We didn't find a node to focus, so we should try to pass focus to Chrome.
        if (!initialFocus && m_page->chrome()->canTakeFocus(direction)) {
            document->setFocusedNode(0);
            setFocusedFrame(0);
            m_page->chrome()->takeFocus(direction);
            return true;
        }

        // Chrome doesn't want focus, so we should wrap focus.
        Document* d = m_page->mainFrame()->document();
        node = (direction == FocusDirectionForward)
            ? d->nextFocusableNode(0, event)
            : d->previousFocusableNode(0, event);

        node = deepFocusableNode(direction, node, event);

        if (!node)
            return false;
    }

    ASSERT(node);

    if (node == document->focusedNode())
        // Focus wrapped around to the same node.
        return true;

    if (!node->isElementNode())
        // FIXME: May need a way to focus a document here.
        return false;

    if (node->isFrameOwnerElement()) {
        // We focus frames rather than frame owners.
        // FIXME: We should not focus frames that have no scrollbars, as focusing them isn't useful to the user.
        HTMLFrameOwnerElement* owner = static_cast<HTMLFrameOwnerElement*>(node);
        if (!owner->contentFrame())
            return false;

        document->setFocusedNode(0);
        setFocusedFrame(owner->contentFrame());
        return true;
    }
    
    // FIXME: It would be nice to just be able to call setFocusedNode(node) here, but we can't do
    // that because some elements (e.g. HTMLInputElement and HTMLTextAreaElement) do extra work in
    // their focus() methods.

    Document* newDocument = node->document();

    if (newDocument != document)
        // Focus is going away from this document, so clear the focused node.
        document->setFocusedNode(0);

    if (newDocument)
        setFocusedFrame(newDocument->frame());

    if (caretBrowsing) {
        VisibleSelection newSelection(Position(node, 0), Position(node, 0), DOWNSTREAM);
        if (frame->shouldChangeSelection(newSelection))
            frame->selection()->setSelection(newSelection);
    }

    static_cast<Element*>(node)->focus(false);
    return true;
}

bool FocusController::advanceFocusDirectionally(FocusDirection direction, KeyboardEvent* event)
{
    Frame* frame = focusedOrMainFrame();
    ASSERT(frame);
    Document* focusedDocument = frame->document();
    if (!focusedDocument)
        return false;

    Node* focusedNode = focusedDocument->focusedNode();
    if (!focusedNode) {
        // Just move to the first focusable node.
        FocusDirection tabDirection = (direction == FocusDirectionUp || direction == FocusDirectionLeft) ?
                                       FocusDirectionForward : FocusDirectionBackward;
        // 'initialFocus' is set to true so the chrome is not focused.
        return advanceFocusInDocumentOrder(tabDirection, event, true);
    }

    // Move up in the chain of nested frames.
    frame = frame->tree()->top();

    FocusCandidate focusCandidate;
    findFocusableNodeInDirection(frame->document()->firstChild(), focusedNode, direction, event, focusCandidate);

    Node* node = focusCandidate.node;
    if (!node || !node->isElementNode()) {
        // FIXME: May need a way to focus a document here.
        Frame* frame = focusedOrMainFrame();
        scrollInDirection(frame, direction);
        return false;
    }

    // In order to avoid crazy jump between links that are either far away from each other,
    // or just not currently visible, lets do a scroll in the given direction and bail out
    // if |node| element is not in the viewport.
    if (hasOffscreenRect(node)) {
        Frame* frame = node->document()->view()->frame();
        scrollInDirection(frame, direction, focusCandidate);
        return true;
    }

    Document* newDocument = node->document();

    if (newDocument != focusedDocument) {
        // Focus is going away from the originally focused document, so clear the focused node.
        focusedDocument->setFocusedNode(0);
    }

    if (newDocument)
        setFocusedFrame(newDocument->frame());

    Element* element = static_cast<Element*>(node);
    ASSERT(element);

    scrollIntoView(element);
    element->focus(false);
    return true;
}

static void updateFocusCandidateInSameContainer(const FocusCandidate& candidate, FocusCandidate& closest)
{
    if (closest.isNull()) {
        closest = candidate;
        return;
    }

    if (candidate.alignment == closest.alignment) {
        if (candidate.distance < closest.distance)
            closest = candidate;
        return;
    }

    if (candidate.alignment > closest.alignment)
        closest = candidate;
}

static void updateFocusCandidateIfCloser(Node* focusedNode, const FocusCandidate& candidate, FocusCandidate& closest)
{
    // First, check the common case: neither candidate nor closest are
    // inside scrollable content, then no need to care about enclosingScrollableBox
    // heuristics or parent{Distance,Alignment}, but only distance and alignment.
    if (!candidate.inScrollableContainer() && !closest.inScrollableContainer()) {
        updateFocusCandidateInSameContainer(candidate, closest);
        return;
    }

    bool sameContainer = candidate.document() == closest.document() && candidate.enclosingScrollableBox == closest.enclosingScrollableBox;

    // Second, if candidate and closest are in the same "container" (i.e. {i}frame or any
    // scrollable block element), we can handle them as common case.
    if (sameContainer) {
        updateFocusCandidateInSameContainer(candidate, closest);
        return;
    }

    // Last, we are considering moving to a candidate located in a different enclosing
    // scrollable box than closest.
    bool isInInnerDocument = !isInRootDocument(focusedNode);

    bool sameContainerAsCandidate = isInInnerDocument ? focusedNode->document() == candidate.document() :
        focusedNode->isDescendantOf(candidate.enclosingScrollableBox);

    bool sameContainerAsClosest = isInInnerDocument ? focusedNode->document() == closest.document() :
        focusedNode->isDescendantOf(closest.enclosingScrollableBox);

    // sameContainerAsCandidate and sameContainerAsClosest are mutually exclusive.
    ASSERT(!(sameContainerAsCandidate && sameContainerAsClosest));

    if (sameContainerAsCandidate) {
        closest = candidate;
        return;
    }

    if (sameContainerAsClosest) {
        // Nothing to be done.
        return;
    }

    // NOTE: !sameContainerAsCandidate && !sameContainerAsClosest
    // If distance is shorter, and we are talking about scrollable container,
    // lets compare parent distance and alignment before anything.
    if (candidate.distance < closest.distance) {
        if (candidate.alignment >= closest.parentAlignment
         || candidate.parentAlignment == closest.parentAlignment) {
            closest = candidate;
            return;
        }

    } else if (candidate.parentDistance < closest.distance) {
        if (candidate.parentAlignment >= closest.alignment) {
            closest = candidate;
            return;
        }
    }
}

void FocusController::findFocusableNodeInDirection(Node* outer, Node* focusedNode,
                                                   FocusDirection direction, KeyboardEvent* event,
                                                   FocusCandidate& closest, const FocusCandidate& candidateParent)
{
    ASSERT(outer);
    ASSERT(candidateParent.isNull()
        || candidateParent.node->hasTagName(frameTag)
        || candidateParent.node->hasTagName(iframeTag)
        || isScrollableContainerNode(candidateParent.node));

    // Walk all the child nodes and update closest if we find a nearer node.
    Node* node = outer;
    while (node) {

        // Inner documents case.
        if (node->isFrameOwnerElement()) {
            deepFindFocusableNodeInDirection(node, focusedNode, direction, event, closest);

        // Scrollable block elements (e.g. <div>, etc) case.
        } else if (isScrollableContainerNode(node)) {
            deepFindFocusableNodeInDirection(node, focusedNode, direction, event, closest);
            node = node->traverseNextSibling();
            continue;

        } else if (node != focusedNode && node->isKeyboardFocusable(event)) {
            FocusCandidate candidate(node);

            // There are two ways to identify we are in a recursive call from deepFindFocusableNodeInDirection
            // (i.e. processing an element in an iframe, frame or a scrollable block element):

            // 1) If candidateParent is not null, and it holds the distance and alignment data of the
            // parent container element itself;
            // 2) Parent of outer is <frame> or <iframe>;
            // 3) Parent is any other scrollable block element.
            if (!candidateParent.isNull()) {
                candidate.parentAlignment = candidateParent.alignment;
                candidate.parentDistance = candidateParent.distance;
                candidate.enclosingScrollableBox = candidateParent.node;

            } else if (!isInRootDocument(outer)) {
                if (Document* document = static_cast<Document*>(outer->parent()))
                    candidate.enclosingScrollableBox = static_cast<Node*>(document->ownerElement());

            } else if (isScrollableContainerNode(outer->parent()))
                candidate.enclosingScrollableBox = outer->parent();

            // Get distance and alignment from current candidate.
            distanceDataForNode(direction, focusedNode, candidate);

            // Bail out if distance is maximum.
            if (candidate.distance == maxDistance()) {
                node = node->traverseNextNode(outer->parent());
                continue;
            }

            updateFocusCandidateIfCloser(focusedNode, candidate, closest);
        }

        node = node->traverseNextNode(outer->parent());
    }
}

void FocusController::deepFindFocusableNodeInDirection(Node* container, Node* focusedNode,
                                                       FocusDirection direction, KeyboardEvent* event,
                                                       FocusCandidate& closest)
{
    ASSERT(container->hasTagName(frameTag)
        || container->hasTagName(iframeTag)
        || isScrollableContainerNode(container));

    // Track if focusedNode is a descendant of the current container node being processed.
    bool descendantOfContainer = false;
    Node* firstChild = 0;

    // Iframe or Frame.
    if (container->hasTagName(frameTag) || container->hasTagName(iframeTag)) {

        HTMLFrameOwnerElement* owner = static_cast<HTMLFrameOwnerElement*>(container);
        if (!owner->contentFrame())
            return;

        Document* innerDocument = owner->contentFrame()->document();
        if (!innerDocument)
            return;

        descendantOfContainer = isNodeDeepDescendantOfDocument(focusedNode, innerDocument);
        firstChild = innerDocument->firstChild();

    // Scrollable block elements (e.g. <div>, etc)
    } else if (isScrollableContainerNode(container)) {

        firstChild = container->firstChild();
        descendantOfContainer = focusedNode->isDescendantOf(container);
    }

    if (descendantOfContainer) {
        findFocusableNodeInDirection(firstChild, focusedNode, direction, event, closest);
        return;
    }

    // Check if the current container element itself is a good candidate
    // to move focus to. If it is, then we traverse its inner nodes.
    FocusCandidate candidateParent = FocusCandidate(container);
    distanceDataForNode(direction, focusedNode, candidateParent);

    // Bail out if distance is maximum.
    if (candidateParent.distance == maxDistance())
        return;

    // FIXME: Consider alignment?
    if (candidateParent.distance < closest.distance)
        findFocusableNodeInDirection(firstChild, focusedNode, direction, event, closest, candidateParent);
}

static bool relinquishesEditingFocus(Node *node)
{
    ASSERT(node);
    ASSERT(node->isContentEditable());

    Node* root = node->rootEditableElement();
    Frame* frame = node->document()->frame();
    if (!frame || !root)
        return false;

    return frame->editor()->shouldEndEditing(rangeOfContents(root).get());
}

static void clearSelectionIfNeeded(Frame* oldFocusedFrame, Frame* newFocusedFrame, Node* newFocusedNode)
{
    if (!oldFocusedFrame || !newFocusedFrame)
        return;
        
    if (oldFocusedFrame->document() != newFocusedFrame->document())
        return;
    
    SelectionController* s = oldFocusedFrame->selection();
    if (s->isNone())
        return;

    bool caretBrowsing = oldFocusedFrame->settings()->caretBrowsingEnabled();
    if (caretBrowsing)
        return;

    Node* selectionStartNode = s->selection().start().node();
    if (selectionStartNode == newFocusedNode || selectionStartNode->isDescendantOf(newFocusedNode) || selectionStartNode->shadowAncestorNode() == newFocusedNode)
        return;
        
    if (Node* mousePressNode = newFocusedFrame->eventHandler()->mousePressNode()) {
        if (mousePressNode->renderer() && !mousePressNode->canStartSelection()) {
            // Don't clear the selection for contentEditable elements, but do clear it for input and textarea. See bug 38696.
            Node * root = s->rootEditableElement();
            if (!root)
                return;

            if (Node* shadowAncestorNode = root->shadowAncestorNode()) {
                if (!shadowAncestorNode->hasTagName(inputTag) && !shadowAncestorNode->hasTagName(textareaTag))
                    return;
            }
        }
    }
    
    s->clear();
}

bool FocusController::setFocusedNode(Node* node, PassRefPtr<Frame> newFocusedFrame)
{
    RefPtr<Frame> oldFocusedFrame = focusedFrame();
    RefPtr<Document> oldDocument = oldFocusedFrame ? oldFocusedFrame->document() : 0;
    
    Node* oldFocusedNode = oldDocument ? oldDocument->focusedNode() : 0;
    if (oldFocusedNode == node)
        return true;

    // FIXME: Might want to disable this check for caretBrowsing
    if (oldFocusedNode && oldFocusedNode->rootEditableElement() == oldFocusedNode && !relinquishesEditingFocus(oldFocusedNode))
        return false;

    m_page->editorClient()->willSetInputMethodState();

    clearSelectionIfNeeded(oldFocusedFrame.get(), newFocusedFrame.get(), node);

    if (!node) {
        if (oldDocument)
            oldDocument->setFocusedNode(0);
        m_page->editorClient()->setInputMethodState(false);
        return true;
    }

    RefPtr<Document> newDocument = node->document();

    if (newDocument && newDocument->focusedNode() == node) {
        m_page->editorClient()->setInputMethodState(node->shouldUseInputMethod());
        return true;
    }
    
    if (oldDocument && oldDocument != newDocument)
        oldDocument->setFocusedNode(0);
    
    setFocusedFrame(newFocusedFrame);

    // Setting the focused node can result in losing our last reft to node when JS event handlers fire.
    RefPtr<Node> protect = node;
    if (newDocument) {
        bool successfullyFocused = newDocument->setFocusedNode(node);
        if (!successfullyFocused)
            return false;
    }

    if (newDocument->focusedNode() == node)
        m_page->editorClient()->setInputMethodState(node->shouldUseInputMethod());

    return true;
}

void FocusController::setActive(bool active)
{
    if (m_isActive == active)
        return;

    m_isActive = active;

    if (FrameView* view = m_page->mainFrame()->view()) {
        if (!view->platformWidget()) {
            view->updateLayoutAndStyleIfNeededRecursive();
            view->updateControlTints();
        }
    }

    focusedOrMainFrame()->selection()->pageActivationChanged();
    
    if (m_focusedFrame && isFocused())
        dispatchEventsOnWindowAndFocusedNode(m_focusedFrame->document(), active);
}

} // namespace WebCore
