/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef CDMSessionAVFoundationObjC_h
#define CDMSessionAVFoundationObjC_h

#include "CDMSession.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>

#if ENABLE(ENCRYPTED_MEDIA_V2) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090

OBJC_CLASS AVAssetResourceLoadingRequest;

namespace WebCore {

class MediaPlayerPrivateAVFoundationObjC;

class CDMSessionAVFoundationObjC : public CDMSession {
public:
    CDMSessionAVFoundationObjC(MediaPlayerPrivateAVFoundationObjC* parent);
    virtual ~CDMSessionAVFoundationObjC() { }

    virtual CDMSessionType type() { return CDMSessionTypeAVFoundationObjC; }
    virtual void setClient(CDMSessionClient* client) override { m_client = client; }
    virtual const String& sessionId() const override { return m_sessionId; }
    virtual PassRefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, unsigned long& systemCode) override;
    virtual void releaseKeys() override;
    virtual bool update(Uint8Array*, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode) override;

protected:
    MediaPlayerPrivateAVFoundationObjC* m_parent;
    CDMSessionClient* m_client;
    String m_sessionId;
    RetainPtr<AVAssetResourceLoadingRequest> m_request;
};

}

#endif

#endif // CDMSessionAVFoundationObjC_h
