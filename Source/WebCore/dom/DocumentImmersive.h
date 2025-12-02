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
    static void exitImmersive(Document&, RefPtr<DeferredPromise>&&);

    // Helpers.
    Document& document() { return m_document.get(); }
    const Document& document() const { return m_document.get(); }
    Ref<Document> protectedDocument() const { return m_document.get(); }

    HTMLModelElement* immersiveElement() const;
    RefPtr<HTMLModelElement> protectedImmersiveElement() const { return immersiveElement(); }

    void requestImmersive(HTMLModelElement*, CompletionHandler<void(ExceptionOr<void>)>&&);
    void exitImmersive(HTMLModelElement*, CompletionHandler<void(ExceptionOr<void>)>&&);
    void exitRemovedImmersiveElement(HTMLModelElement*);

    enum class EventType : bool { Change, Error };
    void dispatchPendingEvents();
    void queueImmersiveEventForElement(EventType, Element&);
    void clear();

protected:
    friend class Document;

    void clearPendingEvents() { m_pendingEvents.clear(); }

private:
    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
    WeakPtr<HTMLModelElement, WeakPtrImplWithEventTargetData> m_immersiveElement;

    Deque<std::pair<EventType, GCReachableRef<Element>>> m_pendingEvents;
};

}

#endif
