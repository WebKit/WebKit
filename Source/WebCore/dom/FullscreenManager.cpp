/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FullscreenManager.h"

#if ENABLE(FULLSCREEN_API)

#include "Chrome.h"
#include "ChromeClient.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLIFrameElement.h"
#include "HTMLMediaElement.h"
#include "Page.h"
#include "QualifiedName.h"
#include "RenderFullScreen.h"
#include "RenderTreeBuilder.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

FullscreenManager::FullscreenManager(Document& document)
    : m_document { document }
{
}

FullscreenManager::~FullscreenManager() = default;

void FullscreenManager::requestFullscreenForElement(Element* element, FullscreenCheckType checkType)
{
    if (!element)
        element = documentElement();

    auto failedPreflights = [this](auto element) mutable {
        m_fullscreenErrorEventTargetQueue.append(WTFMove(element));
        m_fullscreenTaskQueue.enqueueTask([this] {
            dispatchFullscreenChangeEvents();
        });
    };

    // 1. If any of the following conditions are true, terminate these steps and queue a task to fire
    // an event named fullscreenerror with its bubbles attribute set to true on the context object's
    // node document:

    // This algorithm is not allowed to show a pop-up:
    //   An algorithm is allowed to show a pop-up if, in the task in which the algorithm is running, either:
    //   - an activation behavior is currently being processed whose click event was trusted, or
    //   - the event listener for a trusted click event is being handled.
    if (!UserGestureIndicator::processingUserGesture()) {
        failedPreflights(WTFMove(element));
        return;
    }

    // We do not allow pressing the Escape key as a user gesture to enter fullscreen since this is the key
    // to exit fullscreen.
    if (UserGestureIndicator::currentUserGesture()->gestureType() == UserGestureType::EscapeKey) {
        document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, "The Escape key may not be used as a user gesture to enter fullscreen"_s);
        failedPreflights(WTFMove(element));
        return;
    }

    // There is a previously-established user preference, security risk, or platform limitation.
    if (!page() || !page()->settings().fullScreenEnabled()) {
        failedPreflights(WTFMove(element));
        return;
    }

    bool hasKeyboardAccess = true;
    if (!page()->chrome().client().supportsFullScreenForElement(*element, hasKeyboardAccess)) {
        // The new full screen API does not accept a "flags" parameter, so fall back to disallowing
        // keyboard input if the chrome client refuses to allow keyboard input.
        hasKeyboardAccess = false;

        if (!page()->chrome().client().supportsFullScreenForElement(*element, hasKeyboardAccess)) {
            failedPreflights(WTFMove(element));
            return;
        }
    }

    m_pendingFullscreenElement = element;

    m_fullscreenTaskQueue.enqueueTask([this, element = makeRefPtr(element), checkType, hasKeyboardAccess, failedPreflights] () mutable {
        // Don't allow fullscreen if it has been cancelled or a different fullscreen element
        // has requested fullscreen.
        if (m_pendingFullscreenElement != element) {
            failedPreflights(WTFMove(element));
            return;
        }

        // Don't allow fullscreen if document is hidden.
        if (document().hidden()) {
            failedPreflights(WTFMove(element));
            return;
        }

        // The context object is not in a document.
        if (!element->isConnected()) {
            failedPreflights(WTFMove(element));
            return;
        }

        // The context object's node document, or an ancestor browsing context's document does not have
        // the fullscreen enabled flag set.
        if (checkType == EnforceIFrameAllowFullscreenRequirement && !isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Fullscreen, document())) {
            failedPreflights(WTFMove(element));
            return;
        }

        // The context object's node document fullscreen element stack is not empty and its top element
        // is not an ancestor of the context object.
        if (!m_fullscreenElementStack.isEmpty() && !m_fullscreenElementStack.last()->contains(element.get())) {
            failedPreflights(WTFMove(element));
            return;
        }

        // A descendant browsing context's document has a non-empty fullscreen element stack.
        bool descendentHasNonEmptyStack = false;
        for (Frame* descendant = frame() ? frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
            if (descendant->document()->fullscreenManager().fullscreenElement()) {
                descendentHasNonEmptyStack = true;
                break;
            }
        }
        if (descendentHasNonEmptyStack) {
            failedPreflights(WTFMove(element));
            return;
        }

        // 2. Let doc be element's node document. (i.e. "this")
        Document* currentDoc = &document();

        // 3. Let docs be all doc's ancestor browsing context's documents (if any) and doc.
        Deque<Document*> docs;

        do {
            docs.prepend(currentDoc);
            currentDoc = currentDoc->ownerElement() ? &currentDoc->ownerElement()->document() : nullptr;
        } while (currentDoc);

        // 4. For each document in docs, run these substeps:
        Deque<Document*>::iterator current = docs.begin(), following = docs.begin();

        do {
            ++following;

            // 1. Let following document be the document after document in docs, or null if there is no
            // such document.
            Document* currentDoc = *current;
            Document* followingDoc = following != docs.end() ? *following : nullptr;

            // 2. If following document is null, push context object on document's fullscreen element
            // stack, and queue a task to fire an event named fullscreenchange with its bubbles attribute
            // set to true on the document.
            if (!followingDoc) {
                currentDoc->fullscreenManager().pushFullscreenElementStack(*element);
                addDocumentToFullscreenChangeEventQueue(*currentDoc);
                continue;
            }

            // 3. Otherwise, if document's fullscreen element stack is either empty or its top element
            // is not following document's browsing context container,
            Element* topElement = currentDoc->fullscreenManager().fullscreenElement();
            if (!topElement || topElement != followingDoc->ownerElement()) {
                // ...push following document's browsing context container on document's fullscreen element
                // stack, and queue a task to fire an event named fullscreenchange with its bubbles attribute
                // set to true on document.
                currentDoc->fullscreenManager().pushFullscreenElementStack(*followingDoc->ownerElement());
                addDocumentToFullscreenChangeEventQueue(*currentDoc);
                continue;
            }

            // 4. Otherwise, do nothing for this document. It stays the same.
        } while (++current != docs.end());

        // 5. Return, and run the remaining steps asynchronously.
        // 6. Optionally, perform some animation.
        m_areKeysEnabledInFullscreen = hasKeyboardAccess;
        m_fullscreenTaskQueue.enqueueTask([this, element = WTFMove(element), failedPreflights = WTFMove(failedPreflights)] () mutable {
            auto page = this->page();
            if (!page || document().hidden() || m_pendingFullscreenElement != element || !element->isConnected()) {
                failedPreflights(element);
                return;
            }
            page->chrome().client().enterFullScreenForElement(*element.get());
        });

        // 7. Optionally, display a message indicating how the user can exit displaying the context object fullscreen.
    });
}

void FullscreenManager::cancelFullscreen()
{
    // The Mozilla "cancelFullscreen()" API behaves like the W3C "fully exit fullscreen" behavior, which
    // is defined as:
    // "To fully exit fullscreen act as if the exitFullscreen() method was invoked on the top-level browsing
    // context's document and subsequently empty that document's fullscreen element stack."
    Document& topDocument = document().topDocument();
    if (!topDocument.fullscreenManager().fullscreenElement()) {
        // If there is a pending fullscreen element but no top document fullscreen element,
        // there is a pending task in enterFullscreen(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        m_pendingFullscreenElement = nullptr;
        return;
    }

    // To achieve that aim, remove all the elements from the top document's stack except for the first before
    // calling webkitExitFullscreen():
    Vector<RefPtr<Element>> replacementFullscreenElementStack;
    replacementFullscreenElementStack.append(topDocument.fullscreenManager().fullscreenElement());
    topDocument.fullscreenManager().m_fullscreenElementStack.swap(replacementFullscreenElementStack);

    topDocument.fullscreenManager().exitFullscreen();
}

void FullscreenManager::exitFullscreen()
{
    // The exitFullscreen() method must run these steps:

    // 1. Let doc be the context object. (i.e. "this")
    Document* currentDoc = &document();

    // 2. If doc's fullscreen element stack is empty, terminate these steps.
    if (m_fullscreenElementStack.isEmpty()) {
        // If there is a pending fullscreen element but an empty fullscreen element stack,
        // there is a pending task in requestFullscreenForElement(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        m_pendingFullscreenElement = nullptr;
        return;
    }

    // 3. Let descendants be all the doc's descendant browsing context's documents with a non-empty fullscreen
    // element stack (if any), ordered so that the child of the doc is last and the document furthest
    // away from the doc is first.
    Deque<RefPtr<Document>> descendants;
    for (Frame* descendant = frame() ? frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
        if (descendant->document()->fullscreenManager().fullscreenElement())
            descendants.prepend(descendant->document());
    }

    // 4. For each descendant in descendants, empty descendant's fullscreen element stack, and queue a
    // task to fire an event named fullscreenchange with its bubbles attribute set to true on descendant.
    for (auto& document : descendants) {
        document->fullscreenManager().clearFullscreenElementStack();
        addDocumentToFullscreenChangeEventQueue(*document);
    }

    // 5. While doc is not null, run these substeps:
    Element* newTop = nullptr;
    while (currentDoc) {
        // 1. Pop the top element of doc's fullscreen element stack.
        currentDoc->fullscreenManager().popFullscreenElementStack();

        //    If doc's fullscreen element stack is non-empty and the element now at the top is either
        //    not in a document or its node document is not doc, repeat this substep.
        newTop = currentDoc->fullscreenManager().fullscreenElement();
        if (newTop && (!newTop->isConnected() || &newTop->document() != currentDoc))
            continue;

        // 2. Queue a task to fire an event named fullscreenchange with its bubbles attribute set to true
        // on doc.
        addDocumentToFullscreenChangeEventQueue(*currentDoc);

        // 3. If doc's fullscreen element stack is empty and doc's browsing context has a browsing context
        // container, set doc to that browsing context container's node document.
        if (!newTop && currentDoc->ownerElement()) {
            currentDoc = &currentDoc->ownerElement()->document();
            continue;
        }

        // 4. Otherwise, set doc to null.
        currentDoc = nullptr;
    }

    // 6. Return, and run the remaining steps asynchronously.
    // 7. Optionally, perform some animation.
    m_fullscreenTaskQueue.enqueueTask([this, newTop = makeRefPtr(newTop), fullscreenElement = m_fullscreenElement] {
        auto* page = this->page();
        if (!page)
            return;

        // If there is a pending fullscreen element but no fullscreen element
        // there is a pending task in requestFullscreenForElement(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        if (!fullscreenElement && m_pendingFullscreenElement) {
            m_pendingFullscreenElement = nullptr;
            return;
        }

        // Only exit out of full screen window mode if there are no remaining elements in the
        // full screen stack.
        if (!newTop) {
            page->chrome().client().exitFullScreenForElement(fullscreenElement.get());
            return;
        }

        // Otherwise, notify the chrome of the new full screen element.
        page->chrome().client().enterFullScreenForElement(*newTop);
    });
}

bool FullscreenManager::isFullscreenEnabled() const
{
    // 4. The fullscreenEnabled attribute must return true if the context object and all ancestor
    // browsing context's documents have their fullscreen enabled flag set, or false otherwise.

    // Top-level browsing contexts are implied to have their allowFullscreen attribute set.
    return isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Fullscreen, document());
}

static void unwrapFullscreenRenderer(RenderFullScreen* fullscreenRenderer, Element* fullscreenElement)
{
    if (!fullscreenRenderer)
        return;
    bool requiresRenderTreeRebuild;
    fullscreenRenderer->unwrapRenderer(requiresRenderTreeRebuild);

    if (requiresRenderTreeRebuild && fullscreenElement && fullscreenElement->parentElement())
        fullscreenElement->parentElement()->invalidateStyleAndRenderersForSubtree();
}

void FullscreenManager::willEnterFullscreen(Element& element)
{
    if (!document().hasLivingRenderTree() || document().backForwardCacheState() != Document::NotInBackForwardCache)
        return;

    // Protect against being called after the document has been removed from the page.
    if (!page())
        return;

    // If pending fullscreen element is unset or another element's was requested,
    // issue a cancel fullscreen request to the client
    if (m_pendingFullscreenElement != &element) {
        page()->chrome().client().exitFullScreenForElement(&element);
        return;
    }

    ASSERT(page()->settings().fullScreenEnabled());

    unwrapFullscreenRenderer(m_fullscreenRenderer.get(), m_fullscreenElement.get());

    element.willBecomeFullscreenElement();

    ASSERT(&element == m_pendingFullscreenElement);
    m_pendingFullscreenElement = nullptr;
    m_fullscreenElement = &element;

    // Create a placeholder block for a the full-screen element, to keep the page from reflowing
    // when the element is removed from the normal flow. Only do this for a RenderBox, as only
    // a box will have a frameRect. The placeholder will be created in setFullscreenRenderer()
    // during layout.
    auto renderer = m_fullscreenElement->renderer();
    bool shouldCreatePlaceholder = is<RenderBox>(renderer);
    if (shouldCreatePlaceholder) {
        m_savedPlaceholderFrameRect = downcast<RenderBox>(*renderer).frameRect();
        m_savedPlaceholderRenderStyle = RenderStyle::clonePtr(renderer->style());
    }

    if (m_fullscreenElement != documentElement() && renderer)
        RenderFullScreen::wrapExistingRenderer(*renderer, document());

    m_fullscreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

    document().resolveStyle(Document::ResolveStyleType::Rebuild);
    dispatchFullscreenChangeEvents();
}

void FullscreenManager::didEnterFullscreen()
{
    if (!m_fullscreenElement)
        return;

    if (!hasLivingRenderTree() || backForwardCacheState() != Document::NotInBackForwardCache)
        return;

    m_fullscreenElement->didBecomeFullscreenElement();
}

void FullscreenManager::willExitFullscreen()
{
    auto fullscreenElement = fullscreenOrPendingElement();
    if (!fullscreenElement)
        return;

    if (!hasLivingRenderTree() || backForwardCacheState() != Document::NotInBackForwardCache)
        return;

    fullscreenElement->willStopBeingFullscreenElement();
}

void FullscreenManager::didExitFullscreen()
{
    auto fullscreenElement = fullscreenOrPendingElement();
    if (!fullscreenElement)
        return;

    if (!hasLivingRenderTree() || backForwardCacheState() != Document::NotInBackForwardCache)
        return;
    fullscreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);

    if (m_fullscreenElement)
        m_fullscreenElement->didStopBeingFullscreenElement();

    m_areKeysEnabledInFullscreen = false;

    unwrapFullscreenRenderer(m_fullscreenRenderer.get(), m_fullscreenElement.get());

    m_fullscreenElement = nullptr;
    m_pendingFullscreenElement = nullptr;
    scheduleFullStyleRebuild();

    // When webkitCancelFullscreen is called, we call webkitExitFullscreen on the topDocument(). That
    // means that the events will be queued there. So if we have no events here, start the timer on
    // the exiting document.
    bool eventTargetQueuesEmpty = m_fullscreenChangeEventTargetQueue.isEmpty() && m_fullscreenErrorEventTargetQueue.isEmpty();
    Document& exitingDocument = eventTargetQueuesEmpty ? topDocument() : document();

    exitingDocument.fullscreenManager().dispatchFullscreenChangeEvents();
}

void FullscreenManager::setFullscreenRenderer(RenderTreeBuilder& builder, RenderFullScreen& renderer)
{
    if (&renderer == m_fullscreenRenderer)
        return;

    if (m_savedPlaceholderRenderStyle)
        builder.createPlaceholderForFullScreen(renderer, WTFMove(m_savedPlaceholderRenderStyle), m_savedPlaceholderFrameRect);
    else if (m_fullscreenRenderer && m_fullscreenRenderer->placeholder()) {
        auto* placeholder = m_fullscreenRenderer->placeholder();
        builder.createPlaceholderForFullScreen(renderer, RenderStyle::clonePtr(placeholder->style()), placeholder->frameRect());
    }

    if (m_fullscreenRenderer)
        builder.destroy(*m_fullscreenRenderer);
    ASSERT(!m_fullscreenRenderer);

    m_fullscreenRenderer = makeWeakPtr(renderer);
}

RenderFullScreen* FullscreenManager::fullscreenRenderer() const
{
    return m_fullscreenRenderer.get();
}

void FullscreenManager::dispatchFullscreenChangeEvents()
{
    // Since we dispatch events in this function, it's possible that the
    // document will be detached and GC'd. We protect it here to make sure we
    // can finish the function successfully.
    Ref<Document> protectedDocument(document());
    Deque<RefPtr<Node>> changeQueue;
    m_fullscreenChangeEventTargetQueue.swap(changeQueue);
    Deque<RefPtr<Node>> errorQueue;
    m_fullscreenErrorEventTargetQueue.swap(errorQueue);
    dispatchFullscreenChangeOrErrorEvent(changeQueue, eventNames().webkitfullscreenchangeEvent, /* shouldNotifyMediaElement */ true);
    dispatchFullscreenChangeOrErrorEvent(errorQueue, eventNames().webkitfullscreenerrorEvent, /* shouldNotifyMediaElement */ false);
}

void FullscreenManager::dispatchFullscreenChangeOrErrorEvent(Deque<RefPtr<Node>>& queue, const AtomString& eventName, bool shouldNotifyMediaElement)
{
    // Step 3 of https://fullscreen.spec.whatwg.org/#run-the-fullscreen-steps
    while (!queue.isEmpty()) {
        RefPtr<Node> node = queue.takeFirst();
        if (!node)
            node = documentElement();
        // The dispatchEvent below may have blown away our documentElement.
        if (!node)
            continue;

        // If the element was removed from our tree, also message the documentElement. Since we may
        // have a document hierarchy, check that node isn't in another document.
        if (!node->isConnected())
            queue.append(documentElement());

#if ENABLE(VIDEO)
        if (shouldNotifyMediaElement && is<HTMLMediaElement>(*node))
            downcast<HTMLMediaElement>(*node).enteredOrExitedFullscreen();
#else
        UNUSED_PARAM(shouldNotifyMediaElement);
#endif
        node->dispatchEvent(Event::create(eventName, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
    }
}

void FullscreenManager::adjustFullscreenElementOnNodeRemoval(Node& node, Document::NodeRemoval nodeRemoval)
{
    auto fullscreenElement = fullscreenOrPendingElement();
    if (!fullscreenElement)
        return;

    bool elementInSubtree = false;
    if (nodeRemoval == Document::NodeRemoval::ChildrenOfNode)
        elementInSubtree = fullscreenElement->isDescendantOf(node);
    else
        elementInSubtree = (fullscreenElement == &node) || fullscreenElement->isDescendantOf(node);

    if (elementInSubtree) {
        fullscreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
        cancelFullscreen();
    }
}

bool FullscreenManager::isAnimatingFullscreen() const
{
    return m_isAnimatingFullscreen;
}

void FullscreenManager::setAnimatingFullscreen(bool flag)
{
    if (m_isAnimatingFullscreen == flag)
        return;
    m_isAnimatingFullscreen = flag;

    if (m_fullscreenElement && m_fullscreenElement->isDescendantOf(document())) {
        m_fullscreenElement->invalidateStyleForSubtree();
        scheduleFullStyleRebuild();
    }
}

bool FullscreenManager::areFullscreenControlsHidden() const
{
    return m_areFullscreenControlsHidden;
}

void FullscreenManager::setFullscreenControlsHidden(bool flag)
{
    if (m_areFullscreenControlsHidden == flag)
        return;
    m_areFullscreenControlsHidden = flag;

    if (m_fullscreenElement && m_fullscreenElement->isDescendantOf(document())) {
        m_fullscreenElement->invalidateStyleForSubtree();
        scheduleFullStyleRebuild();
    }
}

void FullscreenManager::clear()
{
    m_fullscreenElement = nullptr;
    m_pendingFullscreenElement = nullptr;
    m_fullscreenElementStack.clear();
}

void FullscreenManager::emptyEventQueue()
{
    m_fullscreenChangeEventTargetQueue.clear();
    m_fullscreenErrorEventTargetQueue.clear();
}

void FullscreenManager::clearFullscreenElementStack()
{
    m_fullscreenElementStack.clear();
}

void FullscreenManager::popFullscreenElementStack()
{
    if (m_fullscreenElementStack.isEmpty())
        return;

    m_fullscreenElementStack.removeLast();
}

void FullscreenManager::pushFullscreenElementStack(Element& element)
{
    m_fullscreenElementStack.append(&element);
}

void FullscreenManager::addDocumentToFullscreenChangeEventQueue(Document& document)
{
    Node* target = document.fullscreenManager().fullscreenElement();
    if (!target)
        target = document.fullscreenManager().currentFullscreenElement();
    if (!target)
        target = &document;
    m_fullscreenChangeEventTargetQueue.append(target);
}

}

#endif
