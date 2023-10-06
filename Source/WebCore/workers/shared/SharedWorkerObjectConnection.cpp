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
#include "SharedWorkerObjectConnection.h"

#include "ActiveDOMObject.h"
#include "ErrorEvent.h"
#include "EventNames.h"
#include "Logging.h"
#include "ScriptBuffer.h"
#include "SharedWorker.h"
#include "SharedWorkerScriptLoader.h"
#include "WorkerFetchResult.h"
#include "WorkerInitializationData.h"
#include "WorkerScriptLoader.h"

namespace WebCore {

#define CONNECTION_RELEASE_LOG(fmt, ...) RELEASE_LOG(SharedWorker, "%p - SharedWorkerObjectConnection::" fmt, this, ##__VA_ARGS__)
#define CONNECTION_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(SharedWorker, "%p - SharedWorkerObjectConnection::" fmt, this, ##__VA_ARGS__)

static uint64_t loaderIdentifierSeed = 0;

SharedWorkerObjectConnection::SharedWorkerObjectConnection() = default;

SharedWorkerObjectConnection::~SharedWorkerObjectConnection() = default;

void SharedWorkerObjectConnection::fetchScriptInClient(URL&& url, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, WorkerOptions&& workerOptions, CompletionHandler<void(WorkerFetchResult&&, WorkerInitializationData&&)>&& completionHandler)
{
    ASSERT(isMainThread());

    auto* workerObject = SharedWorker::fromIdentifier(sharedWorkerObjectIdentifier);
    CONNECTION_RELEASE_LOG("fetchScriptInClient: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING ", worker=%p", sharedWorkerObjectIdentifier.toString().utf8().data(), workerObject);
    if (!workerObject)
        return completionHandler(workerFetchError(ResourceError { ResourceError::Type::Cancellation }), { });

    auto loaderIdentifier = ++loaderIdentifierSeed;
    auto loader = makeUniqueRef<SharedWorkerScriptLoader>(WTFMove(url), *workerObject, WTFMove(workerOptions));
    auto loaderPtr = loader.ptr();
    m_loaders.add(loaderIdentifier, WTFMove(loader));

    loaderPtr->load([this, loaderIdentifier, completionHandler = WTFMove(completionHandler)](WorkerFetchResult&& fetchResult, WorkerInitializationData&& initializationData) mutable {
        CONNECTION_RELEASE_LOG("fetchScriptInClient: finished script load, success=%d", fetchResult.error.isNull());
        auto loader = m_loaders.take(loaderIdentifier);
        ASSERT(loader);
        completionHandler(WTFMove(fetchResult), WTFMove(initializationData));
    });
}

void SharedWorkerObjectConnection::notifyWorkerObjectOfLoadCompletion(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, const ResourceError& error)
{
    ASSERT(isMainThread());
    auto* workerObject = SharedWorker::fromIdentifier(sharedWorkerObjectIdentifier);
    if (error.isNull())
        CONNECTION_RELEASE_LOG("notifyWorkerObjectOfLoadCompletion: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING ", worker=%p, load succeeded", sharedWorkerObjectIdentifier.toString().utf8().data(), workerObject);
    else
        CONNECTION_RELEASE_LOG_ERROR("notifyWorkerObjectOfLoadCompletion: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING ", worker=%p, load failed", sharedWorkerObjectIdentifier.toString().utf8().data(), workerObject);
    if (workerObject)
        workerObject->didFinishLoading(error);
}

void SharedWorkerObjectConnection::postErrorToWorkerObject(SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, bool isErrorEvent)
{
    ASSERT(isMainThread());
    auto* workerObject = SharedWorker::fromIdentifier(sharedWorkerObjectIdentifier);
    CONNECTION_RELEASE_LOG_ERROR("postErrorToWorkerObject: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING ", worker=%p", sharedWorkerObjectIdentifier.toString().utf8().data(), workerObject);
    if (!workerObject)
        return;

    auto event = isErrorEvent ? Ref<Event> { ErrorEvent::create(errorMessage, sourceURL, lineNumber, columnNumber, { }) } : Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No);
    ActiveDOMObject::queueTaskToDispatchEvent(*workerObject, TaskSource::DOMManipulation, WTFMove(event));
}

#undef CONNECTION_RELEASE_LOG
#undef CONNECTION_RELEASE_LOG_ERROR

} // namespace WebCore
