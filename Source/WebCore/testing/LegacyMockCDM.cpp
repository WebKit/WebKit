/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LegacyMockCDM.h"

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "LegacyCDM.h"
#include "LegacyCDMSession.h"
#include "WebKitMediaKeyError.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint8Array.h>

namespace WebCore {

class MockCDMSession : public LegacyCDMSession {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MockCDMSession(LegacyCDMSessionClient&);
    virtual ~MockCDMSession() = default;

    const String& sessionId() const override { return m_sessionId; }
    RefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode) override;
    void releaseKeys() override;
    bool update(Uint8Array*, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode) override;
    RefPtr<ArrayBuffer> cachedKeyForKeyID(const String&) const override { return nullptr; }

protected:
    WeakPtr<LegacyCDMSessionClient> m_client;
    String m_sessionId;
};

bool LegacyMockCDM::supportsKeySystem(const String& keySystem)
{
    return equalLettersIgnoringASCIICase(keySystem, "com.webcore.mock"_s);
}

bool LegacyMockCDM::supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType)
{
    if (!supportsKeySystem(keySystem))
        return false;

    return equalLettersIgnoringASCIICase(mimeType, "video/mock"_s);
}

bool LegacyMockCDM::supportsMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "video/mock"_s);
}

std::unique_ptr<LegacyCDMSession> LegacyMockCDM::createSession(LegacyCDMSessionClient& client)
{
    return makeUnique<MockCDMSession>(client);
}

static Uint8Array* initDataPrefix()
{
    const unsigned char prefixData[] = { 'm', 'o', 'c', 'k' };
    static Uint8Array& prefix { Uint8Array::create(prefixData, std::size(prefixData)).leakRef() };

    return &prefix;
}

static Uint8Array* keyPrefix()
{
    static const unsigned char prefixData[] = {'k', 'e', 'y'};
    static Uint8Array& prefix { Uint8Array::create(prefixData, std::size(prefixData)).leakRef() };

    return &prefix;
}

static Uint8Array* keyRequest()
{
    static const unsigned char requestData[] = {'r', 'e', 'q', 'u', 'e', 's', 't'};
    static Uint8Array& request { Uint8Array::create(requestData, std::size(requestData)).leakRef() };

    return &request;
}

static String generateSessionId()
{
    static int monotonicallyIncreasingSessionId = 0;
    return String::number(monotonicallyIncreasingSessionId++);
}

MockCDMSession::MockCDMSession(LegacyCDMSessionClient& client)
    : m_client(client)
    , m_sessionId(generateSessionId())
{
}

RefPtr<Uint8Array> MockCDMSession::generateKeyRequest(const String&, Uint8Array* initData, String&, unsigned short& errorCode, uint32_t&)
{
    for (unsigned i = 0; i < initDataPrefix()->length(); ++i) {
        if (!initData || i >= initData->length() || initData->item(i) != initDataPrefix()->item(i)) {
            errorCode = WebKitMediaKeyError::MEDIA_KEYERR_UNKNOWN;
            return nullptr;
        }
    }
    return keyRequest();
}

void MockCDMSession::releaseKeys()
{
    // no-op
}

bool MockCDMSession::update(Uint8Array* key, RefPtr<Uint8Array>&, unsigned short& errorCode, uint32_t&)
{
    for (unsigned i = 0; i < keyPrefix()->length(); ++i) {
        if (i >= key->length() || key->item(i) != keyPrefix()->item(i)) {
            errorCode = WebKitMediaKeyError::MEDIA_KEYERR_CLIENT;
            return false;
        }
    }
    return true;
}

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)
