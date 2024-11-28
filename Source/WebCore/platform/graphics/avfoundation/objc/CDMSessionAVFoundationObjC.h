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

#pragma once

#include "LegacyCDMSession.h"
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

OBJC_CLASS AVAssetResourceLoadingRequest;
OBJC_CLASS WebCDMSessionAVFoundationObjCListener;

namespace WebCore {

class MediaPlayerPrivateAVFoundationObjC;

class CDMSessionAVFoundationObjC final : public LegacyCDMSession, public CanMakeWeakPtr<CDMSessionAVFoundationObjC>, public RefCounted<CDMSessionAVFoundationObjC> {
    WTF_MAKE_TZONE_ALLOCATED(CDMSessionAVFoundationObjC);
public:
    static Ref<CDMSessionAVFoundationObjC> create(MediaPlayerPrivateAVFoundationObjC* parent, LegacyCDMSessionClient& client)
    {
        return adoptRef(*new CDMSessionAVFoundationObjC(parent, client));
    }
    virtual ~CDMSessionAVFoundationObjC();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    LegacyCDMSessionType type() override { return CDMSessionTypeAVFoundationObjC; }
    const String& sessionId() const override { return m_sessionId; }
    RefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode) override;
    void releaseKeys() override;
    bool update(Uint8Array*, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode) override;
    RefPtr<ArrayBuffer> cachedKeyForKeyID(const String&) const override;

    void playerDidReceiveError(NSError *);

private:
    CDMSessionAVFoundationObjC(MediaPlayerPrivateAVFoundationObjC* parent, LegacyCDMSessionClient&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "CDMSessionAVFoundationObjC"_s; }
    WTFLogChannel& logChannel() const;
#endif

    ThreadSafeWeakPtr<MediaPlayerPrivateAVFoundationObjC> m_parent;
    WeakPtr<LegacyCDMSessionClient> m_client;
    String m_sessionId;
    RetainPtr<AVAssetResourceLoadingRequest> m_request;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

}

#endif
