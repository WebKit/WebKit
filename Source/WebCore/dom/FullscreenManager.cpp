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
#include "DOMWindow.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLDialogElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLMediaElement.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "Page.h"
#include "PseudoClassChangeInvalidation.h"
#include "QualifiedName.h"
#include "Settings.h"
#include <wtf/LoggerHelper.h>

namespace WebCore {

using namespace HTMLNames;

FullscreenManager::FullscreenManager(Document& document)
    : m_document { document }
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
}

FullscreenManager::~FullscreenManager() = default;

// https://fullscreen.spec.whatwg.org/#dom-element-requestfullscreen
void FullscreenManager::requestFullscreenForElement(Ref<Element>&& element, RefPtr<DeferredPromise>&& promise, FullscreenCheckType checkType)
{
    // If pendingDoc is not fully active, then reject promise with a TypeError exception and return promise.
    if (promise && !document().isFullyActive()) {
        promise->reject(Exception { TypeError, "Document is not fully active"_s });
        return;
    }

    auto failedPreflights = [this, weakThis = WeakPtr { *this }](Ref<Element>&& element, RefPtr<DeferredPromise>&& promise) mutable {
        if (!weakThis)
            return;
        m_fullscreenErrorEventTargetQueue.append(WTFMove(element));
        if (promise)
            promise->reject(Exception { TypeError });
        m_document.eventLoop().queueTask(TaskSource::MediaElement, [weakThis = WTFMove(weakThis)]() mutable {
            if (weakThis)
                weakThis->dispatchFullscreenChangeEvents();
        });
    };

    // If any of the following conditions are true, terminate these steps and queue a task to fire
    // an event named fullscreenerror with its bubbles attribute set to true on the context object's
    // node document:
    if (is<HTMLDialogElement>(element)) {
        ERROR_LOG(LOGIDENTIFIER, "Element to fullscreen is a <dialog>; failing.");
        failedPreflights(WTFMove(element), WTFMove(promise));
        return;
    }

    if (!document().domWindow() || !document().domWindow()->hasTransientActivation()) {
        ERROR_LOG(LOGIDENTIFIER, "!hasTransientActivation; failing.");
        failedPreflights(WTFMove(element), WTFMove(promise));
        return;
    }

    // This algorithm is not allowed to show a pop-up:
    //   An algorithm is allowed to show a pop-up if, in the task in which the algorithm is running, either:
    //   - an activation behavior is currently being processed whose click event was trusted, or
    //   - the event listener for a trusted click event is being handled.
    // FIXME: Align prefixed and unprefixed code paths if possible.
    bool isFromPrefixedAPI = !promise;
    if (isFromPrefixedAPI) {
        if (!UserGestureIndicator::processingUserGesture()) {
            ERROR_LOG(LOGIDENTIFIER, "!processingUserGesture; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }
        // We do not allow pressing the Escape key as a user gesture to enter fullscreen since this is the key
        // to exit fullscreen.
        if (UserGestureIndicator::currentUserGesture()->gestureType() == UserGestureType::EscapeKey) {
            ERROR_LOG(LOGIDENTIFIER, "Current gesture is EscapeKey; failing.");
            document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, "The Escape key may not be used as a user gesture to enter fullscreen"_s);
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }
    }

    // There is a previously-established user preference, security risk, or platform limitation.
    if (!page() || !page()->settings().fullScreenEnabled()) {
        ERROR_LOG(LOGIDENTIFIER, "!page() or fullscreen not enabled; failing.");
        failedPreflights(WTFMove(element), WTFMove(promise));
        return;
    }

    bool hasKeyboardAccess = true;
    if (!page()->chrome().client().supportsFullScreenForElement(element, hasKeyboardAccess)) {
        // The new full screen API does not accept a "flags" parameter, so fall back to disallowing
        // keyboard input if the chrome client refuses to allow keyboard input.
        hasKeyboardAccess = false;

        if (!page()->chrome().client().supportsFullScreenForElement(element, hasKeyboardAccess)) {
            ERROR_LOG(LOGIDENTIFIER, "page does not support fullscreen for element; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }
    }

    INFO_LOG(LOGIDENTIFIER);

    m_pendingFullscreenElement = RefPtr { element.ptr() };

    m_document.eventLoop().queueTask(TaskSource::MediaElement, [this, weakThis = WeakPtr { *this }, element = WTFMove(element), promise = WTFMove(promise), checkType, hasKeyboardAccess, failedPreflights, identifier = LOGIDENTIFIER] () mutable {
        if (!weakThis) {
            if (promise)
                promise->reject(Exception { TypeError });
            return;
        }

        // Don't allow fullscreen if it has been cancelled or a different fullscreen element
        // has requested fullscreen.
        if (m_pendingFullscreenElement != element.ptr()) {
            ERROR_LOG(identifier, "task - pending element mismatch; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }

        // Don't allow fullscreen if we're inside an exitFullscreen operation.
        if (m_pendingExitFullscreen) {
            ERROR_LOG(identifier, "task - pending exit fullscreen operation; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }

        // Don't allow fullscreen if document is hidden.
        if (document().hidden()) {
            ERROR_LOG(identifier, "task - document hidden; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }

        // The context object is not in a document.
        if (!element->isConnected()) {
            ERROR_LOG(identifier, "task - element not in document; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }

        // The context object's node document, or an ancestor browsing context's document does not have
        // the fullscreen enabled flag set.
        if (checkType == EnforceIFrameAllowFullscreenRequirement && !isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Fullscreen, document())) {
            ERROR_LOG(identifier, "task - ancestor document does not enable fullscreen; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }

        // The context object's node document fullscreen element stack is not empty and its top element
        // is not an ancestor of the context object.
        if (!m_fullscreenElementStack.isEmpty() && !m_fullscreenElementStack.last()->contains(element.get())) {
            ERROR_LOG(identifier, "task - fullscreen stack not empty; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
            return;
        }

        // A descendant browsing context's document has a non-empty fullscreen element stack.
        bool descendentHasNonEmptyStack = false;
        for (AbstractFrame* descendant = frame() ? frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(descendant);
            if (!localFrame)
                continue;
            if (localFrame->document()->fullscreenManager().fullscreenElement()) {
                descendentHasNonEmptyStack = true;
                break;
            }
        }
        if (descendentHasNonEmptyStack) {
            ERROR_LOG(identifier, "task - descendent document has non-empty fullscreen stack; failing.");
            failedPreflights(WTFMove(element), WTFMove(promise));
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
                currentDoc->fullscreenManager().pushFullscreenElementStack(element);
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
        m_document.eventLoop().queueTask(TaskSource::MediaElement, [this, weakThis = WTFMove(weakThis), promise = WTFMove(promise), element = WTFMove(element), failedPreflights = WTFMove(failedPreflights), identifier] () mutable {
            if (!weakThis) {
                if (promise)
                    promise->reject(Exception { TypeError });
                return;
            }

            auto page = this->page();
            if (!page || document().hidden() || m_pendingFullscreenElement != element.ptr() || !element->isConnected()) {
                ERROR_LOG(identifier, "task - page, document, or element mismatch; failing.");
                failedPreflights(WTFMove(element), WTFMove(promise));
                return;
            }
            INFO_LOG(identifier, "task - success");
            page->chrome().client().enterFullScreenForElement(element);
            if (promise)
                promise->resolve();
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
        INFO_LOG(LOGIDENTIFIER, "Cancelling pending fullscreen request.");
        return;
    }

    INFO_LOG(LOGIDENTIFIER);

    // To achieve that aim, remove all the elements from the top document's stack except for the first before
    // calling webkitExitFullscreen():
    RefPtr fullscrenElement = topDocument.fullscreenManager().fullscreenElement();
    topDocument.fullscreenManager().m_fullscreenElementStack = { WTFMove(fullscrenElement) };

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
        INFO_LOG(LOGIDENTIFIER, "Cancelling pending fullscreen request.");
        m_pendingFullscreenElement = nullptr;
        return;
    }

    INFO_LOG(LOGIDENTIFIER);

    // 3. Let descendants be all the doc's descendant browsing context's documents with a non-empty fullscreen
    // element stack (if any), ordered so that the child of the doc is last and the document furthest
    // away from the doc is first.
    Deque<RefPtr<Document>> descendants;
    for (AbstractFrame* descendant = frame() ? frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(descendant);
        if (!localFrame)
            continue;
        if (localFrame->document()->fullscreenManager().fullscreenElement())
            descendants.prepend(localFrame->document());
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

    m_pendingExitFullscreen = true;

    // 6. Return, and run the remaining steps asynchronously.
    // 7. Optionally, perform some animation.
    m_document.eventLoop().queueTask(TaskSource::MediaElement, [this, weakThis = WeakPtr { *this }, newTop = RefPtr { newTop }, fullscreenElement = m_fullscreenElement, identifier = LOGIDENTIFIER] {
        if (!weakThis)
            return;

        auto* page = this->page();
        if (!page) {
            m_pendingExitFullscreen = false;
            ERROR_LOG(identifier, "task - Document not in page; bailing.");
            return;
        }

        // If there is a pending fullscreen element but no fullscreen element
        // there is a pending task in requestFullscreenForElement(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        if (!fullscreenElement && m_pendingFullscreenElement) {
            INFO_LOG(identifier, "task - Cancelling pending fullscreen request.");
            m_pendingFullscreenElement = nullptr;
            m_pendingExitFullscreen = false;
            return;
        }

        // Only exit out of full screen window mode if there are no remaining elements in the
        // full screen stack.
        if (!newTop) {
            INFO_LOG(identifier, "task - Empty fullscreen stack; exiting.");
            page->chrome().client().exitFullScreenForElement(fullscreenElement.get());
            return;
        }

        // Otherwise, notify the chrome of the new full screen element.
        m_pendingExitFullscreen = false;

        INFO_LOG(identifier, "task - New top of fullscreen stack.");
        m_pendingFullscreenElement = newTop;
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

bool FullscreenManager::willEnterFullscreen(Element& element)
{
    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return false;
    }

    // Protect against being called after the document has been removed from the page.
    if (!page()) {
        ERROR_LOG(LOGIDENTIFIER, "Document no longer in page; bailing");
        return false;
    }

    // If pending fullscreen element is unset or another element's was requested,
    // issue a cancel fullscreen request to the client
    if (m_pendingFullscreenElement != &element) {
        INFO_LOG(LOGIDENTIFIER, "Pending element mismatch; issuing exit fullscreen request");
        page()->chrome().client().exitFullScreenForElement(&element);
        return false;
    }

    INFO_LOG(LOGIDENTIFIER);
    ASSERT(page()->settings().fullScreenEnabled());

    element.willBecomeFullscreenElement();

    ASSERT(&element == m_pendingFullscreenElement);
    m_pendingFullscreenElement = nullptr;
    m_fullscreenElement = &element;

    m_fullscreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

    document().resolveStyle(Document::ResolveStyleType::Rebuild);
    dispatchFullscreenChangeEvents();

    return true;
}

bool FullscreenManager::didEnterFullscreen()
{
    if (!m_fullscreenElement) {
        ERROR_LOG(LOGIDENTIFIER, "No fullscreenElement; bailing");
        return false;
    }

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return false;
    }
    INFO_LOG(LOGIDENTIFIER);

    m_fullscreenElement->didBecomeFullscreenElement();
    return true;
}

bool FullscreenManager::willExitFullscreen()
{
    auto fullscreenElement = fullscreenOrPendingElement();
    if (!fullscreenElement) {
        ERROR_LOG(LOGIDENTIFIER, "No fullscreenOrPendingElement(); bailing");
        return false;
    }

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return false;
    }
    INFO_LOG(LOGIDENTIFIER);

    fullscreenElement->willStopBeingFullscreenElement();
    return true;
}

bool FullscreenManager::didExitFullscreen()
{
    m_pendingExitFullscreen = false;
    auto fullscreenElement = fullscreenOrPendingElement();
    if (!fullscreenElement) {
        ERROR_LOG(LOGIDENTIFIER, "No fullscreenOrPendingElement(); bailing");
        return false;
    }

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return false;
    }
    INFO_LOG(LOGIDENTIFIER);

    fullscreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);

    if (m_fullscreenElement)
        m_fullscreenElement->didStopBeingFullscreenElement();

    m_areKeysEnabledInFullscreen = false;

    m_fullscreenElement = nullptr;
    m_pendingFullscreenElement = nullptr;
    document().scheduleFullStyleRebuild();

    // When webkitCancelFullscreen is called, we call webkitExitFullscreen on the topDocument(). That
    // means that the events will be queued there. So if we have no events here, start the timer on
    // the exiting document.
    bool eventTargetQueuesEmpty = m_fullscreenChangeEventTargetQueue.isEmpty() && m_fullscreenErrorEventTargetQueue.isEmpty();
    Document& exitingDocument = eventTargetQueuesEmpty ? topDocument() : document();

    exitingDocument.fullscreenManager().dispatchFullscreenChangeEvents();
    return true;
}

void FullscreenManager::dispatchFullscreenChangeEvents()
{
    // Since we dispatch events in this function, it's possible that the
    // document will be detached and GC'd. We protect it here to make sure we
    // can finish the function successfully.
    Ref<Document> protectedDocument(document());
    Deque<GCReachableRef<Node>> changeQueue;
    m_fullscreenChangeEventTargetQueue.swap(changeQueue);
    Deque<GCReachableRef<Node>> errorQueue;
    m_fullscreenErrorEventTargetQueue.swap(errorQueue);
    dispatchFullscreenChangeOrErrorEvent(changeQueue, EventType::Change, /* shouldNotifyMediaElement */ true);
    dispatchFullscreenChangeOrErrorEvent(errorQueue, EventType::Error, /* shouldNotifyMediaElement */ false);
}

void FullscreenManager::dispatchEventForNode(Node& node, EventType eventType)
{
    bool supportsUnprefixedAPI = document().settings().unprefixedFullscreenAPIEnabled();
    switch (eventType) {
    case EventType::Change:
        if (supportsUnprefixedAPI)
            node.dispatchEvent(Event::create(eventNames().fullscreenchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
        node.dispatchEvent(Event::create(eventNames().webkitfullscreenchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
        break;
    case EventType::Error:
        if (supportsUnprefixedAPI)
            node.dispatchEvent(Event::create(eventNames().fullscreenerrorEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
        node.dispatchEvent(Event::create(eventNames().webkitfullscreenerrorEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
        break;
    }
}

void FullscreenManager::dispatchFullscreenChangeOrErrorEvent(Deque<GCReachableRef<Node>>& queue, EventType eventType, bool shouldNotifyMediaElement)
{
    // Step 3 of https://fullscreen.spec.whatwg.org/#run-the-fullscreen-steps
    while (!queue.isEmpty()) {
        auto node = queue.takeFirst();

        // If the element was removed from our tree, also message the documentElement. Since we may
        // have a document hierarchy, check that node isn't in another document.
        if (!node->isConnected()) {
            if (auto* element = documentElement())
                queue.append(*element);
        }

#if ENABLE(VIDEO)
        if (shouldNotifyMediaElement && is<HTMLMediaElement>(node.get()))
            downcast<HTMLMediaElement>(node.get()).enteredOrExitedFullscreen();
#else
        UNUSED_PARAM(shouldNotifyMediaElement);
#endif
        dispatchEventForNode(node.get(), eventType);
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
        INFO_LOG(LOGIDENTIFIER, "Ancestor of fullscreen element removed; exiting fullscreen");
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

    INFO_LOG(LOGIDENTIFIER, flag);

    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (m_fullscreenElement)
        emplace(styleInvalidation, *m_fullscreenElement, { { CSSSelector::PseudoClassAnimatingFullScreenTransition, flag } });
    m_isAnimatingFullscreen = flag;
}

bool FullscreenManager::areFullscreenControlsHidden() const
{
    return m_areFullscreenControlsHidden;
}

void FullscreenManager::setFullscreenControlsHidden(bool flag)
{
    if (m_areFullscreenControlsHidden == flag)
        return;

    INFO_LOG(LOGIDENTIFIER, flag);

    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (m_fullscreenElement)
        emplace(styleInvalidation, *m_fullscreenElement, { { CSSSelector::PseudoClassFullScreenControlsHidden, flag } });
    m_areFullscreenControlsHidden = flag;
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
    m_fullscreenChangeEventTargetQueue.append(GCReachableRef(*target));
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& FullscreenManager::logChannel() const
{
    return LogFullscreen;

}
#endif

}

#endif
