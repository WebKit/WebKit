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
#include "WebWorkerImpl.h"

#include "DedicatedWorkerContext.h"
#include "DedicatedWorkerThread.h"
#include "GenericWorkerTask.h"
#include "KURL.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "MessagePortChannel.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "SubstituteData.h"
#include <wtf/Threading.h>

#include "PlatformMessagePortChannel.h"
#include "WebMessagePortChannel.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebWorkerClient.h"

using namespace WebCore;

namespace WebKit {

#if ENABLE(WORKERS)

WebWorker* WebWorker::create(WebWorkerClient* client)
{
    return new WebWorkerImpl(client);
}


WebWorkerImpl::WebWorkerImpl(WebWorkerClient* client)
    : m_client(client)
{
}

WebWorkerImpl::~WebWorkerImpl()
{
}

WebCommonWorkerClient* WebWorkerImpl::commonClient()
{
    return m_client;
}

void WebWorkerImpl::postMessageToWorkerContextTask(WebCore::ScriptExecutionContext* context,
                                                   WebWorkerImpl* thisPtr,
                                                   const String& message,
                                                   PassOwnPtr<MessagePortChannelArray> channels)
{
    ASSERT(context->isWorkerContext());
    DedicatedWorkerContext* workerContext =
        static_cast<DedicatedWorkerContext*>(context);

    OwnPtr<MessagePortArray> ports =
        MessagePort::entanglePorts(*context, channels.release());
    RefPtr<SerializedScriptValue> serializedMessage =
        SerializedScriptValue::createFromWire(message);
    workerContext->dispatchEvent(MessageEvent::create(
        ports.release(), serializedMessage.release()));
    thisPtr->confirmMessageFromWorkerObject(workerContext->hasPendingActivity());
}

// WebWorker -------------------------------------------------------------------

void WebWorkerImpl::startWorkerContext(const WebURL& scriptUrl,
                                       const WebString& userAgent,
                                       const WebString& sourceCode)
{
    initializeLoader(scriptUrl);
    setWorkerThread(DedicatedWorkerThread::create(scriptUrl, userAgent,
                                                  sourceCode, *this, *this));
    // Worker initialization means a pending activity.
    reportPendingActivity(true);
    workerThread()->start();
}

void WebWorkerImpl::terminateWorkerContext()
{
    stopWorkerThread();
}

void WebWorkerImpl::postMessageToWorkerContext(const WebString& message,
                                               const WebMessagePortChannelArray& webChannels)
{
    OwnPtr<MessagePortChannelArray> channels;
    if (webChannels.size()) {
        channels = new MessagePortChannelArray(webChannels.size());
        for (size_t i = 0; i < webChannels.size(); ++i) {
            RefPtr<PlatformMessagePortChannel> platform_channel =
                PlatformMessagePortChannel::create(webChannels[i]);
            webChannels[i]->setClient(platform_channel.get());
            (*channels)[i] = MessagePortChannel::create(platform_channel);
        }
    }

    workerThread()->runLoop().postTask(
        createCallbackTask(&postMessageToWorkerContextTask,
                           this, String(message), channels.release()));
}

void WebWorkerImpl::workerObjectDestroyed()
{
    // Worker object in the renderer was destroyed, perhaps a result of GC.
    // For us, it's a signal to start terminating the WorkerContext too.
    // FIXME: when 'kill a worker' html5 spec algorithm is implemented, it
    // should be used here instead of 'terminate a worker'.
    terminateWorkerContext();
}

void WebWorkerImpl::clientDestroyed()
{
    m_client = 0;
}

#else

WebWorker* WebWorker::create(WebWorkerClient* client)
{
    return 0;
}

#endif // ENABLE(WORKERS)

} // namespace WebKit
