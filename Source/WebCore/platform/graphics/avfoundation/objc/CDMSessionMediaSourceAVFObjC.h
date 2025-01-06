/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#include "SourceBufferPrivateAVFObjC.h"
#include <wtf/AbstractRefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

OBJC_CLASS AVStreamDataParser;
OBJC_CLASS NSError;

namespace WebCore {

class CDMPrivateMediaSourceAVFObjC;

class CDMSessionMediaSourceAVFObjC : public LegacyCDMSession, public SourceBufferPrivateAVFObjCErrorClient, public CanMakeWeakPtr<CDMSessionMediaSourceAVFObjC> {
    WTF_MAKE_TZONE_ALLOCATED(CDMSessionMediaSourceAVFObjC);
public:
    CDMSessionMediaSourceAVFObjC(CDMPrivateMediaSourceAVFObjC&, LegacyCDMSessionClient&);
    virtual ~CDMSessionMediaSourceAVFObjC();

    virtual void addParser(AVStreamDataParser*) = 0;
    virtual void removeParser(AVStreamDataParser*) = 0;

    // LegacyCDMSession
    const String& sessionId() const override { return m_sessionId; }

    // SourceBufferPrivateAVFObjCErrorClient
    void videoRendererDidReceiveError(WebSampleBufferVideoRendering *, NSError *, bool& shouldIgnore) override;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    void audioRendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *, bool& shouldIgnore) override;
ALLOW_NEW_API_WITHOUT_GUARDS_END

    void addSourceBuffer(SourceBufferPrivateAVFObjC*);
    void removeSourceBuffer(SourceBufferPrivateAVFObjC*);
    void setSessionId(const String& sessionId) { m_sessionId = sessionId; }

    using Keys = Vector<Ref<SharedBuffer>>;
    virtual bool isAnyKeyUsable(const Keys&) const = 0;
    virtual void attachContentKeyToSample(const MediaSampleAVFObjC&) = 0;

    void invalidateCDM() { m_cdm = nullptr; }

protected:
    String storagePath() const;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    WTFLogChannel& logChannel() const;
#endif

    WeakPtr<CDMPrivateMediaSourceAVFObjC> m_cdm;
    WeakPtr<LegacyCDMSessionClient> m_client;
    Vector<RefPtr<SourceBufferPrivateAVFObjC>> m_sourceBuffers;
    RefPtr<Uint8Array> m_certificate;
    String m_sessionId;
    bool m_stopped { false };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

inline CDMSessionMediaSourceAVFObjC* toCDMSessionMediaSourceAVFObjC(LegacyCDMSession* session)
{
    if (!session || session->type() != CDMSessionTypeAVContentKeySession)
        return nullptr;
    return static_cast<CDMSessionMediaSourceAVFObjC*>(session);
}

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)
