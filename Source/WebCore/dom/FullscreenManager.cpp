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

Element* FullscreenManager::fullscreenElement() const
{
    for (auto& element : makeReversedRange(document().topLayerElements())) {
        if (element->hasFullscreenFlag())
            return element.ptr();
    }

    return nullptr;
}

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
                weakThis->notifyAboutFullscreenChangeOrError();
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

    if (!document().domWindow() || !document().domWindow()->consumeTransientActivation()) {
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
        if (auto* currentFullscreenElement = fullscreenElement(); currentFullscreenElement && !currentFullscreenElement->contains(element.get())) {
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
            if (m_pendingPromise)
                m_pendingPromise->reject(Exception { TypeError, "Pending operation cancelled by requestFullscreen() call."_s });

            m_pendingPromise = WTFMove(promise);

            INFO_LOG(identifier, "task - success");
            page->chrome().client().enterFullScreenForElement(element);
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
        if (m_pendingPromise) {
            m_pendingPromise->reject(Exception { TypeError, "Pending operation cancelled by webkitCancelFullScreen() call."_s });
            m_pendingPromise = nullptr;
        }
        INFO_LOG(LOGIDENTIFIER, "Cancelling pending fullscreen request.");
        return;
    }

    INFO_LOG(LOGIDENTIFIER);

    m_pendingExitFullscreen = true;

    m_document.eventLoop().queueTask(TaskSource::MediaElement, [this, &topDocument, identifier = LOGIDENTIFIER] {
        if (!topDocument.page()) {
            INFO_LOG(LOGIDENTIFIER, "Top document has no page.");
            return;
        }

        // This triggers finishExitFullscreen with ExitMode::Resize, which fully exits the document.
        if (auto* fullscreenElement = topDocument.fullscreenManager().fullscreenElement())
            page()->chrome().client().exitFullScreenForElement(fullscreenElement);
        else
            INFO_LOG(LOGIDENTIFIER, "Top document has no fullscreen element");
    });
}

// https://fullscreen.spec.whatwg.org/#collect-documents-to-unfullscreen
static Vector<Ref<Document>> documentsToUnfullscreen(Document& firstDocument)
{
    Vector<Ref<Document>> documents { Ref { firstDocument } };
    while (true) {
        auto lastDocument = documents.last();
        ASSERT(lastDocument->fullscreenManager().fullscreenElement());
        if (!lastDocument->fullscreenManager().isSimpleFullscreenDocument())
            break;
        auto frame = lastDocument->frame();
        if (!frame)
            break;
        auto frameOwner = frame->ownerElement();
        if (!frameOwner || frameOwner->hasIFrameFullscreenFlag())
            break;
        documents.append(frameOwner->document());
    }
    return documents;
}

void FullscreenManager::exitFullscreen(RefPtr<DeferredPromise>&& promise)
{
    INFO_LOG(LOGIDENTIFIER);

    auto* exitingDocument = &document();
    auto mode = ExitMode::NoResize;
    auto exitDocuments = documentsToUnfullscreen(*exitingDocument);
    auto& topDocument = this->topDocument();

    bool exitsTopDocument = exitDocuments.containsIf([&](auto& document) {
        return document.ptr() == &topDocument;
    });
    if (exitsTopDocument && topDocument.fullscreenManager().isSimpleFullscreenDocument()) {
        mode = ExitMode::Resize;
        exitingDocument = &topDocument;
    }

    auto element = exitingDocument->fullscreenManager().fullscreenElement();
    if (element && !element->isConnected())
        addDocumentToFullscreenChangeEventQueue(*exitingDocument);

    m_pendingExitFullscreen = true;

    // Return promise, and run the remaining steps in parallel.
    m_document.eventLoop().queueTask(TaskSource::MediaElement, [this, promise = WTFMove(promise), weakThis = WeakPtr { *this }, mode, identifier = LOGIDENTIFIER] () mutable {
        if (!weakThis) {
            if (promise)
                promise->resolve();
            return;
        }

        auto* page = this->page();
        if (!page) {
            m_pendingExitFullscreen = false;
            if (promise)
                promise->resolve();
            ERROR_LOG(identifier, "task - Document not in page; bailing.");
            return;
        }

        // If there is a pending fullscreen element but no fullscreen element
        // there is a pending task in requestFullscreenForElement(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        if (!m_fullscreenElement && m_pendingFullscreenElement) {
            INFO_LOG(identifier, "task - Cancelling pending fullscreen request.");
            m_pendingFullscreenElement = nullptr;
            m_pendingExitFullscreen = false;
            if (promise)
                promise->resolve();
            return;
        }

        if (m_pendingPromise)
            m_pendingPromise->reject(Exception { TypeError, "Pending operation cancelled by exitFullscreen() call."_s });

        m_pendingPromise = WTFMove(promise);

        // Notify the chrome of the new full screen element.
        if (mode == ExitMode::Resize)
            page->chrome().client().exitFullScreenForElement(m_fullscreenElement.get());
        else {
            finishExitFullscreen(document(), ExitMode::NoResize);

            INFO_LOG(identifier, "task - New fullscreen element.");
            m_pendingFullscreenElement = fullscreenElement();
            page->chrome().client().enterFullScreenForElement(*m_pendingFullscreenElement);
        }
    });
}

void FullscreenManager::finishExitFullscreen(Document& currentDocument, ExitMode mode)
{
    if (!currentDocument.fullscreenManager().fullscreenElement())
        return;

    // Let descendantDocs be an ordered set consisting of doc’s descendant browsing contexts' active documents whose fullscreen element is non-null, if any, in tree order.
    Deque<Ref<Document>> descendantDocuments;
    for (AbstractFrame* descendant = currentDocument.frame() ? currentDocument.frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(descendant);
        if (!localFrame || !localFrame->document())
            continue;
        if (localFrame->document()->fullscreenManager().fullscreenElement())
            descendantDocuments.prepend(*localFrame->document());
    }

    auto unfullscreenDocument = [](const Ref<Document>& document) {
        Vector<Ref<Element>> toRemove;
        for (auto& element : document->topLayerElements()) {
            if (!element->hasFullscreenFlag())
                continue;
            element->setFullscreenFlag(false);
            toRemove.append(element);
        }
        for (auto& element : toRemove)
            element->removeFromTopLayer();
    };

    auto exitDocuments = documentsToUnfullscreen(currentDocument);
    for (auto& exitDocument : exitDocuments) {
        addDocumentToFullscreenChangeEventQueue(exitDocument);
        if (mode == ExitMode::Resize)
            unfullscreenDocument(exitDocument);
        else {
            auto fullscreenElement = exitDocument->fullscreenManager().fullscreenElement();
            fullscreenElement->setFullscreenFlag(false);
            fullscreenElement->removeFromTopLayer();
        }
    }

    for (auto& descendantDocument : descendantDocuments) {
        addDocumentToFullscreenChangeEventQueue(descendantDocument);
        unfullscreenDocument(descendantDocument);
    }
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

    Deque<RefPtr<Element>> ancestorsInTreeOrder;
    RefPtr ancestor = &element;
    do {
        ancestorsInTreeOrder.prepend(ancestor);
    } while ((ancestor = ancestor->document().ownerElement()));

    for (auto ancestor : ancestorsInTreeOrder) {
        ancestor->setFullscreenFlag(true);

        if (ancestor == &element)
            document().resolveStyle(Document::ResolveStyleType::Rebuild);

        // Remove before adding, so we always add at the end of the top layer.
        if (ancestor->isInTopLayer())
            ancestor->removeFromTopLayer();
        ancestor->addToTopLayer();

        addDocumentToFullscreenChangeEventQueue(ancestor->document());
    }

    if (is<HTMLIFrameElement>(element))
        element.setIFrameFullscreenFlag(true);

    notifyAboutFullscreenChangeOrError();

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
    auto fullscreenElement = fullscreenOrPendingElement();
    if (!fullscreenElement) {
        ERROR_LOG(LOGIDENTIFIER, "No fullscreenOrPendingElement(); bailing");
        m_pendingExitFullscreen = false;
        return false;
    }

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        m_pendingExitFullscreen = false;
        return false;
    }
    INFO_LOG(LOGIDENTIFIER);

    finishExitFullscreen(topDocument(), ExitMode::Resize);

    if (m_fullscreenElement)
        m_fullscreenElement->didStopBeingFullscreenElement();

    m_areKeysEnabledInFullscreen = false;

    m_fullscreenElement = nullptr;
    m_pendingFullscreenElement = nullptr;
    m_pendingExitFullscreen = false;

    document().scheduleFullStyleRebuild();

    notifyAboutFullscreenChangeOrError();
    return true;
}

void FullscreenManager::notifyAboutFullscreenChangeOrError()
{
    // Since we dispatch events in this function, it's possible that the
    // document will be detached and GC'd. We protect it here to make sure we
    // can finish the function successfully.
    Ref<Document> protectedDocument(document());
    Deque<GCReachableRef<Node>> changeQueue;
    m_fullscreenChangeEventTargetQueue.swap(changeQueue);
    Deque<GCReachableRef<Node>> errorQueue;
    m_fullscreenErrorEventTargetQueue.swap(errorQueue);

    if (m_pendingPromise) {
        ASSERT(!errorQueue.isEmpty() || !changeQueue.isEmpty());
        if (!errorQueue.isEmpty())
            m_pendingPromise->reject(Exception { TypeError });
        else
            m_pendingPromise->resolve();
        m_pendingPromise = nullptr;
    }

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

void FullscreenManager::exitRemovedFullscreenElementIfNeeded(Element& element)
{
    if (!element.hasFullscreenFlag())
        return;

    auto fullscreenElement = fullscreenOrPendingElement();
    if (fullscreenElement == &element) {
        INFO_LOG(LOGIDENTIFIER, "Fullscreen element removed; exiting fullscreen");
        exitFullscreen(nullptr);
    } else
        element.setFullscreenFlag(false);
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
    m_pendingPromise = nullptr;
}

void FullscreenManager::emptyEventQueue()
{
    m_fullscreenChangeEventTargetQueue.clear();
    m_fullscreenErrorEventTargetQueue.clear();
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

bool FullscreenManager::isSimpleFullscreenDocument() const
{
    bool foundFullscreenFlag = false;
    for (auto& element : document().topLayerElements()) {
        if (element->hasFullscreenFlag()) {
            if (foundFullscreenFlag)
                return false;
            foundFullscreenFlag = true;
        }
    }
    return foundFullscreenFlag;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& FullscreenManager::logChannel() const
{
    return LogFullscreen;

}
#endif

}

#endif
