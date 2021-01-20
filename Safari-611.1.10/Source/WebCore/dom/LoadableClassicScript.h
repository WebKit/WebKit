/*
 * Copyright (C) 2016 Apple, Inc. All Rights Reserved.
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

#pragma once

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "CachedScript.h"
#include "LoadableScript.h"
#include "ReferrerPolicy.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

// A CachedResourceHandle alone does not prevent the underlying CachedResource
// from purging its data buffer. This class holds a client until this class is
// destroyed in order to guarantee that the data buffer will not be purged.
class LoadableClassicScript final : public LoadableScript, private CachedResourceClient {
public:
    virtual ~LoadableClassicScript();

    static Ref<LoadableClassicScript> create(const String& nonce, const String& integrity, ReferrerPolicy, const String& crossOriginMode, const String& charset, const AtomString& initiatorName, bool isInUserAgentShadowTree, bool isAsync);
    bool isLoaded() const final;
    Optional<Error> error() const final;
    bool wasCanceled() const final;

    CachedScript& cachedScript() { return *m_cachedScript; }

    bool isClassicScript() const final { return true; }
    bool isModuleScript() const final { return false; }

    void execute(ScriptElement&) final;

    bool load(Document&, const URL&);

private:
    LoadableClassicScript(const String& nonce, const String& integrity, ReferrerPolicy policy, const String& crossOriginMode, const String& charset, const AtomString& initiatorName, bool isInUserAgentShadowTree, bool isAsync)
        : LoadableScript(nonce, policy, crossOriginMode, charset, initiatorName, isInUserAgentShadowTree)
        , m_integrity(integrity)
        , m_isAsync(isAsync)
    {
    }

    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;

    CachedResourceHandle<CachedScript> m_cachedScript { };
    Optional<Error> m_error { WTF::nullopt };
    String m_integrity;
    bool m_isAsync { false };
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LoadableClassicScript)
    static bool isType(const WebCore::LoadableScript& script) { return script.isClassicScript(); }
SPECIALIZE_TYPE_TRAITS_END()
