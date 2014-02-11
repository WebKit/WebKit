/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSInjectedScriptHost.h"

#if ENABLE(INSPECTOR)

#include "DateInstance.h"
#include "Error.h"
#include "InjectedScriptHost.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSInjectedScriptHostPrototype.h"
#include "JSTypedArrays.h"
#include "ObjectConstructor.h"
#include "JSCInlines.h"
#include "RegExpObject.h"
#include "SourceCode.h"
#include "TypedArrayInlines.h"

using namespace JSC;

namespace Inspector {

const ClassInfo JSInjectedScriptHost::s_info = { "InjectedScriptHost", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSInjectedScriptHost) };

JSInjectedScriptHost::JSInjectedScriptHost(VM& vm, Structure* structure, PassRefPtr<InjectedScriptHost> impl)
    : JSDestructibleObject(vm, structure)
    , m_impl(impl.leakRef())
{
}

void JSInjectedScriptHost::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

JSObject* JSInjectedScriptHost::createPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return JSInjectedScriptHostPrototype::create(vm, globalObject, JSInjectedScriptHostPrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
}

void JSInjectedScriptHost::destroy(JSC::JSCell* cell)
{
    JSInjectedScriptHost* thisObject = static_cast<JSInjectedScriptHost*>(cell);
    thisObject->JSInjectedScriptHost::~JSInjectedScriptHost();
}

void JSInjectedScriptHost::releaseImpl()
{
    if (m_impl) {
        m_impl->deref();
        m_impl = nullptr;
    }
}

JSInjectedScriptHost::~JSInjectedScriptHost()
{
    releaseImpl();
}

JSValue JSInjectedScriptHost::evaluate(ExecState* exec) const
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return globalObject->evalFunction();
}

JSValue JSInjectedScriptHost::internalConstructorName(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSObject* thisObject = jsCast<JSObject*>(exec->uncheckedArgument(0).toThis(exec, NotStrictMode));
    String result = thisObject->methodTable()->className(thisObject);
    return jsString(exec, result);
}

JSValue JSInjectedScriptHost::isHTMLAllCollection(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    return jsBoolean(impl().isHTMLAllCollection(value));
}

JSValue JSInjectedScriptHost::type(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return jsUndefined();

    JSValue value = exec->uncheckedArgument(0);
    if (value.isString())
        return exec->vm().smallStrings.stringString();
    if (value.isBoolean())
        return exec->vm().smallStrings.booleanString();
    if (value.isNumber())
        return exec->vm().smallStrings.numberString();

    if (value.inherits(JSArray::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(DateInstance::info()))
        return jsNontrivialString(exec, ASCIILiteral("date"));
    if (value.inherits(RegExpObject::info()))
        return jsNontrivialString(exec, ASCIILiteral("regexp"));
    if (value.inherits(JSInt8Array::info()) || value.inherits(JSInt16Array::info()) || value.inherits(JSInt32Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSUint8Array::info()) || value.inherits(JSUint16Array::info()) || value.inherits(JSUint32Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));
    if (value.inherits(JSFloat32Array::info()) || value.inherits(JSFloat64Array::info()))
        return jsNontrivialString(exec, ASCIILiteral("array"));

    return impl().type(exec, value);
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
        result->putDirect(exec->vm(), Identifier(exec, "name"), jsString(exec, name));

    String displayName = function->displayName(exec);
    if (!displayName.isEmpty())
        result->putDirect(exec->vm(), Identifier(exec, "displayName"), jsString(exec, displayName));

    // FIXME: provide function scope data in "scopesRaw" property when JSC supports it.
    // <https://webkit.org/b/87192> [JSC] expose function (closure) inner context to debugger

    return result;
}

JSValue JSInjectedScriptHost::getInternalProperties(ExecState*)
{
    // FIXME: <https://webkit.org/b/94533> [JSC] expose object inner properties to debugger
    return jsUndefined();
}

JSValue toJS(ExecState* exec, JSGlobalObject* globalObject, InjectedScriptHost* impl)
{
    if (!impl)
        return jsNull();

    JSObject* prototype = JSInjectedScriptHost::createPrototype(exec->vm(), globalObject);
    Structure* structure = JSInjectedScriptHost::createStructure(exec->vm(), globalObject, prototype);
    JSInjectedScriptHost* injectedScriptHost = JSInjectedScriptHost::create(exec->vm(), structure, impl);

    return injectedScriptHost;
}

JSInjectedScriptHost* toJSInjectedScriptHost(JSValue value)
{
    return value.inherits(JSInjectedScriptHost::info()) ? jsCast<JSInjectedScriptHost*>(value) : nullptr;
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
