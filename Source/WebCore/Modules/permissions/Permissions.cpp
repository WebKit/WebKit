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
#include "Permissions.h"

#include "ContextDestructionObserverInlines.h"
#include "DedicatedWorkerGlobalScope.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "Exception.h"
#include "Geolocation.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMPromiseDeferred.h"
#include "JSPermissionDescriptor.h"
#include "JSPermissionStatus.h"
#include "LocalFrame.h"
#include "Navigator.h"
#include "NavigatorBase.h"
#include "NavigatorGeolocation.h"
#include "Page.h"
#include "PermissionController.h"
#include "PermissionDescriptor.h"
#include "PermissionName.h"
#include "PermissionQuerySource.h"
#include "PermissionsPolicy.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerGlobalScope.h"
#include "SharedWorkerGlobalScope.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <JavaScriptCore/HeapCellInlines.h>
#include <optional>
#include <wtf/Expected.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Permissions);

#if ENABLE(GEOLOCATION)

static std::optional<PermissionState> determineGeolocationPermissionState(PermissionState permissionState, const Document& document)
{
    RefPtr window = document.window();
    if (!window)
        return std::nullopt;

    RefPtr geolocation = NavigatorGeolocation::optionalGeolocation(protect(window->navigator()));

    switch (permissionState) {
    case PermissionState::Granted:
        return PermissionState::Granted;
    case PermissionState::Denied:
        if (!geolocation || !geolocation->hasBeenRequested())
            return PermissionState::Prompt;
        return PermissionState::Denied;
    case PermissionState::Prompt:
        if (!geolocation || !geolocation->hasBeenRequested())
            return PermissionState::Prompt;
        return geolocation->isAllowed() ? PermissionState::Granted : PermissionState::Denied;
    };

    return std::nullopt;
}

#endif // ENABLE(GEOLOCATION)

Ref<Permissions> Permissions::create(NavigatorBase& navigator)
{
    return adoptRef(*new Permissions(navigator));
}

Permissions::Permissions(NavigatorBase& navigator)
    : m_navigator(navigator)
{
}

NavigatorBase* Permissions::navigator()
{
    return m_navigator.get();
}

Permissions::~Permissions()
{
    auto queryPromises = std::exchange(m_queryPromises, { });
    for (auto& promise : queryPromises.values())
        promise->reject(ExceptionCode::AbortError, "Promise was rejected because the browsing context is going away"_s);
}

static bool isAllowedByPermissionsPolicy(const Document& document, PermissionName name)
{
    switch (name) {
    case PermissionName::Camera:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Camera, document, PermissionsPolicy::ShouldReportViolation::No);
    case PermissionName::Geolocation:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Geolocation, document, PermissionsPolicy::ShouldReportViolation::No);
    case PermissionName::Microphone:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, document, PermissionsPolicy::ShouldReportViolation::No);
    case PermissionName::StorageAccess:
        return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::StorageAccess, document, PermissionsPolicy::ShouldReportViolation::No);
    default:
        return true;
    }
}

std::optional<PermissionQuerySource> Permissions::sourceFromContext(const ScriptExecutionContext& context)
{
    if (is<Document>(context))
        return PermissionQuerySource::Window;
    if (is<DedicatedWorkerGlobalScope>(context))
        return PermissionQuerySource::DedicatedWorker;
    if (is<SharedWorkerGlobalScope>(context))
        return PermissionQuerySource::SharedWorker;
    if (is<ServiceWorkerGlobalScope>(context))
        return PermissionQuerySource::ServiceWorker;
    return std::nullopt;
}


std::optional<PermissionName> Permissions::toPermissionName(const String& name)
{
    if (name == "camera"_s)
        return PermissionName::Camera;
    if (name == "geolocation"_s)
        return PermissionName::Geolocation;
    if (name == "microphone"_s)
        return PermissionName::Microphone;
    if (name == "notifications"_s)
        return PermissionName::Notifications;
    if (name == "push"_s)
        return PermissionName::Push;
    if (name == "storage-access"_s)
        return PermissionName::StorageAccess;
    return std::nullopt;
}

static Expected<PermissionState, Exception> processPermissionQueryResult(std::optional<PermissionState> permissionState, const PermissionDescriptor& permissionDescriptor, const Document& document)
{
    if (!permissionState)
        return makeUnexpected(Exception { ExceptionCode::NotSupportedError, "Permissions::query does not support this API"_s });

#if !ENABLE(GEOLOCATION) && !ENABLE(MEDIA_STREAM)
    UNUSED_PARAM(permissionDescriptor);
    UNUSED_PARAM(document);
#endif

#if ENABLE(GEOLOCATION)
    if (permissionDescriptor.name == PermissionName::Geolocation) {
        if (auto geolocationPermissionState = determineGeolocationPermissionState(*permissionState, document))
            permissionState = geolocationPermissionState;
        else
            return makeUnexpected(Exception { ExceptionCode::InvalidStateError, "The Document does not have a Geolocation object"_s });
    }
#endif

#if ENABLE(MEDIA_STREAM)
    if (document.quirks().shouldEnableCameraAndMicrophonePermissionStateQuirk() && (permissionDescriptor.name == PermissionName::Camera || permissionDescriptor.name == PermissionName::Microphone) && *permissionState == PermissionState::Prompt)
        permissionState = PermissionState::Granted;
#endif

    return *permissionState;
}

void Permissions::query(JSC::Strong<JSC::JSObject> permissionDescriptorValue, Ref<DeferredPromise>&& promise)
{
    RefPtr context = m_navigator ? m_navigator->scriptExecutionContext() : nullptr;
    if (!context || !context->globalObject()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "The context is invalid"_s });
        return;
    }

    auto source = sourceFromContext(*context);
    if (!source) {
        promise->reject(Exception { ExceptionCode::NotSupportedError, "Permissions::query is not supported in this context"_s  });
        return;
    }

    RefPtr document = dynamicDowncast<Document>(*context);
    if (document && !document->isFullyActive()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "The document is not fully active"_s });
        return;
    }

    auto& vm = context->globalObject()->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto permissionDescriptorConversionResult = convert<IDLDictionary<PermissionDescriptor>>(*context->globalObject(), permissionDescriptorValue.get());
    if (permissionDescriptorConversionResult.hasException(scope)) [[unlikely]] {
        promise->reject(Exception { ExceptionCode::ExistingExceptionError });
        return;
    }

    auto permissionDescriptor = permissionDescriptorConversionResult.releaseReturnValue();

    RefPtr origin = context->securityOrigin();
    auto originData = origin ? origin->data() : SecurityOriginData { };

    auto contextIdentifier = context->identifier();

    auto promiseIdentifier = PromiseIdentifier::generate();
    m_queryPromises.add(promiseIdentifier, WTF::move(promise));

    auto queryPermissionOnMainThread = [originData = WTF::move(originData).isolatedCopy(), permissionDescriptor, contextIdentifier, source = *source, promiseIdentifier, weakThis = WeakPtr { *this }] (ScriptExecutionContext& mainThreadContext) mutable {
        ASSERT(isMainThread());

        auto& document = downcast<Document>(mainThreadContext);
        if (!document.page()) {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [weakThis = WTF::move(weakThis), promiseIdentifier](auto&) mutable {
                RefPtr protectedThis = weakThis;
                if (!protectedThis)
                    return;
                if (RefPtr promise = protectedThis->m_queryPromises.take(promiseIdentifier))
                    promise->reject(Exception { ExceptionCode::InvalidStateError, "The page does not exist"_s });
            });
            return;
        }

        if (source == PermissionQuerySource::Window && !isAllowedByPermissionsPolicy(document, permissionDescriptor.name)) {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [weakThis = WTF::move(weakThis), promiseIdentifier, permissionDescriptor, page = WeakPtr { *document.page() }](auto& context) mutable {
                RefPtr protectedThis = weakThis;
                if (!protectedThis)
                    return;
                if (RefPtr promise = protectedThis->m_queryPromises.take(promiseIdentifier))
                    promise->resolve<IDLInterface<PermissionStatus>>(PermissionStatus::create(context, PermissionState::Denied, permissionDescriptor, PermissionQuerySource::Window, WTF::move(page)));
            });
            return;
        }

        auto page = source == PermissionQuerySource::DedicatedWorker || source == PermissionQuerySource::Window ? WeakPtr { *document.page() } : nullptr;

        PermissionController::singleton().query(ClientOrigin { document.topOrigin().data(), WTF::move(originData) }, permissionDescriptor, page, source, [contextIdentifier, permissionDescriptor, weakThis = WTF::move(weakThis), promiseIdentifier, source, page, document = Ref { document }](auto permissionState) mutable {
            ASSERT(isMainThread());

            auto result = processPermissionQueryResult(permissionState, permissionDescriptor, document);
            if (!result) {
                ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [weakThis = WTF::move(weakThis), promiseIdentifier, exception = result.error()](auto&) mutable {
                    RefPtr protectedThis = weakThis;
                    if (!protectedThis)
                        return;
                    if (RefPtr promise = protectedThis->m_queryPromises.take(promiseIdentifier))
                        promise->reject(WTF::move(exception));
                });
                return;
            }

            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, [weakThis = WTF::move(weakThis), promiseIdentifier, permissionState = *result, permissionDescriptor, source, page = WTF::move(page)](auto& context) mutable {
                RefPtr protectedThis = weakThis;
                if (!protectedThis)
                    return;
                if (RefPtr promise = protectedThis->m_queryPromises.take(promiseIdentifier))
                    promise->resolve<IDLInterface<PermissionStatus>>(PermissionStatus::create(context, permissionState, permissionDescriptor, source, WTF::move(page)));
            });
        });
    };

    if (document)
        queryPermissionOnMainThread(*document);
    else {
        Ref workerGlobalScope = downcast<WorkerGlobalScope>(*context);
        if (CheckedPtr workerLoaderProxy = workerGlobalScope->thread()->workerLoaderProxy())
            workerLoaderProxy->postTaskToLoader(WTF::move(queryPermissionOnMainThread));
    }
}

} // namespace WebCore
