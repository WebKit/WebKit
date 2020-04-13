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

#include "CachedScriptFetcher.h"

namespace WebCore {

class ScriptElementCachedScriptFetcher : public CachedScriptFetcher {
public:
    virtual CachedResourceHandle<CachedScript> requestModuleScript(Document&, const URL& sourceURL, String&& integrity) const;

    virtual bool isClassicScript() const = 0;
    virtual bool isModuleScript() const = 0;

    const String& crossOriginMode() const { return m_crossOriginMode; }

protected:
    ScriptElementCachedScriptFetcher(const String& nonce, ReferrerPolicy policy, const String& crossOriginMode, const String& charset, const AtomString& initiatorName, bool isInUserAgentShadowTree)
        : CachedScriptFetcher(nonce, policy, charset, initiatorName, isInUserAgentShadowTree)
        , m_crossOriginMode(crossOriginMode)
    {
    }

private:
    String m_crossOriginMode;
};

} // namespace WebCore
