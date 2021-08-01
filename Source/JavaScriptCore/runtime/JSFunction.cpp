/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
#include "VMTrapsInlines.h"

namespace JSC {

JSC_DEFINE_HOST_FUNCTION(callHostFunctionAsConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

JSFunction* JSFunction::create(VM& vm, JSGlobalObject* globalObject, unsigned length, const String& name, NativeFunction nativeFunction, Intrinsic intrinsic, NativeFunction nativeConstructor, const DOMJIT::Signature* signature)
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
    // JSCell::{getCallData,getConstructData} relies on the following conditions.
    ASSERT(methodTable(vm)->getConstructData == &JSFunction::getConstructData);
    ASSERT(methodTable(vm)->getCallData == &JSFunction::getCallData);
}

void JSFunction::finishCreation(VM& vm, NativeExecutable*, unsigned length, const String& name)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    ASSERT(type() == JSFunctionType);
    // JSCell::{getCallData,getConstructData} relies on the following conditions.
    ASSERT(methodTable(vm)->getConstructData == &JSFunction::getConstructData);
    ASSERT(methodTable(vm)->getCallData == &JSFunction::getCallData);

    // JSBoundFunction instances use finishCreation(VM&) overload and lazily allocate their name string / length.
    ASSERT(!this->inherits<JSBoundFunction>(vm));

    putDirect(vm, vm.propertyNames->length, jsNumber(length), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    if (!name.isNull())
        putDirect(vm, vm.propertyNames->name, jsString(vm, name), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
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
    DeferTermination deferScope(vm);
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
    if (identifier == vm.propertyNames->starDefaultPrivateName)
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

JSString* JSFunction::toString(JSGlobalObject* globalObject)
{
    VM& vm = getVM(globalObject);
    if (inherits<JSBoundFunction>(vm)) {
        JSBoundFunction* function = jsCast<JSBoundFunction*>(this);
        auto scope = DECLARE_THROW_SCOPE(vm);
        JSValue string = jsMakeNontrivialString(globalObject, "function ", function->nameString(), "() {\n    [native code]\n}");
        RETURN_IF_EXCEPTION(scope, nullptr);
        return asString(string);
    }

    if (isHostFunction())
        return static_cast<NativeExecutable*>(executable())->toString(globalObject);
    return jsExecutable()->toString(globalObject);
}

const SourceCode* JSFunction::sourceCode() const
{
    if (isHostOrBuiltinFunction())
        return nullptr;
    return &jsExecutable()->source();
}
    
template<typename Visitor>
void JSFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSFunction* thisObject = jsCast<JSFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.appendUnbarriered(bitwise_cast<JSCell*>(bitwise_cast<uintptr_t>(thisObject->m_executableOrRareData) & ~rareDataTag));
}

DEFINE_VISIT_CHILDREN(JSFunction);

CallData JSFunction::getCallData(JSCell* cell)
{
    // Keep this function OK for invocation from concurrent compilers.
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
        return true;
    }

    thisObject->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
    RETURN_IF_EXCEPTION(scope, false);

    RELEASE_AND_RETURN(scope, Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot));
}

void JSFunction::getOwnSpecialPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    JSFunction* thisObject = jsCast<JSFunction*>(object);
    VM& vm = globalObject->vm();

    if (mode == DontEnumPropertiesMode::Include) {
        if (!thisObject->hasReifiedLength())
            propertyNames.add(vm.propertyNames->length);
        if (!thisObject->hasReifiedName())
            propertyNames.add(vm.propertyNames->name);
        if (!thisObject->isHostOrBuiltinFunction() && thisObject->jsExecutable()->hasPrototypeProperty())
            propertyNames.add(vm.propertyNames->prototype);
    }
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
        if (propertyName == vm.propertyNames->prototype && thisObject->jsExecutable()->hasPrototypeProperty())
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
    } else {
        thisObject->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
    }

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, throwException));
}

// ECMA 13.2.2 [[Construct]]
CallData JSFunction::getConstructData(JSCell* cell)
{
    // Keep this function OK for invocation from concurrent compilers.
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
    if (ecmaName == vm.propertyNames->starDefaultPrivateName)
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
