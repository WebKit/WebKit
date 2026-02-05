/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "SubtleCrypto.h"

#include "ContextDestructionObserverInlines.h"
#include "CryptoAlgorithm.h"
#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoAlgorithmAesCtrParams.h"
#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoAlgorithmAesKeyParams.h"
#include "CryptoAlgorithmEcKeyParams.h"
#include "CryptoAlgorithmEcdhKeyDeriveParams.h"
#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoAlgorithmHmacKeyParams.h"
#include "CryptoAlgorithmPbkdf2Params.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoAlgorithmRsaHashedImportParams.h"
#include "CryptoAlgorithmRsaHashedKeyGenParams.h"
#include "CryptoAlgorithmRsaKeyGenParams.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoAlgorithmRsaPssParams.h"
#include "CryptoAlgorithmX25519Params.h"
#include "JSAesCbcCfbParams.h"
#include "JSAesCtrParams.h"
#include "JSAesGcmParams.h"
#include "JSAesKeyParams.h"
#include "JSCryptoAlgorithmParameters.h"
#include "JSCryptoKey.h"
#include "JSCryptoKeyPair.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWrapper.h"
#include "JSEcKeyParams.h"
#include "JSEcdhKeyDeriveParams.h"
#include "JSEcdsaParams.h"
#include "JSHkdfParams.h"
#include "JSHmacKeyParams.h"
#include "JSJsonWebKey.h"
#include "JSPbkdf2Params.h"
#include "JSRsaHashedImportParams.h"
#include "JSRsaHashedKeyGenParams.h"
#include "JSRsaKeyGenParams.h"
#include "JSRsaOaepParams.h"
#include "JSRsaPssParams.h"
#include "JSX25519Params.h"
#include "Settings.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/JSONObject.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
using namespace JSC;

SubtleCrypto::SubtleCrypto(ScriptExecutionContext* context)
    : ContextDestructionObserver(context)
    , m_workQueue(WorkQueue::create("com.apple.WebKit.CryptoQueue"_s))
{
}

SubtleCrypto::~SubtleCrypto() = default;

enum class Operations : uint8_t {
    Encrypt,
    Decrypt,
    Sign,
    Verify,
    Digest,
    GenerateKey,
    DeriveBits,
    ImportKey,
    WrapKey,
    UnwrapKey,
    GetKeyLength
};

constexpr auto AESCFBDeprecation = "AES-CFB support is deprecated"_s;
constexpr auto RSAESPKCS1Deprecation = "RSAES-PKCS1-v1_5 support is deprecated"_s;

static ExceptionOr<std::unique_ptr<CryptoAlgorithmParameters>> normalizeCryptoAlgorithmParameters(JSGlobalObject&, WebCore::SubtleCrypto::AlgorithmIdentifier, Operations);

static ExceptionOr<CryptoAlgorithmIdentifier> toHashIdentifier(JSGlobalObject& state, SubtleCrypto::AlgorithmIdentifier algorithmIdentifier)
{
    auto digestParams = normalizeCryptoAlgorithmParameters(state, algorithmIdentifier, Operations::Digest);
    if (digestParams.hasException())
        return digestParams.releaseException();
    return digestParams.returnValue()->identifier;
}

static bool isSafeCurvesEnabled(JSGlobalObject& state)
{
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(&state);
    RefPtr context = globalObject.scriptExecutionContext();
    return context && context->settingsValues().webCryptoSafeCurvesEnabled;
}

static bool isX25519Enabled(JSGlobalObject& state)
{
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(&state);
    RefPtr context = globalObject.scriptExecutionContext();
    return context && context->settingsValues().webCryptoX25519Enabled;
}

template<typename T, typename... Rest>
static std::unique_ptr<CryptoAlgorithmParameters> makeParameters(Rest&&... rest)
{
    return makeUnique<T>(std::forward<Rest>(rest)...);
}

static ExceptionOr<std::unique_ptr<CryptoAlgorithmParameters>> normalizeCryptoAlgorithmParameters(JSGlobalObject& state, SubtleCrypto::AlgorithmIdentifier algorithmIdentifier, Operations operation)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (std::holds_alternative<String>(algorithmIdentifier)) {
        auto newParams = Strong<JSObject>(vm, constructEmptyObject(&state));
        newParams->putDirect(vm, Identifier::fromString(vm, "name"_s), jsString(vm, std::get<String>(algorithmIdentifier)));
        
        return normalizeCryptoAlgorithmParameters(state, newParams, operation);
    }

    auto& value = std::get<JSC::Strong<JSC::JSObject>>(algorithmIdentifier);

    auto params = convertDictionary<CryptoAlgorithmParametersInit>(state, value.get());
    if (params.hasException(scope)) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };
    if (equalIgnoringASCIICase(params.returnValue().name, "RSAES-PKCS1-v1_5"_s))
        return Exception { ExceptionCode::NotSupportedError, RSAESPKCS1Deprecation };

    auto identifier = CryptoAlgorithmRegistry::singleton().identifier(params.returnValue().name);
    if (!identifier) [[unlikely]]
        return Exception { ExceptionCode::NotSupportedError };

    if (*identifier == CryptoAlgorithmIdentifier::Ed25519 && !isSafeCurvesEnabled(state))
        return Exception { ExceptionCode::NotSupportedError };

    if (*identifier == CryptoAlgorithmIdentifier::X25519 && !isX25519Enabled(state))
        return Exception { ExceptionCode::NotSupportedError };

    switch (operation) {
    case Operations::Encrypt:
    case Operations::Decrypt:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
            return Exception { ExceptionCode::NotSupportedError, RSAESPKCS1Deprecation };
        case CryptoAlgorithmIdentifier::RSA_OAEP: {
            auto params = convertDictionary<CryptoAlgorithmRsaOaepParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmRsaOaepParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::AES_CFB:
            return Exception { ExceptionCode::NotSupportedError, AESCFBDeprecation };
        case CryptoAlgorithmIdentifier::AES_CBC: {
            auto params = convertDictionary<CryptoAlgorithmAesCbcCfbParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmAesCbcCfbParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::AES_CTR: {
            auto params = convertDictionary<CryptoAlgorithmAesCtrParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmAesCtrParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::AES_GCM: {
            auto params = convertDictionary<CryptoAlgorithmAesGcmParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmAesGcmParams>(*identifier, params.releaseReturnValue());
        }
        default:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Operations::Sign:
    case Operations::Verify:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
        case CryptoAlgorithmIdentifier::HMAC:
        case CryptoAlgorithmIdentifier::Ed25519:
            return makeParameters<CryptoAlgorithmParameters>(*identifier, params.releaseReturnValue());
        case CryptoAlgorithmIdentifier::ECDSA: {
            auto params = convertDictionary<CryptoAlgorithmEcdsaParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmEcdsaParams>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::RSA_PSS: {
            auto params = convertDictionary<CryptoAlgorithmRsaPssParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmRsaPssParams>(*identifier, params.releaseReturnValue());
        }
        default:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Operations::Digest:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::SHA_1:
        case CryptoAlgorithmIdentifier::SHA_256:
        case CryptoAlgorithmIdentifier::SHA_384:
        case CryptoAlgorithmIdentifier::SHA_512:
            return makeParameters<CryptoAlgorithmParameters>(*identifier, params.releaseReturnValue());
        case CryptoAlgorithmIdentifier::DEPRECATED_SHA_224:
            RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE(sha224DeprecationMessage);
        default:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Operations::GenerateKey:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
            return Exception { ExceptionCode::NotSupportedError, RSAESPKCS1Deprecation };
        case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
        case CryptoAlgorithmIdentifier::RSA_PSS:
        case CryptoAlgorithmIdentifier::RSA_OAEP: {
            auto params = convertDictionary<CryptoAlgorithmRsaHashedKeyGenParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmRsaHashedKeyGenParams>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::AES_CFB:
            return Exception { ExceptionCode::NotSupportedError, AESCFBDeprecation };
        case CryptoAlgorithmIdentifier::AES_CTR:
        case CryptoAlgorithmIdentifier::AES_CBC:
        case CryptoAlgorithmIdentifier::AES_GCM:
        case CryptoAlgorithmIdentifier::AES_KW: {
            auto params = convertDictionary<CryptoAlgorithmAesKeyParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmAesKeyParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::HMAC: {
            auto params = convertDictionary<CryptoAlgorithmHmacKeyParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmHmacKeyParams>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::ECDSA:
        case CryptoAlgorithmIdentifier::ECDH: {
            auto params = convertDictionary<CryptoAlgorithmEcKeyParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmEcKeyParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::X25519:
            return makeParameters<CryptoAlgorithmParameters>(*identifier);
        case CryptoAlgorithmIdentifier::Ed25519:
            return makeParameters<CryptoAlgorithmParameters>(*identifier, params.releaseReturnValue());
        default:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Operations::DeriveBits:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::ECDH: {
            // Remove this hack once https://bugs.webkit.org/show_bug.cgi?id=169333 is fixed.
            JSValue nameValue = value.get()->get(&state, Identifier::fromString(vm, "name"_s));
            JSValue publicValue = value.get()->get(&state, Identifier::fromString(vm, "public"_s));
            JSObject* newValue = constructEmptyObject(&state);
            newValue->putDirect(vm, Identifier::fromString(vm, "name"_s), nameValue);
            newValue->putDirect(vm, Identifier::fromString(vm, "publicKey"_s), publicValue);

            auto params = convertDictionary<CryptoAlgorithmEcdhKeyDeriveParamsInit>(state, newValue);
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmEcdhKeyDeriveParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::X25519: {
            // Remove this hack once https://bugs.webkit.org/show_bug.cgi?id=169333 is fixed.
            JSValue nameValue = value.get()->get(&state, Identifier::fromString(vm, "name"_s));
            JSValue publicValue = value.get()->get(&state, Identifier::fromString(vm, "public"_s));
            JSObject* newValue = constructEmptyObject(&state);
            newValue->putDirect(vm, Identifier::fromString(vm, "name"_s), nameValue);
            newValue->putDirect(vm, Identifier::fromString(vm, "publicKey"_s), publicValue);

            auto params = convertDictionary<CryptoAlgorithmX25519ParamsInit>(state, newValue);
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmX25519Params>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::HKDF: {
            auto params = convertDictionary<CryptoAlgorithmHkdfParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmHkdfParams>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::PBKDF2: {
            auto params = convertDictionary<CryptoAlgorithmPbkdf2ParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmPbkdf2Params>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        default:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Operations::ImportKey:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
            return Exception { ExceptionCode::NotSupportedError, RSAESPKCS1Deprecation };
        case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
        case CryptoAlgorithmIdentifier::RSA_PSS:
        case CryptoAlgorithmIdentifier::RSA_OAEP: {
            auto params = convertDictionary<CryptoAlgorithmRsaHashedImportParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmRsaHashedImportParams>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::AES_CFB:
            return Exception { ExceptionCode::NotSupportedError, AESCFBDeprecation };
        case CryptoAlgorithmIdentifier::AES_CTR:
        case CryptoAlgorithmIdentifier::AES_CBC:
        case CryptoAlgorithmIdentifier::AES_GCM:
        case CryptoAlgorithmIdentifier::AES_KW:
        case CryptoAlgorithmIdentifier::Ed25519:
            return makeParameters<CryptoAlgorithmParameters>(*identifier, params.releaseReturnValue());
        case CryptoAlgorithmIdentifier::HMAC: {
            auto params = convertDictionary<CryptoAlgorithmHmacKeyParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmHmacKeyParams>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::ECDSA:
        case CryptoAlgorithmIdentifier::ECDH: {
            auto params = convertDictionary<CryptoAlgorithmEcKeyParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmEcKeyParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::X25519:
            return makeParameters<CryptoAlgorithmParameters>(*identifier);
        case CryptoAlgorithmIdentifier::HKDF:
        case CryptoAlgorithmIdentifier::PBKDF2:
            return makeParameters<CryptoAlgorithmParameters>(*identifier, params.releaseReturnValue());
        case CryptoAlgorithmIdentifier::SHA_1:
        case CryptoAlgorithmIdentifier::DEPRECATED_SHA_224:
        case CryptoAlgorithmIdentifier::SHA_256:
        case CryptoAlgorithmIdentifier::SHA_384:
        case CryptoAlgorithmIdentifier::SHA_512:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Operations::WrapKey:
    case Operations::UnwrapKey:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::AES_KW:
            return makeParameters<CryptoAlgorithmParameters>(*identifier, params.releaseReturnValue());
        default:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    case Operations::GetKeyLength:
        switch (*identifier) {
        case CryptoAlgorithmIdentifier::AES_CFB:
            return Exception { ExceptionCode::NotSupportedError, AESCFBDeprecation };
        case CryptoAlgorithmIdentifier::AES_CTR:
        case CryptoAlgorithmIdentifier::AES_CBC:
        case CryptoAlgorithmIdentifier::AES_GCM:
        case CryptoAlgorithmIdentifier::AES_KW: {
            auto params = convertDictionary<CryptoAlgorithmAesKeyParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            return makeParameters<CryptoAlgorithmAesKeyParams>(*identifier, params.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::HMAC: {
            auto params = convertDictionary<CryptoAlgorithmHmacKeyParamsInit>(state, value.get());
            if (params.hasException(scope)) [[unlikely]]
                return Exception { ExceptionCode::ExistingExceptionError };
            auto hashIdentifier = toHashIdentifier(state, params.returnValue().hash);
            if (hashIdentifier.hasException()) [[unlikely]]
                return hashIdentifier.releaseException();
            return makeParameters<CryptoAlgorithmHmacKeyParams>(*identifier, params.releaseReturnValue(), hashIdentifier.releaseReturnValue());
        }
        case CryptoAlgorithmIdentifier::HKDF:
        case CryptoAlgorithmIdentifier::PBKDF2:
            return makeParameters<CryptoAlgorithmParameters>(*identifier, params.releaseReturnValue());
        default:
            return Exception { ExceptionCode::NotSupportedError };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static CryptoKeyUsageBitmap toCryptoKeyUsageBitmap(CryptoKeyUsage usage)
{
    switch (usage) {
    case CryptoKeyUsage::Encrypt:
        return CryptoKeyUsageEncrypt;
    case CryptoKeyUsage::Decrypt:
        return CryptoKeyUsageDecrypt;
    case CryptoKeyUsage::Sign:
        return CryptoKeyUsageSign;
    case CryptoKeyUsage::Verify:
        return CryptoKeyUsageVerify;
    case CryptoKeyUsage::DeriveKey:
        return CryptoKeyUsageDeriveKey;
    case CryptoKeyUsage::DeriveBits:
        return CryptoKeyUsageDeriveBits;
    case CryptoKeyUsage::WrapKey:
        return CryptoKeyUsageWrapKey;
    case CryptoKeyUsage::UnwrapKey:
        return CryptoKeyUsageUnwrapKey;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static CryptoKeyUsageBitmap toCryptoKeyUsageBitmap(const Vector<CryptoKeyUsage>& usages)
{
    CryptoKeyUsageBitmap result = 0;
    // Maybe we shouldn't silently bypass duplicated usages?
    for (auto usage : usages)
        result |= toCryptoKeyUsageBitmap(usage);

    return result;
}

// Maybe we want more specific error messages?
static void rejectWithException(Ref<DeferredPromise>&& passedPromise, ExceptionCode ec)
{
    switch (ec) {
    case ExceptionCode::NotSupportedError:
        passedPromise->reject(ec, "The algorithm is not supported"_s);
        return;
    case ExceptionCode::SyntaxError:
        passedPromise->reject(ec, "A required parameter was missing or out-of-range"_s);
        return;
    case ExceptionCode::InvalidStateError:
        passedPromise->reject(ec, "The requested operation is not valid for the current state of the provided key"_s);
        return;
    case ExceptionCode::InvalidAccessError:
        passedPromise->reject(ec, "The requested operation is not valid for the provided key"_s);
        return;
    case ExceptionCode::UnknownError:
        passedPromise->reject(ec, "The operation failed for an unknown transient reason (e.g. out of memory)"_s);
        return;
    case ExceptionCode::DataError:
        passedPromise->reject(ec, "Data provided to an operation does not meet requirements"_s);
        return;
    case ExceptionCode::OperationError:
        passedPromise->reject(ec, "The operation failed for an operation-specific reason"_s);
        return;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
}

static void normalizeJsonWebKey(JsonWebKey& webKey)
{
    // Maybe we shouldn't silently bypass duplicated usages?
    webKey.usages = webKey.key_ops ? toCryptoKeyUsageBitmap(webKey.key_ops.value()) : 0;
}

// FIXME: This returns an std::optional<KeyData> and takes a promise, rather than returning an
// ExceptionOr<KeyData> and letting the caller handle the promise, to work around an issue where
// Variant types (which KeyData is) in ExceptionOr<> cause compile issues on some platforms. This
// should be resolved by adopting a standards compliant Variant (see https://webkit.org/b/175583)
static std::optional<KeyData> toKeyData(SubtleCrypto::KeyFormat format, SubtleCrypto::KeyDataVariant&& keyDataVariant, Ref<DeferredPromise>& promise)
{
    switch (format) {
    case SubtleCrypto::KeyFormat::Spki:
    case SubtleCrypto::KeyFormat::Pkcs8:
    case SubtleCrypto::KeyFormat::Raw:
        return WTF::switchOn(keyDataVariant,
            [&promise] (JsonWebKey&) -> std::optional<KeyData> {
                promise->reject(Exception { ExceptionCode::TypeError });
                return std::nullopt;
            },
            [] (auto& bufferSource) -> std::optional<KeyData> {
                return KeyData { Vector(bufferSource->span()) };
            }
        );
    case SubtleCrypto::KeyFormat::Jwk:
        return WTF::switchOn(keyDataVariant,
            [] (JsonWebKey& webKey) -> std::optional<KeyData> {
                normalizeJsonWebKey(webKey);
                return KeyData { webKey };
            },
            [&promise] (auto&) -> std::optional<KeyData> {
                promise->reject(Exception { ExceptionCode::TypeError });
                return std::nullopt;
            }
        );
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static Vector<uint8_t> copyToVector(BufferSource&& data)
{
    return data.span();
}

static bool isSupportedExportKey(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        return false;
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_KW:
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::X25519:
    case CryptoAlgorithmIdentifier::Ed25519:
        return true;
    default:
        return false;
    }
}

RefPtr<DeferredPromise> getPromise(DeferredPromise* index, WeakPtr<SubtleCrypto> weakThis)
{
    if (weakThis)
        return weakThis->m_pendingPromises.take(index);
    return nullptr;
}

static std::unique_ptr<CryptoAlgorithmParameters> crossThreadCopyImportParams(const CryptoAlgorithmParameters& importParams)
{
    switch (importParams.parametersClass()) {
    case CryptoAlgorithmParameters::Class::None:
        return makeParameters<CryptoAlgorithmParameters>(importParams.identifier);
    case CryptoAlgorithmParameters::Class::EcKeyParams:
        return makeParameters<CryptoAlgorithmEcKeyParams>(crossThreadCopy(downcast<CryptoAlgorithmEcKeyParams>(importParams)));
    case CryptoAlgorithmParameters::Class::HmacKeyParams:
        return makeParameters<CryptoAlgorithmHmacKeyParams>(crossThreadCopy(downcast<CryptoAlgorithmHmacKeyParams>(importParams)));
    case CryptoAlgorithmParameters::Class::RsaHashedImportParams:
        return makeParameters<CryptoAlgorithmRsaHashedImportParams>(crossThreadCopy(downcast<CryptoAlgorithmRsaHashedImportParams>(importParams)));
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

void SubtleCrypto::addAuthenticatedEncryptionWarningIfNecessary(CryptoAlgorithmIdentifier algorithmIdentifier)
{
    if (algorithmIdentifier == CryptoAlgorithmIdentifier::AES_CBC || algorithmIdentifier == CryptoAlgorithmIdentifier::AES_CTR) {
        if (RefPtr context = scriptExecutionContext(); !context->hasLoggedAuthenticatedEncryptionWarning()) {
            context->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "AES-CBC and AES-CTR do not provide authentication by default, and implementing it manually can result in minor, but serious mistakes. We recommended using authenticated encryption like AES-GCM to protect against chosen-ciphertext attacks."_s);
            context->setHasLoggedAuthenticatedEncryptionWarning(true);
        }
    }
}

// MARK: - Exposed functions.

void SubtleCrypto::encrypt(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, CryptoKey& key, BufferSource&& dataBufferSource, Ref<DeferredPromise>&& promise)
{
    addAuthenticatedEncryptionWarningIfNecessary(key.algorithmIdentifier());

    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::Encrypt);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto data = copyToVector(WTF::move(dataBufferSource));

    if (params->identifier != key.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't match AlgorithmIdentifier"_s);
        return;
    }

    if (!key.allows(CryptoKeyUsageEncrypt)) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't support encryption"_s);
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key.algorithmIdentifier());

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](const Vector<uint8_t>& cipherText) mutable {
        if (auto promise = getPromise(index, weakThis))
            fulfillPromiseWithArrayBufferFromSpan(promise.releaseNonNull(), cipherText.span());
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    algorithm->encrypt(*params, key, WTF::move(data), WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

void SubtleCrypto::decrypt(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, CryptoKey& key, BufferSource&& dataBufferSource, Ref<DeferredPromise>&& promise)
{
    addAuthenticatedEncryptionWarningIfNecessary(key.algorithmIdentifier());

    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::Decrypt);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto data = copyToVector(WTF::move(dataBufferSource));

    if (params->identifier != key.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't match AlgorithmIdentifier"_s);
        return;
    }

    if (!key.allows(CryptoKeyUsageDecrypt)) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't support decryption"_s);
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key.algorithmIdentifier());

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](const Vector<uint8_t>& plainText) mutable {
        if (auto promise = getPromise(index, weakThis))
            fulfillPromiseWithArrayBufferFromSpan(promise.releaseNonNull(), plainText.span());
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    algorithm->decrypt(*params, key, WTF::move(data), WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

void SubtleCrypto::sign(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, CryptoKey& key, BufferSource&& dataBufferSource, Ref<DeferredPromise>&& promise)
{
    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::Sign);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto data = copyToVector(WTF::move(dataBufferSource));

    if (params->identifier != key.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't match AlgorithmIdentifier"_s);
        return;
    }

    if (!key.allows(CryptoKeyUsageSign)) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't support signing"_s);
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key.algorithmIdentifier());

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](const Vector<uint8_t>& signature) mutable {
        if (auto promise = getPromise(index, weakThis))
            fulfillPromiseWithArrayBufferFromSpan(promise.releaseNonNull(), signature.span());
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    algorithm->sign(*params, key, WTF::move(data), WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

void SubtleCrypto::doVerify(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, CryptoKey& key, BufferSource&& signatureBufferSource, BufferSource&& dataBufferSource, Ref<DeferredPromise>&& promise)
{
    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::Verify);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto signature = copyToVector(WTF::move(signatureBufferSource));
    auto data = copyToVector(WTF::move(dataBufferSource));

    if (params->identifier != key.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't match AlgorithmIdentifier"_s);
        return;
    }

    if (!key.allows(CryptoKeyUsageVerify)) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't support verification"_s);
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key.algorithmIdentifier());

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](bool result) mutable {
        if (auto promise = getPromise(index, weakThis))
            promise->resolve<IDLBoolean>(result);
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    algorithm->doVerify(*params, key, WTF::move(signature), WTF::move(data), WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

void SubtleCrypto::digest(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, BufferSource&& dataBufferSource, Ref<DeferredPromise>&& promise)
{
    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::Digest);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto data = copyToVector(WTF::move(dataBufferSource));

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](const Vector<uint8_t>& digest) mutable {
        if (auto promise = getPromise(index, weakThis))
            fulfillPromiseWithArrayBufferFromSpan(promise.releaseNonNull(), digest.span());
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    algorithm->digest(WTF::move(data), WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

void SubtleCrypto::generateKey(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, bool extractable, Vector<CryptoKeyUsage>&& keyUsages, Ref<DeferredPromise>&& promise)
{
    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::GenerateKey);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto keyUsagesBitmap = toCryptoKeyUsageBitmap(keyUsages);

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](KeyOrKeyPair&& keyOrKeyPair) mutable {
        if (auto promise = getPromise(index, weakThis)) {
            WTF::switchOn(keyOrKeyPair,
                [&promise] (RefPtr<CryptoKey>& key) {
                    if ((key->type() == CryptoKeyType::Private || key->type() == CryptoKeyType::Secret) && !key->usagesBitmap()) {
                        rejectWithException(promise.releaseNonNull(), ExceptionCode::SyntaxError);
                        return;
                    }
                    promise->resolve<IDLInterface<CryptoKey>>(*key);
                },
                [&promise] (CryptoKeyPair& keyPair) {
                    if (!keyPair.privateKey->usagesBitmap()) {
                        rejectWithException(promise.releaseNonNull(), ExceptionCode::SyntaxError);
                        return;
                    }
                    promise->resolve<IDLDictionary<CryptoKeyPair>>(keyPair);
                }
            );
        }
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    // The 26 January 2017 version of the specification suggests we should perform the following task asynchronously
    // regardless what kind of keys it produces: https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-generateKey
    // That's simply not efficient for AES, HMAC and EC keys. Therefore, we perform it as an async task only for RSA keys.
    algorithm->generateKey(*params, extractable, keyUsagesBitmap, WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext());
}

void SubtleCrypto::deriveKey(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, CryptoKey& baseKey, AlgorithmIdentifier&& derivedKeyType, bool extractable, Vector<CryptoKeyUsage>&& keyUsages, Ref<DeferredPromise>&& promise)
{
    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::DeriveBits);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto importParamsOrException = normalizeCryptoAlgorithmParameters(state, derivedKeyType, Operations::ImportKey);
    if (importParamsOrException.hasException()) {
        promise->reject(importParamsOrException.releaseException());
        return;
    }
    auto importParams = importParamsOrException.releaseReturnValue();

    auto getLengthParamsOrException = normalizeCryptoAlgorithmParameters(state, derivedKeyType, Operations::GetKeyLength);
    if (getLengthParamsOrException.hasException()) {
        promise->reject(getLengthParamsOrException.releaseException());
        return;
    }
    auto getLengthParams = getLengthParamsOrException.releaseReturnValue();

    auto keyUsagesBitmap = toCryptoKeyUsageBitmap(keyUsages);

    if (params->identifier != baseKey.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't match AlgorithmIdentifier"_s);
        return;
    }

    if (!baseKey.allows(CryptoKeyUsageDeriveKey)) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't support CryptoKey derivation"_s);
        return;
    }

    auto getLengthAlgorithm = CryptoAlgorithmRegistry::singleton().create(getLengthParams->identifier);

    auto result = getLengthAlgorithm->getKeyLength(*getLengthParams);
    if (result.hasException()) {
        promise->reject(result.releaseException().code(), "Cannot get key length from derivedKeyType"_s);
        return;
    }
    std::optional<size_t> length = result.releaseReturnValue();

    auto importAlgorithm = CryptoAlgorithmRegistry::singleton().create(importParams->identifier);
    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis, importAlgorithm = WTF::move(importAlgorithm), importParams = crossThreadCopyImportParams(*importParams), extractable, keyUsagesBitmap](const Vector<uint8_t>& derivedKey) mutable {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=169395
        KeyData data = derivedKey;
        auto callback = [index, weakThis](CryptoKey& key) mutable {
            if (auto promise = getPromise(index, weakThis)) {
                if ((key.type() == CryptoKeyType::Private || key.type() == CryptoKeyType::Secret) && !key.usagesBitmap()) {
                    rejectWithException(promise.releaseNonNull(), ExceptionCode::SyntaxError);
                    return;
                }
                promise->resolve<IDLInterface<CryptoKey>>(key);
            }
        };
        auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
            if (auto promise = getPromise(index, weakThis))
                rejectWithException(promise.releaseNonNull(), ec);
        };

        importAlgorithm->importKey(SubtleCrypto::KeyFormat::Raw, WTF::move(data), *importParams, extractable, keyUsagesBitmap, WTF::move(callback), WTF::move(exceptionCallback));
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    algorithm->deriveBits(*params, baseKey, length, WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

void SubtleCrypto::deriveBits(JSC::JSGlobalObject& state, AlgorithmIdentifier&& algorithmIdentifier, CryptoKey& baseKey, std::optional<unsigned> length, Ref<DeferredPromise>&& promise)
{
    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::DeriveBits);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    if (params->identifier != baseKey.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't match AlgorithmIdentifier"_s);
        return;
    }

    if (!baseKey.allows(CryptoKeyUsageDeriveBits)) {
        promise->reject(ExceptionCode::InvalidAccessError, "CryptoKey doesn't support bits derivation"_s);
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](const Vector<uint8_t>& derivedKey) mutable {
        if (auto promise = getPromise(index, weakThis))
            fulfillPromiseWithArrayBufferFromSpan(promise.releaseNonNull(), derivedKey.span());
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    algorithm->deriveBits(*params, baseKey, length, WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

void SubtleCrypto::importKey(JSC::JSGlobalObject& state, KeyFormat format, KeyDataVariant&& keyDataVariant, AlgorithmIdentifier&& algorithmIdentifier, bool extractable, Vector<CryptoKeyUsage>&& keyUsages, Ref<DeferredPromise>&& promise)
{
    auto paramsOrException = normalizeCryptoAlgorithmParameters(state, WTF::move(algorithmIdentifier), Operations::ImportKey);
    if (paramsOrException.hasException()) {
        promise->reject(paramsOrException.releaseException());
        return;
    }
    auto params = paramsOrException.releaseReturnValue();

    auto keyDataOrNull = toKeyData(format, WTF::move(keyDataVariant), promise);
    if (!keyDataOrNull) {
        // When toKeyData, it means the promise has been rejected, and we should return.
        return;
    }

    auto keyData = *keyDataOrNull;
    auto keyUsagesBitmap = toCryptoKeyUsageBitmap(keyUsages);

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](CryptoKey& key) mutable {
        if (auto promise = getPromise(index, weakThis)) {
            if ((key.type() == CryptoKeyType::Private || key.type() == CryptoKeyType::Secret) && !key.usagesBitmap()) {
                rejectWithException(promise.releaseNonNull(), ExceptionCode::SyntaxError);
                return;
            }
            promise->resolve<IDLInterface<CryptoKey>>(key);
        }
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
    // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-importKey
    // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
    algorithm->importKey(format, WTF::move(keyData), *params, extractable, keyUsagesBitmap, WTF::move(callback), WTF::move(exceptionCallback));
}

void SubtleCrypto::exportKey(KeyFormat format, CryptoKey& key, Ref<DeferredPromise>&& promise)
{
    if (!isSupportedExportKey(key.algorithmIdentifier())) {
        promise->reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }

    if (!key.extractable()) {
        promise->reject(ExceptionCode::InvalidAccessError, "The CryptoKey is nonextractable"_s);
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key.algorithmIdentifier());

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis](SubtleCrypto::KeyFormat format, KeyData&& key) mutable {
        if (auto promise = getPromise(index, weakThis)) {
            switch (format) {
            case SubtleCrypto::KeyFormat::Spki:
            case SubtleCrypto::KeyFormat::Pkcs8:
            case SubtleCrypto::KeyFormat::Raw: {
                Vector<uint8_t>& rawKey = std::get<Vector<uint8_t>>(key);
                fulfillPromiseWithArrayBufferFromSpan(promise.releaseNonNull(), rawKey.span());
                return;
            }
            case SubtleCrypto::KeyFormat::Jwk:
                promise->resolve<IDLDictionary<JsonWebKey>>(WTF::move(std::get<JsonWebKey>(key)));
                return;
            }
            ASSERT_NOT_REACHED();
        }
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
    // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-exportKey
    // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
    algorithm->exportKey(format, key, WTF::move(callback), WTF::move(exceptionCallback));
}

void SubtleCrypto::wrapKey(JSC::JSGlobalObject& state, KeyFormat format, CryptoKey& key, CryptoKey& wrappingKey, AlgorithmIdentifier&& wrapAlgorithmIdentifier, Ref<DeferredPromise>&& promise)
{
    bool isEncryption = false;

    auto wrapParamsOrException = normalizeCryptoAlgorithmParameters(state, wrapAlgorithmIdentifier, Operations::WrapKey);
    if (wrapParamsOrException.hasException()) {
        ASSERT(wrapParamsOrException.exception().code() != ExceptionCode::ExistingExceptionError);

        wrapParamsOrException = normalizeCryptoAlgorithmParameters(state, wrapAlgorithmIdentifier, Operations::Encrypt);
        if (wrapParamsOrException.hasException()) {
            promise->reject(wrapParamsOrException.releaseException());
            return;
        }

        isEncryption = true;
    }
    auto wrapParams = wrapParamsOrException.releaseReturnValue();

    if (wrapParams->identifier != wrappingKey.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "Wrapping CryptoKey doesn't match AlgorithmIdentifier"_s);
        return;
    }

    if (!wrappingKey.allows(CryptoKeyUsageWrapKey)) {
        promise->reject(ExceptionCode::InvalidAccessError, "Wrapping CryptoKey doesn't support wrapKey operation"_s);
        return;
    }

    if (!isSupportedExportKey(key.algorithmIdentifier())) {
        promise->reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }

    if (!key.extractable()) {
        promise->reject(ExceptionCode::InvalidAccessError, "The CryptoKey is nonextractable"_s);
        return;
    }

    auto exportAlgorithm = CryptoAlgorithmRegistry::singleton().create(key.algorithmIdentifier());
    auto wrapAlgorithm = CryptoAlgorithmRegistry::singleton().create(wrappingKey.algorithmIdentifier());

    RefPtr context = scriptExecutionContext();

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis, wrapAlgorithm, wrappingKey = Ref { wrappingKey }, wrapParams = WTF::move(wrapParams), isEncryption, context, workQueue = m_workQueue](SubtleCrypto::KeyFormat format, KeyData&& key) mutable {
        if (!weakThis)
            return;
        RefPtr promise = weakThis->m_pendingPromises.get(index);
        if (!promise)
            return;
        Vector<uint8_t> bytes;
        switch (format) {
        case SubtleCrypto::KeyFormat::Spki:
        case SubtleCrypto::KeyFormat::Pkcs8:
        case SubtleCrypto::KeyFormat::Raw:
            bytes = std::get<Vector<uint8_t>>(key);
            break;
        case SubtleCrypto::KeyFormat::Jwk: {
            // FIXME: Converting to JS just to JSON-Stringify seems inefficient. We should find a way to go directly from the struct to JSON.
            auto jwk = toJS<IDLDictionary<JsonWebKey>>(*(promise->globalObject()), *(promise->globalObject()), WTF::move(std::get<JsonWebKey>(key)));
            String jwkString = JSONStringify(promise->globalObject(), jwk, 0);
            bytes.append(jwkString.utf8(StrictConversion).span());
        }
        }

        auto callback = [index, weakThis](const Vector<uint8_t>& wrappedKey) mutable {
            if (auto promise = getPromise(index, weakThis))
                fulfillPromiseWithArrayBufferFromSpan(promise.releaseNonNull(), wrappedKey.span());
        };
        auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
            if (auto promise = getPromise(index, weakThis))
                rejectWithException(promise.releaseNonNull(), ec);
        };

        if (!isEncryption) {

            RefPtr context = weakThis->scriptExecutionContext();
            if (!context)
                return;
            // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
            // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-wrapKey
            // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
            wrapAlgorithm->wrapKey(wrappingKey.get(), WTF::move(bytes), WTF::move(callback), WTF::move(exceptionCallback));
            return;
        }
        // The following operation should be performed asynchronously.
        wrapAlgorithm->encrypt(*wrapParams, WTF::move(wrappingKey), WTF::move(bytes), WTF::move(callback), WTF::move(exceptionCallback), *context, workQueue);
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    // The following operation should be performed synchronously.
    exportAlgorithm->exportKey(format, key, WTF::move(callback), WTF::move(exceptionCallback));
}

void SubtleCrypto::unwrapKey(JSC::JSGlobalObject& state, KeyFormat format, BufferSource&& wrappedKeyBufferSource, CryptoKey& unwrappingKey, AlgorithmIdentifier&& unwrapAlgorithmIdentifier, AlgorithmIdentifier&& unwrappedKeyAlgorithmIdentifier, bool extractable, Vector<CryptoKeyUsage>&& keyUsages, Ref<DeferredPromise>&& promise)
{
    auto wrappedKey = copyToVector(WTF::move(wrappedKeyBufferSource));

    bool isDecryption = false;

    auto unwrapParamsOrException = normalizeCryptoAlgorithmParameters(state, unwrapAlgorithmIdentifier, Operations::UnwrapKey);
    if (unwrapParamsOrException.hasException()) {
        unwrapParamsOrException = normalizeCryptoAlgorithmParameters(state, unwrapAlgorithmIdentifier, Operations::Decrypt);
        if (unwrapParamsOrException.hasException()) {
            promise->reject(unwrapParamsOrException.releaseException());
            return;
        }

        isDecryption = true;
    }
    auto unwrapParams = unwrapParamsOrException.releaseReturnValue();

    auto unwrappedKeyAlgorithmOrException = normalizeCryptoAlgorithmParameters(state, unwrappedKeyAlgorithmIdentifier, Operations::ImportKey);
    if (unwrappedKeyAlgorithmOrException.hasException()) {
        promise->reject(unwrappedKeyAlgorithmOrException.releaseException());
        return;
    }
    auto unwrappedKeyAlgorithm = unwrappedKeyAlgorithmOrException.releaseReturnValue();

    auto keyUsagesBitmap = toCryptoKeyUsageBitmap(keyUsages);

    if (unwrapParams->identifier != unwrappingKey.algorithmIdentifier()) {
        promise->reject(ExceptionCode::InvalidAccessError, "Unwrapping CryptoKey doesn't match unwrap AlgorithmIdentifier"_s);
        return;
    }

    if (!unwrappingKey.allows(CryptoKeyUsageUnwrapKey)) {
        promise->reject(ExceptionCode::InvalidAccessError, "Unwrapping CryptoKey doesn't support unwrapKey operation"_s);
        return;
    }

    auto importAlgorithm = CryptoAlgorithmRegistry::singleton().create(unwrappedKeyAlgorithm->identifier);
    if (!importAlgorithm) [[unlikely]] {
        promise->reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }

    auto unwrapAlgorithm = CryptoAlgorithmRegistry::singleton().create(unwrappingKey.algorithmIdentifier());
    if (!unwrapAlgorithm) [[unlikely]] {
        promise->reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }

    auto index = promise.ptr();
    m_pendingPromises.add(index, WTF::move(promise));
    WeakPtr weakThis { *this };
    auto callback = [index, weakThis, format, importAlgorithm, unwrappedKeyAlgorithm = crossThreadCopyImportParams(*unwrappedKeyAlgorithm), extractable, keyUsagesBitmap](const Vector<uint8_t>& bytes) mutable {
        if (!weakThis)
            return;
        RefPtr promise = weakThis->m_pendingPromises.get(index);
        if (!promise)
            return;
        KeyData keyData;
        switch (format) {
        case SubtleCrypto::KeyFormat::Spki:
        case SubtleCrypto::KeyFormat::Pkcs8:
        case SubtleCrypto::KeyFormat::Raw:
            keyData = bytes;
            break;
        case SubtleCrypto::KeyFormat::Jwk: {
            auto& state = *(promise->globalObject());
            auto& vm = state.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            JSLockHolder locker(vm);
            auto jwkObject = JSONParse(&state, byteCast<Latin1Character>(bytes.span()));
            if (!jwkObject) {
                promise->reject(ExceptionCode::DataError, "WrappedKey cannot be converted to a JSON object"_s);
                return;
            }
            auto jwkConversionResult = convert<IDLDictionary<JsonWebKey>>(state, jwkObject);
            if (jwkConversionResult.hasException(scope)) [[unlikely]]
                return;
            auto jwk = jwkConversionResult.releaseReturnValue();

            normalizeJsonWebKey(jwk);

            keyData = jwk;
            break;
        }
        }

        auto callback = [index, weakThis](CryptoKey& key) mutable {
            if (auto promise = getPromise(index, weakThis)) {
                if ((key.type() == CryptoKeyType::Private || key.type() == CryptoKeyType::Secret) && !key.usagesBitmap()) {
                    rejectWithException(promise.releaseNonNull(), ExceptionCode::SyntaxError);
                    return;
                }
                promise->resolve<IDLInterface<CryptoKey>>(key);
            }
        };
        auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
            if (auto promise = getPromise(index, weakThis))
                rejectWithException(promise.releaseNonNull(), ec);
        };

        // The following operation should be performed synchronously.
        importAlgorithm->importKey(format, WTF::move(keyData), *unwrappedKeyAlgorithm, extractable, keyUsagesBitmap, WTF::move(callback), WTF::move(exceptionCallback));
    };
    auto exceptionCallback = [index, weakThis](ExceptionCode ec) mutable {
        if (auto promise = getPromise(index, weakThis))
            rejectWithException(promise.releaseNonNull(), ec);
    };

    if (!isDecryption) {
        // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
        // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-unwrapKey
        // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
        RefPtr context = weakThis->scriptExecutionContext();
        if (!context)
            return;
        unwrapAlgorithm->unwrapKey(unwrappingKey, WTF::move(wrappedKey), WTF::move(callback), WTF::move(exceptionCallback));
        return;
    }

    unwrapAlgorithm->decrypt(*unwrapParams, unwrappingKey, WTF::move(wrappedKey), WTF::move(callback), WTF::move(exceptionCallback), *protectedScriptExecutionContext(), m_workQueue);
}

} // namespace WebCore
