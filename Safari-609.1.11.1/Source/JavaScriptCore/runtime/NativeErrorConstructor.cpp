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
#include "Interpreter.h"
#include "JSFunction.h"
#include "JSString.h"
#include "NativeErrorPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NativeErrorConstructorBase);

const ClassInfo NativeErrorConstructorBase::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(NativeErrorConstructorBase) };

template<ErrorType errorType>
NativeErrorConstructor<errorType>::NativeErrorConstructor(VM& vm, Structure* structure)
    : NativeErrorConstructorBase(vm, structure, NativeErrorConstructor<errorType>::callNativeErrorConstructor, NativeErrorConstructor<errorType>::constructNativeErrorConstructor)
{
}

void NativeErrorConstructorBase::finishCreation(VM& vm, NativeErrorPrototype* prototype, ErrorType errorType)
{
    Base::finishCreation(vm, errorTypeName(errorType), NameAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(vm, info()));
    
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

template<ErrorType errorType>
EncodedJSValue JSC_HOST_CALL NativeErrorConstructor<errorType>::constructNativeErrorConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = callFrame->argument(0);
    Structure* errorStructure = InternalFunction::createSubclassStructure(globalObject, callFrame->jsCallee(), callFrame->newTarget(), jsCast<NativeErrorConstructor*>(callFrame->jsCallee())->errorStructure(vm));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    ASSERT(errorStructure);
    RELEASE_AND_RETURN(scope, JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, nullptr, TypeNothing, false)));
}

template<ErrorType errorType>
EncodedJSValue JSC_HOST_CALL NativeErrorConstructor<errorType>::callNativeErrorConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    JSValue message = callFrame->argument(0);
    Structure* errorStructure = jsCast<NativeErrorConstructor*>(callFrame->jsCallee())->errorStructure(vm);
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, nullptr, TypeNothing, false));
}

template class NativeErrorConstructor<ErrorType::EvalError>;
template class NativeErrorConstructor<ErrorType::RangeError>;
template class NativeErrorConstructor<ErrorType::ReferenceError>;
template class NativeErrorConstructor<ErrorType::SyntaxError>;
template class NativeErrorConstructor<ErrorType::TypeError>;
template class NativeErrorConstructor<ErrorType::URIError>;

} // namespace JSC
