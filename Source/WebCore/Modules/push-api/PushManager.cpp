/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PushManager.h"

#if ENABLE(SERVICE_WORKER)

#include "CryptoKeyEC.h"
#include "Exception.h"
#include "JSPushPermissionState.h"
#include "JSPushSubscription.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorkerRegistration.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PushManager);

PushManager::PushManager(ServiceWorkerRegistration& serviceWorkerRegistration)
    : m_serviceWorkerRegistration(serviceWorkerRegistration)
{
}

PushManager::~PushManager() = default;

Vector<String> PushManager::supportedContentEncodings()
{
    return Vector<String> { "aesgcm"_s, "aes128gcm"_s };
}

void PushManager::ref() const
{
    m_serviceWorkerRegistration.ref();
}

void PushManager::deref() const
{
    m_serviceWorkerRegistration.deref();
}

void PushManager::subscribe(ScriptExecutionContext& scriptExecutionContext, std::optional<PushSubscriptionOptionsInit>&& options, DOMPromiseDeferred<IDLInterface<PushSubscription>>&& promise)
{
    RELEASE_ASSERT(scriptExecutionContext.isSecureContext());
    
    if (!options || !options->userVisibleOnly) {
        promise.reject(Exception { NotAllowedError, "Subscribing for push requires userVisibleOnly to be true"_s });
        return;
    }
    
    if (!options || !options->applicationServerKey) {
        promise.reject(Exception { NotSupportedError, "Subscribing for push requires an applicationServerKey"_s });
        return;
    }
    
    using KeyDataResult = ExceptionOr<Vector<uint8_t>>;
    auto keyDataResult = WTF::switchOn(*options->applicationServerKey, [](RefPtr<JSC::ArrayBuffer>& value) -> KeyDataResult {
        if (!value)
            return Vector<uint8_t> { };
        return Vector<uint8_t> { reinterpret_cast<const uint8_t*>(value->data()), value->byteLength() };
    }, [](RefPtr<JSC::ArrayBufferView>& value) -> KeyDataResult {
        if (!value)
            return Vector<uint8_t> { };
        return Vector<uint8_t> { reinterpret_cast<const uint8_t*>(value->baseAddress()), value->byteLength() };
    }, [](String& value) -> KeyDataResult {
        auto decoded = base64URLDecode(value);
        if (!decoded)
            return Exception { InvalidCharacterError, "applicationServerKey is not properly base64url-encoded"_s };
        return WTFMove(decoded.value());
    });
    
    if (keyDataResult.hasException()) {
        promise.reject(keyDataResult.releaseException());
        return;
    }
    
#if ENABLE(WEB_CRYPTO)
    auto keyData = keyDataResult.returnValue();
    auto key = CryptoKeyEC::importRaw(CryptoAlgorithmIdentifier::ECDSA, "P-256"_s, WTFMove(keyData), false, CryptoKeyUsageVerify);
#else
    auto key = nullptr;
#endif
    
    if (!key) {
        promise.reject(Exception { InvalidAccessError, "applicationServerKey must contain a valid P-256 public key"_s });
        return;
    }

    if (!m_serviceWorkerRegistration.active()) {
        promise.reject(Exception { InvalidStateError, "Subscribing for push requires an active service worker"_s });
        return;
    }
    
    m_serviceWorkerRegistration.subscribeToPushService(keyDataResult.releaseReturnValue(), WTFMove(promise));
}

void PushManager::getSubscription(DOMPromiseDeferred<IDLNullable<IDLInterface<PushSubscription>>>&& promise)
{
    m_serviceWorkerRegistration.getPushSubscription(WTFMove(promise));
}

void PushManager::permissionState(std::optional<PushSubscriptionOptionsInit>&& options, DOMPromiseDeferred<IDLEnumeration<PushPermissionState>>&& promise)
{
    UNUSED_PARAM(options);
    m_serviceWorkerRegistration.getPushPermissionState(WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
