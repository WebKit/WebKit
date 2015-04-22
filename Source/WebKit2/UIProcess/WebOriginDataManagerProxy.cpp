/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
#include "WebOriginDataManagerProxy.h"

#include "APISecurityOrigin.h"
#include "SecurityOriginData.h"
#include "WebOriginDataManagerMessages.h"
#include "WebOriginDataManagerProxyMessages.h"
#include "WebProcessPool.h"
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

const char* WebOriginDataManagerProxy::supplementName()
{
    return "WebOriginDataManagerProxy";
}

PassRefPtr<WebOriginDataManagerProxy> WebOriginDataManagerProxy::create(WebProcessPool* processPool)
{
    return adoptRef(new WebOriginDataManagerProxy(processPool));
}

WebOriginDataManagerProxy::WebOriginDataManagerProxy(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
{
    processPool->addMessageReceiver(Messages::WebOriginDataManagerProxy::messageReceiverName(), *this);
}

WebOriginDataManagerProxy::~WebOriginDataManagerProxy()
{
}


void WebOriginDataManagerProxy::processPoolDestroyed()
{
    invalidateCallbackMap(m_arrayCallbacks, CallbackBase::Error::OwnerWasInvalidated);
}

void WebOriginDataManagerProxy::processDidClose(WebProcessProxy*)
{
    invalidateCallbackMap(m_arrayCallbacks, CallbackBase::Error::ProcessExited);
}

bool WebOriginDataManagerProxy::shouldTerminate(WebProcessProxy*) const
{
    return m_arrayCallbacks.isEmpty();
}

void WebOriginDataManagerProxy::refWebContextSupplement()
{
    API::Object::ref();
}

void WebOriginDataManagerProxy::derefWebContextSupplement()
{
    API::Object::deref();
}

class CallbackSynchronizer : public RefCounted<CallbackSynchronizer> {
public:
    static PassRefPtr<CallbackSynchronizer> create(const std::function<void (const CallbackBase::Error&)>& callback)
    {
        return adoptRef(new CallbackSynchronizer(callback));
    }

    ~CallbackSynchronizer()
    {
        ASSERT(!m_count);
        ASSERT(!m_callback);
    }

    void taskStarted()
    {
        ++m_count;
    }

    void taskCompleted(const CallbackBase::Error& error)
    {
        if (error != CallbackBase::Error::None)
            m_error = error;

        ASSERT(m_count);
        if (!--m_count) {
            ASSERT(m_callback);
            m_callback(m_error);
            m_callback = nullptr;
        }
    }

protected:
    CallbackSynchronizer(const std::function<void (const CallbackBase::Error&)>& callback)
        : m_count(0)
        , m_callback(callback)
        , m_error(CallbackBase::Error::None)
    {
        ASSERT(m_callback);
    }

    unsigned m_count;
    std::function<void (const CallbackBase::Error&)> m_callback;
    CallbackBase::Error m_error;
};

static std::pair<RefPtr<CallbackSynchronizer>, VoidCallback::CallbackFunction> createSynchronizedCallback(typename VoidCallback::CallbackFunction callback)
{
    RefPtr<CallbackSynchronizer> synchronizer = CallbackSynchronizer::create(callback);
    VoidCallback::CallbackFunction synchronizedCallback = [synchronizer](CallbackBase::Error error) {
        synchronizer->taskCompleted(error);
    };

    return std::make_pair(synchronizer, synchronizedCallback);
}

static std::pair<RefPtr<CallbackSynchronizer>, ArrayCallback::CallbackFunction> createSynchronizedCallback(typename ArrayCallback::CallbackFunction callback)
{
    RefPtr<API::Array> aggregateArray = API::Array::create();
    RefPtr<CallbackSynchronizer> synchronizer = CallbackSynchronizer::create([aggregateArray, callback](const CallbackBase::Error& error) {
        callback(aggregateArray.get(), error);
    });

    ArrayCallback::CallbackFunction synchronizedCallback = [aggregateArray, synchronizer](API::Array* array, CallbackBase::Error error) {
        if (array)
            aggregateArray->elements().appendVector(array->elements());
        synchronizer->taskCompleted(error);
    };

    return std::make_pair(synchronizer, synchronizedCallback);
}

template <typename CallbackType, typename MessageType, typename... Parameters>
static void sendMessageToAllProcessesInProcessPool(WebProcessPool* processPool, typename CallbackType::CallbackFunction callback, HashMap<uint64_t, RefPtr<CallbackType>>& callbackStorage, Parameters... parameters)
{
    if (!processPool) {
        CallbackType::create(callback)->invalidate();
        return;
    }

    auto synchronizerAndCallback = createSynchronizedCallback(callback);
    RefPtr<CallbackSynchronizer> synchronizer = synchronizerAndCallback.first;
    auto perProcessCallback = synchronizerAndCallback.second;

    for (auto& process : processPool->processes()) {
        if (!process || !process->canSendMessage())
            continue;

        synchronizer->taskStarted();
        RefPtr<CallbackType> callback = CallbackType::create(perProcessCallback);
        uint64_t callbackID = callback->callbackID();
        callbackStorage.set(callbackID, callback.release());
        process->send(MessageType(parameters..., callbackID), 0);
    }

    {
        synchronizer->taskStarted();
        RefPtr<CallbackType> callback = CallbackType::create(perProcessCallback);
        uint64_t callbackID = callback->callbackID();
        callbackStorage.set(callbackID, callback.release());
        processPool->sendToDatabaseProcessRelaunchingIfNecessary(MessageType(parameters..., callbackID));
    }
}

void WebOriginDataManagerProxy::getOrigins(WKOriginDataTypes types, std::function<void (API::Array*, CallbackBase::Error)> callbackFunction)
{
    sendMessageToAllProcessesInProcessPool<ArrayCallback, Messages::WebOriginDataManager::GetOrigins>(processPool(), callbackFunction, m_arrayCallbacks, types);
}

void WebOriginDataManagerProxy::didGetOrigins(IPC::Connection& connection, const Vector<SecurityOriginData>& originIdentifiers, uint64_t callbackID)
{
    RefPtr<ArrayCallback> callback = m_arrayCallbacks.take(callbackID);
    MESSAGE_CHECK_BASE(callback, &connection);

    Vector<RefPtr<API::Object>> securityOrigins;
    securityOrigins.reserveInitialCapacity(originIdentifiers.size());

    for (const auto& originIdentifier : originIdentifiers)
        securityOrigins.uncheckedAppend(API::SecurityOrigin::create(originIdentifier.securityOrigin()));

    callback->performCallbackWithReturnValue(API::Array::create(WTF::move(securityOrigins)).ptr());
}

void WebOriginDataManagerProxy::deleteEntriesForOrigin(WKOriginDataTypes types, API::SecurityOrigin* origin, std::function<void (CallbackBase::Error)> callbackFunction)
{
    SecurityOriginData securityOriginData;
    securityOriginData.protocol = origin->securityOrigin().protocol();
    securityOriginData.host = origin->securityOrigin().host();
    securityOriginData.port = origin->securityOrigin().port();

    sendMessageToAllProcessesInProcessPool<VoidCallback, Messages::WebOriginDataManager::DeleteEntriesForOrigin>(processPool(), callbackFunction, m_voidCallbacks, types, securityOriginData);
}

void WebOriginDataManagerProxy::deleteEntriesModifiedBetweenDates(WKOriginDataTypes types, double startDate, double endDate, std::function<void (CallbackBase::Error)> callbackFunction)
{
    sendMessageToAllProcessesInProcessPool<VoidCallback, Messages::WebOriginDataManager::DeleteEntriesModifiedBetweenDates>(processPool(), callbackFunction, m_voidCallbacks, types, startDate, endDate);
}

void WebOriginDataManagerProxy::didDeleteEntries(IPC::Connection& connection, uint64_t callbackID)
{
    RefPtr<VoidCallback> callback = m_voidCallbacks.take(callbackID);
    MESSAGE_CHECK_BASE(callback, &connection);
    callback->performCallback();
}

void WebOriginDataManagerProxy::deleteAllEntries(WKOriginDataTypes types, std::function<void (CallbackBase::Error)> callbackFunction)
{
    sendMessageToAllProcessesInProcessPool<VoidCallback, Messages::WebOriginDataManager::DeleteAllEntries>(processPool(), callbackFunction, m_voidCallbacks, types);
}

void WebOriginDataManagerProxy::didDeleteAllEntries(IPC::Connection& connection, uint64_t callbackID)
{
    RefPtr<VoidCallback> callback = m_voidCallbacks.take(callbackID);
    MESSAGE_CHECK_BASE(callback, &connection);
    callback->performCallback();
}

} // namespace WebKit
