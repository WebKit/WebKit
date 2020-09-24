/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008, 2016 Apple Inc. All rights reserved.
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
#include "NativeErrorConstructor.h"

#include "ErrorInstance.h"
#include "JSCInlines.h"
#include "NativeErrorPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NativeErrorConstructorBase);

const ClassInfo NativeErrorConstructorBase::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(NativeErrorConstructorBase) };

template<ErrorType errorType>
inline EncodedJSValue NativeErrorConstructor<errorType>::constructImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = callFrame->argument(0);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* errorStructure = newTarget == callFrame->jsCallee()
        ? globalObject->errorStructure(errorType)
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->errorStructure(errorType));
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(errorStructure);

    RELEASE_AND_RETURN(scope, JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, nullptr, TypeNothing, false)));
}

template<ErrorType errorType>
inline EncodedJSValue NativeErrorConstructor<errorType>::callImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue message = callFrame->argument(0);
    Structure* errorStructure = globalObject->errorStructure(errorType);
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, nullptr, TypeNothing, false));
}

static EncodedJSValue JSC_HOST_CALL callEvalError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::EvalError>::callImpl(globalObject, callFrame);
}
static EncodedJSValue JSC_HOST_CALL constructEvalError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::EvalError>::constructImpl(globalObject, callFrame);
}

static EncodedJSValue JSC_HOST_CALL callRangeError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::RangeError>::callImpl(globalObject, callFrame);
}
static EncodedJSValue JSC_HOST_CALL constructRangeError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::RangeError>::constructImpl(globalObject, callFrame);
}

static EncodedJSValue JSC_HOST_CALL callReferenceError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::ReferenceError>::callImpl(globalObject, callFrame);
}
static EncodedJSValue JSC_HOST_CALL constructReferenceError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::ReferenceError>::constructImpl(globalObject, callFrame);
}

static EncodedJSValue JSC_HOST_CALL callSyntaxError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::SyntaxError>::callImpl(globalObject, callFrame);
}
static EncodedJSValue JSC_HOST_CALL constructSyntaxError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::SyntaxError>::constructImpl(globalObject, callFrame);
}

static EncodedJSValue JSC_HOST_CALL callTypeError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::TypeError>::callImpl(globalObject, callFrame);
}
static EncodedJSValue JSC_HOST_CALL constructTypeError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::TypeError>::constructImpl(globalObject, callFrame);
}

static EncodedJSValue JSC_HOST_CALL callURIError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::URIError>::callImpl(globalObject, callFrame);
}
static EncodedJSValue JSC_HOST_CALL constructURIError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return NativeErrorConstructor<ErrorType::URIError>::constructImpl(globalObject, callFrame);
}

static constexpr auto callFunction(ErrorType errorType) -> decltype(&callEvalError)
{
    switch (errorType) {
    case ErrorType::EvalError: return callEvalError;
    case ErrorType::RangeError: return callRangeError;
    case ErrorType::ReferenceError: return callReferenceError;
    case ErrorType::SyntaxError: return callSyntaxError;
    case ErrorType::TypeError: return callTypeError;
    case ErrorType::URIError: return callURIError;
    default: return nullptr;
    }
}

static constexpr auto constructFunction(ErrorType errorType) -> decltype(&constructEvalError)
{
    switch (errorType) {
    case ErrorType::EvalError: return constructEvalError;
    case ErrorType::RangeError: return constructRangeError;
    case ErrorType::ReferenceError: return constructReferenceError;
    case ErrorType::SyntaxError: return constructSyntaxError;
    case ErrorType::TypeError: return constructTypeError;
    case ErrorType::URIError: return constructURIError;
    default: return nullptr;
    }
}

template<ErrorType errorType>
NativeErrorConstructor<errorType>::NativeErrorConstructor(VM& vm, Structure* structure)
    : NativeErrorConstructorBase(vm, structure, callFunction(errorType), constructFunction(errorType))
{
}

void NativeErrorConstructorBase::finishCreation(VM& vm, NativeErrorPrototype* prototype, ErrorType errorType)
{
    Base::finishCreation(vm, 1, errorTypeName(errorType), PropertyAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(vm, info()));
    
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

template class NativeErrorConstructor<ErrorType::EvalError>;
template class NativeErrorConstructor<ErrorType::RangeError>;
template class NativeErrorConstructor<ErrorType::ReferenceError>;
template class NativeErrorConstructor<ErrorType::SyntaxError>;
template class NativeErrorConstructor<ErrorType::TypeError>;
template class NativeErrorConstructor<ErrorType::URIError>;

} // namespace JSC
