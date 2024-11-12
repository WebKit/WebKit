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
#include "Document.h"
#include "DocumentInlines.h"
#include "Element.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "HTMLDialogElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLMediaElement.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "Page.h"
#include "PseudoClassChangeInvalidation.h"
#include "QualifiedName.h"
#include "Quirks.h"
#include "RenderBlock.h"
#include "Settings.h"
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FullscreenManager);

using namespace HTMLNames;

FullscreenManager::FullscreenManager(Document& document)
    : m_document(document)
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
}

FullscreenManager::~FullscreenManager() = default;

Element* FullscreenManager::fullscreenElement() const
{
    for (Ref element : makeReversedRange(document().topLayerElements())) {
        if (element->hasFullscreenFlag())
            return element.ptr();
    }

    return nullptr;
}

Ref<Document> FullscreenManager::protectedTopDocument()
{
    return topDocument();
}

// https://fullscreen.spec.whatwg.org/#dom-element-requestfullscreen
void FullscreenManager::requestFullscreenForElement(Ref<Element>&& element, RefPtr<DeferredPromise>&& promise, FullscreenCheckType checkType, CompletionHandler<void(bool)>&& completionHandler, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    auto identifier = LOGIDENTIFIER;

    enum class EmitErrorEvent : bool { No, Yes };
    auto handleError = [this, identifier, weakThis = WeakPtr { *this }](ASCIILiteral message, EmitErrorEvent emitErrorEvent, Ref<Element>&& element, RefPtr<DeferredPromise>&& promise, CompletionHandler<void(bool)>&& completionHandler) mutable {
        if (!weakThis)
            return;
        ERROR_LOG(identifier, message);
        if (promise)
            promise->reject(Exception { ExceptionCode::TypeError, message });
        if (emitErrorEvent == EmitErrorEvent::Yes) {
            m_fullscreenErrorEventTargetQueue.append(WTFMove(element));
            protectedDocument()->eventLoop().queueTask(TaskSource::MediaElement, [weakThis = WTFMove(weakThis)]() mutable {
                if (weakThis)
                    weakThis->notifyAboutFullscreenChangeOrError();
            });
        }
        completionHandler(false);
    };

    // If pendingDoc is not fully active, then reject promise with a TypeError exception and return promise.
    if (promise && !document().isFullyActive()) {
        handleError("Cannot request fullscreen on a document that is not fully active."_s, EmitErrorEvent::No, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
        return;
    }

    // If any of the following conditions are true, terminate these steps and queue a task to fire
    // an event named fullscreenerror with its bubbles attribute set to true on the context object's
    // node document:
    if (is<HTMLDialogElement>(element)) {
        handleError("Cannot request fullscreen on a <dialog> element."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
        return;
    }

    if (element->isPopoverShowing()) {
        handleError("Cannot request fullscreen on an open popover."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
        return;
    }

    if (!document().domWindow() || !document().domWindow()->consumeTransientActivation()) {
        handleError("Cannot request fullscreen without transient activation."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
        return;
    }

    if (UserGestureIndicator::processingUserGesture() && UserGestureIndicator::currentUserGesture()->gestureType() == UserGestureType::EscapeKey) {
        handleError("Cannot request fullscreen with Escape key as current gesture."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
        return;
    }

    // There is a previously-established user preference, security risk, or platform limitation.
    if (!page() || !page()->isFullscreenManagerEnabled()) {
        handleError("Fullscreen API is disabled."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
        return;
    }

    bool hasKeyboardAccess = true;
    if (!page()->chrome().client().supportsFullScreenForElement(element, hasKeyboardAccess)) {
        // The new full screen API does not accept a "flags" parameter, so fall back to disallowing
        // keyboard input if the chrome client refuses to allow keyboard input.
        hasKeyboardAccess = false;

        if (!page()->chrome().client().supportsFullScreenForElement(element, hasKeyboardAccess)) {
            handleError("Cannot request fullscreen with unsupported element."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }
    }

    INFO_LOG(identifier);

    m_pendingFullscreenElement = RefPtr { element.ptr() };

    // We cache the top document here, so we still have the correct one when we exit fullscreen after navigation.
    m_topDocument = document().topDocument();

    protectedDocument()->eventLoop().queueTask(TaskSource::MediaElement, [this, weakThis = WeakPtr { *this }, element = WTFMove(element), promise = WTFMove(promise), completionHandler = WTFMove(completionHandler), checkType, hasKeyboardAccess, handleError, identifier, mode] () mutable {
        if (!weakThis) {
            if (promise)
                promise->reject(Exception { ExceptionCode::TypeError });
            completionHandler(false);
            return;
        }

        // Don't allow fullscreen if it has been cancelled or a different fullscreen element
        // has requested fullscreen.
        if (m_pendingFullscreenElement != element.ptr()) {
            handleError("Fullscreen request aborted by a fullscreen request for another element."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }

        // Don't allow fullscreen if we're inside an exitFullscreen operation.
        if (m_pendingExitFullscreen) {
            handleError("Fullscreen request aborted by a request to exit fullscreen."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }

        // Don't allow fullscreen if document is hidden.
        auto document = protectedDocument();
        if (document->hidden() && mode != HTMLMediaElementEnums::VideoFullscreenModeInWindow) {
            handleError("Cannot request fullscreen in a hidden document."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }

        // The context object is not in a document.
        if (!element->isConnected()) {
            handleError("Cannot request fullscreen on a disconnected element."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }

        // The element is an open popover.
        if (element->isPopoverShowing()) {
            handleError("Cannot request fullscreen on an open popover."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }

        // The context object's node document, or an ancestor browsing context's document does not have
        // the fullscreen enabled flag set.
        if (checkType == EnforceIFrameAllowFullscreenRequirement && !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Fullscreen, document)) {
            handleError("Fullscreen API is disabled by permissions policy."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }

        // A descendant browsing context's document has a non-empty fullscreen element stack.
        bool descendantHasNonEmptyStack = false;
        for (RefPtr descendant = frame() ? frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(descendant.get());
            if (!localFrame)
                continue;
            if (localFrame->document()->fullscreenManager().fullscreenElement()) {
                descendantHasNonEmptyStack = true;
                break;
            }
        }
        if (descendantHasNonEmptyStack) {
            handleError("Cannot request fullscreen because a descendant document already has a fullscreen element."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
            return;
        }

        // 5. Return, and run the remaining steps asynchronously.
        // 6. Optionally, perform some animation.
        m_areKeysEnabledInFullscreen = hasKeyboardAccess;
        document->eventLoop().queueTask(TaskSource::MediaElement, [this, weakThis = WTFMove(weakThis), promise = WTFMove(promise), element = WTFMove(element), completionHandler = WTFMove(completionHandler), handleError = WTFMove(handleError), identifier, mode] () mutable {
            if (!weakThis) {
                if (promise)
                    promise->reject(Exception { ExceptionCode::TypeError });
                completionHandler(false);
                return;
            }

            RefPtr page = this->page();
            if (!page || (this->document().hidden() && mode != HTMLMediaElementEnums::VideoFullscreenModeInWindow) || m_pendingFullscreenElement != element.ptr() || !element->isConnected()) {
                handleError("Invalid state when requesting fullscreen."_s, EmitErrorEvent::Yes, WTFMove(element), WTFMove(promise), WTFMove(completionHandler));
                return;
            }

            // Reject previous promise, but continue with current operation.
            if (m_pendingPromise) {
                ERROR_LOG(identifier, "Pending operation cancelled by requestFullscreen() call.");
                m_pendingPromise->reject(Exception { ExceptionCode::TypeError, "Pending operation cancelled by requestFullscreen() call."_s });
            }

            m_pendingPromise = WTFMove(promise);

            INFO_LOG(identifier, "task - success");

            page->chrome().client().enterFullScreenForElement(element, mode);
            completionHandler(true);
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
    Ref topDocument = this->topDocument();
    if (!topDocument->fullscreenManager().fullscreenElement()) {
        // If there is a pending fullscreen element but no top document fullscreen element,
        // there is a pending task in enterFullscreen(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        m_pendingFullscreenElement = nullptr;
        if (m_pendingPromise) {
            m_pendingPromise->reject(Exception { ExceptionCode::TypeError, "Pending operation cancelled by webkitCancelFullScreen() call."_s });
            m_pendingPromise = nullptr;
        }
        INFO_LOG(LOGIDENTIFIER, "Cancelling pending fullscreen request.");
        return;
    }

    INFO_LOG(LOGIDENTIFIER);

    m_pendingExitFullscreen = true;

    protectedDocument()->eventLoop().queueTask(TaskSource::MediaElement, [this, weakThis = WeakPtr { *this }, topDocument = WTFMove(topDocument), identifier = LOGIDENTIFIER] {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(this);
#endif
        if (!weakThis)
            return;

        if (!topDocument->page()) {
            INFO_LOG(identifier, "Top document has no page.");
            return;
        }

        // This triggers finishExitFullscreen with ExitMode::Resize, which fully exits the document.
        if (RefPtr fullscreenElement = topDocument->fullscreenManager().fullscreenElement())
            topDocument->page()->chrome().client().exitFullScreenForElement(fullscreenElement.get());
        else
            INFO_LOG(identifier, "Top document has no fullscreen element");
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
        if (!frameOwner)
            break;
        if (auto* iframe = dynamicDowncast<HTMLIFrameElement>(frameOwner); iframe && iframe->hasIFrameFullscreenFlag())
            break;
        documents.append(frameOwner->document());
    }
    return documents;
}

static void clearFullscreenFlags(Element& element)
{
    element.setFullscreenFlag(false);
    if (auto* iframe = dynamicDowncast<HTMLIFrameElement>(element))
        iframe->setIFrameFullscreenFlag(false);
}

void FullscreenManager::exitFullscreen(RefPtr<DeferredPromise>&& promise)
{
    INFO_LOG(LOGIDENTIFIER);

    Ref exitingDocument = document();
    auto mode = ExitMode::NoResize;
    auto exitDocuments = documentsToUnfullscreen(exitingDocument);
    Ref topDocument = this->topDocument();

    bool exitsTopDocument = exitDocuments.containsIf([&](auto& document) {
        return document.ptr() == topDocument.ptr();
    });
    if (exitsTopDocument && topDocument->fullscreenManager().isSimpleFullscreenDocument()) {
        mode = ExitMode::Resize;
        exitingDocument = topDocument;
    }

    if (RefPtr element = exitingDocument->fullscreenManager().fullscreenElement(); element && !element->isConnected()) {
        addDocumentToFullscreenChangeEventQueue(exitingDocument);
        clearFullscreenFlags(*element);
        element->removeFromTopLayer();
    }

    m_pendingExitFullscreen = true;

    // Return promise, and run the remaining steps in parallel.
    exitingDocument->eventLoop().queueTask(TaskSource::MediaElement, [this, promise = WTFMove(promise), weakThis = WeakPtr { *this }, mode, identifier = LOGIDENTIFIER] () mutable {
        if (!weakThis) {
            if (promise)
                promise->resolve();
            return;
        }

        RefPtr page = this->page();
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
            m_pendingPromise->reject(Exception { ExceptionCode::TypeError, "Pending operation cancelled by exitFullscreen() call."_s });

        m_pendingPromise = WTFMove(promise);

        // Notify the chrome of the new full screen element.
        if (mode == ExitMode::Resize)
            page->chrome().client().exitFullScreenForElement(m_fullscreenElement.get());
        else {
            finishExitFullscreen(protectedDocument(), ExitMode::NoResize);

            m_pendingFullscreenElement = fullscreenElement();
            if (m_pendingFullscreenElement)
                page->chrome().client().enterFullScreenForElement(*m_pendingFullscreenElement);
            else if (m_pendingPromise) {
                m_pendingPromise->resolve();
                m_pendingPromise = nullptr;
            }
        }
    });
}

void FullscreenManager::finishExitFullscreen(Document& currentDocument, ExitMode mode)
{
    if (!currentDocument.fullscreenManager().fullscreenElement())
        return;

    // Let descendantDocs be an ordered set consisting of doc’s descendant browsing contexts' active documents whose fullscreen element is non-null, if any, in tree order.
    Deque<Ref<Document>> descendantDocuments;
    for (RefPtr descendant = currentDocument.frame() ? currentDocument.frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(descendant);
        if (!localFrame || !localFrame->document())
            continue;
        if (localFrame->document()->fullscreenManager().fullscreenElement())
            descendantDocuments.prepend(*localFrame->document());
    }

    auto unfullscreenDocument = [](const Ref<Document>& document) {
        Vector<Ref<Element>> toRemove;
        for (Ref element : document->topLayerElements()) {
            if (!element->hasFullscreenFlag())
                continue;
            clearFullscreenFlags(element);
            toRemove.append(element);
        }
        for (Ref element : toRemove)
            element->removeFromTopLayer();
    };

    auto exitDocuments = documentsToUnfullscreen(currentDocument);
    for (Ref exitDocument : exitDocuments) {
        addDocumentToFullscreenChangeEventQueue(exitDocument);
        if (mode == ExitMode::Resize)
            unfullscreenDocument(exitDocument);
        else {
            RefPtr fullscreenElement = exitDocument->fullscreenManager().fullscreenElement();
            clearFullscreenFlags(*fullscreenElement);
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
    return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Fullscreen, protectedDocument());
}

bool FullscreenManager::willEnterFullscreen(Element& element, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
#if !ENABLE(VIDEO)
    UNUSED_PARAM(mode);
#endif

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return false;
    }

    // Protect against being called after the document has been removed from the page.
    if (!page()) {
        ERROR_LOG(LOGIDENTIFIER, "Document no longer in page; bailing");
        return false;
    }

    // The element is an open popover.
    if (element.isPopoverShowing()) {
        ERROR_LOG(LOGIDENTIFIER, "Element to fullscreen is an open popover; bailing.");
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
    ASSERT(page()->isFullscreenManagerEnabled());

#if ENABLE(VIDEO)
    if (RefPtr mediaElement = dynamicDowncast<HTMLMediaElement>(element))
        mediaElement->willBecomeFullscreenElement(mode);
    else
#endif
        element.willBecomeFullscreenElement();

    ASSERT(&element == m_pendingFullscreenElement);
    m_pendingFullscreenElement = nullptr;
    m_fullscreenElement = &element;

    Deque<RefPtr<Element>> ancestorsInTreeOrder;
    RefPtr ancestor = &element;
    do {
        ancestorsInTreeOrder.prepend(ancestor);
    } while ((ancestor = ancestor->document().ownerElement()));

    for (auto ancestor : makeReversedRange(ancestorsInTreeOrder)) {
        auto hideUntil = ancestor->topmostPopoverAncestor(Element::TopLayerElementType::Other);

        ancestor->document().hideAllPopoversUntil(hideUntil, FocusPreviousElement::No, FireEvents::No);

        auto containingBlockBeforeStyleResolution = SingleThreadWeakPtr<RenderBlock> { };
        if (auto* renderer = ancestor->renderer())
            containingBlockBeforeStyleResolution = renderer->containingBlock();

        ancestor->setFullscreenFlag(true);
        ancestor->document().resolveStyle(Document::ResolveStyleType::Rebuild);

        // Remove before adding, so we always add at the end of the top layer.
        if (ancestor->isInTopLayer())
            ancestor->removeFromTopLayer();
        ancestor->addToTopLayer();

        RenderElement::markRendererDirtyAfterTopLayerChange(ancestor->checkedRenderer().get(), containingBlockBeforeStyleResolution.get());
    }

    for (auto ancestor : ancestorsInTreeOrder)
        addDocumentToFullscreenChangeEventQueue(ancestor->document());

    if (auto* iframe = dynamicDowncast<HTMLIFrameElement>(element))
        iframe->setIFrameFullscreenFlag(true);

    if (!document().quirks().shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk())
        notifyAboutFullscreenChangeOrError();

    return true;
}

bool FullscreenManager::didEnterFullscreen()
{
    if (document().quirks().shouldDelayFullscreenEventWhenExitingPictureInPictureQuirk())
        notifyAboutFullscreenChangeOrError();

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

    finishExitFullscreen(protectedTopDocument(), ExitMode::Resize);

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
            m_pendingPromise->reject(Exception { ExceptionCode::TypeError });
        else
            m_pendingPromise->resolve();
        m_pendingPromise = nullptr;
    }

    dispatchFullscreenChangeOrErrorEvent(changeQueue, EventType::Change, /* shouldNotifyMediaElement */ true);
    dispatchFullscreenChangeOrErrorEvent(errorQueue, EventType::Error, /* shouldNotifyMediaElement */ false);
}

void FullscreenManager::dispatchEventForNode(Node& node, EventType eventType)
{
    switch (eventType) {
    case EventType::Change: {
        node.dispatchEvent(Event::create(eventNames().fullscreenchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
        bool shouldEmitUnprefixed = !(node.hasEventListeners(eventNames().webkitfullscreenchangeEvent) && node.hasEventListeners(eventNames().fullscreenchangeEvent)) && !(node.document().hasEventListeners(eventNames().webkitfullscreenchangeEvent) && node.document().hasEventListeners(eventNames().fullscreenchangeEvent));
        if (shouldEmitUnprefixed)
            node.dispatchEvent(Event::create(eventNames().webkitfullscreenchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
        break;
    }
    case EventType::Error:
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

        // Gaining or losing fullscreen state may change viewport arguments
        node->protectedDocument()->updateViewportArguments();

#if ENABLE(VIDEO)
        if (shouldNotifyMediaElement) {
            if (RefPtr mediaElement = dynamicDowncast<HTMLMediaElement>(node.get()))
                mediaElement->enteredOrExitedFullscreen();
        }
#else
        UNUSED_PARAM(shouldNotifyMediaElement);
#endif
        dispatchEventForNode(node.get(), eventType);
    }
}

void FullscreenManager::exitRemovedFullscreenElement(Element& element)
{
    ASSERT(element.hasFullscreenFlag());

    auto fullscreenElement = fullscreenOrPendingElement();
    if (fullscreenElement == &element) {
        INFO_LOG(LOGIDENTIFIER, "Fullscreen element removed; exiting fullscreen");
        exitFullscreen(nullptr);
    } else
        clearFullscreenFlags(element);
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
        emplace(styleInvalidation, *m_fullscreenElement, { { CSSSelector::PseudoClass::InternalAnimatingFullscreenTransition, flag } });
    m_isAnimatingFullscreen = flag;
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
    for (Ref element : document().topLayerElements()) {
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
