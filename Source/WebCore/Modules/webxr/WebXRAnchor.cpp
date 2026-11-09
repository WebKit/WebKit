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

#include "config.h"
#include "WebXRAnchor.h"

#if ENABLE(WEBXR_ANCHORS)

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "IDLTypes.h"
#include "JSDOMPromiseDeferred.h"
#include "WebXRSession.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRAnchor);

Ref<WebXRAnchor> WebXRAnchor::create(Document& document, WebXRSession& session, PlatformXR::AnchorIdentifier identifier)
{
    return adoptRef(*new WebXRAnchor(document, session, identifier));
}

WebXRAnchor::WebXRAnchor(Document& document, WebXRSession& session, PlatformXR::AnchorIdentifier identifier)
    : ActiveDOMObject(&document)
    , m_document(document)
    , m_session(session)
    , m_identifier(identifier)
    , m_anchorSpace(WebXRAnchorSpace::create(document, session, identifier))
{
    suspendIfNeeded();
}

WebXRAnchor::~WebXRAnchor() = default;

ExceptionOr<Ref<WebXRSpace>> WebXRAnchor::anchorSpace() const
{
    if (m_deleted)
        return Exception { ExceptionCode::InvalidStateError };
    return Ref<WebXRSpace> { *m_anchorSpace };
}

void WebXRAnchor::requestPersistentHandle(RequestPersistentHandlePromise&& promise)
{
    if (m_deleted) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    RefPtr session = m_session;
    if (!session) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    auto existingHandle = session->persistentHandleForAnchor(*this);
    if (existingHandle) {
        promise.resolve(existingHandle.value());
        return;
    }

    auto uuid = createVersion4UUIDString();
    session->setPersistentHandleForAnchor(*this, uuid);
    promise.resolve(uuid);
}

void WebXRAnchor::deleteAnchor()
{
    if (m_deleted)
        return;

    m_deleted = true;
    if (RefPtr session = m_session)
        session->deleteAnchor(m_identifier);
}

void WebXRAnchor::stop()
{
    if (!m_deleted)
        deleteAnchor();
}

} // namespace WebCore

#endif // ENABLE(WEBXR_ANCHORS)
