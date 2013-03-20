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
#include "ElementShadow.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "KeyboardEvent.h"
#include "NodeRenderingTraversal.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "ScrollAnimator.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SpatialNavigation.h"
#include "Widget.h"
#include "htmlediting.h" // For firstPositionInOrBeforeNode
#include <limits>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

FocusNavigationScope::FocusNavigationScope(TreeScope* treeScope)
    : m_rootTreeScope(treeScope)
{
    ASSERT(treeScope);
    ASSERT(!treeScope->rootNode()->isShadowRoot() || toShadowRoot(treeScope->rootNode())->isYoungest());
}

Node* FocusNavigationScope::rootNode() const
{
    return m_rootTreeScope->rootNode();
}

Element* FocusNavigationScope::owner() const
{
    Node* root = rootNode();
    if (root->isShadowRoot())
        return toShadowRoot(root)->host();
    if (Frame* frame = root->document()->frame())
        return frame->ownerElement();
    return 0;
}

FocusNavigationScope FocusNavigationScope::focusNavigationScopeOf(Node* node)
{
    ASSERT(node);
    Node* root = node;
    for (Node* n = node; n; n = NodeRenderingTraversal::parentInScope(n))
        root = n;
    // The result is not always a ShadowRoot nor a DocumentNode since
    // a starting node is in an orphaned tree in composed shadow tree.
    return FocusNavigationScope(root->treeScope());
}

FocusNavigationScope FocusNavigationScope::focusNavigationScopeOwnedByShadowHost(Node* node)
{
    ASSERT(isShadowHost(node));
    return FocusNavigationScope(toElement(node)->shadow()->youngestShadowRoot());
}

FocusNavigationScope FocusNavigationScope::focusNavigationScopeOwnedByIFrame(HTMLFrameOwnerElement* frame)
{
    ASSERT(frame && frame->contentFrame());
    return FocusNavigationScope(frame->contentFrame()->document());
}

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
        document->focusedNode()->dispatchBlurEvent(0);
    document->dispatchWindowEvent(Event::create(focused ? eventNames().focusEvent : eventNames().blurEvent, false, false));
    if (focused && document->focusedNode())
        document->focusedNode()->dispatchFocusEvent(0, FocusDirectionNone);
}

static inline bool hasCustomFocusLogic(Node* node)
{
    return node->isHTMLElement() && toHTMLElement(node)->hasCustomFocusLogic();
}

static inline bool isNonFocusableShadowHost(Node* node, KeyboardEvent* event)
{
    ASSERT(node);
    return !node->isKeyboardFocusable(event) && isShadowHost(node) && !hasCustomFocusLogic(node);
}

static inline bool isFocusableShadowHost(Node* node, KeyboardEvent* event)
{
    ASSERT(node);
    return node->isKeyboardFocusable(event) && isShadowHost(node) && !hasCustomFocusLogic(node);
}

static inline int adjustedTabIndex(Node* node, KeyboardEvent* event)
{
    ASSERT(node);
    return isNonFocusableShadowHost(node, event) ? 0 : node->tabIndex();
}

static inline bool shouldVisit(Node* node, KeyboardEvent* event)
{
    ASSERT(node);
    return node->isKeyboardFocusable(event) || isNonFocusableShadowHost(node, event);
}

FocusController::FocusController(Page* page)
    : m_page(page)
    , m_isActive(false)
    , m_isFocused(false)
    , m_isChangingFocusedFrame(false)
    , m_containingWindowIsVisible(false)
{
}

PassOwnPtr<FocusController> FocusController::create(Page* page)
{
    return adoptPtr(new FocusController(page));
}

void FocusController::setFocusedFrame(PassRefPtr<Frame> frame)
{
    ASSERT(!frame || frame->page() == m_page);
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

    m_page->chrome()->focusedFrameChanged(newFrame.get());

    m_isChangingFocusedFrame = false;
}

Frame* FocusController::focusedOrMainFrame() const
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

    if (!m_isFocused)
        focusedOrMainFrame()->eventHandler()->stopAutoscrollTimer();

    if (!m_focusedFrame)
        setFocusedFrame(m_page->mainFrame());

    if (m_focusedFrame->view()) {
        m_focusedFrame->selection()->setFocused(focused);
        dispatchEventsOnWindowAndFocusedNode(m_focusedFrame->document(), focused);
    }
}

Node* FocusController::findFocusableNodeDecendingDownIntoFrameDocument(FocusDirection direction, Node* node, KeyboardEvent* event)
{
    // The node we found might be a HTMLFrameOwnerElement, so descend down the tree until we find either:
    // 1) a focusable node, or
    // 2) the deepest-nested HTMLFrameOwnerElement.
    while (node && node->isFrameOwnerElement()) {
        HTMLFrameOwnerElement* owner = static_cast<HTMLFrameOwnerElement*>(node);
        if (!owner->contentFrame())
            break;
        Node* foundNode = findFocusableNode(direction, FocusNavigationScope::focusNavigationScopeOwnedByIFrame(owner), 0, event);
        if (!foundNode)
            break;
        ASSERT(node != foundNode);
        node = foundNode;
    }
    return node;
}

bool FocusController::setInitialFocus(FocusDirection direction, KeyboardEvent* event)
{
    bool didAdvanceFocus = advanceFocus(direction, event, true);
    
    // If focus is being set initially, accessibility needs to be informed that system focus has moved 
    // into the web area again, even if focus did not change within WebCore. PostNotification is called instead
    // of handleFocusedUIElementChanged, because this will send the notification even if the element is the same.
    if (AXObjectCache::accessibilityEnabled())
        focusedOrMainFrame()->document()->axObjectCache()->postNotification(focusedOrMainFrame()->document(), AXObjectCache::AXFocusedUIElementChanged, true);

    return didAdvanceFocus;
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
    bool caretBrowsing = frame->settings() && frame->settings()->caretBrowsingEnabled();

    if (caretBrowsing && !currentNode)
        currentNode = frame->selection()->start().deprecatedNode();

    document->updateLayoutIgnorePendingStylesheets();

    RefPtr<Node> node = findFocusableNodeAcrossFocusScope(direction, FocusNavigationScope::focusNavigationScopeOf(currentNode ? currentNode : document), currentNode, event);

    if (!node) {
        // We didn't find a node to focus, so we should try to pass focus to Chrome.
        if (!initialFocus && m_page->chrome()->canTakeFocus(direction)) {
            document->setFocusedNode(0);
            setFocusedFrame(0);
            m_page->chrome()->takeFocus(direction);
            return true;
        }

        // Chrome doesn't want focus, so we should wrap focus.
        node = findFocusableNodeRecursively(direction, FocusNavigationScope::focusNavigationScopeOf(m_page->mainFrame()->document()), 0, event);
        node = findFocusableNodeDecendingDownIntoFrameDocument(direction, node.get(), event);

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

    if (node->isFrameOwnerElement() && (!node->isPluginElement() || !node->isKeyboardFocusable(event))) {
        // We focus frames rather than frame owners.
        // FIXME: We should not focus frames that have no scrollbars, as focusing them isn't useful to the user.
        HTMLFrameOwnerElement* owner = static_cast<HTMLFrameOwnerElement*>(node.get());
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
        Position position = firstPositionInOrBeforeNode(node.get());
        VisibleSelection newSelection(position, position, DOWNSTREAM);
        if (frame->selection()->shouldChangeSelection(newSelection))
            frame->selection()->setSelection(newSelection);
    }

    toElement(node.get())->focus(false, direction);
    return true;
}

Node* FocusController::findFocusableNodeAcrossFocusScope(FocusDirection direction, FocusNavigationScope scope, Node* currentNode, KeyboardEvent* event)
{
    ASSERT(!currentNode || !isNonFocusableShadowHost(currentNode, event));
    Node* found;
    if (currentNode && direction == FocusDirectionForward && isFocusableShadowHost(currentNode, event)) {
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::focusNavigationScopeOwnedByShadowHost(currentNode), 0, event);
        found = foundInInnerFocusScope ? foundInInnerFocusScope : findFocusableNodeRecursively(direction, scope, currentNode, event);
    } else
        found = findFocusableNodeRecursively(direction, scope, currentNode, event);

    // If there's no focusable node to advance to, move up the focus scopes until we find one.
    while (!found) {
        Node* owner = scope.owner();
        if (!owner)
            break;
        scope = FocusNavigationScope::focusNavigationScopeOf(owner);
        if (direction == FocusDirectionBackward && isFocusableShadowHost(owner, event)) {
            found = owner;
            break;
        }
        found = findFocusableNodeRecursively(direction, scope, owner, event);
    }
    found = findFocusableNodeDecendingDownIntoFrameDocument(direction, found, event);
    return found;
}

Node* FocusController::findFocusableNodeRecursively(FocusDirection direction, FocusNavigationScope scope, Node* start, KeyboardEvent* event)
{
    // Starting node is exclusive.
    Node* found = findFocusableNode(direction, scope, start, event);
    if (!found)
        return 0;
    if (direction == FocusDirectionForward) {
        if (!isNonFocusableShadowHost(found, event))
            return found;
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::focusNavigationScopeOwnedByShadowHost(found), 0, event);
        return foundInInnerFocusScope ? foundInInnerFocusScope : findFocusableNodeRecursively(direction, scope, found, event);
    }
    ASSERT(direction == FocusDirectionBackward);
    if (isFocusableShadowHost(found, event)) {
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::focusNavigationScopeOwnedByShadowHost(found), 0, event);
        return foundInInnerFocusScope ? foundInInnerFocusScope : found;
    }
    if (isNonFocusableShadowHost(found, event)) {
        Node* foundInInnerFocusScope = findFocusableNodeRecursively(direction, FocusNavigationScope::focusNavigationScopeOwnedByShadowHost(found), 0, event);
        return foundInInnerFocusScope ? foundInInnerFocusScope :findFocusableNodeRecursively(direction, scope, found, event);
    }
    return found;
}

Node* FocusController::findFocusableNode(FocusDirection direction, FocusNavigationScope scope, Node* node, KeyboardEvent* event)
{
    return (direction == FocusDirectionForward)
        ? nextFocusableNode(scope, node, event)
        : previousFocusableNode(scope, node, event);
}

Node* FocusController::findNodeWithExactTabIndex(Node* start, int tabIndex, KeyboardEvent* event, FocusDirection direction)
{
    // Search is inclusive of start
    using namespace NodeRenderingTraversal;
    for (Node* node = start; node; node = direction == FocusDirectionForward ? nextInScope(node) : previousInScope(node)) {
        if (shouldVisit(node, event) && adjustedTabIndex(node, event) == tabIndex)
            return node;
    }
    return 0;
}

static Node* nextNodeWithGreaterTabIndex(Node* start, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    int winningTabIndex = std::numeric_limits<short>::max() + 1;
    Node* winner = 0;
    for (Node* node = start; node; node = NodeRenderingTraversal::nextInScope(node)) {
        if (shouldVisit(node, event) && node->tabIndex() > tabIndex && node->tabIndex() < winningTabIndex) {
            winner = node;
            winningTabIndex = node->tabIndex();
        }
    }

    return winner;
}

static Node* previousNodeWithLowerTabIndex(Node* start, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    int winningTabIndex = 0;
    Node* winner = 0;
    for (Node* node = start; node; node = NodeRenderingTraversal::previousInScope(node)) {
        int currentTabIndex = adjustedTabIndex(node, event);
        if ((shouldVisit(node, event) || isNonFocusableShadowHost(node, event)) && currentTabIndex < tabIndex && currentTabIndex > winningTabIndex) {
            winner = node;
            winningTabIndex = currentTabIndex;
        }
    }
    return winner;
}

Node* FocusController::nextFocusableNode(FocusNavigationScope scope, Node* start, KeyboardEvent* event)
{
    using namespace NodeRenderingTraversal;

    if (start) {
        int tabIndex = adjustedTabIndex(start, event);
        // If a node is excluded from the normal tabbing cycle, the next focusable node is determined by tree order
        if (tabIndex < 0) {
            for (Node* node = nextInScope(start); node; node = nextInScope(node)) {
                if (shouldVisit(node, event) && adjustedTabIndex(node, event) >= 0)
                    return node;
            }
        }

        // First try to find a node with the same tabindex as start that comes after start in the scope.
        if (Node* winner = findNodeWithExactTabIndex(nextInScope(start), tabIndex, event, FocusDirectionForward))
            return winner;

        if (!tabIndex)
            // We've reached the last node in the document with a tabindex of 0. This is the end of the tabbing order.
            return 0;
    }

    // Look for the first node in the scope that:
    // 1) has the lowest tabindex that is higher than start's tabindex (or 0, if start is null), and
    // 2) comes first in the scope, if there's a tie.
    if (Node* winner = nextNodeWithGreaterTabIndex(scope.rootNode(), start ? adjustedTabIndex(start, event) : 0, event))
        return winner;

    // There are no nodes with a tabindex greater than start's tabindex,
    // so find the first node with a tabindex of 0.
    return findNodeWithExactTabIndex(scope.rootNode(), 0, event, FocusDirectionForward);
}

Node* FocusController::previousFocusableNode(FocusNavigationScope scope, Node* start, KeyboardEvent* event)
{
    using namespace NodeRenderingTraversal;

    Node* last = 0;
    for (Node* node = scope.rootNode(); node; node = lastChildInScope(node))
        last = node;
    ASSERT(last);

    // First try to find the last node in the scope that comes before start and has the same tabindex as start.
    // If start is null, find the last node in the scope with a tabindex of 0.
    Node* startingNode;
    int startingTabIndex;
    if (start) {
        startingNode = previousInScope(start);
        startingTabIndex = adjustedTabIndex(start, event);
    } else {
        startingNode = last;
        startingTabIndex = 0;
    }

    // However, if a node is excluded from the normal tabbing cycle, the previous focusable node is determined by tree order
    if (startingTabIndex < 0) {
        for (Node* node = startingNode; node; node = previousInScope(node)) {
            if (shouldVisit(node, event) && adjustedTabIndex(node, event) >= 0)
                return node;
        }
    }

    if (Node* winner = findNodeWithExactTabIndex(startingNode, startingTabIndex, event, FocusDirectionBackward))
        return winner;

    // There are no nodes before start with the same tabindex as start, so look for a node that:
    // 1) has the highest non-zero tabindex (that is less than start's tabindex), and
    // 2) comes last in the scope, if there's a tie.
    startingTabIndex = (start && startingTabIndex) ? startingTabIndex : std::numeric_limits<short>::max();
    return previousNodeWithLowerTabIndex(last, startingTabIndex, event);
}

static bool relinquishesEditingFocus(Node *node)
{
    ASSERT(node);
    ASSERT(node->rendererIsEditable());

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
    
    FrameSelection* s = oldFocusedFrame->selection();
    if (s->isNone())
        return;

    bool caretBrowsing = oldFocusedFrame->settings()->caretBrowsingEnabled();
    if (caretBrowsing)
        return;

    Node* selectionStartNode = s->selection().start().deprecatedNode();
    if (selectionStartNode == newFocusedNode || selectionStartNode->isDescendantOf(newFocusedNode) || selectionStartNode->deprecatedShadowAncestorNode() == newFocusedNode)
        return;
        
    if (Node* mousePressNode = newFocusedFrame->eventHandler()->mousePressNode()) {
        if (mousePressNode->renderer() && !mousePressNode->canStartSelection()) {
            // Don't clear the selection for contentEditable elements, but do clear it for input and textarea. See bug 38696.
            Node * root = s->rootEditableElement();
            if (!root)
                return;

            if (Node* shadowAncestorNode = root->deprecatedShadowAncestorNode()) {
                if (!shadowAncestorNode->hasTagName(inputTag) && !shadowAncestorNode->hasTagName(textareaTag))
                    return;
            }
        }
    }
    
    s->clear();
}

bool FocusController::setFocusedNode(Node* node, PassRefPtr<Frame> newFocusedFrame, FocusDirection direction)
{
    RefPtr<Frame> oldFocusedFrame = focusedFrame();
    RefPtr<Document> oldDocument = oldFocusedFrame ? oldFocusedFrame->document() : 0;
    
    Node* oldFocusedNode = oldDocument ? oldDocument->focusedNode() : 0;
    if (oldFocusedNode == node)
        return true;

    // FIXME: Might want to disable this check for caretBrowsing
    if (oldFocusedNode && oldFocusedNode->isRootEditableElement() && !relinquishesEditingFocus(oldFocusedNode))
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

    if (newFocusedFrame && !newFocusedFrame->page()) {
        setFocusedFrame(0);
        return false;
    }
    setFocusedFrame(newFocusedFrame);

    // Setting the focused node can result in losing our last reft to node when JS event handlers fire.
    RefPtr<Node> protect = node;
    if (newDocument) {
        bool successfullyFocused = newDocument->setFocusedNode(node, direction);
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

static void contentAreaDidShowOrHide(ScrollableArea* scrollableArea, bool didShow)
{
    if (didShow)
        scrollableArea->contentAreaDidShow();
    else
        scrollableArea->contentAreaDidHide();
}

void FocusController::setContainingWindowIsVisible(bool containingWindowIsVisible)
{
    if (m_containingWindowIsVisible == containingWindowIsVisible)
        return;

    m_containingWindowIsVisible = containingWindowIsVisible;

    FrameView* view = m_page->mainFrame()->view();
    if (!view)
        return;

    contentAreaDidShowOrHide(view, containingWindowIsVisible);

    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView)
            continue;

        const HashSet<ScrollableArea*>* scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (HashSet<ScrollableArea*>::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
            ASSERT(scrollableArea->scrollbarsCanBeActive() || m_page->shouldSuppressScrollbarAnimations());

            contentAreaDidShowOrHide(scrollableArea, containingWindowIsVisible);
        }
    }
}

static void updateFocusCandidateIfNeeded(FocusDirection direction, const FocusCandidate& current, FocusCandidate& candidate, FocusCandidate& closest)
{
    ASSERT(candidate.visibleNode->isElementNode());
    ASSERT(candidate.visibleNode->renderer());

    // Ignore iframes that don't have a src attribute
    if (frameOwnerElement(candidate) && (!frameOwnerElement(candidate)->contentFrame() || candidate.rect.isEmpty()))
        return;

    // Ignore off screen child nodes of containers that do not scroll (overflow:hidden)
    if (candidate.isOffscreen && !canBeScrolledIntoView(direction, candidate))
        return;

    distanceDataForNode(direction, current, candidate);
    if (candidate.distance == maxDistance())
        return;

    if (candidate.isOffscreenAfterScrolling && candidate.alignment < Full)
        return;

    if (closest.isNull()) {
        closest = candidate;
        return;
    }

    LayoutRect intersectionRect = intersection(candidate.rect, closest.rect);
    if (!intersectionRect.isEmpty() && !areElementsOnSameLine(closest, candidate)) {
        // If 2 nodes are intersecting, do hit test to find which node in on top.
        LayoutUnit x = intersectionRect.x() + intersectionRect.width() / 2;
        LayoutUnit y = intersectionRect.y() + intersectionRect.height() / 2;
        HitTestResult result = candidate.visibleNode->document()->page()->mainFrame()->eventHandler()->hitTestResultAtPoint(IntPoint(x, y), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping);
        if (candidate.visibleNode->contains(result.innerNode())) {
            closest = candidate;
            return;
        }
        if (closest.visibleNode->contains(result.innerNode()))
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

void FocusController::findFocusCandidateInContainer(Node* container, const LayoutRect& startingRect, FocusDirection direction, KeyboardEvent* event, FocusCandidate& closest)
{
    ASSERT(container);
    Node* focusedNode = (focusedFrame() && focusedFrame()->document()) ? focusedFrame()->document()->focusedNode() : 0;

    Element* element = ElementTraversal::firstWithin(container);
    FocusCandidate current;
    current.rect = startingRect;
    current.focusableNode = focusedNode;
    current.visibleNode = focusedNode;

    for (; element; element = (element->isFrameOwnerElement() || canScrollInDirection(element, direction))
        ? ElementTraversal::nextSkippingChildren(element, container)
        : ElementTraversal::next(element, container)) {
        if (element == focusedNode)
            continue;

        if (!element->isKeyboardFocusable(event) && !element->isFrameOwnerElement() && !canScrollInDirection(element, direction))
            continue;

        FocusCandidate candidate = FocusCandidate(element, direction);
        if (candidate.isNull())
            continue;

        candidate.enclosingScrollableBox = container;
        updateFocusCandidateIfNeeded(direction, current, candidate, closest);
    }
}

bool FocusController::advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, FocusDirection direction, KeyboardEvent* event)
{
    if (!container || !container->document())
        return false;

    LayoutRect newStartingRect = startingRect;

    if (startingRect.isEmpty())
        newStartingRect = virtualRectForDirection(direction, nodeRectInAbsoluteCoordinates(container));

    // Find the closest node within current container in the direction of the navigation.
    FocusCandidate focusCandidate;
    findFocusCandidateInContainer(container, newStartingRect, direction, event, focusCandidate);

    if (focusCandidate.isNull()) {
        // Nothing to focus, scroll if possible.
        // NOTE: If no scrolling is performed (i.e. scrollInDirection returns false), the
        // spatial navigation algorithm will skip this container.
        return scrollInDirection(container, direction);
    }

    if (HTMLFrameOwnerElement* frameElement = frameOwnerElement(focusCandidate)) {
        // If we have an iframe without the src attribute, it will not have a contentFrame().
        // We ASSERT here to make sure that
        // updateFocusCandidateIfNeeded() will never consider such an iframe as a candidate.
        ASSERT(frameElement->contentFrame());

        if (focusCandidate.isOffscreenAfterScrolling) {
            scrollInDirection(focusCandidate.visibleNode->document(), direction);
            return true;
        }
        // Navigate into a new frame.
        LayoutRect rect;
        Node* focusedNode = focusedOrMainFrame()->document()->focusedNode();
        if (focusedNode && !hasOffscreenRect(focusedNode))
            rect = nodeRectInAbsoluteCoordinates(focusedNode, true /* ignore border */);
        frameElement->contentFrame()->document()->updateLayoutIgnorePendingStylesheets();
        if (!advanceFocusDirectionallyInContainer(frameElement->contentFrame()->document(), rect, direction, event)) {
            // The new frame had nothing interesting, need to find another candidate.
            return advanceFocusDirectionallyInContainer(container, nodeRectInAbsoluteCoordinates(focusCandidate.visibleNode, true), direction, event);
        }
        return true;
    }

    if (canScrollInDirection(focusCandidate.visibleNode, direction)) {
        if (focusCandidate.isOffscreenAfterScrolling) {
            scrollInDirection(focusCandidate.visibleNode, direction);
            return true;
        }
        // Navigate into a new scrollable container.
        LayoutRect startingRect;
        Node* focusedNode = focusedOrMainFrame()->document()->focusedNode();
        if (focusedNode && !hasOffscreenRect(focusedNode))
            startingRect = nodeRectInAbsoluteCoordinates(focusedNode, true);
        return advanceFocusDirectionallyInContainer(focusCandidate.visibleNode, startingRect, direction, event);
    }
    if (focusCandidate.isOffscreenAfterScrolling) {
        Node* container = focusCandidate.enclosingScrollableBox;
        scrollInDirection(container, direction);
        return true;
    }

    // We found a new focus node, navigate to it.
    Element* element = toElement(focusCandidate.focusableNode);
    ASSERT(element);

    element->focus(false, direction);
    return true;
}

bool FocusController::advanceFocusDirectionally(FocusDirection direction, KeyboardEvent* event)
{
    Frame* curFrame = focusedOrMainFrame();
    ASSERT(curFrame);

    Document* focusedDocument = curFrame->document();
    if (!focusedDocument)
        return false;

    Node* focusedNode = focusedDocument->focusedNode();
    Node* container = focusedDocument;

    if (container->isDocumentNode())
        toDocument(container)->updateLayoutIgnorePendingStylesheets();
        
    // Figure out the starting rect.
    LayoutRect startingRect;
    if (focusedNode) {
        if (!hasOffscreenRect(focusedNode)) {
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, focusedNode);
            startingRect = nodeRectInAbsoluteCoordinates(focusedNode, true /* ignore border */);
        } else if (focusedNode->hasTagName(areaTag)) {
            HTMLAreaElement* area = static_cast<HTMLAreaElement*>(focusedNode);
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, area->imageElement());
            startingRect = virtualRectForAreaElementAndDirection(area, direction);
        }
    }

    bool consumed = false;
    do {
        consumed = advanceFocusDirectionallyInContainer(container, startingRect, direction, event);
        startingRect = nodeRectInAbsoluteCoordinates(container, true /* ignore border */);
        container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, container);
        if (container && container->isDocumentNode())
            toDocument(container)->updateLayoutIgnorePendingStylesheets();
    } while (!consumed && container);

    return consumed;
}

} // namespace WebCore
