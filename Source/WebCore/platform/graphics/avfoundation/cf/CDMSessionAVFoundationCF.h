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

#ifndef CDMSessionAVFoundationCF_h
#define CDMSessionAVFoundationCF_h

#include "CDMSession.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RetainPtr.h>

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && ENABLE(ENCRYPTED_MEDIA_V2)

typedef struct OpaqueAVCFAssetResourceLoadingRequest* AVCFAssetResourceLoadingRequestRef;

namespace WebCore {

class MediaPlayerPrivateAVFoundationCF;

class CDMSessionAVFoundationCF : public CDMSession {
public:
    CDMSessionAVFoundationCF(MediaPlayerPrivateAVFoundationCF* parent);
    virtual ~CDMSessionAVFoundationCF() { }

    virtual void setClient(CDMSessionClient* client) override { m_client = client; }
    virtual const String& sessionId() const override { return m_sessionId; }
    virtual PassRefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, unsigned long& systemCode) override;
    virtual void releaseKeys() override;
    virtual bool update(Uint8Array*, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode) override;

protected:
    MediaPlayerPrivateAVFoundationCF* m_parent;
    CDMSessionClient* m_client;
    String m_sessionId;
    RetainPtr<AVCFAssetResourceLoadingRequestRef> m_request;
};

}

#endif

#endif // CDMSessionAVFoundationCF_h
