/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "LegacyCDMSessionClearKey.h"

#include "Logging.h"
#include "WebKitMediaKeyError.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/TypedArrayAdaptors.h>
#include <pal/text/TextEncoding.h>
#include <wtf/JSONValues.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)


namespace WebCore {

CDMSessionClearKey::CDMSessionClearKey(LegacyCDMSessionClient& client)
    : m_client(client)
    , m_sessionId(createVersion4UUIDString())
{
}

CDMSessionClearKey::~CDMSessionClearKey() = default;

RefPtr<Uint8Array> CDMSessionClearKey::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode)
{
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(destinationURL);
    UNUSED_PARAM(systemCode);

    if (!initData) {
        errorCode = WebKitMediaKeyError::MEDIA_KEYERR_CLIENT;
        return nullptr;
    }
    m_initData = initData;

    bool sawError = false;
    String keyID = PAL::UTF8Encoding().decode(static_cast<char*>(m_initData->baseAddress()), m_initData->byteLength(), true, sawError);
    if (sawError) {
        errorCode = WebKitMediaKeyError::MEDIA_KEYERR_CLIENT;
        return nullptr;
    }

    return initData;
}

void CDMSessionClearKey::releaseKeys()
{
    m_cachedKeys.clear();
}

bool CDMSessionClearKey::update(JSC::Uint8Array* rawKeysData, RefPtr<JSC::Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode)
{
    UNUSED_PARAM(nextMessage);
    UNUSED_PARAM(systemCode);
    ASSERT(rawKeysData);

    do {
        auto rawKeysString = String::fromUTF8(rawKeysData->data(), rawKeysData->length());
        if (rawKeysString.isEmpty())  {
            LOG(Media, "CDMSessionClearKey::update(%p) - failed: empty message", this);
            break;
        }

        auto keysDataValue = JSON::Value::parseJSON(rawKeysString);
        if (!keysDataValue || !keysDataValue->asObject()) {
            LOG(Media, "CDMSessionClearKey::update(%p) - failed: invalid JSON", this);
            break;
        }

        auto keysDataObject = keysDataValue->asObject();
        auto keysArray = keysDataObject->getArray("keys"_s);
        if (!keysArray) {
            LOG(Media, "CDMSessionClearKey::update(%p) - failed: keys array missing or empty", this);
            break;
        }

        auto length = keysArray->length();
        if (!length) {
            LOG(Media, "CDMSessionClearKey::update(%p) - failed: keys array missing or empty", this);
            break;
        }

        bool foundValidKey = false;
        for (unsigned i = 0; i < length; ++i) {
            auto keyObject = keysArray->get(i)->asObject();

            if (!keyObject) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: null keyDictionary", this);
                continue;
            }

            auto getStringProperty = [&keyObject](ASCIILiteral name) -> String {
                String string;
                if (!keyObject->getString(name, string))
                    return { };
                return string;
            };

            auto algorithm = getStringProperty("alg"_s);
            if (!equalLettersIgnoringASCIICase(algorithm, "a128kw"_s)) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: algorithm unsupported", this);
                continue;
            }

            auto keyType = getStringProperty("kty"_s);
            if (!equalLettersIgnoringASCIICase(keyType, "oct"_s)) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: keyType unsupported", this);
                continue;
            }

            auto keyId = getStringProperty("kid"_s);
            if (keyId.isEmpty()) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: keyId missing or empty", this);
                continue;
            }

            auto rawKeyData = getStringProperty("k"_s);
            if (rawKeyData.isEmpty())  {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: key missing or empty", this);
                continue;
            }

            auto keyData = base64Decode(rawKeyData);
            if (!keyData || keyData->isEmpty()) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: unable to base64 decode key", this);
                continue;
            }

            m_cachedKeys.set(keyId, WTFMove(*keyData));
            foundValidKey = true;
        }

        if (foundValidKey)
            return true;

    } while (false);

    errorCode = WebKitMediaKeyError::MEDIA_KEYERR_CLIENT;
    return false;
}

RefPtr<JSC::ArrayBuffer> CDMSessionClearKey::cachedKeyForKeyID(const String& keyId) const
{
    if (!m_cachedKeys.contains(keyId))
        return nullptr;

    auto keyData = m_cachedKeys.get(keyId);
    auto keyDataArray = JSC::Uint8Array::create(keyData.data(), keyData.size());
    return keyDataArray->unsharedBuffer();
}

}

#endif
