/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#include "JSJavaScriptCallFrame.h"

#include "DebuggerScope.h"
#include "JSCInlines.h"
#include "JSJavaScriptCallFramePrototype.h"
#include "ObjectConstructor.h"

namespace Inspector {

using namespace JSC;

const ClassInfo JSJavaScriptCallFrame::s_info = { "JavaScriptCallFrame"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSJavaScriptCallFrame) };

JSJavaScriptCallFrame::JSJavaScriptCallFrame(VM& vm, Structure* structure, Ref<JavaScriptCallFrame>&& impl)
    : Base(vm, structure)
    , m_impl(&impl.leakRef())
{
}

JSObject* JSJavaScriptCallFrame::createPrototype(VM& vm, JSGlobalObject* globalObject)
{
    return JSJavaScriptCallFramePrototype::create(vm, globalObject, JSJavaScriptCallFramePrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
}

void JSJavaScriptCallFrame::destroy(JSC::JSCell* cell)
{
    JSJavaScriptCallFrame* thisObject = static_cast<JSJavaScriptCallFrame*>(cell);
    thisObject->JSJavaScriptCallFrame::~JSJavaScriptCallFrame();
}

void JSJavaScriptCallFrame::releaseImpl()
{
    if (auto impl = std::exchange(m_impl, nullptr))
        impl->deref();
}

JSJavaScriptCallFrame::~JSJavaScriptCallFrame()
{
    releaseImpl();
}

JSValue JSJavaScriptCallFrame::evaluateWithScopeExtension(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue scriptValue = callFrame->argument(0);
    if (!scriptValue.isString())
        return throwTypeError(globalObject, scope, "JSJavaScriptCallFrame.evaluateWithScopeExtension first argument must be a string."_s);

    String script = asString(scriptValue)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());

    NakedPtr<Exception> exception;
    JSObject* scopeExtension = callFrame->argument(1).getObject();
    JSValue result = impl().evaluateWithScopeExtension(vm, script, scopeExtension, exception);
    if (exception)
        throwException(globalObject, scope, exception);

    return result;
}

static JSValue valueForScopeType(DebuggerScope* scope)
{
    if (scope->isCatchScope())
        return jsNumber(JSJavaScriptCallFrame::CATCH_SCOPE);
    if (scope->isFunctionNameScope())
        return jsNumber(JSJavaScriptCallFrame::FUNCTION_NAME_SCOPE);
    if (scope->isWithScope())
        return jsNumber(JSJavaScriptCallFrame::WITH_SCOPE);
    if (scope->isNestedLexicalScope())
        return jsNumber(JSJavaScriptCallFrame::NESTED_LEXICAL_SCOPE);
    if (scope->isGlobalLexicalEnvironment())
        return jsNumber(JSJavaScriptCallFrame::GLOBAL_LEXICAL_ENVIRONMENT_SCOPE);
    if (scope->isGlobalScope())
        return jsNumber(JSJavaScriptCallFrame::GLOBAL_SCOPE);

    ASSERT(scope->isClosureScope());
    return jsNumber(JSJavaScriptCallFrame::CLOSURE_SCOPE);
}

static JSValue valueForScopeLocation(JSGlobalObject* globalObject, const DebuggerLocation& location)
{
    if (location.sourceID == noSourceID)
        return jsNull();

    // Debugger.Location protocol object.
    VM& vm = globalObject->vm();
    JSObject* result = constructEmptyObject(globalObject);
    result->putDirect(vm, Identifier::fromString(vm, "scriptId"_s), jsString(vm, String::number(location.sourceID)));
    result->putDirect(vm, Identifier::fromString(vm, "lineNumber"_s), jsNumber(location.line));
    result->putDirect(vm, Identifier::fromString(vm, "columnNumber"_s), jsNumber(location.column));
    return result;
}

JSValue JSJavaScriptCallFrame::scopeDescriptions(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    DebuggerScope* scopeChain = impl().scopeChain(vm);
    if (!scopeChain)
        return jsUndefined();

    int index = 0;
    JSArray* array = constructEmptyArray(globalObject, nullptr);

    DebuggerScope::iterator end = scopeChain->end();
    for (DebuggerScope::iterator iter = scopeChain->begin(); iter != end; ++iter) {
        DebuggerScope* scope = iter.get();
        JSObject* description = constructEmptyObject(globalObject);
        description->putDirect(vm, Identifier::fromString(vm, "type"_s), valueForScopeType(scope));
        description->putDirect(vm, Identifier::fromString(vm, "name"_s), jsString(vm, scope->name()));
        description->putDirect(vm, Identifier::fromString(vm, "location"_s), valueForScopeLocation(globalObject, scope->location()));
        array->putDirectIndex(globalObject, index++, description);
        RETURN_IF_EXCEPTION(throwScope, JSValue());
    }

    return array;
}

JSValue JSJavaScriptCallFrame::caller(JSGlobalObject* lexicalGlobalObject) const
{
    return toJS(lexicalGlobalObject, this->globalObject(), impl().caller());
}

JSValue JSJavaScriptCallFrame::sourceID(JSGlobalObject*) const
{
    return jsNumber(impl().sourceID());
}

JSValue JSJavaScriptCallFrame::line(JSGlobalObject*) const
{
    return jsNumber(impl().line());
}

JSValue JSJavaScriptCallFrame::column(JSGlobalObject*) const
{
    return jsNumber(impl().column());
}

JSValue JSJavaScriptCallFrame::functionName(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    return jsString(vm, impl().functionName(vm));
}

JSValue JSJavaScriptCallFrame::scopeChain(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!impl().scopeChain(vm))
        return jsNull();

    DebuggerScope* scopeChain = impl().scopeChain(vm);
    DebuggerScope::iterator iter = scopeChain->begin();
    DebuggerScope::iterator end = scopeChain->end();

    // We must always have something in the scope chain.
    ASSERT(iter != end);

    MarkedArgumentBuffer list;
    do {
        list.append(iter.get());
        ++iter;
    } while (iter != end);
    if (UNLIKELY(list.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    return constructArray(this->globalObject(), static_cast<ArrayAllocationProfile*>(nullptr), list);
}

JSValue JSJavaScriptCallFrame::thisObject(JSGlobalObject* globalObject) const
{
    return impl().thisValue(globalObject->vm());
}

JSValue JSJavaScriptCallFrame::isTailDeleted(JSC::JSGlobalObject*) const
{
    return jsBoolean(impl().isTailDeleted());
}

JSValue JSJavaScriptCallFrame::type(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    switch (impl().type(vm)) {
    case DebuggerCallFrame::FunctionType:
        return jsNontrivialString(vm, "function"_s);
    case DebuggerCallFrame::ProgramType:
        return jsNontrivialString(vm, "program"_s);
    }

    ASSERT_NOT_REACHED();
    return jsNull();
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, JavaScriptCallFrame* impl)
{
    if (!impl)
        return jsNull();

    VM& vm = lexicalGlobalObject->vm();
    JSObject* prototype = JSJavaScriptCallFrame::createPrototype(vm, globalObject);
    Structure* structure = JSJavaScriptCallFrame::createStructure(vm, globalObject, prototype);
    JSJavaScriptCallFrame* javaScriptCallFrame = JSJavaScriptCallFrame::create(vm, structure, *impl);

    return javaScriptCallFrame;
}

} // namespace Inspector

