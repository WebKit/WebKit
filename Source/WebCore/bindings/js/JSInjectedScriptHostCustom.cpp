/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
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
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#if ENABLE(INSPECTOR)

#include "JSInjectedScriptHost.h"

#if ENABLE(SQL_DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif
#include "ExceptionCode.h"
#include "InjectedScriptHost.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorValues.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "JSStorage.h"
#include "ScriptValue.h"
#include "Storage.h"
#include <parser/SourceCode.h>
#include <runtime/DateInstance.h>
#include <runtime/Error.h>
#include <runtime/JSArray.h>
#include <runtime/JSFunction.h>
#include <runtime/JSLock.h>
#include <runtime/RegExpObject.h>

using namespace JSC;

namespace WebCore {

Node* InjectedScriptHost::scriptValueAsNode(ScriptValue value)
{
    if (!value.isObject() || value.isNull())
        return 0;
    return toNode(value.jsValue());
}

ScriptValue InjectedScriptHost::nodeAsScriptValue(ScriptState* state, Node* node)
{
    JSLock lock(SilenceAssertionsOnly);
    return ScriptValue(state->globalData(), toJS(state, deprecatedGlobalObjectForPrototype(state), node));
}

JSValue JSInjectedScriptHost::evaluate(ExecState* exec)
{
    JSValue expression = exec->argument(0);
    if (!expression.isString())
        return throwError(exec, createError(exec, "String argument expected."));
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    JSFunction* evalFunction = globalObject->evalFunction();
    CallData callData;
    CallType callType = evalFunction->methodTable()->getCallData(evalFunction, callData);
    if (callType == CallTypeNone)
        return jsUndefined();
    MarkedArgumentBuffer args;
    args.append(expression);

    bool wasEvalEnabled = globalObject->evalEnabled();
    globalObject->setEvalEnabled(true);
    JSValue result = JSC::call(exec, evalFunction, callType, callData, exec->globalThisValue(), args);
    globalObject->setEvalEnabled(wasEvalEnabled);

    return result;
}

JSValue JSInjectedScriptHost::inspectedNode(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    Node* node = impl()->inspectedNode(exec->argument(0).toInt32(exec));
    if (!node)
        return jsUndefined();

    JSLock lock(SilenceAssertionsOnly);
    return toJS(exec, globalObject(), node);
}

JSValue JSInjectedScriptHost::internalConstructorName(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSObject* thisObject = exec->argument(0).toThisObject(exec);
    UString result = thisObject->methodTable()->className(thisObject);
    return jsString(exec, result);
}

JSValue JSInjectedScriptHost::isHTMLAllCollection(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->argument(0);
    return jsBoolean(value.inherits(&JSHTMLAllCollection::s_info));
}

JSValue JSInjectedScriptHost::type(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->argument(0);
    if (value.isString())
        return jsString(exec, String("string"));
    if (value.inherits(&JSArray::s_info))
        return jsString(exec, String("array"));
    if (value.isBoolean())
        return jsString(exec, String("boolean"));
    if (value.isNumber())
        return jsString(exec, String("number"));
    if (value.inherits(&DateInstance::s_info))
        return jsString(exec, String("date"));
    if (value.inherits(&RegExpObject::s_info))
        return jsString(exec, String("regexp"));
    if (value.inherits(&JSNode::s_info))
        return jsString(exec, String("node"));
    if (value.inherits(&JSNodeList::s_info))
        return jsString(exec, String("array"));
    if (value.inherits(&JSHTMLCollection::s_info))
        return jsString(exec, String("array"));
    return jsUndefined();
}

JSValue JSInjectedScriptHost::functionDetails(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();
    JSValue value = exec->argument(0);
    if (!value.asCell()->inherits(&JSFunction::s_info))
        return jsUndefined();
    JSFunction* function = asFunction(value);

    const SourceCode* sourceCode = function->sourceCode();
    if (!sourceCode)
        return jsUndefined();
    int lineNumber = sourceCode->firstLine();
    if (lineNumber)
        lineNumber -= 1; // In the inspector protocol all positions are 0-based while in SourceCode they are 1-based
    UString scriptId = UString::number(sourceCode->provider()->asID());

    JSObject* location = constructEmptyObject(exec);
    location->putDirect(exec->globalData(), Identifier(exec, "lineNumber"), jsNumber(lineNumber));
    location->putDirect(exec->globalData(), Identifier(exec, "scriptId"), jsString(exec, scriptId));

    JSObject* result = constructEmptyObject(exec);
    result->putDirect(exec->globalData(), Identifier(exec, "location"), location);
    UString name = function->name(exec);
    if (!name.isEmpty())
        result->putDirect(exec->globalData(), Identifier(exec, "name"), jsString(exec, name));
    UString displayName = function->displayName(exec);
    if (!displayName.isEmpty())
        result->putDirect(exec->globalData(), Identifier(exec, "displayName"), jsString(exec, displayName));
    return result;
}

JSValue JSInjectedScriptHost::inspect(ExecState* exec)
{
    if (exec->argumentCount() >= 2) {
        ScriptValue object(exec->globalData(), exec->argument(0));
        ScriptValue hints(exec->globalData(), exec->argument(1));
        impl()->inspectImpl(object.toInspectorValue(exec), hints.toInspectorValue(exec));
    }
    return jsUndefined();
}

JSValue JSInjectedScriptHost::databaseId(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();
#if ENABLE(SQL_DATABASE)
    Database* database = toDatabase(exec->argument(0));
    if (database)
        return jsNumber(impl()->databaseIdImpl(database));
#endif
    return jsUndefined();
}

JSValue JSInjectedScriptHost::storageId(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();
    Storage* storage = toStorage(exec->argument(0));
    if (storage)
        return jsNumber(impl()->storageIdImpl(storage));
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
