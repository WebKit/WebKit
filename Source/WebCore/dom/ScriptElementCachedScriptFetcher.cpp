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
#include "ScriptElementCachedScriptFetcher.h"

#include "Element.h"
#include "ScriptElement.h"

namespace WebCore {

const ASCIILiteral ScriptElementCachedScriptFetcher::defaultCrossOriginModeForModule { "anonymous"_s };

CachedResourceHandle<CachedScript> ScriptElementCachedScriptFetcher::requestModuleScript(Document& document, const URL& sourceURL, String&& integrity) const
{
    // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#cors-settings-attributes
    // If the fetcher is not module script, credential mode is always "same-origin" ("anonymous").
    // This code is for dynamic module import (`import` operator).

    return requestScriptWithCache(document, sourceURL, isClassicScript() ? defaultCrossOriginModeForModule : m_crossOriginMode, WTFMove(integrity), { });
}

}
