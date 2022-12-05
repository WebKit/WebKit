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

#include "CachedResourceHandle.h"
#include "ReferrerPolicy.h"
#include "ResourceLoadPriority.h"
#include <JavaScriptCore/ScriptFetcher.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedScript;
class Document;

class CachedScriptFetcher : public JSC::ScriptFetcher {
public:
    virtual CachedResourceHandle<CachedScript> requestModuleScript(Document&, const URL& sourceURL, String&& integrity) const;

    static Ref<CachedScriptFetcher> create(const String& charset);

protected:
    CachedScriptFetcher(const String& nonce, ReferrerPolicy referrerPolicy, const String& charset, const AtomString& initiatorType, bool isInUserAgentShadowTree)
        : m_nonce(nonce)
        , m_charset(charset)
        , m_initiatorType(initiatorType)
        , m_isInUserAgentShadowTree(isInUserAgentShadowTree)
        , m_referrerPolicy(referrerPolicy)
    {
    }

    CachedScriptFetcher(const String& charset)
        : m_charset(charset)
    {
    }

    CachedResourceHandle<CachedScript> requestScriptWithCache(Document&, const URL& sourceURL, const String& crossOriginMode, String&& integrity, std::optional<ResourceLoadPriority>) const;

private:
    String m_nonce;
    String m_charset;
    AtomString m_initiatorType;
    bool m_isInUserAgentShadowTree { false };
    ReferrerPolicy m_referrerPolicy { ReferrerPolicy::EmptyString };
};

} // namespace WebCore
