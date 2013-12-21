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

#include "ExceptionCode.h"
#include "InjectedScriptHost.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include <bindings/ScriptValue.h>
#include <parser/SourceCode.h>
#include <runtime/DateInstance.h>
#include <runtime/Error.h>
#include <runtime/JSArray.h>
#include <runtime/JSFunction.h>
#include <runtime/JSLock.h>
#include <runtime/JSTypedArrays.h>
#include <runtime/ObjectConstructor.h>
#include <runtime/Operations.h>
#include <runtime/RegExpObject.h>
#include <runtime/TypedArrayInlines.h>

using namespace JSC;

namespace WebCore {

JSValue JSInjectedScriptHost::internalConstructorName(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSObject* thisObject = jsCast<JSObject*>(exec->uncheckedArgument(0).toThis(exec, NotStrictMode));
    String result = thisObject->methodTable()->className(thisObject);
    return jsStringWithCache(exec, result);
}

JSValue JSInjectedScriptHost::isHTMLAllCollection(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    return jsBoolean(value.inherits(JSHTMLAllCollection::info()));
}

JSValue JSInjectedScriptHost::type(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    if (value.isString())
        return exec->vm().smallStrings.stringString();
    if (value.inherits(JSArray::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.isBoolean())
        return exec->vm().smallStrings.booleanString();
    if (value.isNumber())
        return exec->vm().smallStrings.numberString();
    if (value.inherits(DateInstance::info()))
        return jsNontrivialString(exec, ASCIILiteral("date"));
    if (value.inherits(RegExpObject::info()))
        return jsNontrivialString(exec, ASCIILiteral("regexp"));
    if (value.inherits(JSNode::info()))
        return jsNontrivialString(exec, ASCIILiteral("node"));
    if (value.inherits(JSNodeList::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSHTMLCollection::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSInt8Array::info()) || value.inherits(JSInt16Array::info()) || value.inherits(JSInt32Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSUint8Array::info()) || value.inherits(JSUint16Array::info()) || value.inherits(JSUint32Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSFloat32Array::info()) || value.inherits(JSFloat64Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));

    return jsUndefined();
}

JSValue JSInjectedScriptHost::functionDetails(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();
    JSValue value = exec->uncheckedArgument(0);
    if (!value.asCell()->inherits(JSFunction::info()))
        return jsUndefined();
    JSFunction* function = jsCast<JSFunction*>(value);

    const SourceCode* sourceCode = function->sourceCode();
    if (!sourceCode)
        return jsUndefined();
    int lineNumber = sourceCode->firstLine();
    if (lineNumber)
        lineNumber -= 1; // In the inspector protocol all positions are 0-based while in SourceCode they are 1-based
    String scriptID = String::number(sourceCode->provider()->asID());

    JSObject* location = constructEmptyObject(exec);
    location->putDirect(exec->vm(), Identifier(exec, "lineNumber"), jsNumber(lineNumber));
    location->putDirect(exec->vm(), Identifier(exec, "scriptId"), jsString(exec, scriptID));

    JSObject* result = constructEmptyObject(exec);
    result->putDirect(exec->vm(), Identifier(exec, "location"), location);
    String name = function->name(exec);
    if (!name.isEmpty())
        result->putDirect(exec->vm(), Identifier(exec, "name"), jsStringWithCache(exec, name));
    String displayName = function->displayName(exec);
    if (!displayName.isEmpty())
        result->putDirect(exec->vm(), Identifier(exec, "displayName"), jsStringWithCache(exec, displayName));
    // FIXME: provide function scope data in "scopesRaw" property when JSC supports it.
    //     https://bugs.webkit.org/show_bug.cgi?id=87192
    return result;
}

JSValue JSInjectedScriptHost::getInternalProperties(ExecState*)
{
    // FIXME: implement this. https://bugs.webkit.org/show_bug.cgi?id=94533
    return jsUndefined();
}

JSValue JSInjectedScriptHost::evaluate(ExecState* exec) const
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return globalObject->evalFunction();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
