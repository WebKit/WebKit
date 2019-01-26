/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "Error.h"
#include "InternalFunction.h"
#include "NativeErrorPrototype.h"

namespace JSC {

class ErrorInstance;
class FunctionPrototype;
class NativeErrorPrototype;

class NativeErrorConstructorBase : public InternalFunction {
public:
    using Base = InternalFunction;

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

protected:
    NativeErrorConstructorBase(VM& vm, Structure* structure, NativeFunction functionForCall, NativeFunction functionForConstruct)
        : InternalFunction(vm, structure, functionForCall, functionForConstruct)
    {
    }

    void finishCreation(VM&, NativeErrorPrototype*, ErrorType);
};

template<ErrorType errorType>
class NativeErrorConstructor final : public NativeErrorConstructorBase {
public:
    static NativeErrorConstructor* create(VM& vm, Structure* structure, NativeErrorPrototype* prototype)
    {
        NativeErrorConstructor* constructor = new (NotNull, allocateCell<NativeErrorConstructor>(vm.heap)) NativeErrorConstructor(vm, structure);
        constructor->finishCreation(vm, prototype, errorType);
        return constructor;
    }

    Structure* errorStructure(VM& vm) { return globalObject(vm)->errorStructure(errorType); }
private:
    static EncodedJSValue JSC_HOST_CALL callNativeErrorConstructor(ExecState*);
    static EncodedJSValue JSC_HOST_CALL constructNativeErrorConstructor(ExecState*);

    NativeErrorConstructor(VM&, Structure*);
};

using EvalErrorConstructor = NativeErrorConstructor<ErrorType::EvalError>;
using RangeErrorConstructor = NativeErrorConstructor<ErrorType::RangeError>;
using ReferenceErrorConstructor = NativeErrorConstructor<ErrorType::ReferenceError>;
using SyntaxErrorConstructor = NativeErrorConstructor<ErrorType::SyntaxError>;
using TypeErrorConstructor = NativeErrorConstructor<ErrorType::TypeError>;
using URIErrorConstructor = NativeErrorConstructor<ErrorType::URIError>;

static_assert(sizeof(EvalErrorConstructor) == sizeof(InternalFunction), "");
static_assert(sizeof(RangeErrorConstructor) == sizeof(InternalFunction), "");
static_assert(sizeof(ReferenceErrorConstructor) == sizeof(InternalFunction), "");
static_assert(sizeof(SyntaxErrorConstructor) == sizeof(InternalFunction), "");
static_assert(sizeof(TypeErrorConstructor) == sizeof(InternalFunction), "");
static_assert(sizeof(URIErrorConstructor) == sizeof(InternalFunction), "");

} // namespace JSC
