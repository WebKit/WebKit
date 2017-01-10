/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaKeySession.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDM.h"
#include "CDMInstance.h"
#include "MediaKeyStatusMap.h"
#include "NotImplemented.h"

namespace WebCore {

Ref<MediaKeySession> MediaKeySession::create(ScriptExecutionContext& context, MediaKeySessionType sessionType, bool useDistinctiveIdentifier, Ref<CDM>&& implementation, Ref<CDMInstance>&& instance)
{
    auto session = adoptRef(*new MediaKeySession(context, sessionType, useDistinctiveIdentifier, WTFMove(implementation), WTFMove(instance)));
    session->suspendIfNeeded();
    return session;
}

MediaKeySession::MediaKeySession(ScriptExecutionContext& context, MediaKeySessionType sessionType, bool useDistinctiveIdentifier, Ref<CDM>&& implementation, Ref<CDMInstance>&& instance)
    : ActiveDOMObject(&context)
    , m_expiration(std::numeric_limits<double>::quiet_NaN())
    , m_keyStatuses(MediaKeyStatusMap::create())
    , m_useDistinctiveIdentifier(useDistinctiveIdentifier)
    , m_sessionType(sessionType)
    , m_implementation(WTFMove(implementation))
    , m_instance(WTFMove(instance))
{
    // https://w3c.github.io/encrypted-media/#dom-mediakeys-setservercertificate
    // W3C Editor's Draft 09 November 2016
    // createSession(), ctd.

    // 3.1. Let the sessionId attribute be the empty string.
    // 3.2. Let the expiration attribute be NaN.
    // 3.3. Let the closed attribute be a new promise.
    // 3.4. Let key status be a new empty MediaKeyStatusMap object, and initialize it as follows:
    // 3.4.1. Let the size attribute be 0.
    // 3.5. Let the session type value be sessionType.
    // 3.6. Let the uninitialized value be true.
    // 3.7. Let the callable value be false.
    // 3.8. Let the use distinctive identifier value be this object's use distinctive identifier value.
    // 3.9. Let the cdm implementation value be this object's cdm implementation.
    // 3.10. Let the cdm instance value be this object's cdm instance.

    UNUSED_PARAM(m_callable);
    UNUSED_PARAM(m_sessionType);
    UNUSED_PARAM(m_useDistinctiveIdentifier);
    UNUSED_PARAM(m_closed);
    UNUSED_PARAM(m_uninitialized);
}

MediaKeySession::~MediaKeySession() = default;

const String& MediaKeySession::sessionId() const
{
    return m_sessionId;
}

double MediaKeySession::expiration() const
{
    return m_expiration;
}

Ref<MediaKeyStatusMap> MediaKeySession::keyStatuses() const
{
    return m_keyStatuses.copyRef();
}

void MediaKeySession::generateRequest(const String&, const BufferSource&, Ref<DeferredPromise>&&)
{
    notImplemented();
}

void MediaKeySession::load(const String&, Ref<DeferredPromise>&&)
{
    notImplemented();
}

void MediaKeySession::update(const BufferSource&, Ref<DeferredPromise>&&)
{
    notImplemented();
}

void MediaKeySession::close(Ref<DeferredPromise>&&)
{
    notImplemented();
}

void MediaKeySession::remove(Ref<DeferredPromise>&&)
{
    notImplemented();
}

bool MediaKeySession::hasPendingActivity() const
{
    notImplemented();
    return false;
}

const char* MediaKeySession::activeDOMObjectName() const
{
    notImplemented();
    return "MediaKeySession";
}

bool MediaKeySession::canSuspendForDocumentSuspension() const
{
    notImplemented();
    return false;
}

void MediaKeySession::stop()
{
    notImplemented();
}

} // namespace WebCore

#endif
