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

#include "ModuleScriptLoader.h"
#include "ModuleScriptLoaderClient.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/URLHash.h>

namespace JSC {

class CallFrame;
class JSGlobalObject;
class JSInternalPromise;
class JSModuleLoader;
class JSModuleRecord;
class SourceOrigin;

}

namespace WebCore {

class JSDOMGlobalObject;
class ScriptExecutionContext;

class ScriptModuleLoader final : private ModuleScriptLoaderClient {
    WTF_MAKE_NONCOPYABLE(ScriptModuleLoader); WTF_MAKE_FAST_ALLOCATED;
public:
    enum class OwnerType : uint8_t { Document, WorkerOrWorklet };
    explicit ScriptModuleLoader(ScriptExecutionContext&, OwnerType);
    ~ScriptModuleLoader();

    ScriptExecutionContext& context() { return m_context; }

    JSC::Identifier resolve(JSC::JSGlobalObject*, JSC::JSModuleLoader*, JSC::JSValue moduleName, JSC::JSValue importerModuleKey, JSC::JSValue scriptFetcher);
    JSC::JSInternalPromise* fetch(JSC::JSGlobalObject*, JSC::JSModuleLoader*, JSC::JSValue moduleKey, JSC::JSValue parameters, JSC::JSValue scriptFetcher);
    JSC::JSValue evaluate(JSC::JSGlobalObject*, JSC::JSModuleLoader*, JSC::JSValue moduleKey, JSC::JSValue moduleRecord, JSC::JSValue scriptFetcher, JSC::JSValue awaitedValue, JSC::JSValue resumeMode);
    JSC::JSInternalPromise* importModule(JSC::JSGlobalObject*, JSC::JSModuleLoader*, JSC::JSString*, JSC::JSValue parameters, const JSC::SourceOrigin&);
    JSC::JSObject* createImportMetaProperties(JSC::JSGlobalObject*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSModuleRecord*, JSC::JSValue);

private:
    void notifyFinished(ModuleScriptLoader&, URL&&, Ref<DeferredPromise>) final;

    URL moduleURL(JSC::JSGlobalObject&, JSC::JSValue);
    URL responseURLFromRequestURL(JSC::JSGlobalObject&, JSC::JSValue);

    ScriptExecutionContext& m_context;
    HashMap<String, URL> m_requestURLToResponseURLMap;
    HashSet<Ref<ModuleScriptLoader>> m_loaders;
    OwnerType m_ownerType;
};

} // namespace WebCore
