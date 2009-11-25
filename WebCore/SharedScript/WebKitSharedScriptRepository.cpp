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

#if ENABLE(SHARED_SCRIPT)

#include "WebKitSharedScriptRepository.h"

#include "ActiveDOMObject.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "SecurityOrigin.h"
#include "SharedScriptContext.h"
#include "WebKitSharedScript.h"
#include "WorkerScriptLoader.h"
#include "WorkerScriptLoaderClient.h"
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

// Helper class to load the initial script on behalf of a SharedScript.
class SharedScriptLoader : public RefCounted<SharedScriptLoader>, public ActiveDOMObject, private WorkerScriptLoaderClient {
public:
    static PassRefPtr<SharedScriptLoader> create(PassRefPtr<WebKitSharedScript> sharedScript, PassRefPtr<SharedScriptContext> innerContext)
    {
        return adoptRef(new SharedScriptLoader(sharedScript, innerContext));
    }

    void load(const KURL&);

private:
    SharedScriptLoader(PassRefPtr<WebKitSharedScript>, PassRefPtr<SharedScriptContext>);
    // WorkerScriptLoaderClient callback
    virtual void notifyFinished();

    RefPtr<WebKitSharedScript> m_sharedScript;
    RefPtr<SharedScriptContext> m_innerContext;
    OwnPtr<WorkerScriptLoader> m_scriptLoader;
};

SharedScriptLoader::SharedScriptLoader(PassRefPtr<WebKitSharedScript> sharedScript, PassRefPtr<SharedScriptContext> innerContext)
    : ActiveDOMObject(sharedScript->scriptExecutionContext(), this)
    , m_sharedScript(sharedScript)
    , m_innerContext(innerContext)
{
}

void SharedScriptLoader::load(const KURL& url)
{
    // Mark this object as active for the duration of the load.
    ASSERT(!hasPendingActivity());
    m_scriptLoader = new WorkerScriptLoader();
    m_scriptLoader->loadAsynchronously(scriptExecutionContext(), url, DenyCrossOriginRequests, this);

    // Stay alive until the load finishes.
    setPendingActivity(this);
    m_sharedScript->setPendingActivity(m_sharedScript.get());
}

void SharedScriptLoader::notifyFinished()
{
    if (m_scriptLoader->failed())
        m_sharedScript->dispatchEvent(Event::create(eventNames().errorEvent, false, true));
    else {
        // If another loader has not yet initialized the SharedScriptContext, do so.
        if (!m_innerContext->loaded())
            m_innerContext->load(scriptExecutionContext()->userAgent(m_scriptLoader->url()), m_scriptLoader->script());
    
        m_sharedScript->scheduleLoadEvent(m_innerContext);
    }

    m_sharedScript->unsetPendingActivity(m_sharedScript.get());
    unsetPendingActivity(this); // This frees this object so it must be the last action in this function.
}

WebKitSharedScriptRepository& WebKitSharedScriptRepository::instance()
{
    DEFINE_STATIC_LOCAL(WebKitSharedScriptRepository, instance, ());
    return instance;
}

void WebKitSharedScriptRepository::connect(PassRefPtr<WebKitSharedScript> sharedScript, const KURL& url, const String& name, ExceptionCode& ec)
{
    instance().connectToSharedScript(sharedScript, url, name, ec);
}

void WebKitSharedScriptRepository::documentDetached(Document* document)
{
    WebKitSharedScriptRepository& repository = instance();
    for (unsigned i = 0; i < repository.m_sharedScriptContexts.size(); i++)
        repository.m_sharedScriptContexts[i]->removeFromDocumentList(document);
}

void WebKitSharedScriptRepository::removeSharedScriptContext(SharedScriptContext* context)
{
    WebKitSharedScriptRepository& repository = instance();
    for (unsigned i = 0; i < repository.m_sharedScriptContexts.size(); i++) {
        if (context == repository.m_sharedScriptContexts[i]) {
            repository.m_sharedScriptContexts.remove(i);
            return;
        }
    }
}

void WebKitSharedScriptRepository::connectToSharedScript(PassRefPtr<WebKitSharedScript> sharedScript, const KURL& url, const String& name, ExceptionCode& ec)
{
    ASSERT(sharedScript->scriptExecutionContext()->securityOrigin()->canAccess(SecurityOrigin::create(url).get()));
    RefPtr<SharedScriptContext> innerContext = getSharedScriptContext(name, url);

    if (innerContext->url() != url) {
        // SharedScript with same name but different URL already exists - return an error.
        ec = URL_MISMATCH_ERR;
        return;
    }

    ASSERT(sharedScript->scriptExecutionContext()->isDocument());
    innerContext->addToDocumentsList(static_cast<Document*>(sharedScript->scriptExecutionContext()));

    // If SharedScriptContext is already running, just schedule a load event - otherwise, kick off a loader to load the script.
    if (innerContext->loaded())
        sharedScript->scheduleLoadEvent(innerContext);
    else {
        RefPtr<SharedScriptLoader> loader = SharedScriptLoader::create(sharedScript, innerContext.release());
        loader->load(url); // Pending activity will keep the loader alive.
    }
}

// Creates a new SharedScriptContext or returns an existing one from the repository.
PassRefPtr<SharedScriptContext> WebKitSharedScriptRepository::getSharedScriptContext(const String& name, const KURL& url)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
    for (unsigned i = 0; i < m_sharedScriptContexts.size(); i++) {
        if (m_sharedScriptContexts[i]->matches(name, *origin, url))
            return m_sharedScriptContexts[i];
    }

    RefPtr<SharedScriptContext> sharedScriptContext = SharedScriptContext::create(name, url, origin.release());
    m_sharedScriptContexts.append(sharedScriptContext.get());
    return sharedScriptContext.release();
}

} // namespace WebCore

#endif // ENABLE(SHARED_SCRIPT)

