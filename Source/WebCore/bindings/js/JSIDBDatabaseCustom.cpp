/*
 * Copyright (C) 2012 Michael Pruett <michael@68k.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INDEXED_DATABASE)

#include "JSIDBDatabase.h"

#include "IDBBindingUtilities.h"
#include "IDBDatabase.h"
#include "IDBKeyPath.h"
#include "IDBObjectStore.h"
#include "JSDOMBinding.h"
#include "JSDOMStringList.h"
#include "JSIDBObjectStore.h"
#include <runtime/Error.h>
#include <runtime/JSString.h>

using namespace JSC;

namespace WebCore {

JSValue JSIDBDatabase::createObjectStore(ExecState& state)
{
    if (state.argumentCount() < 1)
        return state.vm().throwException(&state, createNotEnoughArgumentsError(&state));

    String name = state.argument(0).toString(&state)->value(&state);
    if (state.hadException())
        return jsUndefined();

    JSValue optionsValue = state.argument(1);
    if (!optionsValue.isUndefinedOrNull() && !optionsValue.isObject())
        return throwTypeError(&state, "Not an object.");

    IDBKeyPath keyPath;
    bool autoIncrement = false;
    if (!optionsValue.isUndefinedOrNull()) {
        JSValue keyPathValue = optionsValue.get(&state, Identifier::fromString(&state, "keyPath"));
        if (state.hadException())
            return jsUndefined();

        if (!keyPathValue.isUndefinedOrNull()) {
            keyPath = idbKeyPathFromValue(&state, keyPathValue);
            if (state.hadException())
                return jsUndefined();
        }

        autoIncrement = optionsValue.get(&state, Identifier::fromString(&state, "autoIncrement")).toBoolean(&state);
        if (state.hadException())
            return jsUndefined();
    }

    ExceptionCodeWithMessage ec;
    JSValue result = toJS(&state, globalObject(), wrapped().createObjectStore(name, keyPath, autoIncrement, ec).get());
    setDOMException(&state, ec);
    return result;
}

JSValue JSIDBDatabase::transaction(ExecState& exec)
{
    size_t argsCount = std::min<size_t>(2, exec.argumentCount());
    if (argsCount < 1)
        return exec.vm().throwException(&exec, createNotEnoughArgumentsError(&exec));

    auto* scriptContext = jsCast<JSDOMGlobalObject*>(exec.lexicalGlobalObject())->scriptExecutionContext();
    if (!scriptContext)
        return jsUndefined();

    Vector<String> scope;
    JSValue scopeArg(exec.argument(0));
    auto domStringList = JSDOMStringList::toWrapped(&exec, scopeArg);
    if (exec.hadException())
        return jsUndefined();

    if (domStringList)
        scope = *domStringList;
    else {
        scope.append(scopeArg.toString(&exec)->value(&exec));
        if (exec.hadException())
            return jsUndefined();
    }

    String mode;
    if (argsCount == 2) {
        JSValue modeArg(exec.argument(1));
        mode = modeArg.toString(&exec)->value(&exec);

        if (exec.hadException())
            return jsUndefined();
    }

    ExceptionCodeWithMessage ec;
    JSValue result = toJS(&exec, globalObject(), wrapped().transaction(scriptContext, scope, mode, ec).get());
    setDOMException(&exec, ec);
    return result;
}

}

#endif
