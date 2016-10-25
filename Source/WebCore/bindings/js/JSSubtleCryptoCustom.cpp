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
#include "JSHmacKeyGenParams.h"
#include "JSRsaHashedKeyGenParams.h"
#include "JSRsaKeyGenParams.h"
#include "ScriptState.h"
#include <runtime/Error.h>
#include <runtime/IteratorOperations.h>

using namespace JSC;

namespace WebCore {

enum class Operations {
    GenerateKey,
    Digest,
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

        CryptoAlgorithmIdentifier identifier;
        if (!CryptoAlgorithmRegistry::singleton().getIdentifierForName(params.name, identifier)) {
            setDOMException(&state, NOT_SUPPORTED_ERR);
            return nullptr;
        }

        std::unique_ptr<CryptoAlgorithmParameters> result;
        switch (operation) {
        case Operations::GenerateKey:
            switch (identifier) {
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
                auto params = convertDictionary<CryptoAlgorithmHmacKeyGenParams>(state, value);
                RETURN_IF_EXCEPTION(scope, nullptr);
                params.hashIdentifier = toHashIdentifier(state, params.hash);
                RETURN_IF_EXCEPTION(scope, nullptr);
                result = std::make_unique<CryptoAlgorithmHmacKeyGenParams>(params);
                break;
            }
            default:
                setDOMException(&state, NOT_SUPPORTED_ERR);
                return nullptr;
            }
            break;
        case Operations::Digest:
            switch (identifier) {
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
        default:
            ASSERT_NOT_REACHED();
            return nullptr;
        }

        result->identifier = identifier;
        return result;
    }

    throwTypeError(&state, scope, ASCIILiteral("Invalid AlgorithmIdentifier"));
    return nullptr;
}

static CryptoKeyUsage cryptoKeyUsagesFromJSValue(ExecState& state, JSValue iterable)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CryptoKeyUsage result = 0;
    forEachInIterable(&state, iterable, [&result](VM& vm, ExecState* state, JSValue nextItem) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        String usageString = nextItem.toWTFString(state);
        RETURN_IF_EXCEPTION(scope, void());
        if (usageString == "encrypt")
            result |= CryptoKeyUsageEncrypt;
        else if (usageString == "decrypt")
            result |= CryptoKeyUsageDecrypt;
        else if (usageString == "sign")
            result |= CryptoKeyUsageSign;
        else if (usageString == "verify")
            result |= CryptoKeyUsageVerify;
        else if (usageString == "deriveKey")
            result |= CryptoKeyUsageDeriveKey;
        else if (usageString == "deriveBits")
            result |= CryptoKeyUsageDeriveBits;
        else if (usageString == "wrapKey")
            result |= CryptoKeyUsageWrapKey;
        else if (usageString == "unwrapKey")
            result |= CryptoKeyUsageUnwrapKey;
        else
            throwTypeError(state, scope, ASCIILiteral("Invalid KeyUsages"));
    });
    RETURN_IF_EXCEPTION(scope, 0);
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

    auto keyUsages = cryptoKeyUsagesFromJSValue(state, state.argument(2));
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
    algorithm->generateKey(WTFMove(params), extractable, keyUsages, WTFMove(callback), WTFMove(exceptionCallback), scriptExecutionContextFromExecState(&state));
}

JSValue JSSubtleCrypto::generateKey(ExecState& state)
{
    return callPromiseFunction<jsSubtleCryptoFunctionGenerateKeyPromise, PromiseExecutionScope::WindowOrWorker>(state);
}

} // namespace WebCore

#endif
