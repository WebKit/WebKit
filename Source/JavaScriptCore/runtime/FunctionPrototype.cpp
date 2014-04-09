/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "FunctionPrototype.h"

#include "Arguments.h"
#include "BuiltinExecutables.h"
#include "BuiltinNames.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSString.h"
#include "JSStringBuilder.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(FunctionPrototype);

const ClassInfo FunctionPrototype::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(FunctionPrototype) };

static EncodedJSValue JSC_HOST_CALL functionProtoFuncToString(ExecState*);

FunctionPrototype::FunctionPrototype(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void FunctionPrototype::finishCreation(VM& vm, const String& name)
{
    Base::finishCreation(vm, name);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), DontDelete | ReadOnly | DontEnum);
}

void FunctionPrototype::addFunctionProperties(ExecState* exec, JSGlobalObject* globalObject, JSFunction** callFunction, JSFunction** applyFunction)
{
    VM& vm = exec->vm();

    JSFunction* toStringFunction = JSFunction::create(vm, globalObject, 0, vm.propertyNames->toString.string(), functionProtoFuncToString);
    putDirectWithoutTransition(vm, vm.propertyNames->toString, toStringFunction, DontEnum);

    *applyFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().applyPublicName(), functionPrototypeApplyCodeGenerator(vm), DontEnum);
    *callFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().callPublicName(), functionPrototypeCallCodeGenerator(vm), DontEnum);
    
    putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().bindPublicName(), functionPrototypeBindCodeGenerator(vm), DontEnum);
}

static EncodedJSValue JSC_HOST_CALL callFunctionPrototype(ExecState*)
{
    return JSValue::encode(jsUndefined());
}

// ECMA 15.3.4
CallType FunctionPrototype::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callFunctionPrototype;
    return CallTypeHost;
}

// Functions

// Compatibility hack for the Optimost JavaScript library. (See <rdar://problem/6595040>.)
static inline void insertSemicolonIfNeeded(String& functionBody, bool bodyIncludesBraces)
{
    if (!bodyIncludesBraces)
        functionBody = makeString("{ ", functionBody, "}");

    ASSERT(functionBody[0] == '{');
    ASSERT(functionBody[functionBody.length() - 1] == '}');

    for (size_t i = functionBody.length() - 2; i > 0; --i) {
        UChar ch = functionBody[i];
        if (!Lexer<UChar>::isWhiteSpace(ch) && !Lexer<UChar>::isLineTerminator(ch)) {
            if (ch != ';' && ch != '}')
                functionBody = makeString(functionBody.substringSharingImpl(0, i + 1), ";", functionBody.substringSharingImpl(i + 1, functionBody.length() - (i + 1)));
            return;
        }
    }
}

EncodedJSValue JSC_HOST_CALL functionProtoFuncToString(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (thisValue.inherits(JSFunction::info())) {
        JSFunction* function = jsCast<JSFunction*>(thisValue);
        if (function->isHostOrBuiltinFunction())
            return JSValue::encode(jsMakeNontrivialString(exec, "function ", function->name(exec), "() {\n    [native code]\n}"));
        FunctionExecutable* executable = function->jsExecutable();
        String sourceString = executable->source().toString();
        insertSemicolonIfNeeded(sourceString, executable->bodyIncludesBraces());
        return JSValue::encode(jsMakeNontrivialString(exec, "function ", function->name(exec), "(", executable->paramString(), ") ", sourceString));
    }

    if (thisValue.inherits(InternalFunction::info())) {
        InternalFunction* function = asInternalFunction(thisValue);
        return JSValue::encode(jsMakeNontrivialString(exec, "function ", function->name(exec), "() {\n    [native code]\n}"));
    }

    return throwVMTypeError(exec);
}

} // namespace JSC
