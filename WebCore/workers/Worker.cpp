/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"

#if ENABLE(WORKERS)

#include "Worker.h"

#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorController.h"
#include "MessageEvent.h"
#include "TextEncoding.h"
#include "WorkerContextProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>

namespace WebCore {

Worker::Worker(const String& url, ScriptExecutionContext* context, ExceptionCode& ec)
    : AbstractWorker(context)
    , m_contextProxy(WorkerContextProxy::create(this))
{
    KURL scriptURL = resolveURL(url, ec);
    if (ec)
        return;

    m_scriptLoader = new WorkerScriptLoader();
    m_scriptLoader->loadAsynchronously(scriptExecutionContext(), scriptURL, DenyCrossOriginRequests, this);
    setPendingActivity(this);  // The worker context does not exist while loading, so we must ensure that the worker object is not collected, as well as its event listeners.
#if ENABLE(INSPECTOR)
    if (InspectorController* inspector = scriptExecutionContext()->inspectorController())
        inspector->didCreateWorker(asID(), scriptURL.string(), false);
#endif
}

Worker::~Worker()
{
    ASSERT(isMainThread());
    ASSERT(scriptExecutionContext()); // The context is protected by worker context proxy, so it cannot be destroyed while a Worker exists.
    m_contextProxy->workerObjectDestroyed();
}

// FIXME: remove this when we update the ObjC bindings (bug #28774).
void Worker::postMessage(PassRefPtr<SerializedScriptValue> message, MessagePort* port, ExceptionCode& ec)
{
    MessagePortArray ports;
    if (port)
        ports.append(port);
    postMessage(message, &ports, ec);
}

void Worker::postMessage(PassRefPtr<SerializedScriptValue> message, ExceptionCode& ec)
{
    postMessage(message, static_cast<MessagePortArray*>(0), ec);
}

void Worker::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionCode& ec)
{
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, ec);
    if (ec)
        return;
    m_contextProxy->postMessageToWorkerContext(message, channels.release());
}

void Worker::terminate()
{
    m_contextProxy->terminateWorkerContext();
}

bool Worker::canSuspend() const
{
    // FIXME: It is not currently possible to suspend a worker, so pages with workers can not go into page cache.
    return false;
}

void Worker::stop()
{
    terminate();
}

bool Worker::hasPendingActivity() const
{
    return m_contextProxy->hasPendingActivity() || ActiveDOMObject::hasPendingActivity();
}

void Worker::notifyFinished()
{
    if (m_scriptLoader->failed())
        dispatchEvent(Event::create(eventNames().errorEvent, false, true));
    else
        m_contextProxy->startWorkerContext(m_scriptLoader->url(), scriptExecutionContext()->userAgent(m_scriptLoader->url()), m_scriptLoader->script());

    m_scriptLoader = 0;

    unsetPendingActivity(this);
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
