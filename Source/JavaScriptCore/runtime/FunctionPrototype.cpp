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
#include "JSArray.h"
#include "JSBoundFunction.h"
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
static EncodedJSValue JSC_HOST_CALL functionProtoFuncApply(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionProtoFuncCall(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionProtoFuncBind(ExecState*);

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

    *applyFunction = JSFunction::create(vm, globalObject, 2, vm.propertyNames->apply.string(), functionProtoFuncApply);
    putDirectWithoutTransition(vm, vm.propertyNames->apply, *applyFunction, DontEnum);

    *callFunction = JSFunction::create(vm, globalObject, 1, vm.propertyNames->call.string(), functionProtoFuncCall);
    putDirectWithoutTransition(vm, vm.propertyNames->call, *callFunction, DontEnum);

    JSFunction* bindFunction = JSFunction::create(vm, globalObject, 1, vm.propertyNames->bind.string(), functionProtoFuncBind);
    putDirectWithoutTransition(vm, vm.propertyNames->bind, bindFunction, DontEnum);
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
    JSValue thisValue = exec->hostThisValue();
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

EncodedJSValue JSC_HOST_CALL functionProtoFuncApply(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    CallData callData;
    CallType callType = getCallData(thisValue, callData);
    if (callType == CallTypeNone)
        return throwVMTypeError(exec);

    JSValue array = exec->argument(1);

    MarkedArgumentBuffer applyArgs;
    if (!array.isUndefinedOrNull()) {
        if (!array.isObject())
            return throwVMTypeError(exec);
        if (asObject(array)->classInfo() == Arguments::info()) {
            if (asArguments(array)->length(exec) > Arguments::MaxArguments)
                return JSValue::encode(throwStackOverflowError(exec));
            asArguments(array)->fillArgList(exec, applyArgs);
        } else if (isJSArray(array)) {
            if (asArray(array)->length() > Arguments::MaxArguments)
                return JSValue::encode(throwStackOverflowError(exec));
            asArray(array)->fillArgList(exec, applyArgs);
        } else {
            unsigned length = asObject(array)->get(exec, exec->propertyNames().length).toUInt32(exec);
            if (length > Arguments::MaxArguments)
                return JSValue::encode(throwStackOverflowError(exec));

            for (unsigned i = 0; i < length; ++i)
                applyArgs.append(asObject(array)->get(exec, i));
        }
    }
    
    return JSValue::encode(call(exec, thisValue, callType, callData, exec->argument(0), applyArgs));
}

EncodedJSValue JSC_HOST_CALL functionProtoFuncCall(ExecState* exec)
{
    JSValue thisValue = exec->hostThisValue();
    CallData callData;
    CallType callType = getCallData(thisValue, callData);
    if (callType == CallTypeNone)
        return throwVMTypeError(exec);

    ArgList args(exec);
    ArgList callArgs;
    args.getSlice(1, callArgs);
    return JSValue::encode(call(exec, thisValue, callType, callData, exec->argument(0), callArgs));
}

// 15.3.4.5 Function.prototype.bind (thisArg [, arg1 [, arg2, ...]])
EncodedJSValue JSC_HOST_CALL functionProtoFuncBind(ExecState* exec)
{
    JSGlobalObject* globalObject = exec->callee()->globalObject();

    // Let Target be the this value.
    JSValue target = exec->hostThisValue();

    // If IsCallable(Target) is false, throw a TypeError exception.
    CallData callData;
    CallType callType = getCallData(target, callData);
    if (callType == CallTypeNone)
        return throwVMTypeError(exec);
    // Primitive values are not callable.
    ASSERT(target.isObject());
    JSObject* targetObject = asObject(target);
    VM& vm = exec->vm();

    // Let A be a new (possibly empty) internal list of all of the argument values provided after thisArg (arg1, arg2 etc), in order.
    size_t numBoundArgs = exec->argumentCount() > 1 ? exec->argumentCount() - 1 : 0;
    JSArray* boundArgs = JSArray::tryCreateUninitialized(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), numBoundArgs);
    if (!boundArgs)
        return JSValue::encode(throwOutOfMemoryError(exec));

    for (size_t i = 0; i < numBoundArgs; ++i)
        boundArgs->initializeIndex(vm, i, exec->argument(i + 1));

    // If the [[Class]] internal property of Target is "Function", then ...
    // Else set the length own property of F to 0.
    unsigned length = 0;
    if (targetObject->inherits(JSFunction::info())) {
        ASSERT(target.get(exec, exec->propertyNames().length).isNumber());
        // a. Let L be the length property of Target minus the length of A.
        // b. Set the length own property of F to either 0 or L, whichever is larger.
        unsigned targetLength = (unsigned)target.get(exec, exec->propertyNames().length).asNumber();
        if (targetLength > numBoundArgs)
            length = targetLength - numBoundArgs;
    }

    JSString* name = target.get(exec, exec->propertyNames().name).toString(exec);
    return JSValue::encode(JSBoundFunction::create(vm, globalObject, targetObject, exec->argument(0), boundArgs, length, name->value(exec)));
}

} // namespace JSC
