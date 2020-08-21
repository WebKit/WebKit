/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *  Copyright (C) 2015 Canon Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "JSFunction.h"

#include "AsyncGeneratorPrototype.h"
#include "BuiltinNames.h"
#include "CatchScope.h"
#include "ClonedArguments.h"
#include "CommonIdentifiers.h"
#include "CallFrame.h"
#include "GeneratorPrototype.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSToWasmICCallee.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "StackVisitor.h"
#include "TypeError.h"

namespace JSC {

EncodedJSValue JSC_HOST_CALL callHostFunctionAsConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMError(globalObject, scope, createNotAConstructorError(globalObject, callFrame->jsCallee()));
}

const ClassInfo JSFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFunction) };
const ClassInfo JSStrictFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSStrictFunction) };
const ClassInfo JSSloppyFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSSloppyFunction) };
const ClassInfo JSArrowFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSArrowFunction) };

bool JSFunction::isHostFunctionNonInline() const
{
    return isHostFunction();
}

Structure* JSFunction::selectStructureForNewFuncExp(JSGlobalObject* globalObject, FunctionExecutable* executable)
{
    ASSERT(!executable->isHostFunction());
    bool isBuiltin = executable->isBuiltinFunction();
    if (executable->isArrowFunction())
        return globalObject->arrowFunctionStructure(isBuiltin);
    if (executable->isInStrictContext())
        return globalObject->strictFunctionStructure(isBuiltin);
    return globalObject->sloppyFunctionStructure(isBuiltin);
}

JSFunction* JSFunction::create(VM& vm, FunctionExecutable* executable, JSScope* scope)
{
    return create(vm, executable, scope, selectStructureForNewFuncExp(scope->globalObject(vm), executable));
}

JSFunction* JSFunction::create(VM& vm, FunctionExecutable* executable, JSScope* scope, Structure* structure)
{
    JSFunction* result = createImpl(vm, executable, scope, structure);
    executable->notifyCreation(vm, result, "Allocating a function");
    return result;
}

JSFunction* JSFunction::create(VM& vm, JSGlobalObject* globalObject, int length, const String& name, NativeFunction nativeFunction, Intrinsic intrinsic, NativeFunction nativeConstructor, const DOMJIT::Signature* signature)
{
    NativeExecutable* executable = vm.getHostFunction(nativeFunction, intrinsic, nativeConstructor, signature, name);
    Structure* structure = globalObject->hostFunctionStructure();
    JSFunction* function = new (NotNull, allocateCell<JSFunction>(vm.heap)) JSFunction(vm, executable, globalObject, structure);
    // Can't do this during initialization because getHostFunction might do a GC allocation.
    function->finishCreation(vm, executable, length, name);
    return function;
}

JSFunction::JSFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure)
    : Base(vm, globalObject, structure)
    , m_executableOrRareData(bitwise_cast<uintptr_t>(executable))
{
    assertTypeInfoFlagInvariants();
    ASSERT(structure->globalObject() == globalObject);
}


void JSFunction::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(jsDynamicCast<JSFunction*>(vm, this));
    ASSERT(type() == JSFunctionType);
    ASSERT(methodTable(vm)->getConstructData == &JSFunction::getConstructData);
    ASSERT(methodTable(vm)->getCallData == &JSFunction::getCallData);
}

void JSFunction::finishCreation(VM& vm, NativeExecutable*, int length, const String& name)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    ASSERT(type() == JSFunctionType);
    ASSERT(methodTable(vm)->getConstructData == &JSFunction::getConstructData);
    ASSERT(methodTable(vm)->getCallData == &JSFunction::getCallData);

    // JSBoundFunction instances use finishCreation(VM&) overload and lazily allocate their name string / length.
    ASSERT(!this->inherits<JSBoundFunction>(vm));

    if (!name.isNull())
        putDirect(vm, vm.propertyNames->name, jsString(vm, name), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(length), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

FunctionRareData* JSFunction::allocateRareData(VM& vm)
{
    uintptr_t executableOrRareData = m_executableOrRareData;
    ASSERT(!(executableOrRareData & rareDataTag));
    FunctionRareData* rareData = FunctionRareData::create(vm, bitwise_cast<ExecutableBase*>(executableOrRareData));
    executableOrRareData = bitwise_cast<uintptr_t>(rareData) | rareDataTag;

    // A DFG compilation thread may be trying to read the rare data
    // We want to ensure that it sees it properly allocated
    WTF::storeStoreFence();

    m_executableOrRareData = executableOrRareData;
    vm.heap.writeBarrier(this, rareData);

    return rareData;
}

JSObject* JSFunction::prototypeForConstruction(VM& vm, JSGlobalObject* globalObject)
{
    // This code assumes getting the prototype is not effectful. That's only
    // true when we can use the allocation profile.
    ASSERT(canUseAllocationProfile()); 
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSValue prototype = get(globalObject, vm.propertyNames->prototype);
    scope.releaseAssertNoException();
    if (LIKELY(prototype.isObject()))
        return asObject(prototype);
    if (isHostOrBuiltinFunction())
        return this->globalObject()->objectPrototype();

    JSGlobalObject* scopeGlobalObject = this->scope()->globalObject();
    // https://tc39.github.io/ecma262/#sec-generator-function-definitions-runtime-semantics-evaluatebody
    if (isGeneratorWrapperParseMode(jsExecutable()->parseMode()))
        return scopeGlobalObject->generatorPrototype();
    // https://tc39.github.io/ecma262/#sec-asyncgenerator-definitions-evaluatebody
    if (isAsyncGeneratorWrapperParseMode(jsExecutable()->parseMode()))
        return scopeGlobalObject->asyncGeneratorPrototype();
    return scopeGlobalObject->objectPrototype();
}

FunctionRareData* JSFunction::allocateAndInitializeRareData(JSGlobalObject* globalObject, size_t inlineCapacity)
{
    uintptr_t executableOrRareData = m_executableOrRareData;
    ASSERT(!(executableOrRareData & rareDataTag));
    ASSERT(canUseAllocationProfile());
    VM& vm = globalObject->vm();
    JSObject* prototype = prototypeForConstruction(vm, globalObject);
    FunctionRareData* rareData = FunctionRareData::create(vm, bitwise_cast<ExecutableBase*>(executableOrRareData));
    rareData->initializeObjectAllocationProfile(vm, this->globalObject(), prototype, inlineCapacity, this);
    executableOrRareData = bitwise_cast<uintptr_t>(rareData) | rareDataTag;

    // A DFG compilation thread may be trying to read the rare data
    // We want to ensure that it sees it properly allocated
    WTF::storeStoreFence();

    m_executableOrRareData = executableOrRareData;
    vm.heap.writeBarrier(this, rareData);

    return rareData;
}

FunctionRareData* JSFunction::initializeRareData(JSGlobalObject* globalObject, size_t inlineCapacity)
{
    uintptr_t executableOrRareData = m_executableOrRareData;
    ASSERT(executableOrRareData & rareDataTag);
    ASSERT(canUseAllocationProfile());
    VM& vm = globalObject->vm();
    JSObject* prototype = prototypeForConstruction(vm, globalObject);
    FunctionRareData* rareData = bitwise_cast<FunctionRareData*>(executableOrRareData & ~rareDataTag);
    rareData->initializeObjectAllocationProfile(vm, this->globalObject(), prototype, inlineCapacity, this);
    return rareData;
}

String JSFunction::name(VM& vm)
{
    if (isHostFunction()) {
        if (this->inherits<JSBoundFunction>(vm))
            return jsCast<JSBoundFunction*>(this)->nameString();
        NativeExecutable* executable = jsCast<NativeExecutable*>(this->executable());
        return executable->name();
    }
    const Identifier identifier = jsExecutable()->name();
    if (identifier == vm.propertyNames->builtinNames().starDefaultPrivateName())
        return emptyString();
    return identifier.string();
}

String JSFunction::displayName(VM& vm)
{
    JSValue displayName = getDirect(vm, vm.propertyNames->displayName);
    
    if (displayName && isJSString(displayName))
        return asString(displayName)->tryGetValue();
    
    return String();
}

const String JSFunction::calculatedDisplayName(VM& vm)
{
    const String explicitName = displayName(vm);
    
    if (!explicitName.isEmpty())
        return explicitName;
    
    const String actualName = name(vm);
    if (!actualName.isEmpty() || isHostOrBuiltinFunction())
        return actualName;

    return jsExecutable()->ecmaName().string();
}

const SourceCode* JSFunction::sourceCode() const
{
    if (isHostOrBuiltinFunction())
        return nullptr;
    return &jsExecutable()->source();
}
    
void JSFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.appendUnbarriered(bitwise_cast<JSCell*>(bitwise_cast<uintptr_t>(thisObject->m_executableOrRareData) & ~rareDataTag));
}

CallData JSFunction::getCallData(JSCell* cell)
{
    CallData callData;

    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostFunction()) {
        callData.type = CallData::Type::Native;
        callData.native.function = thisObject->nativeFunction();
    } else {
        callData.type = CallData::Type::JS;
        callData.js.functionExecutable = thisObject->jsExecutable();
        callData.js.scope = thisObject->scope();
    }

    return callData;
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

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (!visitor->callee().isCell())
            return StackVisitor::Continue;

        JSCell* callee = visitor->callee().asCell();
        if (callee != m_targetCallee)
            return StackVisitor::Continue;

        m_result = JSValue(visitor->createArguments(m_vm));
        return StackVisitor::Done;
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
        callFrame->iterate(vm, functor);
    return functor.result();
}

EncodedJSValue JSFunction::argumentsGetter(JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = globalObject->vm();
    JSFunction* thisObj = jsCast<JSFunction*>(JSValue::decode(thisValue));
    ASSERT(!thisObj->isHostFunction());

    return JSValue::encode(retrieveArguments(vm, vm.topCallFrame, thisObj));
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

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (!visitor->callee().isCell())
            return StackVisitor::Continue;

        JSCell* callee = visitor->callee().asCell();

        if (callee && callee->inherits<JSBoundFunction>(callee->vm()))
            return StackVisitor::Continue;

        if (!m_hasFoundFrame && (callee != m_targetCallee))
            return StackVisitor::Continue;

        m_hasFoundFrame = true;
        if (!m_hasSkippedToCallerFrame) {
            m_hasSkippedToCallerFrame = true;
            return StackVisitor::Continue;
        }

        if (callee)
            m_result = callee;
        return StackVisitor::Done;
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
        callFrame->iterate(vm, functor);
    return functor.result();
}

EncodedJSValue JSFunction::callerGetter(JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObj = jsCast<JSFunction*>(JSValue::decode(thisValue));
    ASSERT(!thisObj->isHostFunction());
    JSValue caller = retrieveCallerFunction(vm, vm.topCallFrame, thisObj);

    // See ES5.1 15.3.5.4 - Function.caller may not be used to retrieve a strict caller.
    if (!caller.isObject() || !asObject(caller)->inherits<JSFunction>(vm)) {
        // It isn't a JSFunction, but if it is a JSCallee from a program or eval call or an internal constructor, return null.
        if (jsDynamicCast<JSCallee*>(vm, caller) || jsDynamicCast<InternalFunction*>(vm, caller))
            return JSValue::encode(jsNull());
        return JSValue::encode(caller);
    }
    JSFunction* function = jsCast<JSFunction*>(caller);

    // Firefox returns null for native code callers, so we match that behavior.
    if (function->isHostOrBuiltinFunction())
        return JSValue::encode(jsNull());
    SourceParseMode parseMode = function->jsExecutable()->parseMode();
    switch (parseMode) {
    case SourceParseMode::GeneratorBodyMode:
    case SourceParseMode::AsyncGeneratorBodyMode:
        return JSValue::encode(throwTypeError(globalObject, scope, "Function.caller used to retrieve generator body"_s));
    case SourceParseMode::AsyncFunctionBodyMode:
    case SourceParseMode::AsyncArrowFunctionBodyMode:
        return JSValue::encode(throwTypeError(globalObject, scope, "Function.caller used to retrieve async function body"_s));
    case SourceParseMode::NormalFunctionMode:
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GetterMode:
    case SourceParseMode::SetterMode:
    case SourceParseMode::MethodMode:
    case SourceParseMode::ArrowFunctionMode:
    case SourceParseMode::AsyncFunctionMode:
    case SourceParseMode::AsyncMethodMode:
    case SourceParseMode::AsyncArrowFunctionMode:
    case SourceParseMode::ProgramMode:
    case SourceParseMode::ModuleAnalyzeMode:
    case SourceParseMode::ModuleEvaluateMode:
    case SourceParseMode::AsyncGeneratorWrapperFunctionMode:
    case SourceParseMode::AsyncGeneratorWrapperMethodMode:
    case SourceParseMode::GeneratorWrapperMethodMode:
    case SourceParseMode::InstanceFieldInitializerMode:
        if (!function->jsExecutable()->isInStrictContext())
            return JSValue::encode(caller);
        return JSValue::encode(throwTypeError(globalObject, scope, "Function.caller used to retrieve strict caller"_s));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool JSFunction::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObject = jsCast<JSFunction*>(object);
    if (thisObject->isHostOrBuiltinFunction()) {
        thisObject->reifyLazyPropertyForHostOrBuiltinIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        RELEASE_AND_RETURN(scope, Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot));
    }

    if (propertyName == vm.propertyNames->prototype && thisObject->jsExecutable()->hasPrototypeProperty() && !thisObject->jsExecutable()->isClassConstructorFunction()) {
        // NOTE: class constructors define the prototype property in bytecode using
        // defineOwnProperty, which ends up calling into this code (see our defineOwnProperty
        // implementation below). The bytecode will end up doing the proper definition
        // with the property being non-writable/non-configurable. However, we must ignore
        // the initial materialization of the property so that the defineOwnProperty call
        // from bytecode succeeds. Otherwise, the materialization here would prevent the
        // defineOwnProperty from being able to overwrite the property.
        unsigned attributes;
        PropertyOffset offset = thisObject->getDirectOffset(vm, propertyName, attributes);
        if (!isValidOffset(offset)) {
            JSGlobalObject* scopeGlobalObject = thisObject->scope()->globalObject();
            JSObject* prototype = nullptr;
            if (isGeneratorWrapperParseMode(thisObject->jsExecutable()->parseMode())) {
                // Unlike function instances, the object that is the value of the a GeneratorFunction's prototype
                // property does not have a constructor property whose value is the GeneratorFunction instance.
                // https://tc39.github.io/ecma262/#sec-generatorfunction-instances-prototype
                prototype = constructEmptyObject(globalObject, scopeGlobalObject->generatorPrototype());
            } else if (isAsyncGeneratorWrapperParseMode(thisObject->jsExecutable()->parseMode()))
                prototype = constructEmptyObject(globalObject, scopeGlobalObject->asyncGeneratorPrototype());
            else {
                prototype = constructEmptyObject(globalObject, scopeGlobalObject->objectPrototype());
                prototype->putDirect(vm, vm.propertyNames->constructor, thisObject, static_cast<unsigned>(PropertyAttribute::DontEnum));
            }

            thisObject->putDirect(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum);
            offset = thisObject->getDirectOffset(vm, vm.propertyNames->prototype, attributes);
            ASSERT(isValidOffset(offset));
        }
        slot.setValue(thisObject, attributes, thisObject->getDirect(offset), offset);
    }

    if (propertyName == vm.propertyNames->arguments) {
        if (!thisObject->jsExecutable()->hasCallerAndArgumentsProperties())
            RELEASE_AND_RETURN(scope, Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot));
        
        slot.setCacheableCustom(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete, argumentsGetter);
        return true;

    } else if (propertyName == vm.propertyNames->caller) {
        if (!thisObject->jsExecutable()->hasCallerAndArgumentsProperties())
            RELEASE_AND_RETURN(scope, Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot));

        slot.setCacheableCustom(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete, callerGetter);
        return true;
    }

    thisObject->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
    RETURN_IF_EXCEPTION(scope, false);

    RELEASE_AND_RETURN(scope, Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot));
}

void JSFunction::getOwnNonIndexPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSFunction* thisObject = jsCast<JSFunction*>(object);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (mode.includeDontEnumProperties()) {
        if (!thisObject->isHostOrBuiltinFunction()) {
            // Make sure prototype has been reified.
            PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry, &vm);
            thisObject->methodTable(vm)->getOwnPropertySlot(thisObject, globalObject, vm.propertyNames->prototype, slot);
            RETURN_IF_EXCEPTION(scope, void());

            if (thisObject->jsExecutable()->hasCallerAndArgumentsProperties()) {
                propertyNames.add(vm.propertyNames->arguments);
                propertyNames.add(vm.propertyNames->caller);
            }
            if (!thisObject->hasReifiedLength())
                propertyNames.add(vm.propertyNames->length);
            if (!thisObject->hasReifiedName())
                propertyNames.add(vm.propertyNames->name);
        } else {
            if (thisObject->isBuiltinFunction() || thisObject->inherits<JSBoundFunction>(vm)) {
                if (!thisObject->hasReifiedLength())
                    propertyNames.add(vm.propertyNames->length);
                if (!thisObject->hasReifiedName())
                    propertyNames.add(vm.propertyNames->name);
            }
        }
    }
    RELEASE_AND_RETURN(scope, Base::getOwnNonIndexPropertyNames(thisObject, globalObject, propertyNames, mode));
}

bool JSFunction::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObject = jsCast<JSFunction*>(cell);

    if (propertyName == vm.propertyNames->length || propertyName == vm.propertyNames->name) {
        FunctionRareData* rareData = thisObject->ensureRareData(vm);
        if (propertyName == vm.propertyNames->length)
            rareData->setHasModifiedLengthForNonHostFunction();
        else
            rareData->setHasModifiedNameForNonHostFunction();
    }

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        RELEASE_AND_RETURN(scope, ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode()));


    if (thisObject->isHostOrBuiltinFunction()) {
        PropertyStatus propertyType = thisObject->reifyLazyPropertyForHostOrBuiltinIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        if (isLazy(propertyType))
            slot.disableCaching();
        RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
    }

    if (propertyName == vm.propertyNames->prototype) {
        slot.disableCaching();
        // Make sure prototype has been reified, such that it can only be overwritten
        // following the rules set out in ECMA-262 8.12.9.
        PropertySlot getSlot(thisObject, PropertySlot::InternalMethodType::VMInquiry, &vm);
        thisObject->methodTable(vm)->getOwnPropertySlot(thisObject, globalObject, propertyName, getSlot);
        RETURN_IF_EXCEPTION(scope, false);
        if (FunctionRareData* rareData = thisObject->rareData())
            rareData->clear("Store to prototype property of a function");
        getSlot.disallowVMEntry.reset();
        RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
    }

    if (propertyName == vm.propertyNames->arguments || propertyName == vm.propertyNames->caller) {
        if (!thisObject->jsExecutable()->hasCallerAndArgumentsProperties())
            RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));

        slot.disableCaching();
        return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
    }
    PropertyStatus propertyType = thisObject->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
    if (isLazy(propertyType))
        slot.disableCaching();
    RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
}

bool JSFunction::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSFunction* thisObject = jsCast<JSFunction*>(cell);

    if (propertyName == vm.propertyNames->length || propertyName == vm.propertyNames->name) {
        FunctionRareData* rareData = thisObject->ensureRareData(vm);
        if (propertyName == vm.propertyNames->length)
            rareData->setHasModifiedLengthForNonHostFunction();
        else
            rareData->setHasModifiedNameForNonHostFunction();
    }

    if (thisObject->isHostOrBuiltinFunction()) {
        thisObject->reifyLazyPropertyForHostOrBuiltinIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
    } else if (vm.deletePropertyMode() != VM::DeletePropertyMode::IgnoreConfigurable) {
        // For non-host functions, don't let these properties by deleted - except by DefineOwnProperty.
        FunctionExecutable* executable = thisObject->jsExecutable();
        
        if ((propertyName == vm.propertyNames->caller || propertyName == vm.propertyNames->arguments) && executable->hasCallerAndArgumentsProperties())
            return false;

        if (propertyName == vm.propertyNames->prototype && !executable->isArrowFunction())
            return false;

        thisObject->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
    }
    
    RELEASE_AND_RETURN(scope, Base::deleteProperty(thisObject, globalObject, propertyName, slot));
}

bool JSFunction::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* thisObject = jsCast<JSFunction*>(object);

    if (propertyName == vm.propertyNames->length || propertyName == vm.propertyNames->name) {
        FunctionRareData* rareData = thisObject->ensureRareData(vm);
        if (propertyName == vm.propertyNames->length)
            rareData->setHasModifiedLengthForNonHostFunction();
        else
            rareData->setHasModifiedNameForNonHostFunction();
    }

    if (thisObject->isHostOrBuiltinFunction()) {
        thisObject->reifyLazyPropertyForHostOrBuiltinIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, throwException));
    }

    if (propertyName == vm.propertyNames->prototype) {
        // Make sure prototype has been reified, such that it can only be overwritten
        // following the rules set out in ECMA-262 8.12.9.
        PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry, &vm);
        thisObject->methodTable(vm)->getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, false);
        if (FunctionRareData* rareData = thisObject->rareData())
            rareData->clear("Store to prototype property of a function");
        slot.disallowVMEntry.reset();
        RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, throwException));
    }

    bool valueCheck;
    if (propertyName == vm.propertyNames->arguments) {
        if (!thisObject->jsExecutable()->hasCallerAndArgumentsProperties())
            RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, throwException));

        valueCheck = !descriptor.value();
        if (!valueCheck) {
            valueCheck = sameValue(globalObject, descriptor.value(), retrieveArguments(vm, vm.topCallFrame, thisObject));
            RETURN_IF_EXCEPTION(scope, false);
        }
    } else if (propertyName == vm.propertyNames->caller) {
        if (!thisObject->jsExecutable()->hasCallerAndArgumentsProperties())
            RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, throwException));

        valueCheck = !descriptor.value();
        if (!valueCheck) {
            valueCheck = sameValue(globalObject, descriptor.value(), retrieveCallerFunction(vm, vm.topCallFrame, thisObject));
            RETURN_IF_EXCEPTION(scope, false);
        }
    } else {
        thisObject->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, throwException));
    }
     
    if (descriptor.configurablePresent() && descriptor.configurable())
        return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeConfigurabilityError);
    if (descriptor.enumerablePresent() && descriptor.enumerable())
        return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeEnumerabilityError);
    if (descriptor.isAccessorDescriptor())
        return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeAccessMechanismError);
    if (descriptor.writablePresent() && descriptor.writable())
        return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeWritabilityError);
    if (!valueCheck)
        return typeError(globalObject, scope, throwException, ReadonlyPropertyChangeError);
    return true;
}

// ECMA 13.2.2 [[Construct]]
CallData JSFunction::getConstructData(JSCell* cell)
{
    CallData constructData;

    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    if (thisObject->isHostFunction()) {
        if (thisObject->nativeConstructor() != callHostFunctionAsConstructor) {
            constructData.type = CallData::Type::Native;
            constructData.native.function = thisObject->nativeConstructor();
        }
    } else {
        FunctionExecutable* functionExecutable = thisObject->jsExecutable();
        if (functionExecutable->constructAbility() != ConstructAbility::CannotConstruct) {
            constructData.type = CallData::Type::JS;
            constructData.js.functionExecutable = functionExecutable;
            constructData.js.scope = thisObject->scope();
        }
    }

    return constructData;
}

String getCalculatedDisplayName(VM& vm, JSObject* object)
{
#if ENABLE(WEBASSEMBLY)
    if (jsDynamicCast<JSToWasmICCallee*>(vm, object))
        return "wasm-stub"_s;
#endif

    if (!jsDynamicCast<JSFunction*>(vm, object) && !jsDynamicCast<InternalFunction*>(vm, object))
        return emptyString();

    Structure* structure = object->structure(vm);
    unsigned attributes;
    // This function may be called when the mutator isn't running and we are lazily generating a stack trace.
    PropertyOffset offset = structure->getConcurrently(vm.propertyNames->displayName.impl(), attributes);
    if (offset != invalidOffset && !(attributes & (PropertyAttribute::Accessor | PropertyAttribute::CustomAccessorOrValue))) {
        JSValue displayName = object->getDirect(offset);
        if (displayName && displayName.isString())
            return asString(displayName)->tryGetValue();
    }

    if (auto* function = jsDynamicCast<JSFunction*>(vm, object)) {
        const String actualName = function->name(vm);
        if (!actualName.isEmpty() || function->isHostOrBuiltinFunction())
            return actualName;

        return function->jsExecutable()->ecmaName().string();
    }
    if (auto* function = jsDynamicCast<InternalFunction*>(vm, object))
        return function->name();


    return emptyString();
}

void JSFunction::setFunctionName(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // The "name" property may have been already been defined as part of a property list in an
    // object literal (and therefore reified).
    if (hasReifiedName())
        return;

    ASSERT(!isHostFunction());
    ASSERT(jsExecutable()->ecmaName().isNull());
    String name;
    if (value.isSymbol()) {
        PrivateName privateName = asSymbol(value)->privateName();
        SymbolImpl& uid = privateName.uid();
        if (uid.isNullSymbol())
            name = emptyString();
        else
            name = makeString('[', String(&uid), ']');
    } else {
        ASSERT(value.isString());
        name = asString(value)->value(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
    }
    reifyName(vm, globalObject, name);
}

void JSFunction::reifyLength(VM& vm)
{
    FunctionRareData* rareData = this->ensureRareData(vm);

    ASSERT(!hasReifiedLength());
    double length = 0;
    if (this->inherits<JSBoundFunction>(vm))
        length = jsCast<JSBoundFunction*>(this)->length(vm);
    else {
        ASSERT(!isHostFunction());
        length = jsExecutable()->parameterCount();
    }
    JSValue initialValue = jsNumber(length);
    unsigned initialAttributes = PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly;
    const Identifier& identifier = vm.propertyNames->length;
    rareData->setHasReifiedLength();
    putDirect(vm, identifier, initialValue, initialAttributes);
}

void JSFunction::reifyName(VM& vm, JSGlobalObject* globalObject)
{
    const Identifier& ecmaName = jsExecutable()->ecmaName();
    String name;
    // https://tc39.github.io/ecma262/#sec-exports-runtime-semantics-evaluation
    // When the ident is "*default*", we need to set "default" for the ecma name.
    // This "*default*" name is never shown to users.
    if (ecmaName == vm.propertyNames->builtinNames().starDefaultPrivateName())
        name = vm.propertyNames->defaultKeyword.string();
    else
        name = ecmaName.string();
    reifyName(vm, globalObject, name);
}

void JSFunction::reifyName(VM& vm, JSGlobalObject* globalObject, String name)
{
    FunctionRareData* rareData = this->ensureRareData(vm);

    ASSERT(!hasReifiedName());
    ASSERT(!isHostFunction());
    unsigned initialAttributes = PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly;
    const Identifier& propID = vm.propertyNames->name;

    if (globalObject->needsSiteSpecificQuirks()) {
        auto illegalCharMatcher = [] (UChar ch) -> bool {
            return ch == ' ' || ch == '|';
        };
        if (name.find(illegalCharMatcher) != notFound)
            name = String();
    }
    
    if (jsExecutable()->isGetter())
        name = makeString("get ", name);
    else if (jsExecutable()->isSetter())
        name = makeString("set ", name);

    rareData->setHasReifiedName();
    putDirect(vm, propID, jsString(vm, name), initialAttributes);
}

JSFunction::PropertyStatus JSFunction::reifyLazyPropertyIfNeeded(VM& vm, JSGlobalObject* globalObject, PropertyName propertyName)
{
    if (isHostOrBuiltinFunction() && !this->inherits<JSBoundFunction>(vm))
        return PropertyStatus::Eager;
    PropertyStatus lazyLength = reifyLazyLengthIfNeeded(vm, globalObject, propertyName);
    if (isLazy(lazyLength))
        return lazyLength;
    PropertyStatus lazyName = reifyLazyNameIfNeeded(vm, globalObject, propertyName);
    if (isLazy(lazyName))
        return lazyName;
    return PropertyStatus::Eager;
}

JSFunction::PropertyStatus JSFunction::reifyLazyPropertyForHostOrBuiltinIfNeeded(VM& vm, JSGlobalObject* globalObject, PropertyName propertyName)
{
    ASSERT(isHostOrBuiltinFunction());
    if (isBuiltinFunction() || this->inherits<JSBoundFunction>(vm)) {
        PropertyStatus lazyLength = reifyLazyLengthIfNeeded(vm, globalObject, propertyName);
        if (isLazy(lazyLength))
            return lazyLength;
    }
    return reifyLazyBoundNameIfNeeded(vm, globalObject, propertyName);
}

JSFunction::PropertyStatus JSFunction::reifyLazyLengthIfNeeded(VM& vm, JSGlobalObject*, PropertyName propertyName)
{
    if (propertyName == vm.propertyNames->length) {
        if (!hasReifiedLength()) {
            reifyLength(vm);
            return PropertyStatus::Reified;
        }
        return PropertyStatus::Lazy;
    }
    return PropertyStatus::Eager;
}

JSFunction::PropertyStatus JSFunction::reifyLazyNameIfNeeded(VM& vm, JSGlobalObject* globalObject, PropertyName propertyName)
{
    if (propertyName == vm.propertyNames->name) {
        if (!hasReifiedName()) {
            reifyName(vm, globalObject);
            return PropertyStatus::Reified;
        }
        return PropertyStatus::Lazy;
    }
    return PropertyStatus::Eager;
}

JSFunction::PropertyStatus JSFunction::reifyLazyBoundNameIfNeeded(VM& vm, JSGlobalObject* globalObject, PropertyName propertyName)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    const Identifier& nameIdent = vm.propertyNames->name;
    if (propertyName != nameIdent)
        return PropertyStatus::Eager;

    if (hasReifiedName())
        return PropertyStatus::Lazy;

    if (isBuiltinFunction())
        reifyName(vm, globalObject);
    else if (this->inherits<JSBoundFunction>(vm)) {
        FunctionRareData* rareData = this->ensureRareData(vm);
        JSString* nameMayBeNull = jsCast<JSBoundFunction*>(this)->nameMayBeNull();
        JSString* string = nullptr;
        if (nameMayBeNull) {
            string = jsString(globalObject, vm.smallStrings.boundPrefixString(), nameMayBeNull);
            RETURN_IF_EXCEPTION(scope, PropertyStatus::Lazy);
        } else
            string = jsEmptyString(vm);
        unsigned initialAttributes = PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly;
        rareData->setHasReifiedName();
        putDirect(vm, nameIdent, string, initialAttributes);
    }
    return PropertyStatus::Reified;
}

#if ASSERT_ENABLED
void JSFunction::assertTypeInfoFlagInvariants()
{
    // If you change this, you'll need to update speculationFromClassInfoInheritance.
    const ClassInfo* info = classInfo(vm());
    if (!(inlineTypeFlags() & ImplementsDefaultHasInstance))
        RELEASE_ASSERT(info == JSBoundFunction::info());
    else
        RELEASE_ASSERT(info != JSBoundFunction::info());
}
#endif // ASSERT_ENABLED

} // namespace JSC
