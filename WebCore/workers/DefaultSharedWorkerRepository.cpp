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
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "SharedWorker.h"
#include "SharedWorkerContext.h"
#include "SharedWorkerThread.h"
#include "WorkerLoaderProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerScriptLoaderClient.h"

#include <wtf/Threading.h>

namespace WebCore {

class SharedWorkerProxy : public ThreadSafeShared<SharedWorkerProxy>, public WorkerLoaderProxy {
public:
    static PassRefPtr<SharedWorkerProxy> create(const String& name, const KURL& url) { return adoptRef(new SharedWorkerProxy(name, url)); }

    void setThread(PassRefPtr<SharedWorkerThread> thread) { m_thread = thread; }
    SharedWorkerThread* thread() { return m_thread.get(); }
    bool closing() const { return m_closing; }
    KURL url() const { return m_url.copy(); }
    String name() const { return m_name.copy(); }

    // WorkerLoaderProxy
    // FIXME: Implement WorkerLoaderProxy APIs by proxying to an active document.
    virtual void postTaskToLoader(PassRefPtr<ScriptExecutionContext::Task>) { notImplemented(); }
    virtual void postTaskForModeToWorkerContext(PassRefPtr<ScriptExecutionContext::Task>, const String&) { notImplemented(); }

    // Updates the list of the worker's documents, per section 4.5 of the WebWorkers spec.
    void addToWorkerDocuments(ScriptExecutionContext*);
private:
    SharedWorkerProxy(const String& name, const KURL&);
    bool m_closing;
    String m_name;
    KURL m_url;
    RefPtr<SharedWorkerThread> m_thread;
};

SharedWorkerProxy::SharedWorkerProxy(const String& name, const KURL& url)
    : m_closing(false)
    , m_name(name.copy())
    , m_url(url.copy())
{
}

void SharedWorkerProxy::addToWorkerDocuments(ScriptExecutionContext* context)
{
    // Nested workers are not yet supported, so passed-in context should always be a Document.
    ASSERT(context->isDocument());
    // FIXME: track referring documents so we can shutdown the thread when the last one exits and remove the proxy from the cache.
}

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

// Loads the script on behalf of a worker.
class SharedWorkerScriptLoader : public RefCounted<SharedWorkerScriptLoader>, public ActiveDOMObject, private WorkerScriptLoaderClient {
public:
    SharedWorkerScriptLoader(PassRefPtr<SharedWorker>, PassOwnPtr<MessagePortChannel>, PassRefPtr<SharedWorkerProxy>);
    void load(const KURL&);

private:
    // WorkerScriptLoaderClient callback
    virtual void notifyFinished();

    RefPtr<SharedWorker> m_worker;
    OwnPtr<MessagePortChannel> m_port;
    RefPtr<SharedWorkerProxy> m_proxy;
    OwnPtr<WorkerScriptLoader> m_scriptLoader;
};

SharedWorkerScriptLoader::SharedWorkerScriptLoader(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, PassRefPtr<SharedWorkerProxy> proxy)
    : ActiveDOMObject(worker->scriptExecutionContext(), this)
    , m_worker(worker)
    , m_port(port)
    , m_proxy(proxy)
{
}

void SharedWorkerScriptLoader::load(const KURL& url)
{
    // Mark this object as active for the duration of the load.
    ASSERT(!hasPendingActivity());
    m_scriptLoader = new WorkerScriptLoader();
    m_scriptLoader->loadAsynchronously(scriptExecutionContext(), url, DenyCrossOriginRedirect, this);

    // Stay alive until the load finishes.
    setPendingActivity(this);
}

void SharedWorkerScriptLoader::notifyFinished()
{
    // Hand off the just-loaded code to the repository to start up the worker thread.
    if (m_scriptLoader->failed())
        m_worker->dispatchLoadErrorEvent();
    else
        DefaultSharedWorkerRepository::instance().workerScriptLoaded(*m_proxy, scriptExecutionContext()->userAgent(m_scriptLoader->url()), m_scriptLoader->script(), m_port.release());

    // This frees this object - must be the last action in this function.
    unsetPendingActivity(this);
}

DefaultSharedWorkerRepository& DefaultSharedWorkerRepository::instance()
{
    AtomicallyInitializedStatic(DefaultSharedWorkerRepository*, instance = new DefaultSharedWorkerRepository);
    return *instance;
}

void DefaultSharedWorkerRepository::workerScriptLoaded(SharedWorkerProxy& proxy, const String& userAgent, const String& workerScript, PassOwnPtr<MessagePortChannel> port)
{
    MutexLocker lock(m_lock);
    if (proxy.closing())
        return;

    // Another loader may have already started up a thread for this proxy - if so, just send a connect to the pre-existing thread.
    if (!proxy.thread()) {
        RefPtr<SharedWorkerThread> thread = SharedWorkerThread::create(proxy.name(), proxy.url(), userAgent, workerScript, proxy);
        proxy.setThread(thread);
        thread->start();
    }
    proxy.thread()->runLoop().postTask(SharedWorkerConnectTask::create(port));
}

void SharedWorkerRepository::connect(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const KURL& url, const String& name, ExceptionCode& ec)
{
    DefaultSharedWorkerRepository::instance().connectToWorker(worker, port, url, name, ec);
}

void DefaultSharedWorkerRepository::connectToWorker(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const KURL& url, const String& name, ExceptionCode& ec)
{
    MutexLocker lock(m_lock);
    ASSERT(worker->scriptExecutionContext()->securityOrigin()->canAccess(SecurityOrigin::create(url).get()));
    // Fetch a proxy corresponding to this SharedWorker.
    RefPtr<SharedWorkerProxy> proxy = getProxy(name, url);
    proxy->addToWorkerDocuments(worker->scriptExecutionContext());
    if (proxy->url() != url) {
        // Proxy already existed under alternate URL - return an error.
        ec = URL_MISMATCH_ERR;
        return;
    }
    // If proxy is already running, just connect to it - otherwise, kick off a loader to load the script.
    if (proxy->thread())
        proxy->thread()->runLoop().postTask(SharedWorkerConnectTask::create(port));
    else {
        RefPtr<SharedWorkerScriptLoader> loader = adoptRef(new SharedWorkerScriptLoader(worker, port.release(), proxy.release()));
        loader->load(url);
    }
}

// Creates a new SharedWorkerProxy or returns an existing one from the repository. Must only be called while the repository mutex is held.
PassRefPtr<SharedWorkerProxy> DefaultSharedWorkerRepository::getProxy(const String& name, const KURL& url)
{
    // Look for an existing worker, and create one if it doesn't exist.
    // Items in the cache are freed on another thread, so copy the URL before creating the origin, to make sure no references to external strings linger.
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url.copy());
    SharedWorkerNameMap* nameMap = m_cache.get(origin);
    if (!nameMap) {
        nameMap = new SharedWorkerNameMap();
        m_cache.set(origin, nameMap);
    }

    RefPtr<SharedWorkerProxy> proxy = nameMap->get(name);
    if (!proxy.get()) {
        proxy = SharedWorkerProxy::create(name, url);
        nameMap->set(proxy->name(), proxy);
    }
    return proxy;
}

DefaultSharedWorkerRepository::DefaultSharedWorkerRepository()
{
}

DefaultSharedWorkerRepository::~DefaultSharedWorkerRepository()
{
}

} // namespace WebCore

#endif // ENABLE(SHARED_WORKERS)
