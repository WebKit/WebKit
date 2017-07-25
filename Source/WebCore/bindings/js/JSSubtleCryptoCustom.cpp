/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "JSSubtleCrypto.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithm.h"
#include "CryptoAlgorithmRegistry.h"
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
#include "ScriptState.h"
#include <runtime/Error.h>
#include <runtime/JSArray.h>
#include <runtime/JSONObject.h>

using namespace JSC;

namespace WebCore {

enum class Operations {
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

static std::unique_ptr<CryptoAlgorithmParameters> normalizeCryptoAlgorithmParameters(ExecState&, JSValue, Operations);

static CryptoAlgorithmIdentifier toHashIdentifier(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto digestParams = normalizeCryptoAlgorithmParameters(state, value, Operations::Digest);
    RETURN_IF_EXCEPTION(scope, { });
    return digestParams->identifier;
}

static std::unique_ptr<CryptoAlgorithmParameters> normalizeCryptoAlgorithmParameters(ExecState& state, JSValue value, Operations operation)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isString()) {
        JSObject* newParams = constructEmptyObject(&state);
        newParams->putDirect(vm, Identifier::fromString(&vm, "name"), value);
        return normalizeCryptoAlgorithmParameters(state, newParams, operation);
    }

    if (value.isObject()) {
        auto params = convertDictionary<CryptoAlgorithmParameters>(state, value);
        RETURN_IF_EXCEPTION(scope, nullptr);

        auto identifier = CryptoAlgorithmRegistry::singleton().identifier(params.name);
        if (UNLIKELY(!identifier)) {
            throwNotSupportedError(state, scope);
            return nullptr;
        }

        std::unique_ptr<CryptoAlgorithmParameters> result;
        switch (operation) {
        case Operations::Encrypt:
        case Operations::Decrypt:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            case CryptoAlgorithmIdentifier::RSA_OAEP: {
                auto params = convertDictionary<CryptoAlgorithmRsaOaepParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmRsaOaepParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::AES_CBC:
            case CryptoAlgorithmIdentifier::AES_CFB: {
                auto params = convertDictionary<CryptoAlgorithmAesCbcCfbParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmAesCbcCfbParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::AES_CTR: {
                auto params = convertDictionary<CryptoAlgorithmAesCtrParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmAesCtrParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::AES_GCM: {
                auto params = convertDictionary<CryptoAlgorithmAesGcmParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmAesGcmParams>(params);
                break;
            }
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        case Operations::Sign:
        case Operations::Verify:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
            case CryptoAlgorithmIdentifier::HMAC:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            case CryptoAlgorithmIdentifier::ECDSA: {
                auto params = convertDictionary<CryptoAlgorithmEcdsaParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmEcdsaParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::RSA_PSS: {
                auto params = convertDictionary<CryptoAlgorithmRsaPssParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmRsaPssParams>(params);
                break;
            }
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        case Operations::Digest:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::SHA_1:
            case CryptoAlgorithmIdentifier::SHA_224:
            case CryptoAlgorithmIdentifier::SHA_256:
            case CryptoAlgorithmIdentifier::SHA_384:
            case CryptoAlgorithmIdentifier::SHA_512:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        case Operations::GenerateKey:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5: {
                auto params = convertDictionary<CryptoAlgorithmRsaKeyGenParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmRsaKeyGenParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
            case CryptoAlgorithmIdentifier::RSA_PSS:
            case CryptoAlgorithmIdentifier::RSA_OAEP: {
                auto params = convertDictionary<CryptoAlgorithmRsaHashedKeyGenParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmRsaHashedKeyGenParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::AES_CTR:
            case CryptoAlgorithmIdentifier::AES_CBC:
            case CryptoAlgorithmIdentifier::AES_GCM:
            case CryptoAlgorithmIdentifier::AES_CFB:
            case CryptoAlgorithmIdentifier::AES_KW: {
                auto params = convertDictionary<CryptoAlgorithmAesKeyParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmAesKeyParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::HMAC: {
                auto params = convertDictionary<CryptoAlgorithmHmacKeyParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmHmacKeyParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::ECDSA:
            case CryptoAlgorithmIdentifier::ECDH: {
                auto params = convertDictionary<CryptoAlgorithmEcKeyParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmEcKeyParams>(params);
                break;
            }
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        case Operations::DeriveBits:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::ECDH: {
                // Remove this hack once https://bugs.webkit.org/show_bug.cgi?id=169333 is fixed.
                JSValue nameValue = value.getObject()->get(&state, Identifier::fromString(&state, "name"));
                JSValue publicValue = value.getObject()->get(&state, Identifier::fromString(&state, "public"));
                JSObject* newValue = constructEmptyObject(&state);
                newValue->putDirect(vm, Identifier::fromString(&vm, "name"), nameValue);
                newValue->putDirect(vm, Identifier::fromString(&vm, "publicKey"), publicValue);

                auto params = convertDictionary<CryptoAlgorithmEcdhKeyDeriveParams>(state, newValue);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmEcdhKeyDeriveParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::HKDF: {
                auto params = convertDictionary<CryptoAlgorithmHkdfParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmHkdfParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::PBKDF2: {
                auto params = convertDictionary<CryptoAlgorithmPbkdf2Params>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmPbkdf2Params>(params);
                break;
            }
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        case Operations::ImportKey:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
            case CryptoAlgorithmIdentifier::RSA_PSS:
            case CryptoAlgorithmIdentifier::RSA_OAEP: {
                auto params = convertDictionary<CryptoAlgorithmRsaHashedImportParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmRsaHashedImportParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::AES_CTR:
            case CryptoAlgorithmIdentifier::AES_CBC:
            case CryptoAlgorithmIdentifier::AES_GCM:
            case CryptoAlgorithmIdentifier::AES_CFB:
            case CryptoAlgorithmIdentifier::AES_KW:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            case CryptoAlgorithmIdentifier::HMAC: {
                auto params = convertDictionary<CryptoAlgorithmHmacKeyParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmHmacKeyParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::ECDSA:
            case CryptoAlgorithmIdentifier::ECDH: {
                auto params = convertDictionary<CryptoAlgorithmEcKeyParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmEcKeyParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::HKDF:
            case CryptoAlgorithmIdentifier::PBKDF2:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        case Operations::WrapKey:
        case Operations::UnwrapKey:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::AES_KW:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        case Operations::GetKeyLength:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::AES_CTR:
            case CryptoAlgorithmIdentifier::AES_CBC:
            case CryptoAlgorithmIdentifier::AES_GCM:
            case CryptoAlgorithmIdentifier::AES_CFB:
            case CryptoAlgorithmIdentifier::AES_KW: {
                auto params = convertDictionary<CryptoAlgorithmAesKeyParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmAesKeyParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::HMAC: {
                auto params = convertDictionary<CryptoAlgorithmHmacKeyParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmHmacKeyParams>(params);
                break;
            }
            case CryptoAlgorithmIdentifier::HKDF:
            case CryptoAlgorithmIdentifier::PBKDF2:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            default:
                throwNotSupportedError(state, scope);
                return nullptr;
            }
            break;
        default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        result->identifier = *identifier;
        return result;
    }

    throwTypeError(&state, scope, ASCIILiteral("Invalid AlgorithmIdentifier"));
    return nullptr;
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

    ASSERT_NOT_REACHED();
    return 0;
}

static CryptoKeyUsageBitmap cryptoKeyUsageBitmapFromJSValue(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CryptoKeyUsageBitmap result = 0;
    auto usages = convert<IDLSequence<IDLEnumeration<CryptoKeyUsage>>>(state, value);
    RETURN_IF_EXCEPTION(scope, 0);
    // Maybe we shouldn't silently bypass duplicated usages?
    for (auto usage : usages)
        result |= toCryptoKeyUsageBitmap(usage);

    return result;
}

// Maybe we want more specific error messages?
static void rejectWithException(Ref<DeferredPromise>&& passedPromise, ExceptionCode ec)
{
    switch (ec) {
    case NotSupportedError:
        passedPromise->reject(ec, ASCIILiteral("The algorithm is not supported"));
        return;
    case SyntaxError:
        passedPromise->reject(ec, ASCIILiteral("A required parameter was missing or out-of-range"));
        return;
    case InvalidStateError:
        passedPromise->reject(ec, ASCIILiteral("The requested operation is not valid for the current state of the provided key"));
        return;
    case InvalidAccessError:
        passedPromise->reject(ec, ASCIILiteral("The requested operation is not valid for the provided key"));
        return;
    case UnknownError:
        passedPromise->reject(ec, ASCIILiteral("The operation failed for an unknown transient reason (e.g. out of memory)"));
        return;
    case DataError:
        passedPromise->reject(ec, ASCIILiteral("Data provided to an operation does not meet requirements"));
        return;
    case OperationError:
        passedPromise->reject(ec, ASCIILiteral("The operation failed for an operation-specific reason"));
        return;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
}

static KeyData toKeyData(ExecState& state, SubtleCrypto::KeyFormat format, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    KeyData result;
    switch (format) {
    case SubtleCrypto::KeyFormat::Spki:
    case SubtleCrypto::KeyFormat::Pkcs8:
    case SubtleCrypto::KeyFormat::Raw: {
        BufferSource bufferSource = convert<IDLUnion<IDLArrayBufferView, IDLArrayBuffer>>(state, value);
        RETURN_IF_EXCEPTION(scope, result);
        Vector<uint8_t> vector;
        vector.append(bufferSource.data(), bufferSource.length());
        result = WTFMove(vector);
        return result;
    }
    case SubtleCrypto::KeyFormat::Jwk: {
        result = convertDictionary<JsonWebKey>(state, value);
        RETURN_IF_EXCEPTION(scope, result);
        CryptoKeyUsageBitmap usages = 0;
        if (WTF::get<JsonWebKey>(result).key_ops) {
            // Maybe we shouldn't silently bypass duplicated usages?
            for (auto usage : WTF::get<JsonWebKey>(result).key_ops.value())
                usages |= toCryptoKeyUsageBitmap(usage);
        }
        WTF::get<JsonWebKey>(result).usages = usages;
        return result;
    }
    }
    ASSERT_NOT_REACHED();
    return result;
}

static RefPtr<CryptoKey> toCryptoKey(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RefPtr<CryptoKey> result = JSCryptoKey::toWrapped(vm, value);
    if (!result) {
        throwTypeError(&state, scope, ASCIILiteral("Invalid CryptoKey"));
        return nullptr;
    }
    return result;
}

static Vector<uint8_t> toVector(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    BufferSource data = convert<IDLUnion<IDLArrayBufferView, IDLArrayBuffer>>(state, value);
    RETURN_IF_EXCEPTION(scope, { });
    Vector<uint8_t> dataVector;
    dataVector.append(data.data(), data.length());

    return dataVector;
}

static void supportExportKeyThrow(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
        return;
    default:
        throwNotSupportedError(state, scope);
    }
}

static void jsSubtleCryptoFunctionEncryptPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Encrypt);
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != key->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageEncrypt)) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't support encryption"));
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key->algorithmIdentifier());
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& cipherText) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), cipherText.data(), cipherText.size());
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    algorithm->encrypt(WTFMove(params), key.releaseNonNull(), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionDecryptPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Decrypt);
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != key->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageDecrypt)) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't support decryption"));
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key->algorithmIdentifier());
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& plainText) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), plainText.data(), plainText.size());
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    algorithm->decrypt(WTFMove(params), key.releaseNonNull(), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionSignPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Sign);
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != key->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageSign)) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't support signing"));
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key->algorithmIdentifier());
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& signature) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), signature.data(), signature.size());
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    JSSubtleCrypto* subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    algorithm->sign(WTFMove(params), key.releaseNonNull(), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionVerifyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 4)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Verify);
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto signature = toVector(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(3));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != key->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageVerify)) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't support verification"));
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key->algorithmIdentifier());
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](bool result) mutable {
        capturedPromise->resolve<IDLBoolean>(result);
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    algorithm->verify(WTFMove(params), key.releaseNonNull(), WTFMove(signature), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionDigestPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Digest);
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& digest) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), digest.data(), digest.size());
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    algorithm->digest(WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionGenerateKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::GenerateKey);
    RETURN_IF_EXCEPTION(scope, void());

    auto extractable = state.uncheckedArgument(1).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, void());

    auto keyUsages = cryptoKeyUsageBitmapFromJSValue(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](KeyOrKeyPair&& keyOrKeyPair) mutable {
        WTF::switchOn(keyOrKeyPair,
            [&capturedPromise] (RefPtr<CryptoKey>& key) {
                if ((key->type() == CryptoKeyType::Private || key->type() == CryptoKeyType::Secret) && !key->usagesBitmap()) {
                    rejectWithException(WTFMove(capturedPromise), SyntaxError);
                    return;
                }
                capturedPromise->resolve<IDLInterface<CryptoKey>>(*key);
            },
            [&capturedPromise] (CryptoKeyPair& keyPair) {
                if (!keyPair.privateKey->usagesBitmap()) {
                    rejectWithException(WTFMove(capturedPromise), SyntaxError);
                    return;
                }
                capturedPromise->resolve<IDLDictionary<CryptoKeyPair>>(keyPair);
            }
        );
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    // The 26 January 2017 version of the specification suggests we should perform the following task asynchronously
    // regardless what kind of keys it produces: https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-generateKey
    // That's simply not efficient for AES, HMAC and EC keys. Therefore, we perform it as an async task only for RSA keys.
    algorithm->generateKey(*params, extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state));
}

static void jsSubtleCryptoFunctionDeriveKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 5)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::DeriveBits);
    RETURN_IF_EXCEPTION(scope, void());

    auto baseKey = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto importParams = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(2), Operations::ImportKey);
    RETURN_IF_EXCEPTION(scope, void());

    auto getLengthParams = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(2), Operations::GetKeyLength);
    RETURN_IF_EXCEPTION(scope, void());

    auto extractable = state.uncheckedArgument(3).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, void());

    auto keyUsages = cryptoKeyUsageBitmapFromJSValue(state, state.uncheckedArgument(4));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != baseKey->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!baseKey->allows(CryptoKeyUsageDeriveKey)) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't support CryptoKey derivation"));
        return;
    }

    auto getLengthAlgorithm = CryptoAlgorithmRegistry::singleton().create(getLengthParams->identifier);
    if (UNLIKELY(!getLengthAlgorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }
    auto result = getLengthAlgorithm->getKeyLength(*getLengthParams);
    if (result.hasException()) {
        promise->reject(result.releaseException().code(), ASCIILiteral("Cannot get key length from derivedKeyType"));
        return;
    }
    size_t length = result.releaseReturnValue();

    auto importAlgorithm = CryptoAlgorithmRegistry::singleton().create(importParams->identifier);
    if (UNLIKELY(!importAlgorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [promise = promise.copyRef(), importAlgorithm, importParams = WTFMove(importParams), extractable, keyUsages](const Vector<uint8_t>& derivedKey) mutable {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=169395
        KeyData data = derivedKey;
        auto callback = [capturedPromise = promise.copyRef()](CryptoKey& key) mutable {
            if ((key.type() == CryptoKeyType::Private || key.type() == CryptoKeyType::Secret) && !key.usagesBitmap()) {
                rejectWithException(WTFMove(capturedPromise), SyntaxError);
                return;
            }
            capturedPromise->resolve<IDLInterface<CryptoKey>>(key);
        };
        auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
            rejectWithException(WTFMove(capturedPromise), ec);
        };

        importAlgorithm->importKey(SubtleCrypto::KeyFormat::Raw, WTFMove(data), WTFMove(importParams), extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback));
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    algorithm->deriveBits(WTFMove(params), baseKey.releaseNonNull(), length, WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionDeriveBitsPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::DeriveBits);
    RETURN_IF_EXCEPTION(scope, void());

    auto baseKey = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto length = convert<IDLUnsignedLong>(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != baseKey->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!baseKey->allows(CryptoKeyUsageDeriveBits)) {
        promise->reject(InvalidAccessError, ASCIILiteral("CryptoKey doesn't support bits derivation"));
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& derivedKey) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), derivedKey.data(), derivedKey.size());
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    algorithm->deriveBits(WTFMove(params), baseKey.releaseNonNull(), length, WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionImportKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 5)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto format = convert<IDLEnumeration<SubtleCrypto::KeyFormat>>(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, void());

    auto keyData = toKeyData(state, format, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(2), Operations::ImportKey);
    RETURN_IF_EXCEPTION(scope, void());

    auto extractable = state.uncheckedArgument(3).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, void());

    auto keyUsages = cryptoKeyUsageBitmapFromJSValue(state, state.uncheckedArgument(4));
    RETURN_IF_EXCEPTION(scope, void());

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(params->identifier);
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](CryptoKey& key) mutable {
        if ((key.type() == CryptoKeyType::Private || key.type() == CryptoKeyType::Secret) && !key.usagesBitmap()) {
            rejectWithException(WTFMove(capturedPromise), SyntaxError);
            return;
        }
        capturedPromise->resolve<IDLInterface<CryptoKey>>(key);
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
    // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-importKey
    // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
    algorithm->importKey(format, WTFMove(keyData), WTFMove(params), extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback));
}

static void jsSubtleCryptoFunctionExportKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto format = convert<IDLEnumeration<SubtleCrypto::KeyFormat>>(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    supportExportKeyThrow(state, scope, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    if (!key->extractable()) {
        promise->reject(InvalidAccessError, ASCIILiteral("The CryptoKey is nonextractable"));
        return;
    }

    auto algorithm = CryptoAlgorithmRegistry::singleton().create(key->algorithmIdentifier());
    if (UNLIKELY(!algorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [capturedPromise = promise.copyRef()](SubtleCrypto::KeyFormat format, KeyData&& key) mutable {
        switch (format) {
        case SubtleCrypto::KeyFormat::Spki:
        case SubtleCrypto::KeyFormat::Pkcs8:
        case SubtleCrypto::KeyFormat::Raw: {
            Vector<uint8_t>& rawKey = WTF::get<Vector<uint8_t>>(key);
            fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), rawKey.data(), rawKey.size());
            return;
        }
        case SubtleCrypto::KeyFormat::Jwk:
            capturedPromise->resolve<IDLDictionary<JsonWebKey>>(WTFMove(WTF::get<JsonWebKey>(key)));
            return;
        }
        ASSERT_NOT_REACHED();
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
    // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-exportKey
    // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
    algorithm->exportKey(format, key.releaseNonNull(), WTFMove(callback), WTFMove(exceptionCallback));
}

static void jsSubtleCryptoFunctionWrapKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 4)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto format = convert<IDLEnumeration<SubtleCrypto::KeyFormat>>(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto wrappingKey = toCryptoKey(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    bool isEncryption = false;
    auto wrapParams = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(3), Operations::WrapKey);
    if (catchScope.exception()) {
        catchScope.clearException();
        wrapParams = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(3), Operations::Encrypt);
        RETURN_IF_EXCEPTION(scope, void());
        isEncryption = true;
    }

    if (wrapParams->identifier != wrappingKey->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("Wrapping CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!wrappingKey->allows(CryptoKeyUsageWrapKey)) {
        promise->reject(InvalidAccessError, ASCIILiteral("Wrapping CryptoKey doesn't support wrapKey operation"));
        return;
    }

    supportExportKeyThrow(state, scope, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    if (!key->extractable()) {
        promise->reject(InvalidAccessError, ASCIILiteral("The CryptoKey is nonextractable"));
        return;
    }

    auto exportAlgorithm = CryptoAlgorithmRegistry::singleton().create(key->algorithmIdentifier());
    if (UNLIKELY(!exportAlgorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto wrapAlgorithm = CryptoAlgorithmRegistry::singleton().create(wrappingKey->algorithmIdentifier());
    if (UNLIKELY(!wrapAlgorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto context = scriptExecutionContextFromExecState(&state);

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    auto& workQueue = subtle->wrapped().workQueue();

    auto callback = [promise = promise.copyRef(), wrapAlgorithm, wrappingKey = WTFMove(wrappingKey), wrapParams = WTFMove(wrapParams), isEncryption, context, &workQueue](SubtleCrypto::KeyFormat format, KeyData&& key) mutable {
        Vector<uint8_t> bytes;
        switch (format) {
        case SubtleCrypto::KeyFormat::Spki:
        case SubtleCrypto::KeyFormat::Pkcs8:
        case SubtleCrypto::KeyFormat::Raw:
            bytes = WTF::get<Vector<uint8_t>>(key);
            break;
        case SubtleCrypto::KeyFormat::Jwk: {
            auto jwk = toJS<IDLDictionary<JsonWebKey>>(*(promise->globalObject()->globalExec()), *(promise->globalObject()), WTFMove(WTF::get<JsonWebKey>(key)));
            String jwkString = JSONStringify(promise->globalObject()->globalExec(), jwk, 0);
            CString jwkUtf8String = jwkString.utf8(StrictConversion);
            bytes.append(jwkUtf8String.data(), jwkUtf8String.length());
        }
        }

        auto callback = [promise = promise.copyRef()](const Vector<uint8_t>& wrappedKey) mutable {
            fulfillPromiseWithArrayBuffer(WTFMove(promise), wrappedKey.data(), wrappedKey.size());
            return;
        };
        auto exceptionCallback = [promise = WTFMove(promise)](ExceptionCode ec) mutable {
            rejectWithException(WTFMove(promise), ec);
        };

        if (!isEncryption) {
            // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
            // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-wrapKey
            // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
            wrapAlgorithm->wrapKey(wrappingKey.releaseNonNull(), WTFMove(bytes), WTFMove(callback), WTFMove(exceptionCallback));
            return;
        }
        // The following operation should be performed asynchronously.
        wrapAlgorithm->encrypt(WTFMove(wrapParams), wrappingKey.releaseNonNull(), WTFMove(bytes), WTFMove(callback), WTFMove(exceptionCallback), *context, workQueue);
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    // The following operation should be performed synchronously.
    exportAlgorithm->exportKey(format, key.releaseNonNull(), WTFMove(callback), WTFMove(exceptionCallback));
}

static void jsSubtleCryptoFunctionUnwrapKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 7)) {
        promise->reject<IDLAny>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto format = convert<IDLEnumeration<SubtleCrypto::KeyFormat>>(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, void());

    auto wrappedKey = toVector(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto unwrappingKey = toCryptoKey(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    bool isDecryption = false;
    auto unwrapParams = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(3), Operations::UnwrapKey);
    if (catchScope.exception()) {
        catchScope.clearException();
        unwrapParams = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(3), Operations::Decrypt);
        RETURN_IF_EXCEPTION(scope, void());
        isDecryption = true;
    }

    auto unwrappedKeyAlgorithm = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(4), Operations::ImportKey);
    RETURN_IF_EXCEPTION(scope, void());

    auto extractable = state.uncheckedArgument(5).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, void());

    auto keyUsages = cryptoKeyUsageBitmapFromJSValue(state, state.uncheckedArgument(6));
    RETURN_IF_EXCEPTION(scope, void());

    if (unwrapParams->identifier != unwrappingKey->algorithmIdentifier()) {
        promise->reject(InvalidAccessError, ASCIILiteral("Unwrapping CryptoKey doesn't match unwrap AlgorithmIdentifier"));
        return;
    }

    if (!unwrappingKey->allows(CryptoKeyUsageUnwrapKey)) {
        promise->reject(InvalidAccessError, ASCIILiteral("Unwrapping CryptoKey doesn't support unwrapKey operation"));
        return;
    }

    auto importAlgorithm = CryptoAlgorithmRegistry::singleton().create(unwrappedKeyAlgorithm->identifier);
    if (UNLIKELY(!importAlgorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto unwrapAlgorithm = CryptoAlgorithmRegistry::singleton().create(unwrappingKey->algorithmIdentifier());
    if (UNLIKELY(!unwrapAlgorithm)) {
        throwNotSupportedError(state, scope);
        return;
    }

    auto callback = [promise = promise.copyRef(), format, importAlgorithm, unwrappedKeyAlgorithm = WTFMove(unwrappedKeyAlgorithm), extractable, keyUsages](const Vector<uint8_t>& bytes) mutable {
        ExecState& state = *(promise->globalObject()->globalExec());
        VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        KeyData keyData;
        switch (format) {
        case SubtleCrypto::KeyFormat::Spki:
        case SubtleCrypto::KeyFormat::Pkcs8:
        case SubtleCrypto::KeyFormat::Raw:
            keyData = bytes;
            break;
        case SubtleCrypto::KeyFormat::Jwk: {
            String jwkString(reinterpret_cast_ptr<const char*>(bytes.data()), bytes.size());
            JSC::JSLockHolder locker(vm);
            auto jwk = JSONParse(&state, jwkString);
            if (!jwk) {
                promise->reject(DataError, ASCIILiteral("WrappedKey cannot be converted to a JSON object"));
                return;
            }
            keyData = toKeyData(state, format, jwk);
            RETURN_IF_EXCEPTION(scope, void());
        }
        }

        auto callback = [promise = promise.copyRef()](CryptoKey& key) mutable {
            if ((key.type() == CryptoKeyType::Private || key.type() == CryptoKeyType::Secret) && !key.usagesBitmap()) {
                rejectWithException(WTFMove(promise), SyntaxError);
                return;
            }
            promise->resolve<IDLInterface<CryptoKey>>(key);
        };
        auto exceptionCallback = [promise = WTFMove(promise)](ExceptionCode ec) mutable {
            rejectWithException(WTFMove(promise), ec);
        };

        // The following operation should be performed synchronously.
        importAlgorithm->importKey(format, WTFMove(keyData), WTFMove(unwrappedKeyAlgorithm), extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback));
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    if (!isDecryption) {
        // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously:
        // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-unwrapKey
        // It is not beneficial for less time consuming operations. Therefore, we perform it synchronously.
        unwrapAlgorithm->unwrapKey(unwrappingKey.releaseNonNull(), WTFMove(wrappedKey), WTFMove(callback), WTFMove(exceptionCallback));
        return;
    }
    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(vm, state.thisValue());
    ASSERT(subtle);
    // The following operation should be performed asynchronously.
    unwrapAlgorithm->decrypt(WTFMove(unwrapParams), unwrappingKey.releaseNonNull(), WTFMove(wrappedKey), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

JSValue JSSubtleCrypto::encrypt(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionEncryptPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::decrypt(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionDecryptPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::sign(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionSignPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::verify(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionVerifyPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::digest(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionDigestPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::generateKey(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionGenerateKeyPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::deriveKey(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionDeriveKeyPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::deriveBits(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionDeriveBitsPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::importKey(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionImportKeyPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::exportKey(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionExportKeyPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::wrapKey(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionWrapKeyPromise(state, WTFMove(promise));
    return jsUndefined();
}

JSValue JSSubtleCrypto::unwrapKey(ExecState& state, Ref<DeferredPromise>&& promise)
{
    jsSubtleCryptoFunctionUnwrapKeyPromise(state, WTFMove(promise));
    return jsUndefined();
}

} // namespace WebCore

#endif
