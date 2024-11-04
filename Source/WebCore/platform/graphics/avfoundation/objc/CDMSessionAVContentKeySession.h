/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "CDMSessionMediaSourceAVFObjC.h"
#include "SourceBufferPrivateAVFObjC.h"
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WTFSemaphore.h>

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

OBJC_CLASS AVContentKeyRequest;
OBJC_CLASS AVContentKeySession;
OBJC_CLASS WebCDMSessionAVContentKeySessionDelegate;

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class CDMSessionAVContentKeySession;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::CDMSessionAVContentKeySession> : std::true_type { };
}

namespace WebCore {

class CDMPrivateMediaSourceAVFObjC;

class CDMSessionAVContentKeySession : public CDMSessionMediaSourceAVFObjC, public RefCounted<CDMSessionAVContentKeySession> {
    WTF_MAKE_TZONE_ALLOCATED(CDMSessionAVContentKeySession);
public:
    static Ref<CDMSessionAVContentKeySession> create(Vector<int>&& protocolVersions, int cdmVersion, CDMPrivateMediaSourceAVFObjC& parent, LegacyCDMSessionClient& client)
    {
        return adoptRef(*new CDMSessionAVContentKeySession(WTFMove(protocolVersions), cdmVersion, parent, client));
    }

    virtual ~CDMSessionAVContentKeySession();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static bool isAvailable();

    // LegacyCDMSession
    LegacyCDMSessionType type() override { return CDMSessionTypeAVContentKeySession; }
    RefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode) override;
    void releaseKeys() override;
    bool update(Uint8Array* key, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode) override;
    RefPtr<ArrayBuffer> cachedKeyForKeyID(const String&) const override;

    // CDMSessionMediaSourceAVFObjC
    void addParser(AVStreamDataParser *) override;
    void removeParser(AVStreamDataParser *) override;
    bool isAnyKeyUsable(const Keys&) const override;
    void attachContentKeyToSample(const MediaSampleAVFObjC&) override;

    void didProvideContentKeyRequest(AVContentKeyRequest *);

    bool hasContentKeySession() const { return m_contentKeySession; }
    AVContentKeySession* contentKeySession();

    bool hasContentKeyRequest() const;
    RetainPtr<AVContentKeyRequest> contentKeyRequest() const;

protected:
    CDMSessionAVContentKeySession(Vector<int>&& protocolVersions, int cdmVersion, CDMPrivateMediaSourceAVFObjC&, LegacyCDMSessionClient&);

    RefPtr<Uint8Array> generateKeyReleaseMessage(unsigned short& errorCode, uint32_t& systemCode);

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const { return "CDMSessionAVContentKeySession"_s; }
#endif

    RetainPtr<AVContentKeySession> m_contentKeySession;
    RetainPtr<WebCDMSessionAVContentKeySessionDelegate> m_contentKeySessionDelegate;
    Ref<WTF::WorkQueue> m_delegateQueue;
    Semaphore m_hasKeyRequestSemaphore;
    mutable Lock m_keyRequestLock;
    RetainPtr<AVContentKeyRequest> m_keyRequest;
    RefPtr<Uint8Array> m_identifier;
    RefPtr<SharedBuffer> m_initData;
    RetainPtr<NSData> m_expiredSession;
    Vector<int> m_protocolVersions;
    int m_cdmVersion;
    int32_t m_protectedTrackID { 1 };
    enum { Normal, KeyRelease } m_mode;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

inline CDMSessionAVContentKeySession* toCDMSessionAVContentKeySession(LegacyCDMSession* session)
{
    if (!session || session->type() != CDMSessionTypeAVContentKeySession)
        return nullptr;
    return static_cast<CDMSessionAVContentKeySession*>(session);
}

}

#endif
