/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DocumentImmersive.h"

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

#include "HTMLModelElement.h"
#include "PseudoClassChangeInvalidation.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentImmersive);

DocumentImmersive::DocumentImmersive(Document& document)
    : m_document(document)
{
}

bool DocumentImmersive::immersiveEnabled(Document& document)
{
    if (!document.settings().modelElementImmersiveEnabled())
        return false;

    if (!document.isFullyActive())
        return false;

    return false; // Needs client support
}

Element* DocumentImmersive::immersiveElement(Document& document)
{
    RefPtr documentImmersive = document.immersiveIfExists();
    if (!documentImmersive)
        return nullptr;
    return document.ancestorElementInThisScope(documentImmersive->protectedImmersiveElement().get());
}

HTMLModelElement* DocumentImmersive::immersiveElement() const
{
    return m_immersiveElement.get();
}

void DocumentImmersive::exitImmersive(Document& document, Ref<DeferredPromise>&& promise)
{
    RefPtr immersive = document.immersiveIfExists();
    if (!document.isFullyActive() || !immersive) {
        promise->reject(Exception { ExceptionCode::TypeError, "Not in immersive"_s });
        return;
    }
    immersive->exitImmersive([promise = WTF::move(promise)](auto result) {
        if (result.hasException())
            promise->reject(result.releaseException());
        else
            promise->resolve();
    });
}

void DocumentImmersive::requestImmersive(HTMLModelElement* element, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    RefPtr protectedPage = document().page();
    if (!protectedPage || !protectedPage->settings().modelElementImmersiveEnabled())
        return handleImmersiveError(element, "Immersive API is disabled."_s, EmitErrorEvent::Yes, ExceptionCode::TypeError, WTF::move(completionHandler));

    if (!protect(document())->isFullyActive())
        return handleImmersiveError(element, "Cannot request immersive on a document that is not fully active."_s, EmitErrorEvent::No, ExceptionCode::TypeError, WTF::move(completionHandler));

    if (RefPtr window = document().window(); !window || !window->consumeTransientActivation())
        return handleImmersiveError(element, "Cannot request immersive without transient activation."_s, EmitErrorEvent::Yes, ExceptionCode::TypeError, WTF::move(completionHandler));

    cancelActiveRequest([weakElement = WeakPtr { *element }, weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        RefPtr protectedElement = weakElement.get();
        if (!protectedThis || !protectedElement)
            return completionHandler(Exception { ExceptionCode::AbortError });

        protectedThis->m_pendingImmersiveElement = protectedElement.get();

        protect(protectedThis->document())->eventLoop().queueTask(TaskSource::ModelElement, [weakThis, weakElement, scope = CompletionHandlerScope(WTF::move(completionHandler))]() mutable {
            auto completionHandler = scope.release();
            RefPtr protectedThis = weakThis.get();
            RefPtr protectedElement = weakElement.get();

            if (!protectedThis || !protectedElement)
                return completionHandler(Exception { ExceptionCode::AbortError });

            if (protectedThis->m_pendingImmersiveElement != protectedElement.get())
                return completionHandler(Exception { ExceptionCode::AbortError, "Immersive request was superseded by another request."_s });

            if (!protect(protectedThis->document())->isFullyActive())
                return protectedThis->handleImmersiveError(protectedElement.get(), "Document is no longer fully active."_s, EmitErrorEvent::Yes, ExceptionCode::AbortError, WTF::move(completionHandler));

            if (!protectedElement->isConnected() || &protectedElement->document() != protect(protectedThis->document()).ptr())
                return protectedThis->handleImmersiveError(protectedElement.get(), "Element is not connected to the document."_s, EmitErrorEvent::No, ExceptionCode::AbortError, WTF::move(completionHandler));

            protectedThis->beginImmersiveRequest(protectedElement.releaseNonNull(), WTF::move(completionHandler));
        });
    });
}

void DocumentImmersive::exitImmersive(CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    RefPtr exitingImmersiveElement = immersiveElement();
    if (!exitingImmersiveElement)
        return completionHandler(Exception { ExceptionCode::TypeError, "Not in immersive"_s });

    cancelActiveRequest([weakElement = WeakPtr { *exitingImmersiveElement }, weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        RefPtr protectedElement = weakElement.get();
        if (!protectedThis || !protectedElement)
            return completionHandler(Exception { ExceptionCode::AbortError });

        protectedThis->m_pendingImmersiveElement = nullptr;
        protectedThis->m_pendingExitImmersive = true;
        auto resetPendingExitScope = makeScopeExit([weakThis] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->m_pendingExitImmersive = false;
        });

        protectedThis->dismissClientImmersivePresentation(protectedElement.get(), [weakThis, weakElement, resetPendingExitScope = WTF::move(resetPendingExitScope), completionHandler = WTF::move(completionHandler)]() mutable {
            RefPtr protectedThis = weakThis.get();
            RefPtr protectedElement = weakElement.get();

            if (!protectedThis || !protectedElement)
                return completionHandler(Exception { ExceptionCode::AbortError });

            protectedElement->exitImmersivePresentation([weakThis, weakElement, resetPendingExitScope = WTF::move(resetPendingExitScope), completionHandler = WTF::move(completionHandler)]() mutable {
                RefPtr protectedThis = weakThis.get();
                RefPtr protectedElement = weakElement.get();

                if (!protectedThis || !protectedElement)
                    return completionHandler(Exception { ExceptionCode::AbortError });

                protectedThis->updateElementIsImmersive(protectedElement.get(), false);
                protectedThis->m_immersiveElement = nullptr;

                if (protectedThis->m_pendingExitCompletionHandler) {
                    auto pendingHandler = std::exchange(protectedThis->m_pendingExitCompletionHandler, { });
                    pendingHandler();
                }

                completionHandler({ });
            });
        });
    });
}

void DocumentImmersive::exitImmersive()
{
    if (!immersiveElement())
        return;

    exitImmersive([weakThis = WeakPtr { *this }](auto result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (result.hasException())
            RELEASE_LOG_ERROR(Immersive, "%p - DocumentImmersive: %s", protectedThis.get(), result.releaseException().message().utf8().data());
    });
}

void DocumentImmersive::exitRemovedImmersiveElement(HTMLModelElement* element, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(element->immersive());

    if (immersiveElement() == element) {
        exitImmersive([completionHandler = WTF::move(completionHandler)] (auto) mutable {
            completionHandler();
        });
    } else {
        element->exitImmersivePresentation([] { });
        updateElementIsImmersive(element, false);
        completionHandler();
    }
}

void DocumentImmersive::handleImmersiveError(HTMLModelElement* element, const String& message, EmitErrorEvent emitErrorEvent, ExceptionCode code, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    RELEASE_LOG_ERROR(Immersive, "%p - DocumentImmersive: %s", this, message.utf8().data());

    if (m_activeRequest.element == element) {
        m_activeRequest.stage = ActiveRequest::Stage::None;
        m_activeRequest.element = nullptr;
    }

    if (m_pendingImmersiveElement == element)
        m_pendingImmersiveElement = nullptr;

    if (emitErrorEvent == EmitErrorEvent::Yes && element) {
        queueImmersiveEventForElement(EventType::Error, *element);
        protect(document())->scheduleRenderingUpdate(RenderingUpdateStep::Immersive);
    }
    completionHandler(Exception { code, message });
}

std::optional<Exception> DocumentImmersive::checkRequestStillValid(HTMLModelElement* element, ActiveRequest::Stage expectedStage)
{
    if (m_activeRequest.stage != expectedStage || m_activeRequest.element != element || m_pendingImmersiveElement != element)
        return Exception { ExceptionCode::AbortError, "Immersive request was superseded by another request."_s };

    return std::nullopt;
}

void DocumentImmersive::cancelActiveRequest(CompletionHandler<void()>&& completionHandler)
{
    RefPtr element = m_activeRequest.element.get();
    if (!element)
        return completionHandler();

    auto stage = m_activeRequest.stage;
    m_activeRequest.stage = ActiveRequest::Stage::None;
    m_activeRequest.element = nullptr;

    switch (stage) {
    case ActiveRequest::Stage::None:
    case ActiveRequest::Stage::Permission:
    case ActiveRequest::Stage::ModelPlayer:
        completionHandler();
        break;

    case ActiveRequest::Stage::Presentation:
        dismissClientImmersivePresentation(element.get(), [element, completionHandler = WTF::move(completionHandler)]() mutable {
            element->exitImmersivePresentation([] { });
            completionHandler();
        });
        break;
    }
}

void DocumentImmersive::beginImmersiveRequest(Ref<HTMLModelElement>&& element, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    RefPtr protectedPage = document().page();
    if (!protectedPage)
        return handleImmersiveError(element.ptr(), "Missing page."_s, EmitErrorEvent::Yes, ExceptionCode::AbortError, WTF::move(completionHandler));

    m_activeRequest.stage = ActiveRequest::Stage::Permission;
    m_activeRequest.element = element.ptr();

    protectedPage->chrome().client().allowImmersiveElement(element, [weakElement = WeakPtr { element }, weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](bool allowed) mutable {
        RefPtr protectedThis = weakThis.get();
        RefPtr protectedElement = weakElement.get();

        if (!protectedThis || !protectedElement)
            return completionHandler(Exception { ExceptionCode::AbortError });

        if (auto error = protectedThis->checkRequestStillValid(protectedElement.get(), ActiveRequest::Stage::Permission))
            return completionHandler(WTF::move(*error));

        if (!allowed)
            return protectedThis->handleImmersiveError(protectedElement.get(), "Immersive request was denied."_s, EmitErrorEvent::Yes, ExceptionCode::AbortError, WTF::move(completionHandler));

        protectedThis->createModelPlayerForImmersive(protectedElement.releaseNonNull(), WTF::move(completionHandler));
    });
}

void DocumentImmersive::createModelPlayerForImmersive(Ref<HTMLModelElement>&& element, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    m_activeRequest.stage = ActiveRequest::Stage::ModelPlayer;

    element->ensureImmersivePresentation([weakElement = WeakPtr { element }, weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        RefPtr protectedElement = weakElement.get();

        if (!protectedThis || !protectedElement)
            return completionHandler(Exception { ExceptionCode::AbortError });

        if (auto error = protectedThis->checkRequestStillValid(protectedElement.get(), ActiveRequest::Stage::ModelPlayer)) {
            protectedElement->exitImmersivePresentation([] { });
            return completionHandler(WTF::move(*error));
        }

        if (result.hasException()) {
            auto exception = result.releaseException();
            return protectedThis->handleImmersiveError(protectedElement.get(), exception.message(), EmitErrorEvent::Yes, exception.code(), WTF::move(completionHandler));
        }

        if (protectedThis->m_pendingExitImmersive) {
            if (protectedThis->m_pendingExitCompletionHandler) {
                auto previousRequestHandler = std::exchange(protectedThis->m_pendingExitCompletionHandler, { });
                previousRequestHandler();
            }

            auto contextID = result.releaseReturnValue();
            protectedThis->m_pendingExitCompletionHandler = [weakThis, weakElement, contextID, completionHandler = WTF::move(completionHandler)]() mutable {
                RefPtr protectedThis = weakThis.get();
                RefPtr protectedElement = weakElement.get();

                if (!protectedThis || !protectedElement)
                    return completionHandler(Exception { ExceptionCode::AbortError });

                if (auto error = protectedThis->checkRequestStillValid(protectedElement.get(), ActiveRequest::Stage::ModelPlayer)) {
                    protectedElement->exitImmersivePresentation([] { });
                    return completionHandler(WTF::move(*error));
                }

                protectedThis->presentImmersiveElement(protectedElement.releaseNonNull(), contextID, WTF::move(completionHandler));
            };
            return;
        }
        protectedThis->presentImmersiveElement(protectedElement.releaseNonNull(), result.releaseReturnValue(), WTF::move(completionHandler));
    });
}

void DocumentImmersive::presentImmersiveElement(Ref<HTMLModelElement>&& element, LayerHostingContextIdentifier contextID, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    RefPtr protectedPage = document().page();
    if (!protectedPage) {
        element->exitImmersivePresentation([] { });
        return handleImmersiveError(element.ptr(), "Missing page."_s, EmitErrorEvent::Yes, ExceptionCode::AbortError, WTF::move(completionHandler));
    }

    m_activeRequest.stage = ActiveRequest::Stage::Presentation;

    protectedPage->chrome().client().presentImmersiveElement(element, contextID, [weakElement = WeakPtr { element }, weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](bool success) mutable {
        RefPtr protectedThis = weakThis.get();
        RefPtr protectedElement = weakElement.get();

        if (!protectedElement)
            return completionHandler(Exception { ExceptionCode::AbortError });

        if (!protectedThis) {
            protectedElement->exitImmersivePresentation([] { });
            return completionHandler(Exception { ExceptionCode::AbortError });
        }

        if (auto error = protectedThis->checkRequestStillValid(protectedElement.get(), ActiveRequest::Stage::Presentation)) {
            protectedElement->exitImmersivePresentation([] { });
            return completionHandler(WTF::move(*error));
        }

        if (!success) {
            protectedElement->exitImmersivePresentation([] { });
            return protectedThis->handleImmersiveError(protectedElement.get(), "Failure to present the immersive element."_s, EmitErrorEvent::Yes, ExceptionCode::AbortError, WTF::move(completionHandler));
        }

        if (RefPtr oldImmersiveElement = protectedThis->immersiveElement()) {
            oldImmersiveElement->exitImmersivePresentation([] { });
            protectedThis->updateElementIsImmersive(oldImmersiveElement.get(), false);
        }

        protectedThis->m_immersiveElement = protectedElement.get();
        protectedThis->m_pendingImmersiveElement = nullptr;
        protectedThis->m_activeRequest.stage = ActiveRequest::Stage::None;
        protectedThis->m_activeRequest.element = nullptr;
        protectedThis->updateElementIsImmersive(protectedElement.get(), true);

        completionHandler({ });
    });
}


void DocumentImmersive::updateElementIsImmersive(HTMLModelElement* element, bool isImmersive)
{
    Style::PseudoClassChangeInvalidation styleInvalidation(*element, { { CSSSelector::PseudoClass::Immersive, isImmersive } });
    queueImmersiveEventForElement(DocumentImmersive::EventType::Change, *element);
    document().scheduleRenderingUpdate(RenderingUpdateStep::Immersive);
}

void DocumentImmersive::dismissClientImmersivePresentation(HTMLModelElement* exitingImmersiveElement, CompletionHandler<void()>&& completionHandler)
{
    RefPtr protectedPage = document().page();
    if (!protectedPage)
        return completionHandler();

    protectedPage->chrome().client().dismissImmersiveElement(*exitingImmersiveElement, WTF::move(completionHandler));
}

void DocumentImmersive::dispatchPendingEvents()
{
    auto pendingEvents = std::exchange(m_pendingEvents, { });

    while (!pendingEvents.isEmpty()) {
        auto [eventType, element] = pendingEvents.takeFirst();

        // Let target be element if element is connected and its node document is document, and otherwise let target be document.
        Ref target = [&]() -> Node& {
            if (element->isConnected() && &element->document() == &document())
                return element;
            return document();
        }();

        switch (eventType) {
        case EventType::Change: {
            target->dispatchEvent(Event::create(eventNames().immersivechangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
            break;
        }
        case EventType::Error:
            target->dispatchEvent(Event::create(eventNames().immersiveerrorEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
            break;
        }
    }
}

void DocumentImmersive::queueImmersiveEventForElement(EventType eventType, Element& target)
{
    m_pendingEvents.append({ eventType, GCReachableRef(target) });
}

void DocumentImmersive::clear()
{
    cancelActiveRequest([] { });

    m_pendingImmersiveElement = nullptr;
    m_immersiveElement = nullptr;
    m_pendingExitImmersive = false;

    if (m_pendingExitCompletionHandler) {
        auto handler = std::exchange(m_pendingExitCompletionHandler, { });
        handler();
    }

    clearPendingEvents();
}

}

#endif
