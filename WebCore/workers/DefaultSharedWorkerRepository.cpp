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

#if ENABLE(SHARED_WORKERS)

#include "DefaultSharedWorkerRepository.h"

#include "ActiveDOMObject.h"
#include "MessagePort.h"
#include "NotImplemented.h"
#include "SharedWorker.h"
#include "SharedWorkerContext.h"
#include "SharedWorkerThread.h"
#include "WorkerLoaderProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerScriptLoaderClient.h"

namespace WebCore {

class SharedWorkerProxy : public WorkerLoaderProxy {
public:
    void setThread(PassRefPtr<SharedWorkerThread> thread) { m_thread = thread; }

    // WorkerLoaderProxy
    // FIXME: Implement WorkerLoaderProxy APIs by proxying to an active document.
    virtual void postTaskToLoader(PassRefPtr<ScriptExecutionContext::Task>) { notImplemented(); }
    virtual void postTaskForModeToWorkerContext(PassRefPtr<ScriptExecutionContext::Task>, const String&) { notImplemented(); }
private:
    RefPtr<SharedWorkerThread> m_thread;
};

class SharedWorkerConnectTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<SharedWorkerConnectTask> create(PassOwnPtr<MessagePortChannel> channel)
    {
        return adoptRef(new SharedWorkerConnectTask(channel));
    }

private:
    SharedWorkerConnectTask(PassOwnPtr<MessagePortChannel> channel)
        : m_channel(channel)
    {
    }

    virtual void performTask(ScriptExecutionContext* scriptContext)
    {
        RefPtr<MessagePort> port = MessagePort::create(*scriptContext);
        port->entangle(m_channel.release());
        ASSERT(scriptContext->isWorkerContext());
        WorkerContext* workerContext = static_cast<WorkerContext*>(scriptContext);
        ASSERT(workerContext->isSharedWorkerContext());
        workerContext->toSharedWorkerContext()->dispatchConnect(port);
    }

    OwnPtr<MessagePortChannel> m_channel;
};

class SharedWorkerLoader : public RefCounted<SharedWorkerLoader>, public ActiveDOMObject, private WorkerScriptLoaderClient {
public:
    SharedWorkerLoader(PassRefPtr<SharedWorker>, PassOwnPtr<MessagePortChannel>, const String&);
    void load(const KURL&);

    // WorkerScriptLoaderClient callback
    virtual void notifyFinished();

private:
    RefPtr<SharedWorker> m_worker;
    OwnPtr<MessagePortChannel> m_port;
    String m_name;
    OwnPtr<WorkerScriptLoader> m_scriptLoader;
};

SharedWorkerLoader::SharedWorkerLoader(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const String& name)
    : ActiveDOMObject(worker->scriptExecutionContext(), this)
    , m_worker(worker)
    , m_port(port)
    , m_name(name)
{
}

void SharedWorkerLoader::load(const KURL& url)
{
    // Mark this object as active for the duration of the load.
    ASSERT(!hasPendingActivity());
    m_scriptLoader = new WorkerScriptLoader();
    m_scriptLoader->loadAsynchronously(scriptExecutionContext(), url, DenyCrossOriginRedirect, this);

    // Stay alive until the load finishes.
    setPendingActivity(this);
}

void SharedWorkerLoader::notifyFinished()
{
    // Hand off the just-loaded code to the repository to start up the worker thread.
    if (m_scriptLoader->failed())
        m_worker->dispatchLoadErrorEvent();
    else
        DefaultSharedWorkerRepository::instance()->workerScriptLoaded(m_name, m_scriptLoader->url(), scriptExecutionContext()->userAgent(m_scriptLoader->url()), m_scriptLoader->script(), m_port.release());

    // This frees this object - must be the last action in this function.
    unsetPendingActivity(this);
}

SharedWorkerRepository* SharedWorkerRepository::instance()
{
    return DefaultSharedWorkerRepository::instance();
}

DefaultSharedWorkerRepository* DefaultSharedWorkerRepository::instance()
{
    AtomicallyInitializedStatic(DefaultSharedWorkerRepository*, instance = new DefaultSharedWorkerRepository);
    return instance;
}

void DefaultSharedWorkerRepository::workerScriptLoaded(const String& name, const KURL& url, const String& userAgent, const String& workerScript, PassOwnPtr<MessagePortChannel> port)
{
    // FIXME: Cache the proxy to allow sharing and to allow cleanup after the thread exits.
    SharedWorkerProxy* proxy = new SharedWorkerProxy();
    RefPtr<SharedWorkerThread> thread = SharedWorkerThread::create(name, url, userAgent, workerScript, *proxy);
    proxy->setThread(thread);
    thread->start();
    thread->runLoop().postTask(SharedWorkerConnectTask::create(port));
}

void SharedWorkerRepository::connect(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const KURL& url, const String& name, ExceptionCode&)
{
    // Create a loader to load/initialize the requested worker.
    RefPtr<SharedWorkerLoader> loader = adoptRef(new SharedWorkerLoader(worker, port, name));
    loader->load(url);
}

DefaultSharedWorkerRepository::DefaultSharedWorkerRepository()
{
}

DefaultSharedWorkerRepository::~DefaultSharedWorkerRepository()
{
}

} // namespace WebCore

#endif // ENABLE(SHARED_WORKERS)
