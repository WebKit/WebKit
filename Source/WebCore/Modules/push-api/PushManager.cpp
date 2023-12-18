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

#include "DocumentInlines.h"
#include "EventLoop.h"
#include "Exception.h"
#include "JSDOMPromiseDeferred.h"
#include "JSPushPermissionState.h"
#include "JSPushSubscription.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "NotificationClient.h"
#include "PushCrypto.h"
#include "PushSubscriptionOwner.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorkerRegistration.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PushManager);

PushManager::PushManager(PushSubscriptionOwner& pushSubscriptionOwner)
    : m_pushSubscriptionOwner(pushSubscriptionOwner)
{
}

PushManager::~PushManager() = default;

Vector<String> PushManager::supportedContentEncodings()
{
    return Vector<String> { "aesgcm"_s, "aes128gcm"_s };
}

void PushManager::ref() const
{
    m_pushSubscriptionOwner.ref();
}

void PushManager::deref() const
{
    m_pushSubscriptionOwner.deref();
}

void PushManager::subscribe(ScriptExecutionContext& context, std::optional<PushSubscriptionOptionsInit>&& options, DOMPromiseDeferred<IDLInterface<PushSubscription>>&& promise)
{
    RELEASE_ASSERT(context.isSecureContext());

    context.eventLoop().queueTask(TaskSource::Networking, [this, protectedThis = Ref { *this }, context = Ref { context }, options = WTFMove(options), promise = WTFMove(promise)]() mutable {
        if (!options || !options->userVisibleOnly) {
            promise.reject(Exception { ExceptionCode::NotAllowedError, "Subscribing for push requires userVisibleOnly to be true"_s });
            return;
        }

        if (!options || !options->applicationServerKey) {
            promise.reject(Exception { ExceptionCode::NotSupportedError, "Subscribing for push requires an applicationServerKey"_s });
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
                return Exception { ExceptionCode::InvalidCharacterError, "applicationServerKey is not properly base64url-encoded"_s };
            return WTFMove(decoded.value());
        });

        if (keyDataResult.hasException()) {
            promise.reject(keyDataResult.releaseException());
            return;
        }

        if (!PushCrypto::validateP256DHPublicKey(keyDataResult.returnValue())) {
            promise.reject(Exception { ExceptionCode::InvalidAccessError, "applicationServerKey must contain a valid P-256 public key"_s });
            return;
        }

        if (!m_pushSubscriptionOwner.isActive()) {
            // Only PushSubscriptionOwner objects related to service workers will ever return `false` for isActive(),
            // so this error message is correct.
            promise.reject(Exception { ExceptionCode::InvalidStateError, "Subscribing for push requires an active service worker"_s });
            return;
        }

        auto client = context->notificationClient();
        auto permission = client ? client->checkPermission(context.ptr()) : NotificationPermission::Denied;

        if (permission == NotificationPermission::Denied) {
            promise.reject(Exception { ExceptionCode::NotAllowedError, "User denied push permission"_s });
            return;
        }

        if (permission == NotificationPermission::Default && !context->isDocument()) {
            promise.reject(Exception { ExceptionCode::NotAllowedError, "User denied push permission"_s });
            return;
        }

        if (permission == NotificationPermission::Default) {
            RELEASE_ASSERT(client);
            RELEASE_ASSERT(context->isDocument());

            auto& document = downcast<Document>(context.get());
            if (!document.isSameOriginAsTopDocument()) {
                promise.reject(Exception { ExceptionCode::NotAllowedError, "Cannot request permission from cross-origin iframe"_s });
                return;
            }

            RefPtr window = document.frame() ? document.frame()->window() : nullptr;
            if (!window || !window->consumeTransientActivation()) {
#if !RELEASE_LOG_DISABLED
                Seconds lastActivationDuration = window ? MonotonicTime::now() - window->lastActivationTimestamp() : Seconds::infinity();
                RELEASE_LOG_ERROR(Push, "Failing PushManager.subscribe call due to failed transient activation check; last activated %.2f sec ago", lastActivationDuration.value());
#endif

                auto errorMessage = "Push notification prompting can only be done from a user gesture."_s;
                document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, errorMessage);
                promise.reject(Exception { ExceptionCode::NotAllowedError, errorMessage });
                return;
            }

            client->requestPermission(context, [this, protectedThis = WTFMove(protectedThis), keyData = keyDataResult.releaseReturnValue(), promise = WTFMove(promise)](auto permission) mutable {
                if (permission != NotificationPermission::Granted) {
                    promise.reject(Exception { ExceptionCode::NotAllowedError, "User denied push permission"_s });
                    return;
                }

                m_pushSubscriptionOwner.subscribeToPushService(WTFMove(keyData), WTFMove(promise));
            });
            return;
        }

        RELEASE_ASSERT(permission == NotificationPermission::Granted);
        m_pushSubscriptionOwner.subscribeToPushService(keyDataResult.releaseReturnValue(), WTFMove(promise));
    });
}

void PushManager::getSubscription(ScriptExecutionContext& context, DOMPromiseDeferred<IDLNullable<IDLInterface<PushSubscription>>>&& promise)
{
    context.eventLoop().queueTask(TaskSource::Networking, [this, protectedThis = Ref { *this }, promise = WTFMove(promise)]() mutable {
        m_pushSubscriptionOwner.getPushSubscription(WTFMove(promise));
    });
}

void PushManager::permissionState(ScriptExecutionContext& context, std::optional<PushSubscriptionOptionsInit>&&, DOMPromiseDeferred<IDLEnumeration<PushPermissionState>>&& promise)
{
    context.eventLoop().queueTask(TaskSource::Networking, [context = Ref { context }, promise = WTFMove(promise)]() mutable {
        auto client = context->notificationClient();
        auto permission = client ? client->checkPermission(context.ptr()) : NotificationPermission::Denied;

        switch (permission) {
        case NotificationPermission::Default:
            promise.resolve(PushPermissionState::Prompt);
            break;
        case NotificationPermission::Granted:
            promise.resolve(PushPermissionState::Granted);
            break;
        default:
            promise.resolve(PushPermissionState::Denied);
            break;
        }
    });
}

} // namespace WebCore
