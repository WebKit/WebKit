/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WakeLock.h"

#include "DocumentInlines.h"
#include "EventLoop.h"
#include "Exception.h"
#include "FeaturePolicy.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWakeLockSentinel.h"
#include "LocalDOMWindow.h"
#include "Page.h"
#include "PermissionController.h"
#include "PermissionQuerySource.h"
#include "PermissionState.h"
#include "VisibilityState.h"
#include "WakeLockManager.h"
#include "WakeLockSentinel.h"

namespace WebCore {

WakeLock::WakeLock(Document* document)
    : ContextDestructionObserver(document)
{
}

// https://www.w3.org/TR/screen-wake-lock/#the-request-method
void WakeLock::request(WakeLockType lockType, Ref<DeferredPromise>&& promise)
{
    RefPtr document = this->document();
    if (!document || !document->isFullyActive() || !document->page()) {
        promise->reject(Exception { ExceptionCode::NotAllowedError, "Document is not fully active"_s });
        return;
    }
    if (!isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::ScreenWakeLock, *document, LogFeaturePolicyFailure::Yes)) {
        promise->reject(Exception { ExceptionCode::NotAllowedError, "'screen-wake-lock' is not allowed by Feature-Policy"_s });
        return;
    }
    if (document->visibilityState() == VisibilityState::Hidden) {
        promise->reject(Exception { ExceptionCode::NotAllowedError, "Document is hidden"_s });
        return;
    }

    // FIXME: The permission check can likely be dropped once the specification gets updated to only
    // require transient activation (https://github.com/w3c/screen-wake-lock/pull/326).
    bool hasTransientActivation = document->domWindow() && document->domWindow()->hasTransientActivation();
    PermissionController::shared().query(document->clientOrigin(), PermissionDescriptor { PermissionName::ScreenWakeLock }, *document->page(), PermissionQuerySource::Window, [this, protectedThis = Ref { *this }, document = Ref { *document }, hasTransientActivation, promise = WTFMove(promise), lockType](std::optional<PermissionState> permission) mutable {
        if (!permission || *permission == PermissionState::Prompt) {
            if (hasTransientActivation || m_wasPreviouslyAuthorizedDueToTransientActivation) {
                m_wasPreviouslyAuthorizedDueToTransientActivation = true;
                permission = PermissionState::Granted;
            } else
                permission = PermissionState::Denied;
        } else if (*permission == PermissionState::Denied)
            m_wasPreviouslyAuthorizedDueToTransientActivation = false;
        document->eventLoop().queueTask(TaskSource::ScreenWakelock, [protectedThis = WTFMove(protectedThis), document = WTFMove(document), promise = WTFMove(promise), lockType, permission]() mutable {
            if (permission == PermissionState::Denied) {
                promise->reject(Exception { ExceptionCode::NotAllowedError, "Permission was denied"_s });
                return;
            }
            if (!document->isFullyActive()) {
                promise->reject(Exception { ExceptionCode::NotAllowedError, "Document is not fully active"_s });
                return;
            }
            if (document->visibilityState() == VisibilityState::Hidden) {
                promise->reject(Exception { ExceptionCode::NotAllowedError, "Document is hidden"_s });
                return;
            }
            auto lock = WakeLockSentinel::create(document, lockType);
            promise->resolve<IDLInterface<WakeLockSentinel>>(lock.get());
            document->wakeLockManager().addWakeLock(WTFMove(lock), document->pageID());
        });
    });
}

Document* WakeLock::document()
{
    return downcast<Document>(scriptExecutionContext());
}

} // namespace WebCore
