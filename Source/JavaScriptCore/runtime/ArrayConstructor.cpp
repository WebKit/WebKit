/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2022 Apple Inc. All rights reserved.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "ArrayConstructor.h"

#include "ArrayPrototype.h"
#include "BuiltinNames.h"
#include "ExecutableBaseInlines.h"
#include "JSCInlines.h"
#include "ProxyObject.h"

#include "VMInlines.h"

#include "ArrayConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ArrayConstructor);

const ClassInfo ArrayConstructor::s_info = { "Function"_s, &InternalFunction::s_info, &arrayConstructorTable, nullptr, CREATE_METHOD_TABLE(ArrayConstructor) };

/* Source for ArrayConstructor.lut.h
@begin arrayConstructorTable
  of        JSBuiltin                   DontEnum|Function 0
  from      JSBuiltin                   DontEnum|Function 1
@end
*/

static JSC_DECLARE_HOST_FUNCTION(callArrayConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructWithArrayConstructor);

ArrayConstructor::ArrayConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callArrayConstructor, constructWithArrayConstructor)
{
}

void ArrayConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, ArrayPrototype* arrayPrototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, 1, vm.propertyNames->Array.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, arrayPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, speciesSymbol, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->isArray, arrayConstructorIsArrayCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().fromPrivateName(), arrayConstructorFromCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useArrayFromAsync())
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().fromAsyncPublicName(), arrayConstructorFromAsyncCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// ------------------------------ Functions ---------------------------

JSArray* constructArrayWithSizeQuirk(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, JSValue length, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!length.isNumber())
        RELEASE_AND_RETURN(scope, constructArrayNegativeIndexed(globalObject, profile, &length, 1, newTarget));
    
    uint32_t n = length.toUInt32(globalObject);
    if (n != length.toNumber(globalObject)) {
        throwException(globalObject, scope, createRangeError(globalObject, "Array size is not a small enough positive integer."_s));
        return nullptr;
    }
    RELEASE_AND_RETURN(scope, constructEmptyArray(globalObject, profile, n, newTarget));
}

static inline JSArray* constructArrayWithSizeQuirk(JSGlobalObject* globalObject, const ArgList& args, JSValue newTarget)
{
    // a single numeric argument denotes the array size (!)
    if (args.size() == 1)
        return constructArrayWithSizeQuirk(globalObject, nullptr, args.at(0), newTarget);

    // otherwise the array is constructed with the arguments in it
    return constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), args, newTarget);
}

JSC_DEFINE_HOST_FUNCTION(constructWithArrayConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructArrayWithSizeQuirk(globalObject, args, callFrame->newTarget()));
}

JSC_DEFINE_HOST_FUNCTION(callArrayConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructArrayWithSizeQuirk(globalObject, args, JSValue()));
}

static ALWAYS_INLINE bool isArraySlowInline(JSGlobalObject* globalObject, ProxyObject* proxy)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    while (true) {
        if (UNLIKELY(proxy->isRevoked())) {
            auto* callFrame = vm.topJSCallFrame();
            auto* callee = callFrame && !callFrame->isWasmFrame() ? callFrame->jsCallee() : nullptr;
            ASCIILiteral calleeName = "Array.isArray"_s;
            auto* function = callee ? jsDynamicCast<JSFunction*>(callee) : nullptr;
            // If this function is from a different globalObject than the one passed in above,
            // then this test will fail even if function is Object.prototype.toString. The only
            // way this test will be work everytime is if we check against the
            // Object.prototype.toString of the function's own globalObject.
            if (function && function == function->globalObject()->objectProtoToStringFunctionConcurrently())
                calleeName = "Object.prototype.toString"_s;
            throwTypeError(globalObject, scope, makeString(calleeName, " cannot be called on a Proxy that has been revoked"_s));
            return false;
        }
        JSObject* argument = proxy->target();

        if (argument->type() == ArrayType ||  argument->type() == DerivedArrayType)
            return true;

        if (argument->type() != ProxyObjectType)
            return false;

        proxy = jsCast<ProxyObject*>(argument);
    }

    ASSERT_NOT_REACHED();
}

bool isArraySlow(JSGlobalObject* globalObject, ProxyObject* argument)
{
    return isArraySlowInline(globalObject, argument);
}

// ES6 7.2.2
// https://tc39.github.io/ecma262/#sec-isarray
JSC_DEFINE_HOST_FUNCTION(arrayConstructorPrivateFuncIsArraySlow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT_UNUSED(globalObject, jsDynamicCast<ProxyObject*>(callFrame->argument(0)));
    return JSValue::encode(jsBoolean(isArraySlowInline(globalObject, jsCast<ProxyObject*>(callFrame->uncheckedArgument(0)))));
}

} // namespace JSC
