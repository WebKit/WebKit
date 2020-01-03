/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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
#include "CachedScriptFetcher.h"

#include "CachedResourceLoader.h"
#include "CachedScript.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "Settings.h"

namespace WebCore {

Ref<CachedScriptFetcher> CachedScriptFetcher::create(const String& charset)
{
    return adoptRef(*new CachedScriptFetcher(charset));
}

CachedResourceHandle<CachedScript> CachedScriptFetcher::requestModuleScript(Document& document, const URL& sourceURL, String&& integrity) const
{
    return requestScriptWithCache(document, sourceURL, String { }, WTFMove(integrity));
}

CachedResourceHandle<CachedScript> CachedScriptFetcher::requestScriptWithCache(Document& document, const URL& sourceURL, const String& crossOriginMode, String&& integrity) const
{
    if (!document.settings().isScriptEnabled())
        return nullptr;

    ASSERT(document.contentSecurityPolicy());
    bool hasKnownNonce = document.contentSecurityPolicy()->allowScriptWithNonce(m_nonce, m_isInUserAgentShadowTree);
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = hasKnownNonce ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
    options.integrity = WTFMove(integrity);
    options.referrerPolicy = m_referrerPolicy;

    auto request = createPotentialAccessControlRequest(sourceURL, WTFMove(options), document, crossOriginMode);
    request.upgradeInsecureRequestIfNeeded(document);
    request.setCharset(m_charset);
    if (!m_initiatorName.isNull())
        request.setInitiator(m_initiatorName);
    return document.cachedResourceLoader().requestScript(WTFMove(request)).value_or(nullptr);
}

}
