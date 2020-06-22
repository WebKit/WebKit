/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLResourcePreloader.h"

#include "CachedResourceLoader.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "ScriptElementCachedScriptFetcher.h"

#include "MediaQueryEvaluator.h"
#include "RenderView.h"

namespace WebCore {

URL PreloadRequest::completeURL(Document& document)
{
    return document.completeURL(m_resourceURL, m_baseURL.isEmpty() ? document.baseURL() : m_baseURL);
}

CachedResourceRequest PreloadRequest::resourceRequest(Document& document)
{
    ASSERT(isMainThread());

    bool skipContentSecurityPolicyCheck = false;
    if (m_resourceType == CachedResource::Type::Script)
        skipContentSecurityPolicyCheck = document.contentSecurityPolicy()->allowScriptWithNonce(m_nonceAttribute);
    else if (m_resourceType == CachedResource::Type::CSSStyleSheet)
        skipContentSecurityPolicyCheck = document.contentSecurityPolicy()->allowStyleWithNonce(m_nonceAttribute);

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    if (skipContentSecurityPolicyCheck)
        options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;

    String crossOriginMode = m_crossOriginMode;
    if (m_moduleScript == ModuleScript::Yes) {
        if (crossOriginMode.isNull())
            crossOriginMode = ScriptElementCachedScriptFetcher::defaultCrossOriginModeForModule;
    }
    if (m_resourceType == CachedResource::Type::Script || m_resourceType == CachedResource::Type::ImageResource)
        options.referrerPolicy = m_referrerPolicy;
    auto request = createPotentialAccessControlRequest(completeURL(document), WTFMove(options), document, crossOriginMode);
    request.setInitiator(m_initiator);

    // FIXME: Put priorities for various cases to some central place where they are easy to see.
    if (m_scriptIsAsync && m_resourceType == CachedResource::Type::Script && m_moduleScript == ModuleScript::No)
        request.setPriority(ResourceLoadPriority::Low);

    return request;
}

void HTMLResourcePreloader::preload(PreloadRequestStream requests)
{
    for (auto& request : requests)
        preload(WTFMove(request));
}

void HTMLResourcePreloader::preload(std::unique_ptr<PreloadRequest> preload)
{
    ASSERT(m_document.frame());
    ASSERT(m_document.renderView());
    if (!preload->media().isEmpty() && !MediaQueryEvaluator::mediaAttributeMatches(m_document, preload->media()))
        return;

    m_document.cachedResourceLoader().preload(preload->resourceType(), preload->resourceRequest(m_document));
}


}
