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

#include "DedicatedWorkerContext.h"

#include "DedicatedWorkerThread.h"
#include "DOMWindow.h"
#include "MessageEvent.h"
#include "WorkerObjectProxy.h"

namespace WebCore {

DedicatedWorkerContext::DedicatedWorkerContext(const KURL& url, const String& userAgent, DedicatedWorkerThread* thread)
    : WorkerContext(url, userAgent, thread)
{
}

DedicatedWorkerContext::~DedicatedWorkerContext()
{
    ASSERT(currentThread() == thread()->threadID());
    // Notify parent worker we are going away. This can free the WorkerThread object, so do not access it after this.
    thread()->workerObjectProxy().workerContextDestroyed();
}

void DedicatedWorkerContext::forwardException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    thread()->workerObjectProxy().postExceptionToWorkerObject(errorMessage, lineNumber, sourceURL);
}

void DedicatedWorkerContext::addMessage(MessageDestination destination, MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    thread()->workerObjectProxy().postConsoleMessageToWorkerObject(destination, source, type, level, message, lineNumber, sourceURL);
}

void DedicatedWorkerContext::postMessage(const String& message, ExceptionCode& ec)
{
    postMessage(message, 0, ec);
}

void DedicatedWorkerContext::postMessage(const String& message, MessagePort* port, ExceptionCode& ec)
{
    if (isClosing())
        return;
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannel> channel = port ? port->disentangle(ec) : 0;
    if (ec)
        return;
    thread()->workerObjectProxy().postMessageToWorkerObject(message, channel.release());
}

void DedicatedWorkerContext::dispatchMessage(const String& message, PassRefPtr<MessagePort> port)
{
    // Since close() stops the thread event loop, this should not ever get called while closing.
    ASSERT(!isClosing());
    RefPtr<Event> evt = MessageEvent::create(message, "", "", 0, port);

    if (m_onmessageListener.get()) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onmessageListener->handleEvent(evt.get(), false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void DedicatedWorkerContext::importScripts(const Vector<String>& urls, const String& callerURL, int callerLine, ExceptionCode& ec)
{
    Base::importScripts(urls, callerURL, callerLine, ec);
    thread()->workerObjectProxy().reportPendingActivity(hasPendingActivity());
}

DedicatedWorkerThread* DedicatedWorkerContext::thread()
{
    return static_cast<DedicatedWorkerThread*>(Base::thread());
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
