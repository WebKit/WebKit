/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#ifndef CDMSessionMediaSourceAVFObjC_h
#define CDMSessionMediaSourceAVFObjC_h

#include "CDMSession.h"
#include "SourceBufferPrivateAVFObjC.h"
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

#if ENABLE(ENCRYPTED_MEDIA_V2) && ENABLE(MEDIA_SOURCE)

OBJC_CLASS AVStreamDataParser;
OBJC_CLASS NSError;

namespace WebCore {

class CDMPrivateMediaSourceAVFObjC;

class CDMSessionMediaSourceAVFObjC : public CDMSession, public SourceBufferPrivateAVFObjCErrorClient {
public:
    CDMSessionMediaSourceAVFObjC(CDMPrivateMediaSourceAVFObjC&, CDMSessionClient*);
    virtual ~CDMSessionMediaSourceAVFObjC();

    virtual void addParser(AVStreamDataParser*) = 0;
    virtual void removeParser(AVStreamDataParser*) = 0;

    // CDMSession
    virtual void setClient(CDMSessionClient* client) override { m_client = client; }
    virtual const String& sessionId() const override { return m_sessionId; }

    // SourceBufferPrivateAVFObjCErrorClient
    virtual void layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *, bool& shouldIgnore) override;
    virtual void rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *, bool& shouldIgnore) override;

    void addSourceBuffer(SourceBufferPrivateAVFObjC*);
    void removeSourceBuffer(SourceBufferPrivateAVFObjC*);
    void setSessionId(const String& sessionId) { m_sessionId = sessionId; }

    void invalidateCDM() { m_cdm = nullptr; }

protected:
    String storagePath() const;

    CDMPrivateMediaSourceAVFObjC* m_cdm;
    CDMSessionClient* m_client { nullptr };
    Vector<RefPtr<SourceBufferPrivateAVFObjC>> m_sourceBuffers;
    RefPtr<Uint8Array> m_certificate;
    String m_sessionId;
    bool m_stopped { false };
};

inline CDMSessionMediaSourceAVFObjC* toCDMSessionMediaSourceAVFObjC(CDMSession* session)
{
    if (!session || (session->type() != CDMSessionTypeAVStreamSession && session->type() != CDMSessionTypeAVContentKeySession))
        return nullptr;
    return static_cast<CDMSessionMediaSourceAVFObjC*>(session);
}

}

#endif // ENABLE(ENCRYPTED_MEDIA_V2) && ENABLE(MEDIA_SOURCE)

#endif // CDMSessionMediaSourceAVFObjC_h
