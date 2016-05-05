/*
 * Copyright (C) 2006, 2007, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "ElementTraversal.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "KeyboardEvent.h"
#include "MainFrame.h"
#include "Page.h"
#include "Range.h"
#include "RenderWidget.h"
#include "ScrollAnimator.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SpatialNavigation.h"
#include "Widget.h"
#include "htmlediting.h" // For firstPositionInOrBeforeNode
#include <limits>
#include <wtf/CurrentTime.h>
#include <wtf/Ref.h>

namespace WebCore {

using namespace HTMLNames;

class FocusNavigationScope {
public:
    ContainerNode& rootNode() const;
    Element* owner() const;
    WEBCORE_EXPORT static FocusNavigationScope scopeOf(Node&);
    static FocusNavigationScope scopeOwnedByShadowHost(Element&);
    static FocusNavigationScope scopeOwnedByIFrame(HTMLFrameOwnerElement&);

    Node* nextInScope(const Node*) const;
    Node* previousInScope(const Node*) const;
    Node* lastChildInScope(const Node*) const;

private:
    Node* firstChildInScope(const Node*) const;

    explicit FocusNavigationScope(TreeScope&);
    TreeScope& m_rootTreeScope;
};

// FIXME: Focus navigation should work with shadow trees that have slots.
Node* FocusNavigationScope::firstChildInScope(const Node* node) const
{
    ASSERT(node);
    if (node->shadowRoot())
        return nullptr;
    return node->firstChild();
}

Node* FocusNavigationScope::lastChildInScope(const Node* node) const
{
    ASSERT(node);
    if (node->shadowRoot())
        return nullptr;
    return node->lastChild();
}

static Node* parentInScope(const Node* node)
{
    if (node->isShadowRoot())
        return nullptr;

    ContainerNode* parent = node->parentNode();
    if (parent && parent->shadowRoot())
        return nullptr;

    return parent;
}

Node* FocusNavigationScope::nextInScope(const Node* node) const
{
    if (Node* next = firstChildInScope(node))
        return next;
    if (Node* next = node->nextSibling())
        return next;
    const Node* current = node;
    while (current && !current->nextSibling())
        current = parentInScope(current);
    return current ? current->nextSibling() : nullptr;
}

Node* FocusNavigationScope::previousInScope(const Node* node) const
{
    if (Node* current = node->previousSibling()) {
        while (Node* child = lastChildInScope(current))
            current = child;
        return current;
    }
    return parentInScope(node);
}

FocusNavigationScope::FocusNavigationScope(TreeScope& treeScope)
    : m_rootTreeScope(treeScope)
{
}

ContainerNode& FocusNavigationScope::rootNode() const
{
    return m_rootTreeScope.rootNode();
}

Element* FocusNavigationScope::owner() const
{
    ContainerNode& root = rootNode();
    if (is<ShadowRoot>(root))
        return downcast<ShadowRoot>(root).host();
    if (Frame* frame = root.document().frame())
        return frame->ownerElement();
    return nullptr;
}

FocusNavigationScope FocusNavigationScope::scopeOf(Node& startingNode)
{
    Node* root = nullptr;
    for (Node* currentNode = &startingNode; currentNode; currentNode = parentInScope(currentNode))
        root = currentNode;
    // The result is not always a ShadowRoot nor a DocumentNode since
    // a starting node is in an orphaned tree in composed shadow tree.
    return FocusNavigationScope(root->treeScope());
}

FocusNavigationScope FocusNavigationScope::scopeOwnedByShadowHost(Element& element)
{
    ASSERT(element.shadowRoot());
    return FocusNavigationScope(*element.shadowRoot());
}

FocusNavigationScope FocusNavigationScope::scopeOwnedByIFrame(HTMLFrameOwnerElement& frame)
{
    ASSERT(frame.contentFrame());
    ASSERT(frame.contentFrame()->document());
    return FocusNavigationScope(*frame.contentFrame()->document());
}

static inline void dispatchEventsOnWindowAndFocusedElement(Document* document, bool focused)
{
    // If we have a focused node we should dispatch blur on it before we blur the window.
    // If we have a focused node we should dispatch focus on it after we focus the window.
    // https://bugs.webkit.org/show_bug.cgi?id=27105

    // Do not fire events while modal dialogs are up.  See https://bugs.webkit.org/show_bug.cgi?id=33962
    if (Page* page = document->page()) {
        if (page->defersLoading())
            return;
    }

    if (!focused && document->focusedElement())
        document->focusedElement()->dispatchBlurEvent(nullptr);
    document->dispatchWindowEvent(Event::create(focused ? eventNames().focusEvent : eventNames().blurEvent, false, false));
    if (focused && document->focusedElement())
        document->focusedElement()->dispatchFocusEvent(nullptr, FocusDirectionNone);
}

static inline bool hasCustomFocusLogic(Element& element)
{
    return is<HTMLElement>(element) && downcast<HTMLElement>(element).hasCustomFocusLogic();
}

static inline bool isNonFocusableShadowHost(Element& element, KeyboardEvent& event)
{
    return !element.isKeyboardFocusable(&event) && element.shadowRoot() && !hasCustomFocusLogic(element);
}

static inline bool isFocusableShadowHost(Node& node, KeyboardEvent& event)
{
    return is<Element>(node) && downcast<Element>(node).isKeyboardFocusable(&event) && downcast<Element>(node).shadowRoot() && !hasCustomFocusLogic(downcast<Element>(node));
}

static inline int shadowAdjustedTabIndex(Element& element, KeyboardEvent& event)
{
    if (isNonFocusableShadowHost(element, event)) {
        if (!element.tabIndexSetExplicitly())
            return 0; // Treat a shadow host without tabindex if it has tabindex=0 even though HTMLElement::tabIndex returns -1 on such an element.
    }
    return element.tabIndex();
}

static inline bool isFocusableOrHasShadowTreeWithoutCustomFocusLogic(Element& element, KeyboardEvent& event)
{
    return element.isKeyboardFocusable(&event) || isNonFocusableShadowHost(element, event);
}

FocusController::FocusController(Page& page, ViewState::Flags viewState)
    : m_page(page)
    , m_isChangingFocusedFrame(false)
    , m_viewState(viewState)
    , m_focusRepaintTimer(*this, &FocusController::focusRepaintTimerFired)
{
}

void FocusController::setFocusedFrame(PassRefPtr<Frame> frame)
{
    ASSERT(!frame || frame->page() == &m_page);
    if (m_focusedFrame == frame || m_isChangingFocusedFrame)
        return;

    m_isChangingFocusedFrame = true;

    RefPtr<Frame> oldFrame = m_focusedFrame;
    RefPtr<Frame> newFrame = frame;

    m_focusedFrame = newFrame;

    // Now that the frame is updated, fire events and update the selection focused states of both frames.
    if (oldFrame && oldFrame->view()) {
        oldFrame->selection().setFocused(false);
        oldFrame->document()->dispatchWindowEvent(Event::create(eventNames().blurEvent, false, false));
    }

    if (newFrame && newFrame->view() && isFocused()) {
        newFrame->selection().setFocused(true);
        newFrame->document()->dispatchWindowEvent(Event::create(eventNames().focusEvent, false, false));
    }

    m_page.chrome().focusedFrameChanged(newFrame.get());

    m_isChangingFocusedFrame = false;
}

Frame& FocusController::focusedOrMainFrame() const
{
    if (Frame* frame = focusedFrame())
        return *frame;
    return m_page.mainFrame();
}

void FocusController::setFocused(bool focused)
{
    m_page.setViewState(focused ? m_viewState | ViewState::IsFocused : m_viewState & ~ViewState::IsFocused);
}

void FocusController::setFocusedInternal(bool focused)
{
    if (!isFocused())
        focusedOrMainFrame().eventHandler().stopAutoscrollTimer();

    if (!m_focusedFrame)
        setFocusedFrame(&m_page.mainFrame());

    if (m_focusedFrame->view()) {
        m_focusedFrame->selection().setFocused(focused);
        dispatchEventsOnWindowAndFocusedElement(m_focusedFrame->document(), focused);
    }
}

Element* FocusController::findFocusableElementDescendingDownIntoFrameDocument(FocusDirection direction, Element* element, KeyboardEvent* event)
{
    // The node we found might be a HTMLFrameOwnerElement, so descend down the tree until we find either:
    // 1) a focusable node, or
    // 2) the deepest-nested HTMLFrameOwnerElement.
    while (is<HTMLFrameOwnerElement>(element)) {
        HTMLFrameOwnerElement& owner = downcast<HTMLFrameOwnerElement>(*element);
        if (!owner.contentFrame())
            break;
        Element* foundElement = findFocusableElement(direction, FocusNavigationScope::scopeOwnedByIFrame(owner), 0, event);
        if (!foundElement)
            break;
        ASSERT(element != foundElement);
        element = foundElement;
    }
    return element;
}

bool FocusController::setInitialFocus(FocusDirection direction, KeyboardEvent* event)
{
    bool didAdvanceFocus = advanceFocus(direction, event, true);
    
    // If focus is being set initially, accessibility needs to be informed that system focus has moved 
    // into the web area again, even if focus did not change within WebCore. PostNotification is called instead
    // of handleFocusedUIElementChanged, because this will send the notification even if the element is the same.
    if (AXObjectCache* cache = focusedOrMainFrame().document()->existingAXObjectCache())
        cache->postNotification(focusedOrMainFrame().document(), AXObjectCache::AXFocusedUIElementChanged);

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
    Frame& frame = focusedOrMainFrame();
    Document* document = frame.document();

    Node* currentNode = document->focusedElement();
    // FIXME: Not quite correct when it comes to focus transitions leaving/entering the WebView itself
    bool caretBrowsing = frame.settings().caretBrowsingEnabled();

    if (caretBrowsing && !currentNode)
        currentNode = frame.selection().selection().start().deprecatedNode();

    document->updateLayoutIgnorePendingStylesheets();

    RefPtr<Element> element = findFocusableElementAcrossFocusScope(direction, FocusNavigationScope::scopeOf(currentNode ? *currentNode : *document), currentNode, event);

    if (!element) {
        // We didn't find a node to focus, so we should try to pass focus to Chrome.
        if (!initialFocus && m_page.chrome().canTakeFocus(direction)) {
            document->setFocusedElement(nullptr);
            setFocusedFrame(nullptr);
            m_page.chrome().takeFocus(direction);
            return true;
        }

        // Chrome doesn't want focus, so we should wrap focus.
        element = findFocusableElementRecursively(direction, FocusNavigationScope::scopeOf(*m_page.mainFrame().document()), 0, event);
        element = findFocusableElementDescendingDownIntoFrameDocument(direction, element.get(), event);

        if (!element)
            return false;
    }

    ASSERT(element);

    if (element == document->focusedElement()) {
        // Focus wrapped around to the same element.
        return true;
    }

    if (is<HTMLFrameOwnerElement>(*element) && (!is<HTMLPlugInElement>(*element) || !element->isKeyboardFocusable(event))) {
        // We focus frames rather than frame owners.
        // FIXME: We should not focus frames that have no scrollbars, as focusing them isn't useful to the user.
        HTMLFrameOwnerElement& owner = downcast<HTMLFrameOwnerElement>(*element);
        if (!owner.contentFrame())
            return false;

        document->setFocusedElement(nullptr);
        setFocusedFrame(owner.contentFrame());
        return true;
    }
    
    // FIXME: It would be nice to just be able to call setFocusedElement(node) here, but we can't do
    // that because some elements (e.g. HTMLInputElement and HTMLTextAreaElement) do extra work in
    // their focus() methods.

    Document& newDocument = element->document();

    if (&newDocument != document) {
        // Focus is going away from this document, so clear the focused node.
        document->setFocusedElement(nullptr);
    }

    setFocusedFrame(newDocument.frame());

    if (caretBrowsing) {
        Position position = firstPositionInOrBeforeNode(element.get());
        VisibleSelection newSelection(position, position, DOWNSTREAM);
        if (frame.selection().shouldChangeSelection(newSelection)) {
            AXTextStateChangeIntent intent(AXTextStateChangeTypeSelectionMove, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, true });
            frame.selection().setSelection(newSelection, FrameSelection::defaultSetSelectionOptions(UserTriggered), intent);
        }
    }

    element->focus(false, direction);
    return true;
}

Element* FocusController::findFocusableElementAcrossFocusScope(FocusDirection direction, const FocusNavigationScope& scope, Node* currentNode, KeyboardEvent* event)
{
    ASSERT(!is<Element>(currentNode) || !isNonFocusableShadowHost(*downcast<Element>(currentNode), *event));
    Element* found;
    if (currentNode && direction == FocusDirectionForward && isFocusableShadowHost(*currentNode, *event)) {
        Element* foundInInnerFocusScope = findFocusableElementRecursively(direction, FocusNavigationScope::scopeOwnedByShadowHost(downcast<Element>(*currentNode)), 0, event);
        found = foundInInnerFocusScope ? foundInInnerFocusScope : findFocusableElementRecursively(direction, scope, currentNode, event);
    } else
        found = findFocusableElementRecursively(direction, scope, currentNode, event);

    // If there's no focusable node to advance to, move up the focus scopes until we find one.
    Element* owner = scope.owner();
    while (!found && owner) {
        FocusNavigationScope currentScope = FocusNavigationScope::scopeOf(*owner);
        if (direction == FocusDirectionBackward && isFocusableShadowHost(*owner, *event)) {
            found = owner;
            break;
        }
        found = findFocusableElementRecursively(direction, currentScope, owner, event);
        owner = currentScope.owner();
    }
    found = findFocusableElementDescendingDownIntoFrameDocument(direction, found, event);
    return found;
}

Element* FocusController::findFocusableElementRecursively(FocusDirection direction, const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    // Starting node is exclusive.
    Element* found = findFocusableElement(direction, scope, start, event);
    if (!found)
        return nullptr;
    if (direction == FocusDirectionForward) {
        if (!isNonFocusableShadowHost(*found, *event))
            return found;
        Element* foundInInnerFocusScope = findFocusableElementRecursively(direction, FocusNavigationScope::scopeOwnedByShadowHost(*found), 0, event);
        return foundInInnerFocusScope ? foundInInnerFocusScope : findFocusableElementRecursively(direction, scope, found, event);
    }
    ASSERT(direction == FocusDirectionBackward);
    if (isFocusableShadowHost(*found, *event)) {
        Element* foundInInnerFocusScope = findFocusableElementRecursively(direction, FocusNavigationScope::scopeOwnedByShadowHost(*found), 0, event);
        return foundInInnerFocusScope ? foundInInnerFocusScope : found;
    }
    if (isNonFocusableShadowHost(*found, *event)) {
        Element* foundInInnerFocusScope = findFocusableElementRecursively(direction, FocusNavigationScope::scopeOwnedByShadowHost(*found), 0, event);
        return foundInInnerFocusScope ? foundInInnerFocusScope :findFocusableElementRecursively(direction, scope, found, event);
    }
    return found;
}

Element* FocusController::findFocusableElement(FocusDirection direction, const FocusNavigationScope& scope, Node* node, KeyboardEvent* event)
{
    return (direction == FocusDirectionForward)
        ? nextFocusableElement(scope, node, event)
        : previousFocusableElement(scope, node, event);
}

Element* FocusController::findElementWithExactTabIndex(const FocusNavigationScope& scope, Node* start, int tabIndex, KeyboardEvent* event, FocusDirection direction)
{
    // Search is inclusive of start
    for (Node* node = start; node; node = direction == FocusDirectionForward ? scope.nextInScope(node) : scope.previousInScope(node)) {
        if (!is<Element>(*node))
            continue;
        Element& element = downcast<Element>(*node);
        if (isFocusableOrHasShadowTreeWithoutCustomFocusLogic(element, *event) && shadowAdjustedTabIndex(element, *event) == tabIndex)
            return &element;
    }
    return nullptr;
}

static Element* nextElementWithGreaterTabIndex(const FocusNavigationScope& scope, int tabIndex, KeyboardEvent& event)
{
    // Search is inclusive of start
    int winningTabIndex = std::numeric_limits<int>::max();
    Element* winner = nullptr;
    for (Node* node = &scope.rootNode(); node; node = scope.nextInScope(node)) {
        if (!is<Element>(*node))
            continue;
        Element& candidate = downcast<Element>(*node);
        int candidateTabIndex = candidate.tabIndex();
        if (isFocusableOrHasShadowTreeWithoutCustomFocusLogic(candidate, event) && candidateTabIndex > tabIndex) {
            if (!winner || candidateTabIndex < winningTabIndex) {
                winner = &candidate;
                winningTabIndex = candidateTabIndex;
            }
        }
    }

    return winner;
}

static Element* previousElementWithLowerTabIndex(const FocusNavigationScope& scope, Node* start, int tabIndex, KeyboardEvent& event)
{
    // Search is inclusive of start
    int winningTabIndex = 0;
    Element* winner = nullptr;
    for (Node* node = start; node; node = scope.previousInScope(node)) {
        if (!is<Element>(*node))
            continue;
        Element& element = downcast<Element>(*node);
        int currentTabIndex = shadowAdjustedTabIndex(element, event);
        if ((isFocusableOrHasShadowTreeWithoutCustomFocusLogic(element, event) || isNonFocusableShadowHost(element, event)) && currentTabIndex < tabIndex && currentTabIndex > winningTabIndex) {
            winner = &element;
            winningTabIndex = currentTabIndex;
        }
    }
    return winner;
}

Element* FocusController::nextFocusableElement(Node& start)
{
    Ref<KeyboardEvent> keyEvent = KeyboardEvent::createForDummy();
    return nextFocusableElement(FocusNavigationScope::scopeOf(start), &start, keyEvent.ptr());
}

Element* FocusController::previousFocusableElement(Node& start)
{
    Ref<KeyboardEvent> keyEvent = KeyboardEvent::createForDummy();
    return previousFocusableElement(FocusNavigationScope::scopeOf(start), &start, keyEvent.ptr());
}

Element* FocusController::nextFocusableElement(const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    int startTabIndex = 0;
    if (start && is<Element>(*start))
        startTabIndex = shadowAdjustedTabIndex(downcast<Element>(*start), *event);

    if (start) {
        // If a node is excluded from the normal tabbing cycle, the next focusable node is determined by tree order
        if (startTabIndex < 0) {
            for (Node* node = scope.nextInScope(start); node; node = scope.nextInScope(node)) {
                if (!is<Element>(*node))
                    continue;
                Element& element = downcast<Element>(*node);
                if (isFocusableOrHasShadowTreeWithoutCustomFocusLogic(element, *event) && shadowAdjustedTabIndex(element, *event) >= 0)
                    return &element;
            }
        }

        // First try to find a node with the same tabindex as start that comes after start in the scope.
        if (Element* winner = findElementWithExactTabIndex(scope, scope.nextInScope(start), startTabIndex, event, FocusDirectionForward))
            return winner;

        if (!startTabIndex)
            return nullptr; // We've reached the last node in the document with a tabindex of 0. This is the end of the tabbing order.
    }

    // Look for the first Element in the scope that:
    // 1) has the lowest tabindex that is higher than start's tabindex (or 0, if start is null), and
    // 2) comes first in the scope, if there's a tie.
    if (Element* winner = nextElementWithGreaterTabIndex(scope, startTabIndex, *event))
        return winner;

    // There are no nodes with a tabindex greater than start's tabindex,
    // so find the first node with a tabindex of 0.
    return findElementWithExactTabIndex(scope, &scope.rootNode(), 0, event, FocusDirectionForward);
}

Element* FocusController::previousFocusableElement(const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    Node* last = nullptr;
    for (Node* node = &scope.rootNode(); node; node = scope.lastChildInScope(node))
        last = node;
    ASSERT(last);

    // First try to find the last node in the scope that comes before start and has the same tabindex as start.
    // If start is null, find the last node in the scope with a tabindex of 0.
    Node* startingNode;
    int startingTabIndex = 0;
    if (start) {
        startingNode = scope.previousInScope(start);
        if (is<Element>(*start))
            startingTabIndex = shadowAdjustedTabIndex(downcast<Element>(*start), *event);
    } else
        startingNode = last;

    // However, if a node is excluded from the normal tabbing cycle, the previous focusable node is determined by tree order
    if (startingTabIndex < 0) {
        for (Node* node = startingNode; node; node = scope.previousInScope(node)) {
            if (!is<Element>(*node))
                continue;
            Element& element = downcast<Element>(*node);
            if (isFocusableOrHasShadowTreeWithoutCustomFocusLogic(element, *event) && shadowAdjustedTabIndex(element, *event) >= 0)
                return &element;
        }
    }

    if (Element* winner = findElementWithExactTabIndex(scope, startingNode, startingTabIndex, event, FocusDirectionBackward))
        return winner;

    // There are no nodes before start with the same tabindex as start, so look for a node that:
    // 1) has the highest non-zero tabindex (that is less than start's tabindex), and
    // 2) comes last in the scope, if there's a tie.
    startingTabIndex = (start && startingTabIndex) ? startingTabIndex : std::numeric_limits<int>::max();
    return previousElementWithLowerTabIndex(scope, last, startingTabIndex, *event);
}

static bool relinquishesEditingFocus(Node *node)
{
    ASSERT(node);
    ASSERT(node->hasEditableStyle());

    Node* root = node->rootEditableElement();
    Frame* frame = node->document().frame();
    if (!frame || !root)
        return false;

    return frame->editor().shouldEndEditing(rangeOfContents(*root).ptr());
}

static void clearSelectionIfNeeded(Frame* oldFocusedFrame, Frame* newFocusedFrame, Node* newFocusedNode)
{
    if (!oldFocusedFrame || !newFocusedFrame)
        return;
        
    if (oldFocusedFrame->document() != newFocusedFrame->document())
        return;

    const VisibleSelection& selection = oldFocusedFrame->selection().selection();
    if (selection.isNone())
        return;

    bool caretBrowsing = oldFocusedFrame->settings().caretBrowsingEnabled();
    if (caretBrowsing)
        return;

    Node* selectionStartNode = selection.start().deprecatedNode();
    if (selectionStartNode == newFocusedNode || selectionStartNode->isDescendantOf(newFocusedNode) || selectionStartNode->deprecatedShadowAncestorNode() == newFocusedNode)
        return;
        
    if (Node* mousePressNode = newFocusedFrame->eventHandler().mousePressNode()) {
        if (mousePressNode->renderer() && !mousePressNode->canStartSelection()) {
            // Don't clear the selection for contentEditable elements, but do clear it for input and textarea. See bug 38696.
            Node * root = selection.rootEditableElement();
            if (!root)
                return;

            if (Node* shadowAncestorNode = root->deprecatedShadowAncestorNode()) {
                if (!is<HTMLInputElement>(*shadowAncestorNode) && !is<HTMLTextAreaElement>(*shadowAncestorNode))
                    return;
            }
        }
    }

    oldFocusedFrame->selection().clear();
}

bool FocusController::setFocusedElement(Element* element, PassRefPtr<Frame> newFocusedFrame, FocusDirection direction)
{
    RefPtr<Frame> oldFocusedFrame = focusedFrame();
    RefPtr<Document> oldDocument = oldFocusedFrame ? oldFocusedFrame->document() : nullptr;
    
    Element* oldFocusedElement = oldDocument ? oldDocument->focusedElement() : nullptr;
    if (oldFocusedElement == element)
        return true;

    // FIXME: Might want to disable this check for caretBrowsing
    if (oldFocusedElement && oldFocusedElement->isRootEditableElement() && !relinquishesEditingFocus(oldFocusedElement))
        return false;

    m_page.editorClient().willSetInputMethodState();

    clearSelectionIfNeeded(oldFocusedFrame.get(), newFocusedFrame.get(), element);

    if (!element) {
        if (oldDocument)
            oldDocument->setFocusedElement(nullptr);
        m_page.editorClient().setInputMethodState(false);
        return true;
    }

    Ref<Document> newDocument(element->document());

    if (newDocument->focusedElement() == element) {
        m_page.editorClient().setInputMethodState(element->shouldUseInputMethod());
        return true;
    }
    
    if (oldDocument && oldDocument != newDocument.ptr())
        oldDocument->setFocusedElement(nullptr);

    if (newFocusedFrame && !newFocusedFrame->page()) {
        setFocusedFrame(nullptr);
        return false;
    }
    setFocusedFrame(newFocusedFrame);

    Ref<Element> protect(*element);

    bool successfullyFocused = newDocument->setFocusedElement(element, direction);
    if (!successfullyFocused)
        return false;

    if (newDocument->focusedElement() == element)
        m_page.editorClient().setInputMethodState(element->shouldUseInputMethod());

    m_focusSetTime = monotonicallyIncreasingTime();
    m_focusRepaintTimer.stop();

    return true;
}

void FocusController::setViewState(ViewState::Flags viewState)
{
    ViewState::Flags changed = m_viewState ^ viewState;
    m_viewState = viewState;

    if (changed & ViewState::IsFocused)
        setFocusedInternal(viewState & ViewState::IsFocused);
    if (changed & ViewState::WindowIsActive) {
        setActiveInternal(viewState & ViewState::WindowIsActive);
        if (changed & ViewState::IsVisible)
            setIsVisibleAndActiveInternal(viewState & ViewState::WindowIsActive);
    }
}

void FocusController::setActive(bool active)
{
    m_page.setViewState(active ? m_viewState | ViewState::WindowIsActive : m_viewState & ~ViewState::WindowIsActive);
}

void FocusController::setActiveInternal(bool active)
{
    if (FrameView* view = m_page.mainFrame().view()) {
        if (!view->platformWidget()) {
            view->updateLayoutAndStyleIfNeededRecursive();
            view->updateControlTints();
        }
    }

    focusedOrMainFrame().selection().pageActivationChanged();
    
    if (m_focusedFrame && isFocused())
        dispatchEventsOnWindowAndFocusedElement(m_focusedFrame->document(), active);
}

static void contentAreaDidShowOrHide(ScrollableArea* scrollableArea, bool didShow)
{
    if (didShow)
        scrollableArea->contentAreaDidShow();
    else
        scrollableArea->contentAreaDidHide();
}

void FocusController::setIsVisibleAndActiveInternal(bool contentIsVisible)
{
    FrameView* view = m_page.mainFrame().view();
    if (!view)
        return;

    contentAreaDidShowOrHide(view, contentIsVisible);

    for (Frame* frame = &m_page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView)
            continue;

        const HashSet<ScrollableArea*>* scrollableAreas = frameView->scrollableAreas();
        if (!scrollableAreas)
            continue;

        for (auto& scrollableArea : *scrollableAreas) {
            ASSERT(scrollableArea->scrollbarsCanBeActive() || m_page.shouldSuppressScrollbarAnimations());

            contentAreaDidShowOrHide(scrollableArea, contentIsVisible);
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
        HitTestResult result = candidate.visibleNode->document().page()->mainFrame().eventHandler().hitTestResultAtPoint(IntPoint(x, y), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent);
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

void FocusController::findFocusCandidateInContainer(Node& container, const LayoutRect& startingRect, FocusDirection direction, KeyboardEvent* event, FocusCandidate& closest)
{
    Node* focusedNode = (focusedFrame() && focusedFrame()->document()) ? focusedFrame()->document()->focusedElement() : 0;

    Element* element = ElementTraversal::firstWithin(container);
    FocusCandidate current;
    current.rect = startingRect;
    current.focusableNode = focusedNode;
    current.visibleNode = focusedNode;

    unsigned candidateCount = 0;
    for (; element; element = (element->isFrameOwnerElement() || canScrollInDirection(element, direction))
        ? ElementTraversal::nextSkippingChildren(*element, &container)
        : ElementTraversal::next(*element, &container)) {
        if (element == focusedNode)
            continue;

        if (!element->isKeyboardFocusable(event) && !element->isFrameOwnerElement() && !canScrollInDirection(element, direction))
            continue;

        FocusCandidate candidate = FocusCandidate(element, direction);
        if (candidate.isNull())
            continue;

        if (!isValidCandidate(direction, current, candidate))
            continue;

        candidateCount++;
        candidate.enclosingScrollableBox = &container;
        updateFocusCandidateIfNeeded(direction, current, candidate, closest);
    }

    // The variable 'candidateCount' keeps track of the number of nodes traversed in a given container.
    // If we have more than one container in a page then the total number of nodes traversed is equal to the sum of nodes traversed in each container.
    if (focusedFrame() && focusedFrame()->document()) {
        candidateCount += focusedFrame()->document()->page()->lastSpatialNavigationCandidateCount();
        focusedFrame()->document()->page()->setLastSpatialNavigationCandidateCount(candidateCount);
    }
}

bool FocusController::advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, FocusDirection direction, KeyboardEvent* event)
{
    if (!container)
        return false;

    LayoutRect newStartingRect = startingRect;

    if (startingRect.isEmpty())
        newStartingRect = virtualRectForDirection(direction, nodeRectInAbsoluteCoordinates(container));

    // Find the closest node within current container in the direction of the navigation.
    FocusCandidate focusCandidate;
    findFocusCandidateInContainer(*container, newStartingRect, direction, event, focusCandidate);

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
            scrollInDirection(&focusCandidate.visibleNode->document(), direction);
            return true;
        }
        // Navigate into a new frame.
        LayoutRect rect;
        Element* focusedElement = focusedOrMainFrame().document()->focusedElement();
        if (focusedElement && !hasOffscreenRect(focusedElement))
            rect = nodeRectInAbsoluteCoordinates(focusedElement, true /* ignore border */);
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
        Element* focusedElement = focusedOrMainFrame().document()->focusedElement();
        if (focusedElement && !hasOffscreenRect(focusedElement))
            startingRect = nodeRectInAbsoluteCoordinates(focusedElement, true);
        return advanceFocusDirectionallyInContainer(focusCandidate.visibleNode, startingRect, direction, event);
    }
    if (focusCandidate.isOffscreenAfterScrolling) {
        Node* container = focusCandidate.enclosingScrollableBox;
        scrollInDirection(container, direction);
        return true;
    }

    // We found a new focus node, navigate to it.
    Element* element = downcast<Element>(focusCandidate.focusableNode);
    ASSERT(element);

    element->focus(false, direction);
    return true;
}

bool FocusController::advanceFocusDirectionally(FocusDirection direction, KeyboardEvent* event)
{
    Document* focusedDocument = focusedOrMainFrame().document();
    if (!focusedDocument)
        return false;

    Element* focusedElement = focusedDocument->focusedElement();
    Node* container = focusedDocument;

    if (is<Document>(*container))
        downcast<Document>(*container).updateLayoutIgnorePendingStylesheets();

    // Figure out the starting rect.
    LayoutRect startingRect;
    if (focusedElement) {
        if (!hasOffscreenRect(focusedElement)) {
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, focusedElement);
            startingRect = nodeRectInAbsoluteCoordinates(focusedElement, true /* ignore border */);
        } else if (is<HTMLAreaElement>(*focusedElement)) {
            HTMLAreaElement& area = downcast<HTMLAreaElement>(*focusedElement);
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, area.imageElement());
            startingRect = virtualRectForAreaElementAndDirection(&area, direction);
        }
    }

    if (focusedFrame() && focusedFrame()->document())
        focusedDocument->page()->setLastSpatialNavigationCandidateCount(0);

    bool consumed = false;
    do {
        consumed = advanceFocusDirectionallyInContainer(container, startingRect, direction, event);
        startingRect = nodeRectInAbsoluteCoordinates(container, true /* ignore border */);
        container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, container);
        if (is<Document>(container))
            downcast<Document>(*container).updateLayoutIgnorePendingStylesheets();
    } while (!consumed && container);

    return consumed;
}

void FocusController::setFocusedElementNeedsRepaint()
{
    m_focusRepaintTimer.startOneShot(0.033);
}

void FocusController::focusRepaintTimerFired()
{
    Document* focusedDocument = focusedOrMainFrame().document();
    if (!focusedDocument)
        return;

    Element* focusedElement = focusedDocument->focusedElement();
    if (!focusedElement)
        return;

    if (focusedElement->renderer())
        focusedElement->renderer()->repaint();
}

double FocusController::timeSinceFocusWasSet() const
{
    return monotonicallyIncreasingTime() - m_focusSetTime;
}

} // namespace WebCore
