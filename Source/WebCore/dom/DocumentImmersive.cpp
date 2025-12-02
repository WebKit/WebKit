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

void DocumentImmersive::exitImmersive(Document& document, RefPtr<DeferredPromise>&& promise)
{
    RefPtr immersiveElement = document.immersiveIfExists()->immersiveElement();
    if (!document.isFullyActive() || !immersiveElement) {
        promise->reject(Exception { ExceptionCode::TypeError, "Not in immersive"_s });
        return;
    }
    document.protectedImmersive()->exitImmersive(immersiveElement.get(), [promise = WTFMove(promise)] (auto result) {
        if (!promise)
            return;
        if (result.hasException())
            promise->reject(result.releaseException());
        else
            promise->resolve();
    });
}

void DocumentImmersive::requestImmersive(HTMLModelElement* element, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    enum class EmitErrorEvent : bool { No, Yes };
    auto handleError = [weakElement = WeakPtr { *element }, weakThis = WeakPtr { *this }](String message, EmitErrorEvent emitErrorEvent, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler) mutable {
        RefPtr protectedThis = weakThis.get();
        RefPtr protectedElement = weakElement.get();
        if (!protectedThis || !protectedElement)
            return completionHandler(Exception { ExceptionCode::TypeError, message });
        RELEASE_LOG_ERROR(Immersive, "%p - DocumentImmersive: %s", protectedThis.get(), message.utf8().data());
        if (emitErrorEvent == EmitErrorEvent::Yes) {
            protectedThis->queueImmersiveEventForElement(DocumentImmersive::EventType::Error, *protectedElement);
            protectedThis->protectedDocument()->scheduleRenderingUpdate(RenderingUpdateStep::Immersive);
        }
        completionHandler(Exception { ExceptionCode::TypeError, message });
    };

    if (!protectedDocument()->isFullyActive())
        return handleError("Cannot request immersive on a document that is not fully active."_s, EmitErrorEvent::No, WTFMove(completionHandler));

    if (RefPtr window = document().window(); !window || !window->consumeTransientActivation())
        return handleError("Cannot request immersive without transient activation."_s, EmitErrorEvent::Yes, WTFMove(completionHandler));

    RefPtr protectedPage = document().page();
    if (!protectedPage || !protectedPage->settings().modelElementImmersiveEnabled())
        return handleError("Immersive API is disabled."_s, EmitErrorEvent::Yes, WTFMove(completionHandler));

    protectedPage->chrome().client().canEnterImmersiveElement(*element, [weakElement = WeakPtr { *element }, handleError, completionHandler = WTFMove(completionHandler)](auto canEnterImmersive) mutable {
        if (!canEnterImmersive)
            return handleError("Immersive request was denied."_s, EmitErrorEvent::Yes, WTFMove(completionHandler));

        RefPtr protectedElement = weakElement.get();
        if (!protectedElement)
            return completionHandler(Exception { ExceptionCode::TypeError });

        protectedElement->ensureImmersivePresentation([handleError, completionHandler = WTFMove(completionHandler)](auto result) mutable {
            if (result.hasException())
                return handleError(result.releaseException().message(), EmitErrorEvent::Yes, WTFMove(completionHandler));
            handleError("Not implemented"_s, EmitErrorEvent::Yes, WTFMove(completionHandler));
        });
    });
}

void DocumentImmersive::exitImmersive(HTMLModelElement* element, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    element->exitImmersivePresentation([weakElement = WeakPtr { *element }, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        RefPtr protectedThis = weakThis.get();
        RefPtr protectedElement = weakElement.get();
        if (!protectedThis || !protectedElement)
            return completionHandler(Exception { ExceptionCode::TypeError });

        protectedThis->queueImmersiveEventForElement(DocumentImmersive::EventType::Error, *protectedElement);
        protectedThis->document().scheduleRenderingUpdate(RenderingUpdateStep::Immersive);
        completionHandler(Exception { ExceptionCode::AbortError, "Not implemented"_s });
    });
}

void DocumentImmersive::exitRemovedImmersiveElement(HTMLModelElement* element)
{
    queueImmersiveEventForElement(DocumentImmersive::EventType::Error, *element);
    document().scheduleRenderingUpdate(RenderingUpdateStep::Immersive);
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
    m_immersiveElement = nullptr;
}

}

#endif
