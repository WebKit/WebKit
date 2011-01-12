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

#include "SharedWorkerRepository.h"

#include "Event.h"
#include "EventNames.h"
#include "InspectorInstrumentation.h"
#include "MessagePortChannel.h"
#include "PlatformMessagePortChannel.h"
#include "ScriptExecutionContext.h"
#include "SharedWorker.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebMessagePortChannel.h"
#include "WebSharedWorker.h"
#include "WebSharedWorkerRepository.h"
#include "WebString.h"
#include "WebURL.h"
#include "WorkerScriptLoader.h"
#include "WorkerScriptLoaderClient.h"

namespace WebCore {

class Document;
using WebKit::WebFrameImpl;
using WebKit::WebMessagePortChannel;
using WebKit::WebSharedWorker;
using WebKit::WebSharedWorkerRepository;

// Callback class that keeps the SharedWorker and WebSharedWorker objects alive while loads are potentially happening, and also translates load errors into error events on the worker.
class SharedWorkerScriptLoader : private WorkerScriptLoaderClient, private WebSharedWorker::ConnectListener {
public:
    SharedWorkerScriptLoader(PassRefPtr<SharedWorker> worker, const KURL& url, const String& name, PassOwnPtr<MessagePortChannel> port, PassOwnPtr<WebSharedWorker> webWorker)
        : m_worker(worker)
        , m_url(url)
        , m_name(name)
        , m_webWorker(webWorker)
        , m_port(port)
        , m_scriptLoader(ResourceRequestBase::TargetIsSharedWorker)
        , m_loading(false)
        , m_responseAppCacheID(0)
    {
    }

    ~SharedWorkerScriptLoader();
    void load();
    static void stopAllLoadersForContext(ScriptExecutionContext*);

private:
    // WorkerScriptLoaderClient callback
    virtual void didReceiveResponse(const ResourceResponse&);
    virtual void notifyFinished();

    virtual void connected();

    const ScriptExecutionContext* loadingContext() { return m_worker->scriptExecutionContext(); }

    void sendConnect();

    RefPtr<SharedWorker> m_worker;
    KURL m_url;
    String m_name;
    OwnPtr<WebSharedWorker> m_webWorker;
    OwnPtr<MessagePortChannel> m_port;
    WorkerScriptLoader m_scriptLoader;
    bool m_loading;
    long long m_responseAppCacheID;
};

static Vector<SharedWorkerScriptLoader*>& pendingLoaders()
{
    AtomicallyInitializedStatic(Vector<SharedWorkerScriptLoader*>&, loaders = *new Vector<SharedWorkerScriptLoader*>);
    return loaders;
}

void SharedWorkerScriptLoader::stopAllLoadersForContext(ScriptExecutionContext* context)
{
    // Walk our list of pending loaders and shutdown any that belong to this context.
    Vector<SharedWorkerScriptLoader*>& loaders = pendingLoaders();
    for (unsigned i = 0; i < loaders.size(); ) {
        SharedWorkerScriptLoader* loader = loaders[i];
        if (context == loader->loadingContext()) {
            loaders.remove(i);
            delete loader;
        } else
            i++;
    }
}

SharedWorkerScriptLoader::~SharedWorkerScriptLoader()
{
    if (m_loading)
        m_worker->unsetPendingActivity(m_worker.get());
}

void SharedWorkerScriptLoader::load()
{
    ASSERT(!m_loading);
    // If the shared worker is not yet running, load the script resource for it, otherwise just send it a connect event.
    if (m_webWorker->isStarted())
        sendConnect();
    else {
        m_scriptLoader.loadAsynchronously(m_worker->scriptExecutionContext(), m_url, DenyCrossOriginRequests, this);
        // Keep the worker + JS wrapper alive until the resource load is complete in case we need to dispatch an error event.
        m_worker->setPendingActivity(m_worker.get());
        m_loading = true;
    }
}

// Extracts a WebMessagePortChannel from a MessagePortChannel.
static WebMessagePortChannel* getWebPort(PassOwnPtr<MessagePortChannel> port)
{
    // Extract the WebMessagePortChannel to send to the worker.
    PlatformMessagePortChannel* platformChannel = port->channel();
    WebMessagePortChannel* webPort = platformChannel->webChannelRelease();
    webPort->setClient(0);
    return webPort;
}

void SharedWorkerScriptLoader::didReceiveResponse(const ResourceResponse& response)
{
    m_responseAppCacheID = response.appCacheID();
}

void SharedWorkerScriptLoader::notifyFinished()
{
    if (m_scriptLoader.failed()) {
        m_worker->dispatchEvent(Event::create(eventNames().errorEvent, false, true));
        delete this;
    } else {
        InspectorInstrumentation::scriptImported(m_worker->scriptExecutionContext(), m_scriptLoader.identifier(), m_scriptLoader.script());
        // Pass the script off to the worker, then send a connect event.
        m_webWorker->startWorkerContext(m_url, m_name, m_worker->scriptExecutionContext()->userAgent(m_url), m_scriptLoader.script(), m_responseAppCacheID);
        sendConnect();
    }
}

void SharedWorkerScriptLoader::sendConnect()
{
    // Send the connect event off, and linger until it is done sending.
    m_webWorker->connect(getWebPort(m_port.release()), this);
}

void SharedWorkerScriptLoader::connected()
{
    // Connect event has been sent, so free ourselves (this releases the SharedWorker so it can be freed as well if unreferenced).
    delete this;
}

bool SharedWorkerRepository::isAvailable()
{
    // Allow the WebKitClient to determine if SharedWorkers are available.
    return WebKit::webKitClient()->sharedWorkerRepository();
}

static WebSharedWorkerRepository::DocumentID getId(void* document)
{
    ASSERT(document);
    return reinterpret_cast<WebSharedWorkerRepository::DocumentID>(document);
}

void SharedWorkerRepository::connect(PassRefPtr<SharedWorker> worker, PassOwnPtr<MessagePortChannel> port, const KURL& url, const String& name, ExceptionCode& ec)
{
    // This should not be callable unless there's a SharedWorkerRepository for
    // this context (since isAvailable() should have returned null).
    ASSERT(WebKit::webKitClient()->sharedWorkerRepository());

    // No nested workers (for now) - connect() should only be called from document context.
    ASSERT(worker->scriptExecutionContext()->isDocument());
    Document* document = static_cast<Document*>(worker->scriptExecutionContext());
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
    OwnPtr<WebSharedWorker> webWorker;
    webWorker = webFrame->client()->createSharedWorker(webFrame, url, name, getId(document));

    if (!webWorker) {
        // Existing worker does not match this url, so return an error back to the caller.
        ec = URL_MISMATCH_ERR;
        return;
    }

    WebKit::webKitClient()->sharedWorkerRepository()->addSharedWorker(
        webWorker.get(), getId(document));

    // The loader object manages its own lifecycle (and the lifecycles of the two worker objects).
    // It will free itself once loading is completed.
    SharedWorkerScriptLoader* loader = new SharedWorkerScriptLoader(worker, url, name, port, webWorker.release());
    loader->load();
}

void SharedWorkerRepository::documentDetached(Document* document)
{
    WebSharedWorkerRepository* repo = WebKit::webKitClient()->sharedWorkerRepository();
    if (repo)
        repo->documentDetached(getId(document));

    // Stop the creation of any pending SharedWorkers for this context.
    // FIXME: Need a way to invoke this for WorkerContexts as well when we support for nested workers.
    SharedWorkerScriptLoader::stopAllLoadersForContext(document);
}

bool SharedWorkerRepository::hasSharedWorkers(Document* document)
{
    WebSharedWorkerRepository* repo = WebKit::webKitClient()->sharedWorkerRepository();
    return repo && repo->hasSharedWorkers(getId(document));
}



} // namespace WebCore

#endif // ENABLE(SHARED_WORKERS)
