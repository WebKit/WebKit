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

#pragma once

#include <JavaScriptCore/ScriptFetcher.h>
#include <WebCore/CachedResourceHandle.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/RequestPriority.h>
#include <WebCore/ResourceLoadPriority.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedScript;
class Document;

enum class FetchOptionsDestination : uint8_t;

class CachedScriptFetcher : public JSC::ScriptFetcher {
public:
    virtual CachedResourceHandle<CachedScript> requestModuleScript(Document&, const URL& sourceURL, FetchOptionsDestination, String&& integrity, std::optional<ServiceWorkersMode>) const;

    static Ref<CachedScriptFetcher> create(const AtomString& charset);

protected:
    CachedScriptFetcher(const String& nonce, ReferrerPolicy referrerPolicy, RequestPriority fetchPriority, const AtomString& charset, const AtomString& initiatorType, bool isInUserAgentShadowTree)
        : m_nonce(nonce)
        , m_charset(charset)
        , m_initiatorType(initiatorType)
        , m_isInUserAgentShadowTree(isInUserAgentShadowTree)
        , m_referrerPolicy(referrerPolicy)
        , m_fetchPriority(fetchPriority)
    {
    }

    CachedScriptFetcher(const AtomString& charset)
        : m_charset(charset)
    {
    }

    CachedResourceHandle<CachedScript> requestScriptWithCache(Document&, const URL& sourceURL, FetchOptionsDestination, const String& crossOriginMode, String&& integrity, std::optional<ResourceLoadPriority>, std::optional<ServiceWorkersMode>) const;

private:
    String m_nonce;
    AtomString m_charset;
    AtomString m_initiatorType;
    bool m_isInUserAgentShadowTree { false };
    ReferrerPolicy m_referrerPolicy { ReferrerPolicy::EmptyString };
    RequestPriority m_fetchPriority { RequestPriority::Auto };
};

} // namespace WebCore
