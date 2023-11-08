/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "ModuleScriptLoader.h"
#include "ResourceLoaderIdentifier.h"
#include "ScriptBuffer.h"
#include "WorkerScriptFetcher.h"
#include "WorkerScriptLoaderClient.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>

namespace WebCore {

class DeferredPromise;
class JSDOMGlobalObject;
class ModuleFetchParameters;
class ScriptExecutionContext;
class ModuleScriptLoaderClient;
class WorkerScriptLoader;

class WorkerModuleScriptLoader final : public ModuleScriptLoader, private WorkerScriptLoaderClient {
public:
    static Ref<WorkerModuleScriptLoader> create(ModuleScriptLoaderClient&, DeferredPromise&, WorkerScriptFetcher&, RefPtr<JSC::ScriptFetchParameters>&&);

    virtual ~WorkerModuleScriptLoader();

    void load(ScriptExecutionContext&, URL&& sourceURL);

    WorkerScriptLoader& scriptLoader() { return m_scriptLoader.get(); }
    Ref<WorkerScriptLoader> protectedScriptLoader();

    static String taskMode();
    ReferrerPolicy referrerPolicy();
    bool failed() const { return m_failed; }
    bool retrievedFromServiceWorkerCache() const { return m_retrievedFromServiceWorkerCache; }

    const ScriptBuffer& script() { return m_script; }
    const URL& responseURL() const { return m_responseURL; }
    const String& responseMIMEType() const { return m_responseMIMEType; }

private:
    WorkerModuleScriptLoader(ModuleScriptLoaderClient&, DeferredPromise&, WorkerScriptFetcher&, RefPtr<JSC::ScriptFetchParameters>&&);

    void didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse&) final { }
    void notifyFinished() final;

    void notifyClientFinished();

    Ref<WorkerScriptLoader> m_scriptLoader;
    URL m_sourceURL;
    ScriptBuffer m_script;
    URL m_responseURL;
    String m_responseMIMEType;
    bool m_failed { false };
    bool m_retrievedFromServiceWorkerCache { false };
};

} // namespace WebCore
