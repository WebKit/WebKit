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

#include "LegacyCDMSession.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

namespace WebCore {

class CDMSessionClearKey final : public LegacyCDMSession {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CDMSessionClearKey(LegacyCDMSessionClient*);
    virtual ~CDMSessionClearKey();

    // CDMSessionPrivate
    LegacyCDMSessionType type() override { return CDMSessionTypeClearKey; }
    void setClient(LegacyCDMSessionClient* client) override { m_client = client; }
    const String& sessionId() const override { return m_sessionId; }
    RefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array*, String&, unsigned short&, uint32_t&) override;
    void releaseKeys() override;
    bool update(Uint8Array*, RefPtr<Uint8Array>&, unsigned short&, uint32_t&) override;
    RefPtr<ArrayBuffer> cachedKeyForKeyID(const String&) const override;

protected:
    LegacyCDMSessionClient* m_client;
    RefPtr<Uint8Array> m_initData;
    HashMap<String, Vector<uint8_t>> m_cachedKeys;
    String m_sessionId;
};

} // namespace WebCore

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)
