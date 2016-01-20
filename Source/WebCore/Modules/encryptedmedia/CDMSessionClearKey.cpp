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

#include "config.h"
#include "CDMSessionClearKey.h"

#include "ArrayValue.h"
#include "CryptoAlgorithm.h"
#include "CryptoAlgorithmIdentifier.h"
#include "CryptoAlgorithmParameters.h"
#include "CryptoKeyDataOctetSequence.h"
#include "Dictionary.h"
#include "JSMainThreadExecState.h"
#include "Logging.h"
#include "MediaKeyError.h"
#include "TextEncoding.h"
#include "UUID.h"
#include <runtime/JSGlobalObject.h>
#include <runtime/JSLock.h>
#include <runtime/JSONObject.h>
#include <runtime/VM.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>

#if ENABLE(ENCRYPTED_MEDIA_V2)

using namespace JSC;

namespace WebCore {

static VM& clearKeyVM()
{
    static NeverDestroyed<RefPtr<VM>> vm;
    if (!vm.get())
        vm.get() = VM::create();

    return *vm.get();
}

CDMSessionClearKey::CDMSessionClearKey(CDMSessionClient* client)
    : m_client(client)
    , m_sessionId(createCanonicalUUIDString())
{
}

CDMSessionClearKey::~CDMSessionClearKey()
{
}

RefPtr<Uint8Array> CDMSessionClearKey::generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, unsigned long& systemCode)
{
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(destinationURL);
    UNUSED_PARAM(systemCode);

    if (!initData) {
        errorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        return nullptr;
    }
    m_initData = initData;

    bool sawError = false;
    String keyID = UTF8Encoding().decode(reinterpret_cast_ptr<char*>(m_initData->baseAddress()), m_initData->byteLength(), true, sawError);
    if (sawError) {
        errorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        return nullptr;
    }

    return initData;
}

void CDMSessionClearKey::releaseKeys()
{
    m_cachedKeys.clear();
}

bool CDMSessionClearKey::update(Uint8Array* rawKeysData, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, unsigned long& systemCode)
{
    UNUSED_PARAM(nextMessage);
    UNUSED_PARAM(systemCode);
    ASSERT(rawKeysData);

    do {
        String rawKeysString = String::fromUTF8(rawKeysData->data(), rawKeysData->length());
        if (rawKeysString.isEmpty())  {
            LOG(Media, "CDMSessionClearKey::update(%p) - failed: empty message", this);
            continue;
        }

        VM& vm = clearKeyVM();
        JSLockHolder lock(vm);
        JSGlobalObject* globalObject = JSGlobalObject::create(vm, JSGlobalObject::createStructure(vm, jsNull()));
        ExecState* exec = globalObject->globalExec();

        JSLockHolder locker(clearKeyVM());
        JSValue keysDataObject = JSONParse(exec, rawKeysString);
        if (exec->hadException() || !keysDataObject) {
            LOG(Media, "CDMSessionClearKey::update(%p) - failed: invalid JSON", this);
            break;
        }
        Dictionary keysDataDictionary(exec, keysDataObject);
        ArrayValue keysArray;
        size_t length;
        if (!keysDataDictionary.get("keys", keysArray) || keysArray.isUndefinedOrNull() || !keysArray.length(length) || !length) {
            LOG(Media, "CDMSessionClearKey::update(%p) - failed: keys array missing or empty", this);
            break;
        }

        bool foundValidKey = false;
        for (size_t i = 0; i < length; ++i) {
            Dictionary keyDictionary;
            if (!keysArray.get(i, keyDictionary) || keyDictionary.isUndefinedOrNull()) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: null keyDictionary", this);
                continue;
            }

            String algorithm;
            if (!keyDictionary.get("alg", algorithm) || !equalIgnoringCase(algorithm, "a128kw")) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: algorithm unsupported", this);
                continue;
            }

            String keyType;
            if (!keyDictionary.get("kty", keyType) || !equalIgnoringCase(keyType, "oct")) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: keyType unsupported", this);
                continue;
            }

            String keyId;
            if (!keyDictionary.get("kid", keyId) || keyId.isEmpty()) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: keyId missing or empty", this);
                continue;
            }

            String rawKeyData;
            if (!keyDictionary.get("k", rawKeyData) || rawKeyData.isEmpty())  {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: key missing or empty", this);
                continue;
            }

            Vector<uint8_t> keyData;
            if (!base64Decode(rawKeyData, keyData) ||  keyData.isEmpty()) {
                LOG(Media, "CDMSessionClearKey::update(%p) - failed: unable to base64 decode key", this);
                continue;
            }

            m_cachedKeys.set(keyId, WTFMove(keyData));
            foundValidKey = true;
        }

        if (foundValidKey)
            return true;

    } while (false);

    errorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
    return false;
}

RefPtr<ArrayBuffer> CDMSessionClearKey::cachedKeyForKeyID(const String& keyId) const
{
    if (!m_cachedKeys.contains(keyId))
        return nullptr;

    auto keyData = m_cachedKeys.get(keyId);
    RefPtr<Uint8Array> keyDataArray = Uint8Array::create(keyData.data(), keyData.size());
    return keyDataArray->buffer();
}

}

#endif
