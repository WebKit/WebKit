/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "Error.h"
#include "JSCJSValue.h"
#include "JSCellInlines.h"
#include "JSJavaScriptCallFramePrototype.h"
#include "StructureInlines.h"

using namespace JSC;

namespace Inspector {

const ClassInfo JSJavaScriptCallFrame::s_info = { "JavaScriptCallFrame", &Base::s_info, 0, CREATE_METHOD_TABLE(JSJavaScriptCallFrame) };

JSJavaScriptCallFrame::JSJavaScriptCallFrame(VM& vm, Structure* structure, PassRefPtr<JavaScriptCallFrame> impl)
    : JSDestructibleObject(vm, structure)
    , m_impl(impl.leakRef())
{
}

void JSJavaScriptCallFrame::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
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

JSValue JSJavaScriptCallFrame::evaluate(ExecState* exec)
{
    NakedPtr<Exception> exception;
    JSValue result = impl().evaluate(exec->argument(0).toString(exec)->value(exec), exception);
    if (exception)
        exec->vm().throwException(exec, exception);

    return result;
}

JSValue JSJavaScriptCallFrame::scopeType(ExecState* exec)
{
    if (!impl().scopeChain())
        return jsUndefined();

    if (!exec->argument(0).isInt32())
        return jsUndefined();
    int index = exec->argument(0).asInt32();

    DebuggerScope* scopeChain = impl().scopeChain();
    DebuggerScope::iterator end = scopeChain->end();

    for (DebuggerScope::iterator iter = scopeChain->begin(); iter != end; ++iter) {
        DebuggerScope* scope = iter.get();

        if (!index) {
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
            if (scope->isGlobalScope()) {
                ASSERT(++iter == end);
                return jsNumber(JSJavaScriptCallFrame::GLOBAL_SCOPE);
            }
            ASSERT(scope->isClosureScope());
            return jsNumber(JSJavaScriptCallFrame::CLOSURE_SCOPE);
        }

        --index;
    }

    ASSERT_NOT_REACHED();
    return jsUndefined();
}

JSValue JSJavaScriptCallFrame::caller(ExecState* exec) const
{
    return toJS(exec, globalObject(), impl().caller());
}

JSValue JSJavaScriptCallFrame::sourceID(ExecState*) const
{
    return jsNumber(impl().sourceID());
}

JSValue JSJavaScriptCallFrame::line(ExecState*) const
{
    return jsNumber(impl().line());
}

JSValue JSJavaScriptCallFrame::column(ExecState*) const
{
    return jsNumber(impl().column());
}

JSValue JSJavaScriptCallFrame::functionName(ExecState* exec) const
{
    return jsString(exec, impl().functionName());
}

JSValue JSJavaScriptCallFrame::scopeChain(ExecState* exec) const
{
    if (!impl().scopeChain())
        return jsNull();

    DebuggerScope* scopeChain = impl().scopeChain();
    DebuggerScope::iterator iter = scopeChain->begin();
    DebuggerScope::iterator end = scopeChain->end();

    // We must always have something in the scope chain.
    ASSERT(iter != end);

    MarkedArgumentBuffer list;
    do {
        list.append(iter.get());
        ++iter;
    } while (iter != end);

    return constructArray(exec, nullptr, globalObject(), list);
}

JSValue JSJavaScriptCallFrame::thisObject(ExecState*) const
{
    return impl().thisValue();
}

JSValue JSJavaScriptCallFrame::type(ExecState* exec) const
{
    switch (impl().type()) {
    case DebuggerCallFrame::FunctionType:
        return jsNontrivialString(exec, ASCIILiteral("function"));
    case DebuggerCallFrame::ProgramType:
        return jsNontrivialString(exec, ASCIILiteral("program"));
    }

    ASSERT_NOT_REACHED();
    return jsNull();
}

JSValue toJS(ExecState* exec, JSGlobalObject* globalObject, JavaScriptCallFrame* impl)
{
    if (!impl)
        return jsNull();

    JSObject* prototype = JSJavaScriptCallFrame::createPrototype(exec->vm(), globalObject);
    Structure* structure = JSJavaScriptCallFrame::createStructure(exec->vm(), globalObject, prototype);
    JSJavaScriptCallFrame* javaScriptCallFrame = JSJavaScriptCallFrame::create(exec->vm(), structure, impl);

    return javaScriptCallFrame;
}

JSJavaScriptCallFrame* toJSJavaScriptCallFrame(JSValue value)
{
    return value.inherits(JSJavaScriptCallFrame::info()) ? jsCast<JSJavaScriptCallFrame*>(value) : nullptr;
}

} // namespace Inspector

