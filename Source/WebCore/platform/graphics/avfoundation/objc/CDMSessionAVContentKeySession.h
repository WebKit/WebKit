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
#include <wtf/RetainPtr.h>

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

OBJC_CLASS AVContentKeyRequest;
OBJC_CLASS AVContentKeySession;
OBJC_CLASS WebCDMSessionAVContentKeySessionDelegate;

namespace WebCore {

class CDMPrivateMediaSourceAVFObjC;

class CDMSessionAVContentKeySession : public CDMSessionMediaSourceAVFObjC {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CDMSessionAVContentKeySession(Vector<int>&& protocolVersions, int cdmVersion, CDMPrivateMediaSourceAVFObjC&, LegacyCDMSessionClient&);
    virtual ~CDMSessionAVContentKeySession();

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

    void didProvideContentKeyRequest(AVContentKeyRequest *);

protected:
    RefPtr<Uint8Array> generateKeyReleaseMessage(unsigned short& errorCode, uint32_t& systemCode);

    bool hasContentKeySession() const { return m_contentKeySession; }
    AVContentKeySession* contentKeySession();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const { return "CDMSessionAVContentKeySession"; }
#endif

    RetainPtr<AVContentKeySession> m_contentKeySession;
    RetainPtr<WebCDMSessionAVContentKeySessionDelegate> m_contentKeySessionDelegate;
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
    const void* m_logIdentifier;
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
