/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DedicatedWorkerGlobalScope.h"

#include "ContentSecurityPolicyResponseHeaders.h"
#include "DOMWindow.h"
#include "DedicatedWorkerThread.h"
#include "JSRTCRtpScriptTransformer.h"
#include "JSRTCRtpScriptTransformerConstructor.h"
#include "MessageEvent.h"
#include "RTCRtpScriptTransformer.h"
#include "RequestAnimationFrameCallback.h"
#include "SecurityOrigin.h"
#if ENABLE(OFFSCREEN_CANVAS)
#include "WorkerAnimationController.h"
#endif
#include "WorkerObjectProxy.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DedicatedWorkerGlobalScope);

Ref<DedicatedWorkerGlobalScope> DedicatedWorkerGlobalScope::create(const WorkerParameters& params, Ref<SecurityOrigin>&& origin, DedicatedWorkerThread& thread, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider)
{
    auto context = adoptRef(*new DedicatedWorkerGlobalScope(params, WTFMove(origin), thread, WTFMove(topOrigin), connectionProxy, socketProvider));
    if (!params.shouldBypassMainWorldContentSecurityPolicy)
        context->applyContentSecurityPolicyResponseHeaders(params.contentSecurityPolicyResponseHeaders);
    return context;
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(const WorkerParameters& params, Ref<SecurityOrigin>&& origin, DedicatedWorkerThread& thread, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider)
    : WorkerGlobalScope(WorkerThreadType::DedicatedWorker, params, WTFMove(origin), thread, WTFMove(topOrigin), connectionProxy, socketProvider)
    , m_name(params.name)
{
}

DedicatedWorkerGlobalScope::~DedicatedWorkerGlobalScope() = default;

EventTargetInterface DedicatedWorkerGlobalScope::eventTargetInterface() const
{
    return DedicatedWorkerGlobalScopeEventTargetInterfaceType;
}

void DedicatedWorkerGlobalScope::prepareForDestruction()
{
    WorkerGlobalScope::prepareForDestruction();

#if ENABLE(WEB_RTC)
    m_rtcRtpTransformerConstructorMap.clear();
#endif
}

ExceptionOr<void> DedicatedWorkerGlobalScope::postMessage(JSC::JSGlobalObject& state, JSC::JSValue messageValue, PostMessageOptions&& options)
{
    Vector<RefPtr<MessagePort>> ports;
    auto message = SerializedScriptValue::create(state, messageValue, WTFMove(options.transfer), ports, SerializationContext::WorkerPostMessage);
    if (message.hasException())
        return message.releaseException();

    // Disentangle the port in preparation for sending it to the remote context.
    auto channels = MessagePort::disentanglePorts(WTFMove(ports));
    if (channels.hasException())
        return channels.releaseException();

    thread().workerObjectProxy().postMessageToWorkerObject({ message.releaseReturnValue(), channels.releaseReturnValue() });
    return { };
}

ExceptionOr<void> DedicatedWorkerGlobalScope::importScripts(const Vector<String>& urls)
{
    auto result = Base::importScripts(urls);
    thread().workerObjectProxy().reportPendingActivity(hasPendingActivity());
    return result;
}

DedicatedWorkerThread& DedicatedWorkerGlobalScope::thread()
{
    return static_cast<DedicatedWorkerThread&>(Base::thread());
}

#if ENABLE(OFFSCREEN_CANVAS)
CallbackId DedicatedWorkerGlobalScope::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    if (!m_workerAnimationController)
        m_workerAnimationController = WorkerAnimationController::create(*this);
    return m_workerAnimationController->requestAnimationFrame(WTFMove(callback));
}

void DedicatedWorkerGlobalScope::cancelAnimationFrame(CallbackId callbackId)
{
    if (m_workerAnimationController)
        m_workerAnimationController->cancelAnimationFrame(callbackId);
}
#endif

#if ENABLE(WEB_RTC)
ExceptionOr<void> DedicatedWorkerGlobalScope::registerRTCRtpScriptTransformer(String&& name, Ref<JSRTCRtpScriptTransformerConstructor>&& transformerConstructor)
{
    ASSERT(!isMainThread());

    if (name.isEmpty())
        return Exception { NotSupportedError, "Name cannot be the empty string"_s };

    if (m_rtcRtpTransformerConstructorMap.contains(name))
        return Exception { NotSupportedError, "A transformer was already registered with this name"_s };

    JSC::JSObject* jsConstructor = transformerConstructor->callbackData()->callback();
    auto* globalObject = jsConstructor->globalObject();
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!jsConstructor->isConstructor(vm))
        return Exception { TypeError, "Class definitition passed to registerRTCRtpScriptTransformer() is not a constructor"_s };

    auto prototype = jsConstructor->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

    if (!prototype.isObject())
        return Exception { TypeError, "Class definitition passed to registerRTCRtpScriptTransformer() has invalid prototype"_s };

    m_rtcRtpTransformerConstructorMap.add(name, WTFMove(transformerConstructor));

    thread().workerObjectProxy().postTaskToWorkerObject([name = name.isolatedCopy()](auto& worker) mutable {
        worker.addRTCRtpScriptTransformer(WTFMove(name));
    });

    return { };
}

RefPtr<RTCRtpScriptTransformer> DedicatedWorkerGlobalScope::createRTCRtpScriptTransformer(String&& name, TransferredMessagePort port)
{
    auto constructor = m_rtcRtpTransformerConstructorMap.get(name);
    ASSERT(constructor);
    if (!constructor)
        return nullptr;

    auto* jsConstructor = constructor->callbackData()->callback();
    auto* globalObject = constructor->callbackData()->globalObject();
    auto& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSC::JSLockHolder lock { globalObject };

    m_pendingRTCTransfomerMessagePort = MessagePort::entangle(*this, WTFMove(port));

    JSC::MarkedArgumentBuffer args;
    auto* object = JSC::construct(globalObject, jsConstructor, args, "Failed to construct RTCRtpScriptTransformer");
    ASSERT(!!scope.exception() == !object);

    if (scope.exception()) {
        scope.clearException();
        return nullptr;
    }
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto& jsTransformer = *JSC::jsCast<JSRTCRtpScriptTransformer*>(object);
    auto& transformer = jsTransformer.wrapped();
    transformer.setCallback(makeUnique<JSCallbackDataStrong>(&jsTransformer, globalObject));

    return &transformer;
}

#endif

} // namespace WebCore
