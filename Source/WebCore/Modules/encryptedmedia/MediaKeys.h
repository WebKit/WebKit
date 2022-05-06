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

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMInstance.h"
#include "ExceptionOr.h"
#include "MediaKeySessionType.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class Logger;
}

namespace WebCore {

class CDM;
class CDMClient;
class CDMInstance;
class BufferSource;
class DeferredPromise;
class Document;
class MediaKeySession;

class MediaKeys final
    : public RefCounted<MediaKeys>
    , public CDMInstanceClient {
public:
    using KeySessionType = MediaKeySessionType;

    static Ref<MediaKeys> create(Document& document, bool useDistinctiveIdentifier, bool persistentStateAllowed, const Vector<MediaKeySessionType>& supportedSessionTypes, Ref<CDM>&& implementation, Ref<CDMInstance>&& instance)
    {
        return adoptRef(*new MediaKeys(document, useDistinctiveIdentifier, persistentStateAllowed, supportedSessionTypes, WTFMove(implementation), WTFMove(instance)));
    }

    ~MediaKeys();

    ExceptionOr<Ref<MediaKeySession>> createSession(Document&, MediaKeySessionType);
    void setServerCertificate(const BufferSource&, Ref<DeferredPromise>&&);

    void attachCDMClient(CDMClient&);
    void detachCDMClient(CDMClient&);
    void attemptToResumePlaybackOnClients();

    bool hasOpenSessions() const;
    CDMInstance& cdmInstance() { return m_instance; }
    const CDMInstance& cdmInstance() const { return m_instance; }

#if !RELEASE_LOG_DISABLED
    const void* nextChildIdentifier() const;
#endif

    unsigned internalInstanceObjectRefCount() const { return m_instance->refCount(); }

protected:
    MediaKeys(Document&, bool useDistinctiveIdentifier, bool persistentStateAllowed, const Vector<MediaKeySessionType>&, Ref<CDM>&&, Ref<CDMInstance>&&);

    // CDMInstanceClient
    void unrequestedInitializationDataReceived(const String&, Ref<SharedBuffer>&&) final;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    const void* logIdentifier() const { return m_logIdentifier; }
#endif

    bool m_useDistinctiveIdentifier;
    bool m_persistentStateAllowed;
    Vector<MediaKeySessionType> m_supportedSessionTypes;
    Ref<CDM> m_implementation;
    Ref<CDMInstance> m_instance;

    Vector<Ref<MediaKeySession>> m_sessions;
    WeakHashSet<CDMClient> m_cdmClients;

#if !RELEASE_LOG_DISABLED
    Ref<Logger> m_logger;
    const void* m_logIdentifier;
    mutable uint64_t m_childIdentifierSeed { 0 };
#endif
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
