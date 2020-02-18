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
#include "ThreadableLoader.h"

#include "CachedResourceRequestInitiators.h"
#include "Document.h"
#include "DocumentThreadableLoader.h"
#include "ResourceError.h"
#include "ScriptExecutionContext.h"
#include "WorkerGlobalScope.h"
#include "WorkerRunLoop.h"
#include "WorkerThreadableLoader.h"

namespace WebCore {

ThreadableLoaderOptions::ThreadableLoaderOptions()
{
    mode = FetchOptions::Mode::SameOrigin;
}

ThreadableLoaderOptions::~ThreadableLoaderOptions() = default;

ThreadableLoaderOptions::ThreadableLoaderOptions(FetchOptions&& baseOptions)
    : ResourceLoaderOptions { WTFMove(baseOptions) }
{
}

ThreadableLoaderOptions::ThreadableLoaderOptions(const ResourceLoaderOptions& baseOptions, ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement, String&& initiator, ResponseFilteringPolicy filteringPolicy)
    : ResourceLoaderOptions(baseOptions)
    , contentSecurityPolicyEnforcement(contentSecurityPolicyEnforcement)
    , initiator(WTFMove(initiator))
    , filteringPolicy(filteringPolicy)
{
}

ThreadableLoaderOptions ThreadableLoaderOptions::isolatedCopy() const
{
    ThreadableLoaderOptions copy;

    // FetchOptions
    copy.destination = this->destination;
    copy.mode = this->mode;
    copy.credentials = this->credentials;
    copy.cache = this->cache;
    copy.redirect = this->redirect;
    copy.referrerPolicy = this->referrerPolicy;
    copy.integrity = this->integrity.isolatedCopy();

    // ResourceLoaderOptions
    copy.sendLoadCallbacks = this->sendLoadCallbacks;
    copy.sniffContent = this->sniffContent;
    copy.dataBufferingPolicy = this->dataBufferingPolicy;
    copy.storedCredentialsPolicy = this->storedCredentialsPolicy;
    copy.securityCheck = this->securityCheck;
    copy.certificateInfoPolicy = this->certificateInfoPolicy;
    copy.contentSecurityPolicyImposition = this->contentSecurityPolicyImposition;
    copy.defersLoadingPolicy = this->defersLoadingPolicy;
    copy.cachingPolicy = this->cachingPolicy;
    copy.sameOriginDataURLFlag = this->sameOriginDataURLFlag;
    copy.initiatorContext = this->initiatorContext;
    copy.clientCredentialPolicy = this->clientCredentialPolicy;
    copy.maxRedirectCount = this->maxRedirectCount;
    copy.preflightPolicy = this->preflightPolicy;

    // ThreadableLoaderOptions
    copy.contentSecurityPolicyEnforcement = this->contentSecurityPolicyEnforcement;
    copy.initiator = this->initiator.isolatedCopy();
    copy.filteringPolicy = this->filteringPolicy;

    return copy;
}


RefPtr<ThreadableLoader> ThreadableLoader::create(ScriptExecutionContext& context, ThreadableLoaderClient& client, ResourceRequest&& request, const ThreadableLoaderOptions& options, String&& referrer)
{
    if (is<WorkerGlobalScope>(context))
        return WorkerThreadableLoader::create(downcast<WorkerGlobalScope>(context), client, WorkerRunLoop::defaultMode(), WTFMove(request), options, WTFMove(referrer));

    return DocumentThreadableLoader::create(downcast<Document>(context), client, WTFMove(request), options, WTFMove(referrer));
}

void ThreadableLoader::loadResourceSynchronously(ScriptExecutionContext& context, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    auto resourceURL = request.url();
    if (is<WorkerGlobalScope>(context))
        WorkerThreadableLoader::loadResourceSynchronously(downcast<WorkerGlobalScope>(context), WTFMove(request), client, options);
    else
        DocumentThreadableLoader::loadResourceSynchronously(downcast<Document>(context), WTFMove(request), client, options);
    context.didLoadResourceSynchronously(resourceURL);
}

void ThreadableLoader::logError(ScriptExecutionContext& context, const ResourceError& error, const String& initiator)
{
    if (error.isCancellation())
        return;

    // FIXME: Some errors are returned with null URLs. This leads to poor console messages. We should do better for these errors.
    if (error.failingURL().isNull())
        return;

    // We further reduce logging to some errors.
    // FIXME: Log more errors when making so do not make some layout tests flaky.
    if (error.domain() != errorDomainWebKitInternal && error.domain() != errorDomainWebKitServiceWorker && !error.isAccessControl())
        return;

    const char* messageStart;
    if (initiator == cachedResourceRequestInitiators().eventsource)
        messageStart = "EventSource cannot load ";
    else if (initiator == cachedResourceRequestInitiators().fetch)
        messageStart = "Fetch API cannot load ";
    else if (initiator == cachedResourceRequestInitiators().xmlhttprequest)
        messageStart = "XMLHttpRequest cannot load ";
    else
        messageStart = "Cannot load ";

    String messageEnd = error.isAccessControl() ? " due to access control checks."_s : "."_s;
    context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString(messageStart, error.failingURL().string(), messageEnd));
}

} // namespace WebCore
