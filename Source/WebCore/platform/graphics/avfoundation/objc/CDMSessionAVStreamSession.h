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

#ifndef CDMSessionAVStreamSession_h
#define CDMSessionAVStreamSession_h

#include "CDMSessionMediaSourceAVFObjC.h"
#include "SourceBufferPrivateAVFObjC.h"
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

#if HAVE(AVSTREAMSESSION) && ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

OBJC_CLASS AVStreamSession;
OBJC_CLASS WebCDMSessionAVStreamSessionObserver;

namespace WebCore {

class CDMPrivateMediaSourceAVFObjC;

class CDMSessionAVStreamSession : public CDMSessionMediaSourceAVFObjC, public CanMakeWeakPtr<CDMSessionAVStreamSession> {
public:
    CDMSessionAVStreamSession(const Vector<int>& protocolVersions, CDMPrivateMediaSourceAVFObjC&, LegacyCDMSessionClient*);
    virtual ~CDMSessionAVStreamSession();

    // LegacyCDMSession
    LegacyCDMSessionType type() override { return CDMSessionTypeAVStreamSession; }
    RefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode) override;
    void releaseKeys() override;
    bool update(Uint8Array*, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode) override;

    // CDMSessionMediaSourceAVFObjC
    void addParser(AVStreamDataParser*) override;
    void removeParser(AVStreamDataParser*) override;

    void setStreamSession(AVStreamSession*);

protected:
    RefPtr<Uint8Array> generateKeyReleaseMessage(unsigned short& errorCode, uint32_t& systemCode);

    RetainPtr<AVStreamSession> m_streamSession;
    RefPtr<Uint8Array> m_initData;
    RefPtr<Uint8Array> m_certificate;
    RetainPtr<NSData> m_expiredSession;
    RetainPtr<WebCDMSessionAVStreamSessionObserver> m_dataParserObserver;
    Vector<int> m_protocolVersions;
    enum { Normal, KeyRelease } m_mode;
};

inline CDMSessionAVStreamSession* toCDMSessionAVStreamSession(LegacyCDMSession* session)
{
    if (!session || session->type() != CDMSessionTypeAVStreamSession)
        return nullptr;
    return static_cast<CDMSessionAVStreamSession*>(session);
}

}

#endif

#endif // CDMSessionAVStreamSession_h
