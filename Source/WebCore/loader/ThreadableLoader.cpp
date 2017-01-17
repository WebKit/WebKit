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

ThreadableLoaderOptions::~ThreadableLoaderOptions()
{
}

ThreadableLoaderOptions::ThreadableLoaderOptions(const ResourceLoaderOptions& baseOptions, PreflightPolicy preflightPolicy, ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement, String&& initiator, ResponseFilteringPolicy filteringPolicy)
    : ResourceLoaderOptions(baseOptions)
    , preflightPolicy(preflightPolicy)
    , contentSecurityPolicyEnforcement(contentSecurityPolicyEnforcement)
    , initiator(WTFMove(initiator))
    , filteringPolicy(filteringPolicy)
{
}

RefPtr<ThreadableLoader> ThreadableLoader::create(ScriptExecutionContext& context, ThreadableLoaderClient& client, ResourceRequest&& request, const ThreadableLoaderOptions& options, String&& referrer)
{
    if (is<WorkerGlobalScope>(context))
        return WorkerThreadableLoader::create(downcast<WorkerGlobalScope>(context), client, WorkerRunLoop::defaultMode(), WTFMove(request), options, referrer);

    return DocumentThreadableLoader::create(downcast<Document>(context), client, WTFMove(request), options, WTFMove(referrer));
}

void ThreadableLoader::loadResourceSynchronously(ScriptExecutionContext& context, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    if (is<WorkerGlobalScope>(context))
        WorkerThreadableLoader::loadResourceSynchronously(downcast<WorkerGlobalScope>(context), WTFMove(request), client, options);
    else
        DocumentThreadableLoader::loadResourceSynchronously(downcast<Document>(context), WTFMove(request), client, options);
    context.didLoadResourceSynchronously();
}

void ThreadableLoader::logError(ScriptExecutionContext& context, const ResourceError& error, const String& initiator)
{
    // FIXME: extend centralized logging to other clients than fetch, at least XHR and EventSource.
    if (initiator != cachedResourceRequestInitiators().fetch)
        return;

    if (error.isCancellation())
        return;

    // FIXME: Some errors are returned with null URLs. This leads to poor console messages. We should do better for these errors.
    if (error.failingURL().isNull())
        return;

    // We further reduce logging to some errors.
    // FIXME: Log more errors when making so do not make some layout tests flaky.
    if (error.domain() != errorDomainWebKitInternal && !error.isAccessControl())
        return;

    const char* messageStart;
    if (initiator == cachedResourceRequestInitiators().fetch)
        messageStart = "Fetch API cannot load ";
    else
        messageStart = "Cannot load ";

    const char* messageMiddle = ". ";
    String description = error.localizedDescription();
    if (description.isEmpty()) {
        // FIXME: We should probably define default description error message for all error types.
        if (error.isAccessControl())
            messageMiddle = ASCIILiteral(" due to access control checks.");
        else
            messageMiddle = ".";
    }

    context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString(messageStart, error.failingURL().string(), messageMiddle, description));
}

} // namespace WebCore
