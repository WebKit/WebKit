/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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

#include "BuiltinNames.h"
#include "ClonedArguments.h"
#include "ExecutableBaseInlines.h"
#include "FunctionExecutable.h"
#include "IntegrityInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(FunctionPrototype);

const ClassInfo FunctionPrototype::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionPrototype) };

static JSC_DECLARE_HOST_FUNCTION(functionProtoFuncToString);
static JSC_DECLARE_HOST_FUNCTION(functionProtoFuncBind);
static JSC_DECLARE_HOST_FUNCTION(callFunctionPrototype);

static JSC_DECLARE_CUSTOM_GETTER(argumentsGetter);
static JSC_DECLARE_CUSTOM_GETTER(callerGetter);
static JSC_DECLARE_CUSTOM_SETTER(callerAndArgumentsSetter);

// https://tc39.es/ecma262/#sec-properties-of-the-function-prototype-object
JSC_DEFINE_HOST_FUNCTION(callFunctionPrototype, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsUndefined());
}

FunctionPrototype::FunctionPrototype(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callFunctionPrototype, nullptr)
{
}

void FunctionPrototype::finishCreation(VM& vm, const String& name)
{
    Base::finishCreation(vm, 0, name, PropertyAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(info()));
}

void FunctionPrototype::addFunctionProperties(VM& vm, JSGlobalObject* globalObject, JSFunction** callFunction, JSFunction** applyFunction, JSFunction** hasInstanceSymbolFunction)
{
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().toStringPublicName(), functionProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, FunctionToStringIntrinsic);

    *applyFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().applyPublicName(), functionPrototypeApplyCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));
    *callFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().callPublicName(), functionPrototypeCallCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->bind, functionProtoFuncBind, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, FunctionBindIntrinsic);

    putDirectCustomGetterSetterWithoutTransition(vm, vm.propertyNames->arguments, CustomGetterSetter::create(vm, argumentsGetter, callerAndArgumentsSetter), PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor);
    putDirectCustomGetterSetterWithoutTransition(vm, vm.propertyNames->caller, CustomGetterSetter::create(vm, callerGetter, callerAndArgumentsSetter), PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor);

    *hasInstanceSymbolFunction = JSFunction::create(vm, functionPrototypeSymbolHasInstanceCodeGenerator(vm), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->hasInstanceSymbol, *hasInstanceSymbolFunction, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

JSC_DEFINE_HOST_FUNCTION(functionProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (thisValue.inherits<JSFunction>()) {
        JSFunction* function = jsCast<JSFunction*>(thisValue);
        Integrity::auditStructureID(function->structureID());
        RELEASE_AND_RETURN(scope, JSValue::encode(function->toString(globalObject)));
    }

    if (thisValue.inherits<InternalFunction>()) {
        InternalFunction* function = jsCast<InternalFunction*>(thisValue);
        Integrity::auditStructureID(function->structureID());
        RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, "function ", function->name(), "() {\n    [native code]\n}")));
    }

    if (thisValue.isObject()) {
        JSObject* object = asObject(thisValue);
        Integrity::auditStructureID(object->structureID());
        if (object->isCallable())
            RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, "function ", object->classInfo()->className, "() {\n    [native code]\n}")));
    }

    return throwVMTypeError(globalObject, scope);
}

JSC_DEFINE_HOST_FUNCTION(functionProtoFuncBind, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (UNLIKELY(!thisValue.isCallable()))
        return throwVMTypeError(globalObject, scope, "|this| is not a function inside Function.prototype.bind"_s);
    JSObject* target = asObject(thisValue);

    JSValue boundThis;
    ArgList boundArgs;
    size_t numBoundArgs;
    if (size_t argCount = callFrame->argumentCount(); argCount > 1) {
        boundThis = callFrame->uncheckedArgument(0);
        boundArgs = ArgList(callFrame, 1);
        numBoundArgs = argCount - 1;
    } else {
        boundThis = callFrame->argument(0);
        boundArgs = ArgList();
        numBoundArgs = 0;
    }

    double length = 0;
    JSString* name = nullptr;
    JSFunction* function = jsDynamicCast<JSFunction*>(target);
    if (LIKELY(function && function->canAssumeNameAndLengthAreOriginal(vm))) {
        // Do nothing! 'length' and 'name' computation are lazily done.
        // And this is totally OK since we know that wrapped functions have canAssumeNameAndLengthAreOriginal condition
        // at the time of creation of JSBoundFunction.
        length = PNaN; // Defer computation.
    } else {
        bool found = target->hasOwnProperty(globalObject, vm.propertyNames->length);
        RETURN_IF_EXCEPTION(scope, { });
        if (found) {
            JSValue lengthValue = target->get(globalObject, vm.propertyNames->length);
            RETURN_IF_EXCEPTION(scope, { });
            if (lengthValue.isNumber()) {
                length = lengthValue.toIntegerOrInfinity(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                if (length > numBoundArgs)
                    length -= numBoundArgs;
                else
                    length = 0;
            }
        }
        JSValue nameValue = target->get(globalObject, vm.propertyNames->name);
        RETURN_IF_EXCEPTION(scope, { });
        if (nameValue.isString())
            name = asString(nameValue);
        else
            name = jsEmptyString(vm);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(JSBoundFunction::create(vm, globalObject, target, boundThis, boundArgs, length, name)));
}

// https://github.com/claudepache/es-legacy-function-reflection/blob/master/spec.md#isallowedreceiverfunctionforcallerandargumentsfunc-expectedrealm (except step 3)
static ALWAYS_INLINE bool isAllowedReceiverFunctionForCallerAndArguments(JSFunction* function)
{
    if (function->isHostOrBuiltinFunction())
        return false;

    FunctionExecutable* executable = function->jsExecutable();
    if (executable->implementationVisibility() != ImplementationVisibility::Public)
        return false;
    return !executable->isInStrictContext() && executable->parseMode() == SourceParseMode::NormalFunctionMode && !executable->isClassConstructorFunction();
}

class RetrieveArgumentsFunctor {
public:
    RetrieveArgumentsFunctor(VM& vm, JSFunction* functionObj)
        : m_vm(vm)
        , m_targetCallee(functionObj)
        , m_result(jsNull())
    {
    }

    JSValue result() const { return m_result; }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        if (!visitor->callee().isCell())
            return IterationStatus::Continue;

        JSCell* callee = visitor->callee().asCell();
        if (callee != m_targetCallee)
            return IterationStatus::Continue;

        m_result = JSValue(visitor->createArguments(m_vm));
        return IterationStatus::Done;
    }

private:
    VM& m_vm;
    JSObject* m_targetCallee;
    mutable JSValue m_result;
};

static JSValue retrieveArguments(VM& vm, CallFrame* callFrame, JSFunction* functionObj)
{
    RetrieveArgumentsFunctor functor(vm, functionObj);
    if (callFrame)
        StackVisitor::visit(callFrame, vm, functor);
    return functor.result();
}

// https://github.com/claudepache/es-legacy-function-reflection/blob/master/spec.md#get-functionprototypearguments
JSC_DEFINE_CUSTOM_GETTER(argumentsGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObj = jsDynamicCast<JSFunction*>(JSValue::decode(thisValue));
    if (!thisObj || !isAllowedReceiverFunctionForCallerAndArguments(thisObj))
        return throwVMTypeError(globalObject, scope, RestrictedPropertyAccessError);

    JSValue result = retrieveArguments(vm, vm.topCallFrame, thisObj);
    EXCEPTION_ASSERT(scope.exception() || result);
    return JSValue::encode(result);
}

class RetrieveCallerFunctionFunctor {
public:
    RetrieveCallerFunctionFunctor(JSFunction* functionObj)
        : m_targetCallee(functionObj)
        , m_hasFoundFrame(false)
        , m_hasSkippedToCallerFrame(false)
        , m_result(jsNull())
    {
    }

    JSValue result() const { return m_result; }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        if (!visitor->callee().isCell())
            return IterationStatus::Continue;

        JSCell* callee = visitor->callee().asCell();

        if (!m_hasFoundFrame && callee != m_targetCallee)
            return IterationStatus::Continue;

        m_hasFoundFrame = true;
        if (!m_hasSkippedToCallerFrame) {
            m_hasSkippedToCallerFrame = true;
            return IterationStatus::Continue;
        }

        if (callee) {
            if (callee->inherits<JSBoundFunction>() || callee->inherits<JSRemoteFunction>() || callee->type() == ProxyObjectType)
                return IterationStatus::Continue;
            if (callee->inherits<JSFunction>()) {
                if (jsCast<JSFunction*>(callee)->executable()->implementationVisibility() != ImplementationVisibility::Public)
                    return IterationStatus::Continue;
            }

            m_result = callee;
        }
        return IterationStatus::Done;
    }

private:
    JSObject* m_targetCallee;
    mutable bool m_hasFoundFrame;
    mutable bool m_hasSkippedToCallerFrame;
    mutable JSValue m_result;
};

static JSValue retrieveCallerFunction(VM& vm, CallFrame* callFrame, JSFunction* functionObj)
{
    RetrieveCallerFunctionFunctor functor(functionObj);
    if (callFrame)
        StackVisitor::visit(callFrame, vm, functor);
    return functor.result();
}

// https://github.com/claudepache/es-legacy-function-reflection/blob/master/spec.md#get-functionprototypecaller
JSC_DEFINE_CUSTOM_GETTER(callerGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObj = jsDynamicCast<JSFunction*>(JSValue::decode(thisValue));
    if (!thisObj || !isAllowedReceiverFunctionForCallerAndArguments(thisObj))
        return throwVMTypeError(globalObject, scope, RestrictedPropertyAccessError);

    JSValue caller = retrieveCallerFunction(vm, vm.topCallFrame, thisObj);
    if (caller.isNull())
        return JSValue::encode(jsNull());

    // 11. If caller is not an ECMAScript function object, return null.
    JSFunction* function = jsDynamicCast<JSFunction*>(caller);
    if (!function || function->isHostOrBuiltinFunction())
        return JSValue::encode(jsNull());

    // 12. If caller.[[Strict]] is true, return null.
    if (function->jsExecutable()->isInStrictContext())
        return JSValue::encode(jsNull());

    // Prevent bodies (private implementations) of generator / async functions from being exposed.
    // They expect to be called by @generatorResume() & friends with certain arguments, and crash otherwise.
    // 14. If caller.[[ECMAScriptCode]] is a GeneratorBody, an AsyncFunctionBody, an AsyncGeneratorBody, or an AsyncConciseBody, return null.
    SourceParseMode parseMode = function->jsExecutable()->parseMode();
    if (isGeneratorParseMode(parseMode) || isAsyncFunctionParseMode(parseMode))
        return JSValue::encode(jsNull());

    return JSValue::encode(caller);
}

JSC_DEFINE_CUSTOM_SETTER(callerAndArgumentsSetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObj = jsDynamicCast<JSFunction*>(JSValue::decode(thisValue));
    if (!thisObj || !isAllowedReceiverFunctionForCallerAndArguments(thisObj))
        throwTypeError(globalObject, scope, RestrictedPropertyAccessError);

    return true;
}

} // namespace JSC
