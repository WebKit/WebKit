/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CachedModuleScriptLoader.h"

#include "CachedScript.h"
#include "CachedScriptFetcher.h"
#include "DOMWrapperWorld.h"
#include "Frame.h"
#include "JSDOMBinding.h"
#include "JSDOMPromiseDeferred.h"
#include "ModuleFetchParameters.h"
#include "ResourceLoaderOptions.h"
#include "ScriptController.h"
#include "ScriptModuleLoader.h"
#include "ScriptSourceCode.h"

namespace WebCore {

Ref<CachedModuleScriptLoader> CachedModuleScriptLoader::create(CachedModuleScriptLoaderClient& client, DeferredPromise& promise, CachedScriptFetcher& scriptFetcher, RefPtr<ModuleFetchParameters>&& parameters)
{
    return adoptRef(*new CachedModuleScriptLoader(client, promise, scriptFetcher, WTFMove(parameters)));
}

CachedModuleScriptLoader::CachedModuleScriptLoader(CachedModuleScriptLoaderClient& client, DeferredPromise& promise, CachedScriptFetcher& scriptFetcher, RefPtr<ModuleFetchParameters>&& parameters)
    : m_client(&client)
    , m_promise(&promise)
    , m_scriptFetcher(scriptFetcher)
    , m_parameters(WTFMove(parameters))
{
}

CachedModuleScriptLoader::~CachedModuleScriptLoader()
{
    if (m_cachedScript) {
        m_cachedScript->removeClient(*this);
        m_cachedScript = nullptr;
    }
}

bool CachedModuleScriptLoader::load(Document& document, const URL& sourceURL)
{
    ASSERT(!m_cachedScript);
    String integrity = m_parameters ? m_parameters->integrity() : String { };
    m_cachedScript = m_scriptFetcher->requestModuleScript(document, sourceURL, WTFMove(integrity));
    if (!m_cachedScript)
        return false;
    m_sourceURL = sourceURL;

    // If the content is already cached, this immediately calls notifyFinished.
    m_cachedScript->addClient(*this);
    return true;
}

void CachedModuleScriptLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    ASSERT_UNUSED(resource, &resource == m_cachedScript);
    ASSERT(m_cachedScript);
    ASSERT(m_promise);

    Ref<CachedModuleScriptLoader> protectedThis(*this);
    if (m_client)
        m_client->notifyFinished(*this, WTFMove(m_promise));

    // Remove the client after calling notifyFinished to keep the data buffer in
    // CachedResource alive while notifyFinished processes the resource.
    m_cachedScript->removeClient(*this);
    m_cachedScript = nullptr;
}

}
