/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(DOCUMENT_PIP)

#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include "LocalDOMWindowProperty.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class Document;
class LocalFrame;
class WindowProxy;

// https://wicg.github.io/document-picture-in-picture/
class DocumentPictureInPicture final : public RefCounted<DocumentPictureInPicture>, public EventTarget, public LocalDOMWindowProperty {
    WTF_MAKE_TZONE_ALLOCATED(DocumentPictureInPicture);
public:
    struct Options {
        uint64_t width { 0 };
        uint64_t height { 0 };
        bool disallowReturnToOpener { false };
        bool preferInitialWindowPlacement { false };
    };

    static Ref<DocumentPictureInPicture> create(LocalDOMWindow& window) { return adoptRef(*new DocumentPictureInPicture(window)); }
    ~DocumentPictureInPicture();

    using RefCounted::ref;
    using RefCounted::deref;

    void requestWindow(Options&&, Ref<DeferredPromise>&&);
    WindowProxy* pictureInPictureWindow() const;

    // https://wicg.github.io/document-picture-in-picture/#close-any-associated-document-picture-in-picture-windows
    static void closeAssociatedWindows(Document&);

private:
    explicit DocumentPictureInPicture(LocalDOMWindow&);

    LocalFrame* openFrame() const;

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::DocumentPictureInPicture; }
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    WeakPtr<LocalFrame> m_lastOpenedFrame;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENTTARGET(DocumentPictureInPicture)

#endif // ENABLE(DOCUMENT_PIP)
