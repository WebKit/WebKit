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

#include "CachedModuleScriptLoader.h"
#include "CachedModuleScriptLoaderClient.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/Noncopyable.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>

namespace JSC {

class ExecState;
class JSGlobalObject;
class JSInternalPromise;
class JSModuleLoader;
class SourceOrigin;

}

namespace WebCore {

class Document;
class JSDOMGlobalObject;

class ScriptModuleLoader final : private CachedModuleScriptLoaderClient {
    WTF_MAKE_NONCOPYABLE(ScriptModuleLoader); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ScriptModuleLoader(Document&);
    ~ScriptModuleLoader();

    Document& document() { return m_document; }

    JSC::Identifier resolve(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue moduleName, JSC::JSValue importerModuleKey, JSC::JSValue scriptFetcher);
    JSC::JSInternalPromise* fetch(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue moduleKey, JSC::JSValue parameters, JSC::JSValue scriptFetcher);
    JSC::JSValue evaluate(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue moduleKey, JSC::JSValue moduleRecord, JSC::JSValue scriptFetcher);
    JSC::JSInternalPromise* importModule(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSString*, JSC::JSValue parameters, const JSC::SourceOrigin&);
    JSC::JSObject* createImportMetaProperties(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSModuleRecord*, JSC::JSValue);

private:
    void notifyFinished(CachedModuleScriptLoader&, RefPtr<DeferredPromise>) final;
    URL moduleURL(JSC::ExecState&, JSC::JSValue);

    Document& m_document;
    HashMap<URL, URL> m_requestURLToResponseURLMap;
    HashSet<Ref<CachedModuleScriptLoader>> m_loaders;
};

} // namespace WebCore
