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

#pragma once

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

#include <WebCore/Document.h>
#include <WebCore/GCReachableRef.h>
#include <WebCore/HTMLModelElement.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <wtf/Deque.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DocumentImmersive final : public CanMakeWeakPtr<DocumentImmersive> {
    WTF_MAKE_TZONE_ALLOCATED(DocumentImmersive);
public:
    DocumentImmersive(Document&);
    ~DocumentImmersive() = default;

    void ref() const { m_document->ref(); }
    void deref() const { m_document->deref(); }

    // Document+Immersive.idl methods.
    static bool immersiveEnabled(Document&);
    static Element* immersiveElement(Document&);
    static void exitImmersive(Document&, Ref<DeferredPromise>&&);

    // Helpers.
    Document& document() { return m_document; }
    const Document& document() const { return m_document; }

    HTMLModelElement* immersiveElement() const;
    RefPtr<HTMLModelElement> protectedImmersiveElement() const { return immersiveElement(); }

    void requestImmersive(HTMLModelElement*, CompletionHandler<void(ExceptionOr<void>)>&&);
    void exitImmersive(CompletionHandler<void(ExceptionOr<void>)>&&);
    WEBCORE_EXPORT void exitImmersive();
    void exitRemovedImmersiveElement(HTMLModelElement*, CompletionHandler<void()>&&);

    enum class EventType : bool { Change, Error };
    void dispatchPendingEvents();
    void queueImmersiveEventForElement(EventType, Element&);
    void clear();

protected:
    friend class Document;

    void clearPendingEvents() { m_pendingEvents.clear(); }

private:
    using CompletionHandlerScope = Document::CompletionHandlerScope;

    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
    WeakPtr<HTMLModelElement, WeakPtrImplWithEventTargetData> m_immersiveElement;
    void updateElementIsImmersive(HTMLModelElement*, bool);

    WeakPtr<HTMLModelElement, WeakPtrImplWithEventTargetData> m_pendingImmersiveElement;
    bool m_pendingExitImmersive { false };
    CompletionHandler<void()> m_pendingExitCompletionHandler;

    struct ActiveRequest {
        enum class Stage : uint8_t { None, Permission, ModelPlayer, Presentation };
        Stage stage { Stage::None };
        WeakPtr<HTMLModelElement, WeakPtrImplWithEventTargetData> element;
    };
    ActiveRequest m_activeRequest;
    std::optional<Exception> checkRequestStillValid(HTMLModelElement*, ActiveRequest::Stage expectedStage);

    void cancelActiveRequest(CompletionHandler<void()>&&);
    void beginImmersiveRequest(Ref<HTMLModelElement>&&, CompletionHandler<void(ExceptionOr<void>)>&&);
    void createModelPlayerForImmersive(Ref<HTMLModelElement>&&, CompletionHandler<void(ExceptionOr<void>)>&&);
    void presentImmersiveElement(Ref<HTMLModelElement>&&, LayerHostingContextIdentifier, CompletionHandler<void(ExceptionOr<void>)>&&);
    void dismissClientImmersivePresentation(HTMLModelElement*, CompletionHandler<void()>&&);

    enum class EmitErrorEvent : bool { No, Yes };
    void handleImmersiveError(HTMLModelElement*, const String& message, EmitErrorEvent, ExceptionCode, CompletionHandler<void(ExceptionOr<void>)>&&);

    Deque<std::pair<EventType, GCReachableRef<Element>>> m_pendingEvents;
};

}

#endif
