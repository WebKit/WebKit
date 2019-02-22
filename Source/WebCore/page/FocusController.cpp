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
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "ElementTraversal.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "HTMLSlotElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "KeyboardEvent.h"
#include "Page.h"
#include "Range.h"
#include "RenderWidget.h"
#include "ScrollAnimator.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SpatialNavigation.h"
#include "Widget.h"
#include <limits>
#include <wtf/Ref.h>

namespace WebCore {

using namespace HTMLNames;

static inline bool hasCustomFocusLogic(const Element& element)
{
    return is<HTMLElement>(element) && downcast<HTMLElement>(element).hasCustomFocusLogic();
}

static inline bool isFocusScopeOwner(const Element& element)
{
    if (element.shadowRoot() && !hasCustomFocusLogic(element))
        return true;
    if (is<HTMLSlotElement>(element)) {
        ShadowRoot* root = element.containingShadowRoot();
        if (root && root->host() && !hasCustomFocusLogic(*root->host()))
            return true;
    } 
    return false;
}

class FocusNavigationScope {
public:
    Element* owner() const;
    WEBCORE_EXPORT static FocusNavigationScope scopeOf(Node&);
    static FocusNavigationScope scopeOwnedByScopeOwner(Element&);
    static FocusNavigationScope scopeOwnedByIFrame(HTMLFrameOwnerElement&);

    Node* firstNodeInScope() const;
    Node* lastNodeInScope() const;
    Node* nextInScope(const Node*) const;
    Node* previousInScope(const Node*) const;
    Node* lastChildInScope(const Node&) const;

private:
    enum class SlotKind : uint8_t { Assigned, Fallback };

    Node* firstChildInScope(const Node&) const;

    Node* parentInScope(const Node&) const;

    Node* nextSiblingInScope(const Node&) const;
    Node* previousSiblingInScope(const Node&) const;

    explicit FocusNavigationScope(TreeScope&);
    explicit FocusNavigationScope(HTMLSlotElement&, SlotKind);

    RefPtr<ContainerNode> m_treeScopeRootNode;
    RefPtr<HTMLSlotElement> m_slotElement;
    SlotKind m_slotKind { SlotKind::Assigned };
};

// FIXME: Focus navigation should work with shadow trees that have slots.
Node* FocusNavigationScope::firstChildInScope(const Node& node) const
{
    if (is<Element>(node) && isFocusScopeOwner(downcast<Element>(node)))
        return nullptr;
    return node.firstChild();
}

Node* FocusNavigationScope::lastChildInScope(const Node& node) const
{
    if (is<Element>(node) && isFocusScopeOwner(downcast<Element>(node)))
        return nullptr;
    return node.lastChild();
}

Node* FocusNavigationScope::parentInScope(const Node& node) const
{
    if (m_treeScopeRootNode == &node)
        return nullptr;

    if (UNLIKELY(m_slotElement)) {
        if (m_slotKind == SlotKind::Assigned) {
            if (m_slotElement == node.assignedSlot())
                return nullptr;
        } else {
            ASSERT(m_slotKind == SlotKind::Fallback);
            auto* parentNode = node.parentNode();
            if (parentNode == m_slotElement)
                return nullptr;
        }
    }

    return node.parentNode();
}

Node* FocusNavigationScope::nextSiblingInScope(const Node& node) const
{
    if (UNLIKELY(m_slotElement && m_slotElement == node.assignedSlot())) {
        for (Node* current = node.nextSibling(); current; current = current->nextSibling()) {
            if (current->assignedSlot() == m_slotElement)
                return current;
        }
        return nullptr;
    }
    return node.nextSibling();
}

Node* FocusNavigationScope::previousSiblingInScope(const Node& node) const
{
    if (UNLIKELY(m_slotElement && m_slotElement == node.assignedSlot())) {
        for (Node* current = node.previousSibling(); current; current = current->previousSibling()) {
            if (current->assignedSlot() == m_slotElement)
                return current;
        }
        return nullptr;
    }
    return node.previousSibling();
}

Node* FocusNavigationScope::firstNodeInScope() const
{
    if (UNLIKELY(m_slotElement)) {
        auto* assigneNodes = m_slotElement->assignedNodes();
        if (m_slotKind == SlotKind::Assigned) {
            ASSERT(assigneNodes);
            return assigneNodes->first();
        }
        ASSERT(m_slotKind == SlotKind::Fallback);
        return m_slotElement->firstChild();
    }
    ASSERT(m_treeScopeRootNode);
    return m_treeScopeRootNode.get();
}

Node* FocusNavigationScope::lastNodeInScope() const
{
    if (UNLIKELY(m_slotElement)) {
        auto* assigneNodes = m_slotElement->assignedNodes();
        if (m_slotKind == SlotKind::Assigned) {
            ASSERT(assigneNodes);
            return assigneNodes->last();
        }
        ASSERT(m_slotKind == SlotKind::Fallback);
        return m_slotElement->lastChild();
    }
    ASSERT(m_treeScopeRootNode);
    return m_treeScopeRootNode.get();
}

Node* FocusNavigationScope::nextInScope(const Node* node) const
{
    ASSERT(node);
    if (Node* next = firstChildInScope(*node))
        return next;
    if (Node* next = nextSiblingInScope(*node))
        return next;
    const Node* current = node;
    while (current && !nextSiblingInScope(*current))
        current = parentInScope(*current);
    return current ? nextSiblingInScope(*current) : nullptr;
}

Node* FocusNavigationScope::previousInScope(const Node* node) const
{
    ASSERT(node);
    if (Node* current = previousSiblingInScope(*node)) {
        while (Node* child = lastChildInScope(*current))
            current = child;
        return current;
    }
    return parentInScope(*node);
}

FocusNavigationScope::FocusNavigationScope(TreeScope& treeScope)
    : m_treeScopeRootNode(&treeScope.rootNode())
{
}

FocusNavigationScope::FocusNavigationScope(HTMLSlotElement& slotElement, SlotKind slotKind)
    : m_slotElement(&slotElement)
    , m_slotKind(slotKind)
{
}

Element* FocusNavigationScope::owner() const
{
    if (m_slotElement)
        return m_slotElement.get();

    ASSERT(m_treeScopeRootNode);
    if (is<ShadowRoot>(*m_treeScopeRootNode))
        return downcast<ShadowRoot>(*m_treeScopeRootNode).host();
    if (Frame* frame = m_treeScopeRootNode->document().frame())
        return frame->ownerElement();
    return nullptr;
}

FocusNavigationScope FocusNavigationScope::scopeOf(Node& startingNode)
{
    ASSERT(startingNode.isInTreeScope());
    RefPtr<Node> root;
    RefPtr<Node> parentNode;
    for (RefPtr<Node> currentNode = &startingNode; currentNode; currentNode = parentNode) {
        root = currentNode;
        if (HTMLSlotElement* slot = currentNode->assignedSlot()) {
            if (isFocusScopeOwner(*slot))
                return FocusNavigationScope(*slot, SlotKind::Assigned);
        }
        if (is<ShadowRoot>(currentNode))
            return FocusNavigationScope(downcast<ShadowRoot>(*currentNode));
        parentNode = currentNode->parentNode();
        // The scope of a fallback content of a HTMLSlotElement is the slot element
        // but the scope of a HTMLSlotElement is its parent scope.
        if (parentNode && is<HTMLSlotElement>(parentNode) && !downcast<HTMLSlotElement>(*parentNode).assignedNodes())
            return FocusNavigationScope(downcast<HTMLSlotElement>(*parentNode), SlotKind::Fallback);
    }
    ASSERT(root);
    return FocusNavigationScope(root->treeScope());
}

FocusNavigationScope FocusNavigationScope::scopeOwnedByScopeOwner(Element& element)
{
    ASSERT(element.shadowRoot() || is<HTMLSlotElement>(element));
    if (is<HTMLSlotElement>(element)) {
        auto& slot = downcast<HTMLSlotElement>(element);
        return FocusNavigationScope(slot, slot.assignedNodes() ? SlotKind::Assigned : SlotKind::Fallback);
    }
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
    document->dispatchWindowEvent(Event::create(focused ? eventNames().focusEvent : eventNames().blurEvent, Event::CanBubble::No, Event::IsCancelable::No));
    if (focused && document->focusedElement())
        document->focusedElement()->dispatchFocusEvent(nullptr, FocusDirectionNone);
}

static inline bool isFocusableElementOrScopeOwner(Element& element, KeyboardEvent* event)
{
    return element.isKeyboardFocusable(event) || isFocusScopeOwner(element);
}

static inline bool isNonFocusableScopeOwner(Element& element, KeyboardEvent* event)
{
    return !element.isKeyboardFocusable(event) && isFocusScopeOwner(element);
}

static inline bool isFocusableScopeOwner(Element& element, KeyboardEvent* event)
{
    return element.isKeyboardFocusable(event) && isFocusScopeOwner(element);
}

static inline int shadowAdjustedTabIndex(Element& element, KeyboardEvent* event)
{
    if (isNonFocusableScopeOwner(element, event)) {
        if (!element.tabIndexSetExplicitly())
            return 0; // Treat a shadow host without tabindex if it has tabindex=0 even though HTMLElement::tabIndex returns -1 on such an element.
    }
    return element.tabIndex();
}

FocusController::FocusController(Page& page, OptionSet<ActivityState::Flag> activityState)
    : m_page(page)
    , m_isChangingFocusedFrame(false)
    , m_activityState(activityState)
    , m_focusRepaintTimer(*this, &FocusController::focusRepaintTimerFired)
{
}

void FocusController::setFocusedFrame(Frame* frame)
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
        oldFrame->document()->dispatchWindowEvent(Event::create(eventNames().blurEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }

    if (newFrame && newFrame->view() && isFocused()) {
        newFrame->selection().setFocused(true);
        newFrame->document()->dispatchWindowEvent(Event::create(eventNames().focusEvent, Event::CanBubble::No, Event::IsCancelable::No));
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
    m_page.setActivityState(focused ? m_activityState | ActivityState::IsFocused : m_activityState - ActivityState::IsFocused);
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

Element* FocusController::findFocusableElementDescendingIntoSubframes(FocusDirection direction, Element* element, KeyboardEvent* event)
{
    // The node we found might be a HTMLFrameOwnerElement, so descend down the tree until we find either:
    // 1) a focusable node, or
    // 2) the deepest-nested HTMLFrameOwnerElement.
    while (is<HTMLFrameOwnerElement>(element)) {
        HTMLFrameOwnerElement& owner = downcast<HTMLFrameOwnerElement>(*element);
        if (!owner.contentFrame() || !owner.contentFrame()->document())
            break;
        owner.contentFrame()->document()->updateLayoutIgnorePendingStylesheets();
        Element* foundElement = findFocusableElementWithinScope(direction, FocusNavigationScope::scopeOwnedByIFrame(owner), nullptr, event);
        if (!foundElement)
            break;
        ASSERT(element != foundElement);
        element = foundElement;
    }
    return element;
}

bool FocusController::setInitialFocus(FocusDirection direction, KeyboardEvent* providedEvent)
{
    bool didAdvanceFocus = advanceFocus(direction, providedEvent, true);

    // If focus is being set initially, accessibility needs to be informed that system focus has moved 
    // into the web area again, even if focus did not change within WebCore. PostNotification is called instead
    // of handleFocusedUIElementChanged, because this will send the notification even if the element is the same.
    if (auto* cache = focusedOrMainFrame().document()->existingAXObjectCache())
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

    Node* currentNode = document->focusNavigationStartingNode(direction);
    // FIXME: Not quite correct when it comes to focus transitions leaving/entering the WebView itself
    bool caretBrowsing = frame.settings().caretBrowsingEnabled();

    if (caretBrowsing && !currentNode)
        currentNode = frame.selection().selection().start().deprecatedNode();

    document->updateLayoutIgnorePendingStylesheets();

    RefPtr<Element> element = findFocusableElementAcrossFocusScope(direction, FocusNavigationScope::scopeOf(currentNode ? *currentNode : *document), currentNode, event);

    if (!element) {
        // We didn't find a node to focus, so we should try to pass focus to Chrome.
        if (!initialFocus && m_page.chrome().canTakeFocus(direction) && !m_page.isControlledByAutomation()) {
            document->setFocusedElement(nullptr);
            setFocusedFrame(nullptr);
            m_page.chrome().takeFocus(direction);
            return true;
        }

        // Chrome doesn't want focus, so we should wrap focus.
        element = findFocusableElementAcrossFocusScope(direction, FocusNavigationScope::scopeOf(*m_page.mainFrame().document()), nullptr, event);

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
    ASSERT(!is<Element>(currentNode) || !isNonFocusableScopeOwner(downcast<Element>(*currentNode), event));

    if (currentNode && direction == FocusDirectionForward && is<Element>(currentNode) && isFocusableScopeOwner(downcast<Element>(*currentNode), event)) {
        if (Element* candidateInInnerScope = findFocusableElementWithinScope(direction, FocusNavigationScope::scopeOwnedByScopeOwner(downcast<Element>(*currentNode)), 0, event))
            return candidateInInnerScope;
    }

    if (Element* candidateInCurrentScope = findFocusableElementWithinScope(direction, scope, currentNode, event))
        return candidateInCurrentScope;

    // If there's no focusable node to advance to, move up the focus scopes until we find one.
    Element* owner = scope.owner();
    while (owner) {
        if (direction == FocusDirectionBackward && isFocusableScopeOwner(*owner, event))
            return findFocusableElementDescendingIntoSubframes(direction, owner, event);

        auto outerScope = FocusNavigationScope::scopeOf(*owner);
        if (Element* candidateInOuterScope = findFocusableElementWithinScope(direction, outerScope, owner, event))
            return candidateInOuterScope;
        owner = outerScope.owner();
    }
    return nullptr;
}

Element* FocusController::findFocusableElementWithinScope(FocusDirection direction, const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    // Starting node is exclusive.
    Element* candidate = direction == FocusDirectionForward
        ? nextFocusableElementWithinScope(scope, start, event)
        : previousFocusableElementWithinScope(scope, start, event);
    return findFocusableElementDescendingIntoSubframes(direction, candidate, event);
}

Element* FocusController::nextFocusableElementWithinScope(const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    Element* found = nextFocusableElementOrScopeOwner(scope, start, event);
    if (!found)
        return nullptr;
    if (isNonFocusableScopeOwner(*found, event)) {
        if (Element* foundInInnerFocusScope = nextFocusableElementWithinScope(FocusNavigationScope::scopeOwnedByScopeOwner(*found), 0, event))
            return foundInInnerFocusScope;
        return nextFocusableElementWithinScope(scope, found, event);
    }
    return found;
}

Element* FocusController::previousFocusableElementWithinScope(const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    Element* found = previousFocusableElementOrScopeOwner(scope, start, event);
    if (!found)
        return nullptr;
    if (isFocusableScopeOwner(*found, event)) {
        // Search an inner focusable element in the shadow tree from the end.
        if (Element* foundInInnerFocusScope = previousFocusableElementWithinScope(FocusNavigationScope::scopeOwnedByScopeOwner(*found), 0, event))
            return foundInInnerFocusScope;
        return found;
    }
    if (isNonFocusableScopeOwner(*found, event)) {
        if (Element* foundInInnerFocusScope = previousFocusableElementWithinScope(FocusNavigationScope::scopeOwnedByScopeOwner(*found), 0, event))
            return foundInInnerFocusScope;
        return previousFocusableElementWithinScope(scope, found, event);
    }
    return found;
}

Element* FocusController::findFocusableElementOrScopeOwner(FocusDirection direction, const FocusNavigationScope& scope, Node* node, KeyboardEvent* event)
{
    return (direction == FocusDirectionForward)
        ? nextFocusableElementOrScopeOwner(scope, node, event)
        : previousFocusableElementOrScopeOwner(scope, node, event);
}

Element* FocusController::findElementWithExactTabIndex(const FocusNavigationScope& scope, Node* start, int tabIndex, KeyboardEvent* event, FocusDirection direction)
{
    // Search is inclusive of start
    for (Node* node = start; node; node = direction == FocusDirectionForward ? scope.nextInScope(node) : scope.previousInScope(node)) {
        if (!is<Element>(*node))
            continue;
        Element& element = downcast<Element>(*node);
        if (isFocusableElementOrScopeOwner(element, event) && shadowAdjustedTabIndex(element, event) == tabIndex)
            return &element;
    }
    return nullptr;
}

static Element* nextElementWithGreaterTabIndex(const FocusNavigationScope& scope, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    int winningTabIndex = std::numeric_limits<int>::max();
    Element* winner = nullptr;
    for (Node* node = scope.firstNodeInScope(); node; node = scope.nextInScope(node)) {
        if (!is<Element>(*node))
            continue;
        Element& candidate = downcast<Element>(*node);
        int candidateTabIndex = candidate.tabIndex();
        if (isFocusableElementOrScopeOwner(candidate, event) && candidateTabIndex > tabIndex && (!winner || candidateTabIndex < winningTabIndex)) {
            winner = &candidate;
            winningTabIndex = candidateTabIndex;
        }
    }

    return winner;
}

static Element* previousElementWithLowerTabIndex(const FocusNavigationScope& scope, Node* start, int tabIndex, KeyboardEvent* event)
{
    // Search is inclusive of start
    int winningTabIndex = 0;
    Element* winner = nullptr;
    for (Node* node = start; node; node = scope.previousInScope(node)) {
        if (!is<Element>(*node))
            continue;
        Element& element = downcast<Element>(*node);
        int currentTabIndex = shadowAdjustedTabIndex(element, event);
        if (isFocusableElementOrScopeOwner(element, event) && currentTabIndex < tabIndex && currentTabIndex > winningTabIndex) {
            winner = &element;
            winningTabIndex = currentTabIndex;
        }
    }
    return winner;
}

Element* FocusController::nextFocusableElement(Node& start)
{
    // FIXME: This can return a non-focusable shadow host.
    // FIXME: This can't give the correct answer that takes modifier keys into account since it doesn't pass an event.
    return nextFocusableElementOrScopeOwner(FocusNavigationScope::scopeOf(start), &start, nullptr);
}

Element* FocusController::previousFocusableElement(Node& start)
{
    // FIXME: This can return a non-focusable shadow host.
    // FIXME: This can't give the correct answer that takes modifier keys into account since it doesn't pass an event.
    return previousFocusableElementOrScopeOwner(FocusNavigationScope::scopeOf(start), &start, nullptr);
}

Element* FocusController::nextFocusableElementOrScopeOwner(const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    int startTabIndex = 0;
    if (start && is<Element>(*start))
        startTabIndex = shadowAdjustedTabIndex(downcast<Element>(*start), event);

    if (start) {
        // If a node is excluded from the normal tabbing cycle, the next focusable node is determined by tree order
        if (startTabIndex < 0) {
            for (Node* node = scope.nextInScope(start); node; node = scope.nextInScope(node)) {
                if (!is<Element>(*node))
                    continue;
                Element& element = downcast<Element>(*node);
                if (isFocusableElementOrScopeOwner(element, event) && shadowAdjustedTabIndex(element, event) >= 0)
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
    if (Element* winner = nextElementWithGreaterTabIndex(scope, startTabIndex, event))
        return winner;

    // There are no nodes with a tabindex greater than start's tabindex,
    // so find the first node with a tabindex of 0.
    return findElementWithExactTabIndex(scope, scope.firstNodeInScope(), 0, event, FocusDirectionForward);
}

Element* FocusController::previousFocusableElementOrScopeOwner(const FocusNavigationScope& scope, Node* start, KeyboardEvent* event)
{
    Node* last = nullptr;
    for (Node* node = scope.lastNodeInScope(); node; node = scope.lastChildInScope(*node))
        last = node;
    ASSERT(last);

    // First try to find the last node in the scope that comes before start and has the same tabindex as start.
    // If start is null, find the last node in the scope with a tabindex of 0.
    Node* startingNode;
    int startingTabIndex = 0;
    if (start) {
        startingNode = scope.previousInScope(start);
        if (is<Element>(*start))
            startingTabIndex = shadowAdjustedTabIndex(downcast<Element>(*start), event);
    } else
        startingNode = last;

    // However, if a node is excluded from the normal tabbing cycle, the previous focusable node is determined by tree order
    if (startingTabIndex < 0) {
        for (Node* node = startingNode; node; node = scope.previousInScope(node)) {
            if (!is<Element>(*node))
                continue;
            Element& element = downcast<Element>(*node);
            if (isFocusableElementOrScopeOwner(element, event) && shadowAdjustedTabIndex(element, event) >= 0)
                return &element;
        }
    }

    if (Element* winner = findElementWithExactTabIndex(scope, startingNode, startingTabIndex, event, FocusDirectionBackward))
        return winner;

    // There are no nodes before start with the same tabindex as start, so look for a node that:
    // 1) has the highest non-zero tabindex (that is less than start's tabindex), and
    // 2) comes last in the scope, if there's a tie.
    startingTabIndex = (start && startingTabIndex) ? startingTabIndex : std::numeric_limits<int>::max();
    return previousElementWithLowerTabIndex(scope, last, startingTabIndex, event);
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

    if (newFocusedNode) {
        Node* selectionStartNode = selection.start().deprecatedNode();
        if (newFocusedNode->contains(selectionStartNode) || selectionStartNode->shadowHost() == newFocusedNode)
            return;
    }

    if (Node* mousePressNode = newFocusedFrame->eventHandler().mousePressNode()) {
        if (mousePressNode->renderer() && !mousePressNode->canStartSelection()) {
            // Don't clear the selection for contentEditable elements, but do clear it for input and textarea. See bug 38696.
            auto* root = selection.rootEditableElement();
            if (!root)
                return;
            auto* host = root->shadowHost();
            // FIXME: Seems likely we can just do the check on "host" here instead of "rootOrHost".
            auto* rootOrHost = host ? host : root;
            if (!is<HTMLInputElement>(*rootOrHost) && !is<HTMLTextAreaElement>(*rootOrHost))
                return;
        }
    }

    oldFocusedFrame->selection().clear();
}

static bool shouldClearSelectionWhenChangingFocusedElement(const Page& page, RefPtr<Element> oldFocusedElement, RefPtr<Element> newFocusedElement)
{
#if ENABLE(DATA_INTERACTION)
    if (newFocusedElement || !oldFocusedElement)
        return true;

    // FIXME: These additional checks should not be necessary. We should consider generally keeping the selection whenever the
    // focused element is blurred, with no new element taking focus.
    if (!oldFocusedElement->isRootEditableElement() && !is<HTMLInputElement>(oldFocusedElement) && !is<HTMLTextAreaElement>(oldFocusedElement))
        return true;

    for (auto ancestor = page.mainFrame().eventHandler().draggedElement(); ancestor; ancestor = ancestor->parentOrShadowHostElement()) {
        if (ancestor == oldFocusedElement)
            return false;
    }
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(oldFocusedElement);
    UNUSED_PARAM(newFocusedElement);
#endif
    return true;
}

bool FocusController::setFocusedElement(Element* element, Frame& newFocusedFrame, FocusDirection direction)
{
    Ref<Frame> protectedNewFocusedFrame = newFocusedFrame;
    RefPtr<Frame> oldFocusedFrame = focusedFrame();
    RefPtr<Document> oldDocument = oldFocusedFrame ? oldFocusedFrame->document() : nullptr;
    
    Element* oldFocusedElement = oldDocument ? oldDocument->focusedElement() : nullptr;
    if (oldFocusedElement == element)
        return true;

    // FIXME: Might want to disable this check for caretBrowsing
    if (oldFocusedElement && oldFocusedElement->isRootEditableElement() && !relinquishesEditingFocus(oldFocusedElement))
        return false;

    m_page.editorClient().willSetInputMethodState();

    if (shouldClearSelectionWhenChangingFocusedElement(m_page, oldFocusedElement, element))
        clearSelectionIfNeeded(oldFocusedFrame.get(), &newFocusedFrame, element);

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

    if (!newFocusedFrame.page()) {
        setFocusedFrame(nullptr);
        return false;
    }
    setFocusedFrame(&newFocusedFrame);

    Ref<Element> protect(*element);

    bool successfullyFocused = newDocument->setFocusedElement(element, direction);
    if (!successfullyFocused)
        return false;

    if (newDocument->focusedElement() == element)
        m_page.editorClient().setInputMethodState(element->shouldUseInputMethod());

    m_focusSetTime = MonotonicTime::now();
    m_focusRepaintTimer.stop();

    return true;
}

void FocusController::setActivityState(OptionSet<ActivityState::Flag> activityState)
{
    auto changed = m_activityState ^ activityState;
    m_activityState = activityState;

    if (changed & ActivityState::IsFocused)
        setFocusedInternal(activityState.contains(ActivityState::IsFocused));
    if (changed & ActivityState::WindowIsActive) {
        setActiveInternal(activityState.contains(ActivityState::WindowIsActive));
        if (changed & ActivityState::IsVisible)
            setIsVisibleAndActiveInternal(activityState.contains(ActivityState::WindowIsActive));
    }
}

void FocusController::setActive(bool active)
{
    m_page.setActivityState(active ? m_activityState | ActivityState::WindowIsActive : m_activityState - ActivityState::WindowIsActive);
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
        HitTestResult result = candidate.visibleNode->document().page()->mainFrame().eventHandler().hitTestResultAtPoint(IntPoint(x, y), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowUserAgentShadowContent);
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

    focusedDocument->updateLayoutIgnorePendingStylesheets();

    // Figure out the starting rect.
    Node* container = focusedDocument;
    LayoutRect startingRect;
    if (auto* focusedElement = focusedDocument->focusedElement()) {
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
        focusedDocument->updateLayoutIgnorePendingStylesheets();
        startingRect = nodeRectInAbsoluteCoordinates(container, true /* ignore border */);
        container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(direction, container);
    } while (!consumed && container);

    return consumed;
}

void FocusController::setFocusedElementNeedsRepaint()
{
    m_focusRepaintTimer.startOneShot(33_ms);
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

Seconds FocusController::timeSinceFocusWasSet() const
{
    return MonotonicTime::now() - m_focusSetTime;
}

} // namespace WebCore
