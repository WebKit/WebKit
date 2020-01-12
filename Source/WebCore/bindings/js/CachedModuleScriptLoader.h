/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>

namespace WebCore {

class CachedModuleScriptLoaderClient;
class CachedScript;
class CachedScriptFetcher;
class DeferredPromise;
class Document;
class JSDOMGlobalObject;
class ModuleFetchParameters;

class CachedModuleScriptLoader final : public RefCounted<CachedModuleScriptLoader>, private CachedResourceClient {
public:
    static Ref<CachedModuleScriptLoader> create(CachedModuleScriptLoaderClient&, DeferredPromise&, CachedScriptFetcher&, RefPtr<ModuleFetchParameters>&&);

    virtual ~CachedModuleScriptLoader();

    bool load(Document&, const URL& sourceURL);

    CachedScriptFetcher& scriptFetcher() { return m_scriptFetcher.get(); }
    CachedScript* cachedScript() { return m_cachedScript.get(); }
    ModuleFetchParameters* parameters() { return m_parameters.get(); }
    const URL& sourceURL() const { return m_sourceURL; }

    void clearClient()
    {
        ASSERT(m_client);
        m_client = nullptr;
    }

private:
    CachedModuleScriptLoader(CachedModuleScriptLoaderClient&, DeferredPromise&, CachedScriptFetcher&, RefPtr<ModuleFetchParameters>&&);

    void notifyFinished(CachedResource&) final;

    CachedModuleScriptLoaderClient* m_client { nullptr };
    RefPtr<DeferredPromise> m_promise;
    Ref<CachedScriptFetcher> m_scriptFetcher;
    RefPtr<ModuleFetchParameters> m_parameters;
    CachedResourceHandle<CachedScript> m_cachedScript;
    URL m_sourceURL;
};

} // namespace WebCore
