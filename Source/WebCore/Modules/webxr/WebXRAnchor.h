/*
 * Copyright (C) 2026 Igalia, S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in source code must reproduce the above copyright
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

#if ENABLE(WEBXR_ANCHORS)

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferredForward.h"
#include "PlatformXR.h"
#include "WebXRAnchorSpace.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class WebXRSession;

class WebXRAnchor : public RefCounted<WebXRAnchor>, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED(WebXRAnchor);
public:
    static Ref<WebXRAnchor> create(Document&, WebXRSession&, PlatformXR::AnchorIdentifier);
    ~WebXRAnchor();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    ExceptionOr<Ref<WebXRSpace>> anchorSpace() const;
    using RequestPersistentHandlePromise = DOMPromiseDeferred<IDLDOMString>;
    void requestPersistentHandle(RequestPersistentHandlePromise&&);
    void deleteAnchor();

    PlatformXR::AnchorIdentifier identifier() const { return m_identifier; }
    bool isDeleted() const { return m_deleted; }

private:
    WebXRAnchor(Document&, WebXRSession&, PlatformXR::AnchorIdentifier);

    // ActiveDOMObject
    void stop() override;

    Ref<Document> m_document;
    WeakPtr<WebXRSession> m_session;
    PlatformXR::AnchorIdentifier m_identifier;
    RefPtr<WebXRAnchorSpace> m_anchorSpace;
    bool m_deleted { false };
};

} // namespace WebCore

#endif // ENABLE(WEBXR_ANCHORS)
