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

#include "JSIDBObjectStore.h"

#include "IDBBindingUtilities.h"
#include "IDBDatabaseException.h"
#include "IDBKeyPath.h"
#include "IDBObjectStoreImpl.h"
#include "JSDOMBinding.h"
#include "JSIDBIndex.h"
#include "JSIDBRequest.h"
#include <runtime/Error.h>
#include <runtime/JSString.h>

using namespace JSC;

namespace WebCore {

void JSIDBObjectStore::visitAdditionalChildren(SlotVisitor& visitor)
{
    if (!wrapped().isModern())
        return;

    static_cast<IDBClient::IDBObjectStore&>(wrapped()).visitReferencedIndexes(visitor);
}

static JSValue putOrAdd(JSC::ExecState& state, bool overwrite)
{
    JSValue thisValue = state.thisValue();
    JSIDBObjectStore* castedThis = jsDynamicCast<JSIDBObjectStore*>(thisValue);
    if (UNLIKELY(!castedThis))
        return JSValue::decode(throwThisTypeError(state, "IDBObjectStore", "put"));

    ASSERT_GC_OBJECT_INHERITS(castedThis, JSIDBObjectStore::info());
    auto& wrapped = castedThis->wrapped();

    size_t argsCount = state.argumentCount();
    if (UNLIKELY(argsCount < 1))
        return JSValue::decode(throwVMError(&state, createNotEnoughArgumentsError(&state)));

    ExceptionCodeWithMessage ec;
    auto value = state.uncheckedArgument(0);

    if (argsCount == 1) {
        JSValue result;
        if (overwrite)
            result = toJS(&state, castedThis->globalObject(), wrapped.put(state, value, ec).get());
        else
            result = toJS(&state, castedThis->globalObject(), wrapped.add(state, value, ec).get());

        setDOMException(&state, ec);
        return result;
    }

    auto key = state.uncheckedArgument(1);
    JSValue result;
    if (overwrite)
        result = toJS(&state, castedThis->globalObject(), wrapped.put(state, value, key, ec).get());
    else
        result = toJS(&state, castedThis->globalObject(), wrapped.add(state, value, key, ec).get());

    setDOMException(&state, ec);
    return result;
}

JSValue JSIDBObjectStore::putFunction(JSC::ExecState& state)
{
    return putOrAdd(state, true);
}

JSValue JSIDBObjectStore::add(JSC::ExecState& state)
{
    return putOrAdd(state, false);
}

JSValue JSIDBObjectStore::createIndex(ExecState& state)
{
    ScriptExecutionContext* context = jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject())->scriptExecutionContext();
    if (!context)
        return state.vm().throwException(&state, createReferenceError(&state, "IDBObjectStore script &stateution context is unavailable"));

    if (state.argumentCount() < 2)
        return state.vm().throwException(&state, createNotEnoughArgumentsError(&state));

    String name;
    JSValue nameValue = state.argument(0);
    if (!nameValue.isUndefinedOrNull())
        name = nameValue.toString(&state)->value(&state);

    if (state.hadException())
        return jsUndefined();

    IDBKeyPath keyPath;
    JSValue keyPathValue = state.argument(1);
    if (!keyPathValue.isUndefinedOrNull())
        keyPath = idbKeyPathFromValue(&state, keyPathValue);
    else {
        ExceptionCodeWithMessage ec;
        ec.code = IDBDatabaseException::SyntaxError;
        ec.message = ASCIILiteral("Failed to execute 'createIndex' on 'IDBObjectStore': The keyPath argument contains an invalid key path.");
        setDOMException(&state, ec);
        return jsUndefined();
    }

    if (state.hadException())
        return jsUndefined();

    JSValue optionsValue = state.argument(2);
    if (!optionsValue.isUndefinedOrNull() && !optionsValue.isObject())
        return throwTypeError(&state, "Failed to execute 'createIndex' on 'IDBObjectStore': No function was found that matched the signature provided.");

    bool unique = false;
    bool multiEntry = false;
    if (!optionsValue.isUndefinedOrNull()) {
        unique = optionsValue.get(&state, Identifier::fromString(&state, "unique")).toBoolean(&state);
        if (state.hadException())
            return jsUndefined();

        multiEntry = optionsValue.get(&state, Identifier::fromString(&state, "multiEntry")).toBoolean(&state);
        if (state.hadException())
            return jsUndefined();
    }

    ExceptionCodeWithMessage ec;
    JSValue result = toJS(&state, globalObject(), wrapped().createIndex(context, name, keyPath, unique, multiEntry, ec).get());
    setDOMException(&state, ec);
    return result;
}

}

#endif
