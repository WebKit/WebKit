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
#include "JSAesCbcParams.h"
#include "JSAesKeyGenParams.h"
#include "JSCryptoAlgorithmParameters.h"
#include "JSCryptoKey.h"
#include "JSCryptoKeyPair.h"
#include "JSDOMPromise.h"
#include "JSHmacKeyParams.h"
#include "JSJsonWebKey.h"
#include "JSRsaHashedImportParams.h"
#include "JSRsaHashedKeyGenParams.h"
#include "JSRsaKeyGenParams.h"
#include "JSRsaOaepParams.h"
#include "ScriptState.h"
#include <runtime/Error.h>
#include <runtime/IteratorOperations.h>
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
    DeriveKey,
    GenerateKey,
    ImportKey,
    WrapKey,
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
        if (!identifier) {
            setDOMException(&state, NOT_SUPPORTED_ERR);
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
            case CryptoAlgorithmIdentifier::AES_CBC: {
                auto params = convertDictionary<CryptoAlgorithmAesCbcParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmAesCbcParams>(params);
                break;
            }
            default:
                setDOMException(&state, NOT_SUPPORTED_ERR);
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
            default:
                setDOMException(&state, NOT_SUPPORTED_ERR);
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
                setDOMException(&state, NOT_SUPPORTED_ERR);
                return nullptr;
            }
            break;
        case Operations::DeriveKey:
            setDOMException(&state, NOT_SUPPORTED_ERR);
            return nullptr;
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
            case CryptoAlgorithmIdentifier::AES_CMAC:
            case CryptoAlgorithmIdentifier::AES_GCM:
            case CryptoAlgorithmIdentifier::AES_CFB:
            case CryptoAlgorithmIdentifier::AES_KW: {
                auto params = convertDictionary<CryptoAlgorithmAesKeyGenParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmAesKeyGenParams>(params);
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
            default:
                setDOMException(&state, NOT_SUPPORTED_ERR);
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
            case CryptoAlgorithmIdentifier::AES_CMAC:
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
            default:
                setDOMException(&state, NOT_SUPPORTED_ERR);
                return nullptr;
            }
            break;
        case Operations::WrapKey:
            switch (*identifier) {
            case CryptoAlgorithmIdentifier::AES_KW:
                result = std::make_unique<CryptoAlgorithmParameters>(params);
                break;
            default:
                setDOMException(&state, NOT_SUPPORTED_ERR);
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

static RefPtr<CryptoAlgorithm> createAlgorithm(ExecState& state, CryptoAlgorithmIdentifier identifier)
{
    auto result = CryptoAlgorithmRegistry::singleton().create(identifier);
    if (!result)
        setDOMException(&state, NOT_SUPPORTED_ERR);
    return result;
}

// Maybe we want more specific error messages?
static void rejectWithException(Ref<DeferredPromise>&& passedPromise, ExceptionCode ec)
{
    switch (ec) {
    case NOT_SUPPORTED_ERR:
        passedPromise->reject(ec, ASCIILiteral("The algorithm is not supported"));
        return;
    case SYNTAX_ERR:
        passedPromise->reject(ec, ASCIILiteral("A required parameter was missing or out-of-range"));
        return;
    case INVALID_STATE_ERR:
        passedPromise->reject(ec, ASCIILiteral("The requested operation is not valid for the current state of the provided key"));
        return;
    case INVALID_ACCESS_ERR:
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return result;
    case SubtleCrypto::KeyFormat::Raw: {
        BufferSource bufferSource = convert<IDLBufferSource>(state, value);
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

// FIXME: We should get rid of this once https://bugs.webkit.org/show_bug.cgi?id=163711 is fixed.
static JSValue toJSValueFromJsonWebKey(JSDOMGlobalObject& globalObject, JsonWebKey&& key)
{
    ExecState& state = *globalObject.globalExec();
    VM& vm = state.vm();

    auto* result = constructEmptyObject(&state);
    result->putDirect(vm, Identifier::fromString(&vm, "kty"), toJS<IDLDOMString>(state, key.kty));
    if (key.use)
        result->putDirect(vm, Identifier::fromString(&vm, "use"), toJS<IDLDOMString>(state, key.use.value()));
    if (key.key_ops)
        result->putDirect(vm, Identifier::fromString(&vm, "key_ops"), toJS<IDLSequence<IDLEnumeration<CryptoKeyUsage>>>(state, globalObject, key.key_ops.value()));
    if (key.alg)
        result->putDirect(vm, Identifier::fromString(&vm, "alg"), toJS<IDLDOMString>(state, key.alg.value()));
    if (key.ext)
        result->putDirect(vm, Identifier::fromString(&vm, "ext"), toJS<IDLBoolean>(state, key.ext.value()));
    if (key.crv)
        result->putDirect(vm, Identifier::fromString(&vm, "crv"), toJS<IDLDOMString>(state, key.crv.value()));
    if (key.x)
        result->putDirect(vm, Identifier::fromString(&vm, "x"), toJS<IDLDOMString>(state, key.x.value()));
    if (key.y)
        result->putDirect(vm, Identifier::fromString(&vm, "y"), toJS<IDLDOMString>(state, key.y.value()));
    if (key.d)
        result->putDirect(vm, Identifier::fromString(&vm, "d"), toJS<IDLDOMString>(state, key.d.value()));
    if (key.n)
        result->putDirect(vm, Identifier::fromString(&vm, "n"), toJS<IDLDOMString>(state, key.n.value()));
    if (key.e)
        result->putDirect(vm, Identifier::fromString(&vm, "e"), toJS<IDLDOMString>(state, key.e.value()));
    if (key.p)
        result->putDirect(vm, Identifier::fromString(&vm, "p"), toJS<IDLDOMString>(state, key.p.value()));
    if (key.q)
        result->putDirect(vm, Identifier::fromString(&vm, "q"), toJS<IDLDOMString>(state, key.q.value()));
    if (key.dp)
        result->putDirect(vm, Identifier::fromString(&vm, "dp"), toJS<IDLDOMString>(state, key.dp.value()));
    if (key.dq)
        result->putDirect(vm, Identifier::fromString(&vm, "dq"), toJS<IDLDOMString>(state, key.dq.value()));
    if (key.qi)
        result->putDirect(vm, Identifier::fromString(&vm, "qi"), toJS<IDLDOMString>(state, key.qi.value()));
    if (key.oth) {
        MarkedArgumentBuffer list;
        for (auto& value : key.oth.value()) {
            auto* info = constructEmptyObject(&state);
            info->putDirect(vm, Identifier::fromString(&vm, "r"), toJS<IDLDOMString>(state, value.r));
            info->putDirect(vm, Identifier::fromString(&vm, "d"), toJS<IDLDOMString>(state, value.d));
            info->putDirect(vm, Identifier::fromString(&vm, "t"), toJS<IDLDOMString>(state, value.t));
            list.append(info);
        }
        result->putDirect(vm, Identifier::fromString(&vm, "oth"), constructArray(&state, static_cast<Structure*>(nullptr), list));
    }
    if (key.k)
        result->putDirect(vm, Identifier::fromString(&vm, "k"), toJS<IDLDOMString>(state, key.k.value()));

    return result;
}

static RefPtr<CryptoKey> toCryptoKey(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RefPtr<CryptoKey> result = JSCryptoKey::toWrapped(value);
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

    BufferSource data = convert<IDLBufferSource>(state, value);
    RETURN_IF_EXCEPTION(scope, { });
    Vector<uint8_t> dataVector;
    dataVector.append(data.data(), data.length());

    return dataVector;
}

static void supportExportKeyThrow(ExecState& state, CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
    case CryptoAlgorithmIdentifier::HMAC:
        return;
    default:
        setDOMException(&state, NOT_SUPPORTED_ERR);
    }
}

static void jsSubtleCryptoFunctionEncryptPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Encrypt);
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != key->algorithmIdentifier()) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageEncrypt)) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't support encryption"));
        return;
    }

    auto algorithm = createAlgorithm(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& cipherText) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), cipherText.data(), cipherText.size());
        return;
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(state.thisValue());
    ASSERT(subtle);
    algorithm->encrypt(WTFMove(params), key.releaseNonNull(), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionDecryptPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Decrypt);
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != key->algorithmIdentifier()) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageDecrypt)) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't support decryption"));
        return;
    }

    auto algorithm = createAlgorithm(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& plainText) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), plainText.data(), plainText.size());
        return;
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(state.thisValue());
    ASSERT(subtle);
    algorithm->decrypt(WTFMove(params), key.releaseNonNull(), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionSignPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Sign);
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    if (params->identifier != key->algorithmIdentifier()) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageSign)) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't support signing"));
        return;
    }

    auto algorithm = createAlgorithm(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& signature) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), signature.data(), signature.size());
        return;
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    JSSubtleCrypto* subtle = jsDynamicDowncast<JSSubtleCrypto*>(state.thisValue());
    ASSERT(subtle);
    algorithm->sign(key.releaseNonNull(), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionVerifyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 4)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
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
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!key->allows(CryptoKeyUsageVerify)) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("CryptoKey doesn't support verification"));
        return;
    }

    auto algorithm = createAlgorithm(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    auto callback = [capturedPromise = promise.copyRef()](bool result) mutable {
        capturedPromise->resolve(result);
        return;
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(state.thisValue());
    ASSERT(subtle);
    algorithm->verify(key.releaseNonNull(), WTFMove(signature), WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionDigestPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::Digest);
    RETURN_IF_EXCEPTION(scope, void());

    auto data = toVector(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto algorithm = createAlgorithm(state, params->identifier);
    RETURN_IF_EXCEPTION(scope, void());

    auto callback = [capturedPromise = promise.copyRef()](const Vector<uint8_t>& digest) mutable {
        fulfillPromiseWithArrayBuffer(WTFMove(capturedPromise), digest.data(), digest.size());
        return;
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(state.thisValue());
    ASSERT(subtle);
    algorithm->digest(WTFMove(data), WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state), subtle->wrapped().workQueue());
}

static void jsSubtleCryptoFunctionDeriveKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 5)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::DeriveKey);
    RETURN_IF_EXCEPTION(scope, void());

    // We should always return a NOT_SUPPORTED_ERR since we currently don't support any algorithms that has deriveKey operation.
    ASSERT_NOT_REACHED();
}

static void jsSubtleCryptoFunctionGenerateKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 3)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(0), Operations::GenerateKey);
    RETURN_IF_EXCEPTION(scope, void());

    auto extractable = state.uncheckedArgument(1).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, void());

    auto keyUsages = cryptoKeyUsageBitmapFromJSValue(state, state.uncheckedArgument(2));
    RETURN_IF_EXCEPTION(scope, void());

    auto algorithm = createAlgorithm(state, params->identifier);
    RETURN_IF_EXCEPTION(scope, void());

    auto callback = [capturedPromise = promise.copyRef()](CryptoKey* key, CryptoKeyPair* keyPair) mutable {
        ASSERT(key || keyPair);
        ASSERT(!key || !keyPair);
        if (key) {
            if ((key->type() == CryptoKeyType::Private || key->type() == CryptoKeyType::Secret) && !key->usagesBitmap()) {
                rejectWithException(WTFMove(capturedPromise), SYNTAX_ERR);
                return;
            }
            capturedPromise->resolve(key);
        } else {
            if (!keyPair->privateKey()->usagesBitmap()) {
                rejectWithException(WTFMove(capturedPromise), SYNTAX_ERR);
                return;
            }
            capturedPromise->resolve(keyPair);
        }
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    // The 11 December 2014 version of the specification suggests we should perform the following task asynchronously
    // regardless what kind of keys it produces: https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-generateKey
    // That's simply not efficient for AES and HMAC keys. Therefore, we perform it as an async task conditionally.
    algorithm->generateKey(*params, extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state));
}

static void jsSubtleCryptoFunctionImportKeyPromise(ExecState& state, Ref<DeferredPromise>&& promise)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 5)) {
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto format = convertEnumeration<SubtleCrypto::KeyFormat>(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, void());

    auto keyData = toKeyData(state, format, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    auto params = normalizeCryptoAlgorithmParameters(state, state.uncheckedArgument(2), Operations::ImportKey);
    RETURN_IF_EXCEPTION(scope, void());

    auto extractable = state.uncheckedArgument(3).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, void());

    auto keyUsages = cryptoKeyUsageBitmapFromJSValue(state, state.uncheckedArgument(4));
    RETURN_IF_EXCEPTION(scope, void());

    auto algorithm = createAlgorithm(state, params->identifier);
    RETURN_IF_EXCEPTION(scope, void());

    auto callback = [capturedPromise = promise.copyRef()](CryptoKey& key) mutable {
        if ((key.type() == CryptoKeyType::Private || key.type() == CryptoKeyType::Secret) && !key.usagesBitmap()) {
            rejectWithException(WTFMove(capturedPromise), SYNTAX_ERR);
            return;
        }
        capturedPromise->resolve(key);
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
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto format = convertEnumeration<SubtleCrypto::KeyFormat>(state, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, void());

    auto key = toCryptoKey(state, state.uncheckedArgument(1));
    RETURN_IF_EXCEPTION(scope, void());

    supportExportKeyThrow(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    if (!key->extractable()) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("The CryptoKey is nonextractable"));
        return;
    }

    auto algorithm = createAlgorithm(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

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
            capturedPromise->resolve(toJSValueFromJsonWebKey(*(capturedPromise->globalObject()), WTFMove(WTF::get<JsonWebKey>(key))));
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
        promise->reject<JSValue>(createNotEnoughArgumentsError(&state));
        return;
    }

    auto format = convertEnumeration<SubtleCrypto::KeyFormat>(state, state.uncheckedArgument(0));
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
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("Wrapping CryptoKey doesn't match AlgorithmIdentifier"));
        return;
    }

    if (!wrappingKey->allows(CryptoKeyUsageWrapKey)) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("Wrapping CryptoKey doesn't support wrapKey operation"));
        return;
    }

    supportExportKeyThrow(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    if (!key->extractable()) {
        promise->reject(INVALID_ACCESS_ERR, ASCIILiteral("The CryptoKey is nonextractable"));
        return;
    }

    auto exportAlgorithm = createAlgorithm(state, key->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    auto wrapAlgorithm = createAlgorithm(state, wrappingKey->algorithmIdentifier());
    RETURN_IF_EXCEPTION(scope, void());

    auto context = scriptExecutionContextFromExecState(&state);

    auto subtle = jsDynamicDowncast<JSSubtleCrypto*>(state.thisValue());
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
            auto jwk = toJSValueFromJsonWebKey(*(promise->globalObject()), WTFMove(WTF::get<JsonWebKey>(key)));
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
        wrapAlgorithm->encrypt(WTFMove(wrapParams), wrappingKey.releaseNonNull(), WTFMove(bytes), WTFMove(callback), WTFMove(exceptionCallback), *context, workQueue);
    };
    auto exceptionCallback = [capturedPromise = WTFMove(promise)](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    exportAlgorithm->exportKey(format, key.releaseNonNull(), WTFMove(callback), WTFMove(exceptionCallback));
}

JSValue JSSubtleCrypto::encrypt(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionEncryptPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::decrypt(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionDecryptPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::sign(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionSignPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::verify(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionVerifyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::digest(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionDigestPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::deriveKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionDeriveKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::generateKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionGenerateKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::importKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionImportKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::exportKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionExportKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::wrapKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionWrapKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

} // namespace WebCore

#endif
