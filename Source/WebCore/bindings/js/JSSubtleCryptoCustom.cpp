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
#include "ScriptState.h"
#include <runtime/Error.h>
#include <runtime/IteratorOperations.h>

using namespace JSC;

namespace WebCore {

enum class Operations {
    Digest,
    GenerateKey,
    ImportKey,
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
    auto exceptionCallback = [capturedPromise =  promise.copyRef()](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    // The spec suggests we should perform the following task asynchronously regardless what kind of keys it produces
    // as of 11 December 2014: https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-generateKey
    // That's simply not efficient for AES and HMAC keys. Therefore, we perform it as an async task conditionally.
    algorithm->generateKey(WTFMove(params), extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback), *scriptExecutionContextFromExecState(&state));
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
    auto exceptionCallback = [capturedPromise =  promise.copyRef()](ExceptionCode ec) mutable {
        rejectWithException(WTFMove(capturedPromise), ec);
    };

    // The spec suggests we should perform the following task asynchronously as of 11 December 2014:
    // https://www.w3.org/TR/WebCryptoAPI/#SubtleCrypto-method-importKey
    // That's simply not necessary. Therefore, we perform it synchronously.
    algorithm->importKey(format, WTFMove(keyData), WTFMove(params), extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback));
}

JSValue JSSubtleCrypto::generateKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionGenerateKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

JSValue JSSubtleCrypto::importKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionImportKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

} // namespace WebCore

#endif
