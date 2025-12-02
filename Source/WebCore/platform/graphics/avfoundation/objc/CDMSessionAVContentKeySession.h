/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include "LegacyCDMSession.h"
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WTFSemaphore.h>

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

OBJC_CLASS AVContentKeyRequest;
OBJC_CLASS AVContentKeySession;
OBJC_CLASS WebCDMSessionAVContentKeySessionDelegate;

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class AudioVideoRenderer;
class LegacyCDMPrivateAVFObjC;
class MediaSampleAVFObjC;
class SharedBuffer;

class CDMSessionAVContentKeySession final : public LegacyCDMSession, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<CDMSessionAVContentKeySession> {
    WTF_MAKE_TZONE_ALLOCATED(CDMSessionAVContentKeySession);
public:
    static Ref<CDMSessionAVContentKeySession> create(Vector<int>&& protocolVersions, int cdmVersion, LegacyCDMPrivateAVFObjC& parent, LegacyCDMSessionClient& client)
    {
        return adoptRef(*new CDMSessionAVContentKeySession(WTFMove(protocolVersions), cdmVersion, parent, client));
    }

    ~CDMSessionAVContentKeySession();

    static bool isAvailable();

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    // LegacyCDMSession
    LegacyCDMSessionType type() const final { return CDMSessionTypeAVContentKeySession; }
    RefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode) final;
    void releaseKeys() final;
    bool update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode) final;
    RefPtr<ArrayBuffer> cachedKeyForKeyID(const String&) const final;
    const String& sessionId() const final { return m_sessionId; }
    void setSessionId(const String& sessionId) { m_sessionId = sessionId; }

    void addRenderer(AudioVideoRenderer&);
    void removeRenderer(AudioVideoRenderer&);
    void setInitData(SharedBuffer&);

    using Keys = Vector<Ref<SharedBuffer>>;
    bool isAnyKeyUsable(const Keys&) const;
    void attachContentKeyToSample(const MediaSampleAVFObjC&);

    void invalidateCDM() { m_cdm = nullptr; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    WTFLogChannel& logChannel() const;
#endif

    void didProvideContentKeyRequest(AVContentKeyRequest *);

private:
    CDMSessionAVContentKeySession(Vector<int>&& protocolVersions, int cdmVersion, LegacyCDMPrivateAVFObjC&, LegacyCDMSessionClient&);

    RefPtr<Uint8Array> generateKeyReleaseMessage(unsigned short& errorCode, uint32_t& systemCode);

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const { return "CDMSessionAVContentKeySession"_s; }
#endif

    String storagePath() const;

    static RetainPtr<AVContentKeySession> createContentKeySession(NSURL *);
    bool hasContentKeySession() const { return !!m_contentKeySession; }
    RetainPtr<AVContentKeySession> contentKeySession();

    bool hasContentKeyRequest() const;
    RetainPtr<AVContentKeyRequest> contentKeyRequest() const;

    WeakPtr<LegacyCDMPrivateAVFObjC> m_cdm;
    const WeakPtr<LegacyCDMSessionClient> m_client;
    const RetainPtr<AVContentKeySession> m_contentKeySession;
    const RetainPtr<WebCDMSessionAVContentKeySessionDelegate> m_contentKeySessionDelegate;
    const Ref<WTF::WorkQueue> m_delegateQueue;
    Semaphore m_hasKeyRequestSemaphore;
    mutable Lock m_keyRequestLock;
    RetainPtr<AVContentKeyRequest> m_keyRequest;
    RefPtr<Uint8Array> m_identifier;
    RefPtr<SharedBuffer> m_sourceBufferInitData;
    RefPtr<SharedBuffer> m_initData;
    RetainPtr<NSData> m_expiredSession;
    Vector<int> m_protocolVersions;
    int m_cdmVersion;
    enum { Normal, KeyRelease } m_mode;
    ThreadSafeWeakHashSet<AudioVideoRenderer> m_renderers;

    RefPtr<Uint8Array> m_certificate;
    String m_sessionId;
    bool m_stopped { false };

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CDMSessionAVContentKeySession)
static bool isType(const WebCore::LegacyCDMSession& session) { return session.type() == WebCore::CDMSessionTypeAVContentKeySession; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
