/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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

#include "WorkerScriptLoader.h"

#include "GenericWorkerTask.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "WorkerContext.h"
#include "WorkerScriptLoaderClient.h"
#include "WorkerThreadableLoader.h"
#include <wtf/OwnPtr.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

WorkerScriptLoader::WorkerScriptLoader()
    : m_client(0)
    , m_failed(false)
    , m_identifier(0)
{
}

static CrossOriginRedirectPolicy toCrossOriginRedirectPolicy(CrossOriginLoadPolicy crossOriginLoadPolicy)
{
    return (crossOriginLoadPolicy == DenyCrossOriginLoad) ? DenyCrossOriginRedirect : AllowCrossOriginRedirect;
}

void WorkerScriptLoader::loadSynchronously(ScriptExecutionContext* scriptExecutionContext, const String& url, URLCompletionPolicy urlCompletionPolicy, CrossOriginLoadPolicy crossOriginLoadPolicy)
{
    OwnPtr<ResourceRequest> request(createResourceRequest(scriptExecutionContext, url, urlCompletionPolicy, crossOriginLoadPolicy));
    if (!request)
        return;

    ASSERT(scriptExecutionContext->isWorkerContext());
    WorkerThreadableLoader::loadResourceSynchronously(static_cast<WorkerContext*>(scriptExecutionContext), *request, *this, AllowStoredCredentials, toCrossOriginRedirectPolicy(crossOriginLoadPolicy));
}
    
void WorkerScriptLoader::loadAsynchronously(ScriptExecutionContext* scriptExecutionContext, const String& url, URLCompletionPolicy urlCompletionPolicy, CrossOriginLoadPolicy crossOriginLoadPolicy, WorkerScriptLoaderClient* client)
{
    ASSERT(client);
    m_client = client;

    OwnPtr<ResourceRequest> request(createResourceRequest(scriptExecutionContext, url, urlCompletionPolicy, crossOriginLoadPolicy));
    if (!request)
        return;

    m_threadableLoader = ThreadableLoader::create(scriptExecutionContext, this, *request, DoNotSendLoadCallbacks, DoNotSniffContent, AllowStoredCredentials, toCrossOriginRedirectPolicy(crossOriginLoadPolicy));
}

static void notifyLoadErrorTask(ScriptExecutionContext* context, WorkerScriptLoader* loader)
{
    UNUSED_PARAM(context);
    loader->notifyError();
}

PassOwnPtr<ResourceRequest> WorkerScriptLoader::createResourceRequest(ScriptExecutionContext* scriptExecutionContext, const String& url, URLCompletionPolicy urlCompletionPolicy, CrossOriginLoadPolicy crossOriginLoadPolicy)
{
    if (urlCompletionPolicy == CompleteURL) {
        m_url = scriptExecutionContext->completeURL(url);
        if (url.isEmpty() || !m_url.isValid()) {
            scriptExecutionContext->postTask(createCallbackTask(&notifyLoadErrorTask, this));
            return 0;
        }
    } else
        m_url = KURL(url);

    if (crossOriginLoadPolicy == DenyCrossOriginLoad && !scriptExecutionContext->securityOrigin()->canAccess(SecurityOrigin::create(m_url).get())) {
        scriptExecutionContext->postTask(createCallbackTask(&notifyLoadErrorTask, this));
        return 0;
    }

    OwnPtr<ResourceRequest> request(new ResourceRequest(m_url));
    request->setHTTPMethod("GET");

    return request.release();
}
    
void WorkerScriptLoader::didReceiveResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() / 100 != 2 && response.httpStatusCode()) {
        m_failed = true;
        return;
    }
    m_responseEncoding = response.textEncodingName();
}

void WorkerScriptLoader::didReceiveData(const char* data, int len)
{
    if (m_failed)
        return;

    if (!m_decoder) {
        if (!m_responseEncoding.isEmpty())
            m_decoder = TextResourceDecoder::create("text/javascript", m_responseEncoding);
        else
            m_decoder = TextResourceDecoder::create("text/javascript", "UTF-8");
    }

    if (!len)
        return;
    
    if (len == -1)
        len = strlen(data);
    
    m_script += m_decoder->decode(data, len);
}

void WorkerScriptLoader::didFinishLoading(unsigned long identifier)
{
    if (m_failed)
        return;

    if (m_decoder)
        m_script += m_decoder->flush();

    m_identifier = identifier;
    notifyFinished();
}

void WorkerScriptLoader::didFail(const ResourceError&)
{
    notifyError();
}

void WorkerScriptLoader::didFailRedirectCheck()
{
    notifyError();
}

void WorkerScriptLoader::didReceiveAuthenticationCancellation(const ResourceResponse&)
{
    notifyError();
}

void WorkerScriptLoader::notifyError()
{
    m_failed = true;
    notifyFinished();
}
    
void WorkerScriptLoader::notifyFinished()
{
    if (m_client)
        m_client->notifyFinished();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
