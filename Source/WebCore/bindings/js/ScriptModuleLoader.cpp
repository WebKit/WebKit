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

#include "config.h"
#include "ScriptModuleLoader.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "JSDOMBinding.h"
#include <runtime/JSInternalPromiseDeferred.h>
#include <runtime/JSModuleRecord.h>
#include <runtime/JSString.h>
#include <runtime/Symbol.h>

namespace WebCore {

ScriptModuleLoader::ScriptModuleLoader(Document& document)
    : m_document(document)
{
}

JSC::JSInternalPromise* ScriptModuleLoader::resolve(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSValue moduleNameValue, JSC::JSValue importerModuleKey, JSC::JSValue)
{
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::create(exec, globalObject);

    // We use a Symbol as a special purpose; It means this module is an inline module.
    // So there is no correct URL to retrieve the module source code. If the module name
    // value is a Symbol, it is used directly as a module key.
    //
    // FIXME: Using symbols for an inline module is a current implementation details of WebKit.
    // Once the spec of this part is specified, we will recast these part.
    if (moduleNameValue.isSymbol())
        return deferred->resolve(exec, moduleNameValue);

    if (!moduleNameValue.isString())
        return deferred->reject(exec, JSC::createTypeError(exec, "Module name is not Symbol or String."));

    String moduleName = asString(moduleNameValue)->value(exec);

    // Now, we consider the given moduleName as the same to the `import "..."` in the module code.
    // We use the completed URL as the unique module key.
    URL completedUrl;

    if (importerModuleKey.isSymbol())
        completedUrl = m_document.completeURL(moduleName);
    else if (importerModuleKey.isUndefined())
        completedUrl = m_document.completeURL(moduleName);
    else if (importerModuleKey.isString()) {
        URL importerModuleUrl(URL(), asString(importerModuleKey)->value(exec));
        if (!importerModuleUrl.isValid())
            return deferred->reject(exec, JSC::createTypeError(exec, "Importer module key is an invalid URL."));
        completedUrl = m_document.completeURL(moduleName, importerModuleUrl);
    } else
        return deferred->reject(exec, JSC::createTypeError(exec, "Importer module key is not Symbol or String."));

    if (!completedUrl.isValid())
        return deferred->reject(exec, JSC::createTypeError(exec, "Module name constructs an invalid URL."));

    return deferred->resolve(exec, jsString(exec, completedUrl.string()));
}

JSC::JSInternalPromise* ScriptModuleLoader::fetch(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSValue)
{
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::create(exec, globalObject);

    if (moduleKeyValue.isSymbol())
        return deferred->reject(exec, JSC::createTypeError(exec, "Symbol module key should be already fulfilled with the inlined resource."));

    if (!moduleKeyValue.isString())
        return deferred->reject(exec, JSC::createTypeError(exec, "Module key is not Symbol or String."));

    URL completedUrl(URL(), asString(moduleKeyValue)->value(exec));
    if (!completedUrl.isValid())
        return deferred->reject(exec, JSC::createTypeError(exec, "Module key is an invalid URL."));

    // FIXME: Implement the module fetcher.

    return deferred->promise();
}

JSC::JSValue ScriptModuleLoader::evaluate(JSC::JSGlobalObject*, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSValue moduleRecordValue, JSC::JSValue)
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: Currently, we only support JSModuleRecord.
    // Once the reflective part of the module loader is supported, we will handle arbitrary values.
    // https://whatwg.github.io/loader/#registry-prototype-provide
    JSC::JSModuleRecord* moduleRecord = JSC::jsDynamicCast<JSC::JSModuleRecord*>(moduleRecordValue);
    if (!moduleRecord)
        return JSC::jsUndefined();

    URL sourceUrl;
    if (moduleKeyValue.isSymbol())
        sourceUrl = m_document.url();
    else if (moduleKeyValue.isString())
        sourceUrl = URL(URL(), asString(moduleKeyValue)->value(exec));
    else
        return JSC::throwTypeError(exec, scope, ASCIILiteral("Module key is not Symbol or String."));

    if (!sourceUrl.isValid())
        return JSC::throwTypeError(exec, scope, ASCIILiteral("Module key is an invalid URL."));

    // FIXME: Implement evaluating module code.

    return JSC::jsUndefined();
}

}
