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
    
    if (m_focusedFrame && m_focusedFrame->view()) {
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
        scrollInDirection(frame, direction);
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

// FIXME: Make this method more modular, and simpler to understand and maintain.
static void updateFocusCandidateIfCloser(Node* focusedNode, const FocusCandidate& candidate, FocusCandidate& closest)
{
    bool sameDocument = candidate.document() == closest.document();
    if (sameDocument) {
        if (closest.alignment > candidate.alignment
         || (closest.parentAlignment && candidate.alignment > closest.parentAlignment))
            return;
    } else if (closest.alignment > candidate.alignment
            && (closest.parentAlignment && candidate.alignment > closest.parentAlignment))
        return;

    if (candidate.alignment != None
     || (closest.parentAlignment >= candidate.alignment
     && closest.document() == candidate.document())) {

        // If we are now in an higher precedent case, lets reset the current closest's
        // distance so we force it to be bigger than any result we will get from
        // spatialDistance().
        if (closest.alignment < candidate.alignment
         && closest.parentAlignment < candidate.alignment)
            closest.distance = maxDistance();
    }

    // Bail out if candidate's distance is larger than that of the closest candidate.
    if (candidate.distance >= closest.distance)
        return;

    if (closest.isNull()) {
        closest = candidate;
        return;
    }

    // If the focused node and the candadate are in the same document and current
    // closest candidate is not in an {i}frame that is preferable to get focused ...
    if (focusedNode->document() == candidate.document()
        && candidate.distance < closest.parentDistance)
        closest = candidate;
    else if (focusedNode->document() != candidate.document()) {
        // If the focusedNode is in an inner document and candidate is in a
        // different document, we only consider to change focus if there is not
        // another already good focusable candidate in the same document as focusedNode.
        if (!((isInRootDocument(candidate.node) && !isInRootDocument(focusedNode))
            && focusedNode->document() == closest.document()))
            closest = candidate;
    }
}

void FocusController::findFocusableNodeInDirection(Node* outer, Node* focusedNode,
                                                   FocusDirection direction, KeyboardEvent* event,
                                                   FocusCandidate& closestFocusCandidate,
                                                   const FocusCandidate& candidateParent)
{
    ASSERT(outer);
    ASSERT(candidateParent.isNull()
        || candidateParent.node->hasTagName(frameTag)
        || candidateParent.node->hasTagName(iframeTag));

    // Walk all the child nodes and update closestFocusCandidate if we find a nearer node.
    Node* candidate = outer;
    while (candidate) {
        // Inner documents case.

        if (candidate->isFrameOwnerElement())
            deepFindFocusableNodeInDirection(candidate, focusedNode, direction, event, closestFocusCandidate);
        else if (candidate != focusedNode && candidate->isKeyboardFocusable(event)) {
            FocusCandidate currentFocusCandidate(candidate);

            // Get distance and alignment from current candidate.
            distanceDataForNode(direction, focusedNode, currentFocusCandidate);

            // Bail out if distance is maximum.
            if (currentFocusCandidate.distance == maxDistance()) {
                candidate = candidate->traverseNextNode(outer->parent());
                continue;
            }

            // If candidateParent is not null, it means that we are in a recursive call
            // from deepFineFocusableNodeInDirection (i.e. processing an element in an iframe),
            // and holds the distance and alignment data of the iframe element itself.
            if (!candidateParent.isNull()) {
                currentFocusCandidate.parentAlignment = candidateParent.alignment;
                currentFocusCandidate.parentDistance = candidateParent.distance;
            }

            updateFocusCandidateIfCloser(focusedNode, currentFocusCandidate, closestFocusCandidate);
        }

        candidate = candidate->traverseNextNode(outer->parent());
    }
}

void FocusController::deepFindFocusableNodeInDirection(Node* container, Node* focusedNode,
                                                       FocusDirection direction, KeyboardEvent* event,
                                                       FocusCandidate& closestFocusCandidate)
{
    ASSERT(container->hasTagName(frameTag) || container->hasTagName(iframeTag));

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

        descendantOfContainer = innerDocument == focusedNode->document();
        firstChild = innerDocument->firstChild();

    }

    if (descendantOfContainer) {
        findFocusableNodeInDirection(firstChild, focusedNode, direction, event, closestFocusCandidate);
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
    if (candidateParent.distance < closestFocusCandidate.distance)
        findFocusableNodeInDirection(firstChild, focusedNode, direction, event, closestFocusCandidate, candidateParent);
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
        
    if (Node* mousePressNode = newFocusedFrame->eventHandler()->mousePressNode())
        if (mousePressNode->renderer() && !mousePressNode->canStartSelection())
            if (Node* root = s->rootEditableElement())
                if (Node* shadowAncestorNode = root->shadowAncestorNode())
                    // Don't do this for textareas and text fields, when they lose focus their selections should be cleared
                    // and then restored when they regain focus, to match other browsers.
                    if (!shadowAncestorNode->hasTagName(inputTag) && !shadowAncestorNode->hasTagName(textareaTag))
                        return;
    
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
    if (newDocument)
        newDocument->setFocusedNode(node);

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
            view->layoutIfNeededRecursive();
            view->updateControlTints();
        }
    }

    focusedOrMainFrame()->selection()->pageActivationChanged();
    
    if (m_focusedFrame && isFocused())
        dispatchEventsOnWindowAndFocusedNode(m_focusedFrame->document(), active);
}

} // namespace WebCore
