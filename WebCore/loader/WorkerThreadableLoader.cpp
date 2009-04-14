/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#if ENABLE(WORKERS)

#include "WorkerThreadableLoader.h"

#include "GenericWorkerTask.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ThreadableLoader.h"
#include "WorkerContext.h"
#include "WorkerMessagingProxy.h"
#include "WorkerThread.h"
#include <memory>
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

static const char loadResourceSynchronouslyMode[] = "loadResourceSynchronouslyMode";

// FIXME: The assumption that we can upcast worker object proxy to WorkerMessagingProxy will not be true in multi-process implementation.
WorkerThreadableLoader::WorkerThreadableLoader(WorkerContext* workerContext, ThreadableLoaderClient* client, const String& taskMode, const ResourceRequest& request, LoadCallbacks callbacksSetting,
                                               ContentSniff contentSniff, StoredCredentials storedCredentials)
    : m_workerContext(workerContext)
    , m_workerClientWrapper(ThreadableLoaderClientWrapper::create(client))
    , m_bridge(*(new MainThreadBridge(m_workerClientWrapper, *(static_cast<WorkerMessagingProxy*>(m_workerContext->thread()->workerObjectProxy())), taskMode, request, callbacksSetting,
                                      contentSniff, storedCredentials)))
{
}

WorkerThreadableLoader::~WorkerThreadableLoader()
{
    m_bridge.destroy();
}

void WorkerThreadableLoader::loadResourceSynchronously(WorkerContext* workerContext, const ResourceRequest& request, ThreadableLoaderClient& client, StoredCredentials storedCredentials)
{
    WorkerRunLoop& runLoop = workerContext->thread()->runLoop();

    // Create a unique mode just for this synchronous resource load.
    String mode = loadResourceSynchronouslyMode;
    mode.append(String::number(runLoop.createUniqueId()));

    ContentSniff contentSniff = request.url().isLocalFile() ? SniffContent : DoNotSniffContent;
    RefPtr<WorkerThreadableLoader> loader = WorkerThreadableLoader::create(workerContext, &client, mode, request, DoNotSendLoadCallbacks, contentSniff, storedCredentials);

    MessageQueueWaitResult result = MessageQueueMessageReceived;
    while (!loader->done() && result != MessageQueueTerminated)
        result = runLoop.runInMode(workerContext, mode);

    if (!loader->done() && result == MessageQueueTerminated)
        loader->cancel();
}

void WorkerThreadableLoader::cancel()
{
    m_bridge.cancel();
}

WorkerThreadableLoader::MainThreadBridge::MainThreadBridge(PassRefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, WorkerMessagingProxy& messagingProxy, const String& taskMode,
                                                           const ResourceRequest& request, LoadCallbacks callbacksSetting, ContentSniff contentSniff, StoredCredentials storedCredentials)
    : m_workerClientWrapper(workerClientWrapper)
    , m_messagingProxy(messagingProxy)
    , m_taskMode(taskMode.copy())
{
    ASSERT(m_workerClientWrapper.get());
    m_messagingProxy.postTaskToWorkerObject(createCallbackTask(&MainThreadBridge::mainThreadCreateLoader, this, request, callbacksSetting, contentSniff, storedCredentials));
}

WorkerThreadableLoader::MainThreadBridge::~MainThreadBridge()
{
}

void WorkerThreadableLoader::MainThreadBridge::mainThreadCreateLoader(ScriptExecutionContext* context, MainThreadBridge* thisPtr, auto_ptr<CrossThreadResourceRequestData> requestData, LoadCallbacks callbacksSetting, ContentSniff contentSniff, StoredCredentials storedCredentials)
{
    // FIXME: This assert fails for nested workers.  Removing the assert would allow it to work,
    // but then there would be one WorkerThreadableLoader in every intermediate worker simply
    // chaining the requests, which is not very good. Instead, the postTaskToWorkerObject should be a
    // postTaskToDocumentContext.
    ASSERT(isMainThread());
    ASSERT(context->isDocument());

    if (thisPtr->m_messagingProxy.askedToTerminate())
        return;

    // FIXME: the created loader has no knowledge of the origin of the worker doing the load request.
    // Basically every setting done in SubresourceLoader::create (including the contents of addExtraFieldsToRequest)
    // needs to be examined for how it should take into account a different originator.
    OwnPtr<ResourceRequest> request(ResourceRequest::adopt(requestData));
    // FIXME: If the a site requests a local resource, then this will return a non-zero value but the sync path
    // will return a 0 value.  Either this should return 0 or the other code path should do a callback with
    // a failure.
    thisPtr->m_mainThreadLoader = ThreadableLoader::create(context, thisPtr, *request, callbacksSetting, contentSniff, storedCredentials);
    ASSERT(thisPtr->m_mainThreadLoader);
}

void WorkerThreadableLoader::MainThreadBridge::mainThreadDestroy(ScriptExecutionContext* context, MainThreadBridge* thisPtr)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());
    delete thisPtr;
}

void WorkerThreadableLoader::MainThreadBridge::destroy()
{
    // Ensure that no more client callbacks are done in the worker context's thread.
    clearClientWrapper();

    // "delete this" and m_mainThreadLoader::deref() on the worker object's thread.
    m_messagingProxy.postTaskToWorkerObject(createCallbackTask(&MainThreadBridge::mainThreadDestroy, this));
}

void WorkerThreadableLoader::MainThreadBridge::mainThreadCancel(ScriptExecutionContext* context, MainThreadBridge* thisPtr)
{
    ASSERT(isMainThread());
    ASSERT_UNUSED(context, context->isDocument());

    if (!thisPtr->m_mainThreadLoader)
        return;
    thisPtr->m_mainThreadLoader->cancel();
    thisPtr->m_mainThreadLoader = 0;
}

void WorkerThreadableLoader::MainThreadBridge::cancel()
{
    m_messagingProxy.postTaskToWorkerObject(createCallbackTask(&MainThreadBridge::mainThreadCancel, this));
    ThreadableLoaderClientWrapper* clientWrapper = static_cast<ThreadableLoaderClientWrapper*>(m_workerClientWrapper.get());
    if (!clientWrapper->done()) {
        // If the client hasn't reached a termination state, then transition it by sending a cancellation error.
        // Note: no more client callbacks will be done after this method -- the clearClientWrapper() call ensures that.
        ResourceError error(String(), 0, String(), String());
        error.setIsCancellation(true);
        clientWrapper->didFail(error);
    }
    clearClientWrapper();
}

void WorkerThreadableLoader::MainThreadBridge::clearClientWrapper()
{
    static_cast<ThreadableLoaderClientWrapper*>(m_workerClientWrapper.get())->clearClient();
}

static void workerContextDidSendData(ScriptExecutionContext* context, RefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT_UNUSED(context, context->isWorkerContext());
    workerClientWrapper->didSendData(bytesSent, totalBytesToBeSent);
}

void WorkerThreadableLoader::MainThreadBridge::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_messagingProxy.postTaskForModeToWorkerContext(createCallbackTask(&workerContextDidSendData, m_workerClientWrapper, bytesSent, totalBytesToBeSent), m_taskMode);
}

static void workerContextDidReceiveResponse(ScriptExecutionContext* context, RefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, auto_ptr<CrossThreadResourceResponseData> responseData)
{
    ASSERT_UNUSED(context, context->isWorkerContext());
    OwnPtr<ResourceResponse> response(ResourceResponse::adopt(responseData));
    workerClientWrapper->didReceiveResponse(*response);
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveResponse(const ResourceResponse& response)
{
    m_messagingProxy.postTaskForModeToWorkerContext(createCallbackTask(&workerContextDidReceiveResponse, m_workerClientWrapper, response), m_taskMode);
}

static void workerContextDidReceiveData(ScriptExecutionContext* context, RefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, auto_ptr<Vector<char> > vectorData)
{
    ASSERT_UNUSED(context, context->isWorkerContext());
    workerClientWrapper->didReceiveData(vectorData->data(), vectorData->size());
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveData(const char* data, int lengthReceived)
{
    auto_ptr<Vector<char> > vector(new Vector<char>(lengthReceived)); // needs to be an auto_ptr for usage with createCallbackTask.
    memcpy(vector->data(), data, lengthReceived);
    m_messagingProxy.postTaskForModeToWorkerContext(createCallbackTask(&workerContextDidReceiveData, m_workerClientWrapper, vector), m_taskMode);
}

static void workerContextDidFinishLoading(ScriptExecutionContext* context, RefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, unsigned long identifier)
{
    ASSERT_UNUSED(context, context->isWorkerContext());
    workerClientWrapper->didFinishLoading(identifier);
}

void WorkerThreadableLoader::MainThreadBridge::didFinishLoading(unsigned long identifier)
{
    m_messagingProxy.postTaskForModeToWorkerContext(createCallbackTask(&workerContextDidFinishLoading, m_workerClientWrapper, identifier), m_taskMode);
}

static void workerContextDidFail(ScriptExecutionContext* context, RefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, const ResourceError& error)
{
    ASSERT_UNUSED(context, context->isWorkerContext());
    workerClientWrapper->didFail(error);
}

void WorkerThreadableLoader::MainThreadBridge::didFail(const ResourceError& error)
{
    m_messagingProxy.postTaskForModeToWorkerContext(createCallbackTask(&workerContextDidFail, m_workerClientWrapper, error), m_taskMode);
}

static void workerContextDidFailRedirectCheck(ScriptExecutionContext* context, RefPtr<ThreadableLoaderClientWrapper> workerClientWrapper)
{
    ASSERT_UNUSED(context, context->isWorkerContext());
    workerClientWrapper->didFailRedirectCheck();
}

void WorkerThreadableLoader::MainThreadBridge::didFailRedirectCheck()
{
    m_messagingProxy.postTaskForModeToWorkerContext(createCallbackTask(&workerContextDidFailRedirectCheck, m_workerClientWrapper), m_taskMode);
}

static void workerContextDidReceiveAuthenticationCancellation(ScriptExecutionContext* context, RefPtr<ThreadableLoaderClientWrapper> workerClientWrapper, auto_ptr<CrossThreadResourceResponseData> responseData)
{
    ASSERT_UNUSED(context, context->isWorkerContext());
    OwnPtr<ResourceResponse> response(ResourceResponse::adopt(responseData));
    workerClientWrapper->didReceiveAuthenticationCancellation(*response);
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveAuthenticationCancellation(const ResourceResponse& response)
{
    m_messagingProxy.postTaskForModeToWorkerContext(createCallbackTask(&workerContextDidReceiveAuthenticationCancellation, m_workerClientWrapper, response), m_taskMode);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
