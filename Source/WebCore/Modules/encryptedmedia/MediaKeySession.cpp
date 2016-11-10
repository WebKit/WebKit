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

#include "MediaKeyStatusMap.h"
#include "NotImplemented.h"

namespace WebCore {

Ref<MediaKeySession> MediaKeySession::create(ScriptExecutionContext& context)
{
    auto session = adoptRef(*new MediaKeySession(context));
    session->suspendIfNeeded();
    return session;
}

MediaKeySession::MediaKeySession(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
{
}

MediaKeySession::~MediaKeySession() = default;

const String& MediaKeySession::sessionId() const
{
    notImplemented();
    return emptyString();
}

double MediaKeySession::expiration() const
{
    notImplemented();
    return 0;
}

RefPtr<MediaKeyStatusMap> MediaKeySession::keyStatuses() const
{
    notImplemented();
    return nullptr;
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
