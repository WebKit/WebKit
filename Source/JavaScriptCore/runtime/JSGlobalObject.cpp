/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSGlobalObject.h"

#include "ArrayConstructor.h"
#include "ArrayIteratorPrototype.h"
#include "ArrayPrototype.h"
#include "AsyncFromSyncIteratorPrototype.h"
#include "AtomicsObject.h"
#include "AsyncFunctionConstructor.h"
#include "AsyncFunctionPrototype.h"
#include "AsyncGeneratorFunctionConstructor.h"
#include "AsyncGeneratorFunctionPrototype.h"
#include "AsyncGeneratorPrototype.h"
#include "AsyncIteratorPrototype.h"
#include "BigIntConstructor.h"
#include "BigIntObject.h"
#include "BigIntPrototype.h"
#include "BooleanConstructor.h"
#include "BooleanPrototype.h"
#include "BuiltinNames.h"
#include "CatchScope.h"
#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CodeBlockSetInlines.h"
#include "CodeCache.h"
#include "ConsoleObject.h"
#include "DateConstructor.h"
#include "DatePrototype.h"
#include "Debugger.h"
#include "DebuggerScope.h"
#include "DirectArguments.h"
#include "DirectEvalExecutable.h"
#include "ECMAScriptSpecInternalFunctions.h"
#include "Error.h"
#include "ErrorConstructor.h"
#include "ErrorPrototype.h"
#include "Exception.h"
#include "FunctionConstructor.h"
#include "FunctionPrototype.h"
#include "GeneratorFunctionConstructor.h"
#include "GeneratorFunctionPrototype.h"
#include "GeneratorPrototype.h"
#include "GetterSetter.h"
#include "HeapIterationScope.h"
#include "IndirectEvalExecutable.h"
#include "InspectorInstrumentationObject.h"
#include "Interpreter.h"
#include "IteratorPrototype.h"
#include "JSAPIWrapperObject.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferConstructor.h"
#include "JSArrayBufferPrototype.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSBigInt.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSCustomGetterSetterFunction.h"
#include "JSDataView.h"
#include "JSDataViewPrototype.h"
#include "JSDollarVM.h"
#include "JSFunction.h"
#include "JSGeneratorFunction.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSGenericTypedArrayViewPrototypeInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSInternalPromisePrototype.h"
#include "JSLexicalEnvironment.h"
#include "JSLock.h"
#include "JSMap.h"
#include "JSMicrotask.h"
#include "JSModuleEnvironment.h"
#include "JSModuleLoader.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSNativeStdFunction.h"
#include "JSONObject.h"
#include "JSPromise.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSSet.h"
#include "JSStringIterator.h"
#include "JSTypedArrayConstructors.h"
#include "JSTypedArrayPrototypes.h"
#include "JSTypedArrayViewConstructor.h"
#include "JSTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"
#include "JSWeakMap.h"
#include "JSWeakSet.h"
#include "JSWebAssembly.h"
#include "JSWithScope.h"
#include "LazyClassStructureInlines.h"
#include "LazyPropertyInlines.h"
#include "Lookup.h"
#include "MapConstructor.h"
#include "MapIteratorPrototype.h"
#include "MapPrototype.h"
#include "MarkedSpaceInlines.h"
#include "MathObject.h"
#include "Microtask.h"
#include "NativeErrorConstructor.h"
#include "NativeErrorPrototype.h"
#include "NullGetterFunction.h"
#include "NullSetterFunction.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"
#include "ObjCCallbackFunction.h"
#include "ObjectConstructor.h"
#include "ObjectPropertyChangeAdaptiveWatchpoint.h"
#include "ObjectPropertyConditionSet.h"
#include "ObjectPrototype.h"
#include "ParserError.h"
#include "ProxyConstructor.h"
#include "ProxyObject.h"
#include "ProxyRevoke.h"
#include "ReflectObject.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "ScopedArguments.h"
#include "SetConstructor.h"
#include "SetIteratorPrototype.h"
#include "SetPrototype.h"
#include "StrictEvalActivation.h"
#include "StringConstructor.h"
#include "StringIteratorPrototype.h"
#include "StringPrototype.h"
#include "Symbol.h"
#include "SymbolConstructor.h"
#include "SymbolObject.h"
#include "SymbolPrototype.h"
#include "VariableWriteFireDetail.h"
#include "WeakGCMapInlines.h"
#include "WeakMapConstructor.h"
#include "WeakMapPrototype.h"
#include "WeakSetConstructor.h"
#include "WeakSetPrototype.h"
#include "WebAssemblyPrototype.h"
#include "WebAssemblyToJSCallee.h"
#include <wtf/RandomNumber.h>

#if ENABLE(INTL)
#include "IntlObject.h"
#include <unicode/ucol.h>
#include <unicode/udat.h>
#include <unicode/unum.h>
#endif // ENABLE(INTL)

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectDebuggable.h"
#include "JSGlobalObjectInspectorController.h"
#endif

#ifdef JSC_GLIB_API_ENABLED
#include "JSCCallbackFunction.h"
#include "JSCWrapperMap.h"
#endif

namespace JSC {

static JSValue createProxyProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return ProxyConstructor::create(vm, ProxyConstructor::createStructure(vm, global, global->functionPrototype()));
}

static JSValue createJSONProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return JSONObject::create(vm, JSONObject::createStructure(vm, global, global->objectPrototype()));
}

static JSValue createMathProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return MathObject::create(vm, global, MathObject::createStructure(vm, global, global->objectPrototype()));
}

static JSValue createConsoleProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return ConsoleObject::create(vm, global, ConsoleObject::createStructure(vm, global, constructEmptyObject(global->globalExec())));
}

static EncodedJSValue JSC_HOST_CALL makeBoundFunction(ExecState* exec)
{
    VM& vm = exec->vm();
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    JSObject* target = asObject(exec->uncheckedArgument(0));
    JSValue boundThis = exec->uncheckedArgument(1);
    JSValue boundArgs = exec->uncheckedArgument(2);
    JSValue length = exec->uncheckedArgument(3);
    JSString* name = asString(exec->uncheckedArgument(4));

    return JSValue::encode(JSBoundFunction::create(
        vm, exec, globalObject, target, boundThis, boundArgs.isCell() ? jsCast<JSArray*>(boundArgs) : nullptr, length.asInt32(), name->value(exec)));
}

static EncodedJSValue JSC_HOST_CALL hasOwnLengthProperty(ExecState* exec)
{
    VM& vm = exec->vm();
    JSObject* target = asObject(exec->uncheckedArgument(0));
    return JSValue::encode(jsBoolean(target->hasOwnProperty(exec, vm.propertyNames->length)));
}

#if !ASSERT_DISABLED
static EncodedJSValue JSC_HOST_CALL assertCall(ExecState* exec)
{
    RELEASE_ASSERT(exec->argument(0).isBoolean());
    if (exec->argument(0).asBoolean())
        return JSValue::encode(jsUndefined());

    bool iteratedOnce = false;
    CodeBlock* codeBlock = nullptr;
    unsigned line;
    exec->iterate([&] (StackVisitor& visitor) {
        if (!iteratedOnce) {
            iteratedOnce = true;
            return StackVisitor::Continue;
        }

        RELEASE_ASSERT(visitor->hasLineAndColumnInfo());
        unsigned column;
        visitor->computeLineAndColumn(line, column);
        codeBlock = visitor->codeBlock();
        return StackVisitor::Done;
    });
    RELEASE_ASSERT(!!codeBlock);
    RELEASE_ASSERT_WITH_MESSAGE(false, "JS assertion failed at line %u in:\n%s\n", line, codeBlock->sourceCodeForTools().data());
    return JSValue::encode(jsUndefined());
}
#endif

} // namespace JSC

#include "JSGlobalObject.lut.h"

namespace JSC {

const ClassInfo JSGlobalObject::s_info = { "GlobalObject", &Base::s_info, &globalObjectTable, nullptr, CREATE_METHOD_TABLE(JSGlobalObject) };

const GlobalObjectMethodTable JSGlobalObject::s_globalObjectMethodTable = {
    &supportsRichSourceInfo,
    &shouldInterruptScript,
    &javaScriptRuntimeFlags,
    nullptr, // queueTaskToEventLoop
    &shouldInterruptScriptBeforeTimeout,
    nullptr, // moduleLoaderImportModule
    nullptr, // moduleLoaderResolve
    nullptr, // moduleLoaderFetch
    nullptr, // moduleLoaderCreateImportMetaProperties
    nullptr, // moduleLoaderEvaluate
    nullptr, // promiseRejectionTracker
    nullptr, // defaultLanguage
    nullptr, // compileStreaming
    nullptr, // instantiateStreaming
};

/* Source for JSGlobalObject.lut.h
@begin globalObjectTable
  isNaN                 JSBuiltin                                    DontEnum|Function 1
  isFinite              JSBuiltin                                    DontEnum|Function 1
  escape                globalFuncEscape                             DontEnum|Function 1
  unescape              globalFuncUnescape                           DontEnum|Function 1
  decodeURI             globalFuncDecodeURI                          DontEnum|Function 1
  decodeURIComponent    globalFuncDecodeURIComponent                 DontEnum|Function 1
  encodeURI             globalFuncEncodeURI                          DontEnum|Function 1
  encodeURIComponent    globalFuncEncodeURIComponent                 DontEnum|Function 1
  EvalError             JSGlobalObject::m_evalErrorConstructor       DontEnum|CellProperty
  globalThis            JSGlobalObject::m_globalThis                 DontEnum|CellProperty
  ReferenceError        JSGlobalObject::m_referenceErrorConstructor  DontEnum|CellProperty
  SyntaxError           JSGlobalObject::m_syntaxErrorConstructor     DontEnum|CellProperty
  URIError              JSGlobalObject::m_URIErrorConstructor        DontEnum|CellProperty
  Proxy                 createProxyProperty                          DontEnum|PropertyCallback
  JSON                  createJSONProperty                           DontEnum|PropertyCallback
  Math                  createMathProperty                           DontEnum|PropertyCallback
  console               createConsoleProperty                        DontEnum|PropertyCallback
  Int8Array             JSGlobalObject::m_typedArrayInt8             DontEnum|ClassStructure
  Int16Array            JSGlobalObject::m_typedArrayInt16            DontEnum|ClassStructure
  Int32Array            JSGlobalObject::m_typedArrayInt32            DontEnum|ClassStructure
  Uint8Array            JSGlobalObject::m_typedArrayUint8            DontEnum|ClassStructure
  Uint8ClampedArray     JSGlobalObject::m_typedArrayUint8Clamped     DontEnum|ClassStructure
  Uint16Array           JSGlobalObject::m_typedArrayUint16           DontEnum|ClassStructure
  Uint32Array           JSGlobalObject::m_typedArrayUint32           DontEnum|ClassStructure
  Float32Array          JSGlobalObject::m_typedArrayFloat32          DontEnum|ClassStructure
  Float64Array          JSGlobalObject::m_typedArrayFloat64          DontEnum|ClassStructure
  DataView              JSGlobalObject::m_typedArrayDataView         DontEnum|ClassStructure
  Date                  JSGlobalObject::m_dateStructure              DontEnum|ClassStructure
  Boolean               JSGlobalObject::m_booleanObjectStructure     DontEnum|ClassStructure
  Number                JSGlobalObject::m_numberObjectStructure      DontEnum|ClassStructure
  WeakMap               JSGlobalObject::m_weakMapStructure           DontEnum|ClassStructure
  WeakSet               JSGlobalObject::m_weakSetStructure           DontEnum|ClassStructure
@end
*/

static EncodedJSValue JSC_HOST_CALL enqueueJob(ExecState* exec)
{
    VM& vm = exec->vm();
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    JSValue job = exec->argument(0);
    JSValue arguments = exec->argument(1);
    ASSERT(arguments.inherits<JSArray>(vm));

    globalObject->queueMicrotask(createJSMicrotask(vm, job, jsCast<JSArray*>(arguments)));

    return JSValue::encode(jsUndefined());
}

JSGlobalObject::JSGlobalObject(VM& vm, Structure* structure, const GlobalObjectMethodTable* globalObjectMethodTable)
    : Base(vm, structure, 0)
    , m_vm(vm)
    , m_masqueradesAsUndefinedWatchpoint(adoptRef(new WatchpointSet(IsWatched)))
    , m_havingABadTimeWatchpoint(adoptRef(new WatchpointSet(IsWatched)))
    , m_varInjectionWatchpoint(adoptRef(new WatchpointSet(IsWatched)))
    , m_weakRandom(Options::forceWeakRandomSeed() ? Options::forcedWeakRandomSeed() : static_cast<unsigned>(randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0)))
    , m_arrayIteratorProtocolWatchpoint(IsWatched)
    , m_mapIteratorProtocolWatchpoint(IsWatched)
    , m_setIteratorProtocolWatchpoint(IsWatched)
    , m_stringIteratorProtocolWatchpoint(IsWatched)
    , m_mapSetWatchpoint(IsWatched)
    , m_setAddWatchpoint(IsWatched)
    , m_arraySpeciesWatchpoint(ClearWatchpoint)
    , m_numberToStringWatchpoint(IsWatched)
    , m_runtimeFlags()
    , m_globalObjectMethodTable(globalObjectMethodTable ? globalObjectMethodTable : &s_globalObjectMethodTable)
{
}

JSGlobalObject::~JSGlobalObject()
{
#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorController->globalObjectDestroyed();
#endif

    if (m_debugger)
        m_debugger->detach(this, Debugger::GlobalObjectIsDestructing);
}

void JSGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSGlobalObject*>(cell)->JSGlobalObject::~JSGlobalObject();
}

void JSGlobalObject::setGlobalThis(VM& vm, JSObject* globalThis)
{
    m_globalThis.set(vm, this, globalThis);
}

static JSObject* getGetterById(ExecState* exec, JSObject* base, const Identifier& ident)
{
    JSValue baseValue = JSValue(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry);
    baseValue.getPropertySlot(exec, ident, slot);
    return slot.getPureResult().toObject(exec);
}

void JSGlobalObject::init(VM& vm)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    Base::setStructure(vm, Structure::toCacheableDictionaryTransition(vm, structure(vm)));

    m_debugger = 0;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorController = std::make_unique<Inspector::JSGlobalObjectInspectorController>(*this);
    m_inspectorDebuggable = std::make_unique<JSGlobalObjectDebuggable>(*this);
    m_inspectorDebuggable->init();
    m_consoleClient = m_inspectorController->consoleClient();
#endif

    m_functionPrototype.set(vm, this, FunctionPrototype::create(vm, FunctionPrototype::createStructure(vm, this, jsNull()))); // The real prototype will be set once ObjectPrototype is created.
    m_calleeStructure.set(vm, this, JSCallee::createStructure(vm, this, jsNull()));

    m_globalLexicalEnvironment.set(vm, this, JSGlobalLexicalEnvironment::create(vm, JSGlobalLexicalEnvironment::createStructure(vm, this), this));
    // Need to create the callee structure (above) before creating the callee.
    JSCallee* globalCallee = JSCallee::create(vm, this, globalScope());
    m_globalCallee.set(vm, this, globalCallee);

    ExecState::initGlobalExec(JSGlobalObject::globalExec(), globalCallee);
    ExecState* exec = JSGlobalObject::globalExec();

    JSCallee* stackOverflowFrameCallee = JSCallee::create(vm, this, globalScope());
    m_stackOverflowFrameCallee.set(vm, this, stackOverflowFrameCallee);

    m_hostFunctionStructure.set(vm, this, JSFunction::createStructure(vm, this, m_functionPrototype.get()));

    auto initFunctionStructures = [&] (FunctionStructures& structures) {
        structures.strictFunctionStructure.set(vm, this, JSFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.sloppyFunctionStructure.set(vm, this, JSFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.arrowFunctionStructure.set(vm, this, JSFunction::createStructure(vm, this, m_functionPrototype.get()));
    };
    initFunctionStructures(m_builtinFunctions);
    initFunctionStructures(m_ordinaryFunctions);

    m_customGetterSetterFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCustomGetterSetterFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_boundFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSBoundFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_getterSetterStructure.set(vm, this, GetterSetter::createStructure(vm, this, jsNull()));
    m_nativeStdFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSNativeStdFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    JSFunction* callFunction = nullptr;
    JSFunction* applyFunction = nullptr;
    JSFunction* hasInstanceSymbolFunction = nullptr;
    m_functionPrototype->addFunctionProperties(exec, this, &callFunction, &applyFunction, &hasInstanceSymbolFunction);
    m_callFunction.set(vm, this, callFunction);
    m_applyFunction.set(vm, this, applyFunction);
    m_arrayProtoToStringFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 0, init.vm.propertyNames->toString.string(), arrayProtoFuncToString, NoIntrinsic));
        });
    m_arrayProtoValuesFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, arrayPrototypeValuesCodeGenerator(init.vm), init.owner));
        });
    m_initializePromiseFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, promiseOperationsInitializePromiseCodeGenerator(init.vm), init.owner));
        });

    m_iteratorProtocolFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, iteratorHelpersPerformIterationCodeGenerator(init.vm), init.owner));
        });

    m_promiseResolveFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, promiseConstructorResolveCodeGenerator(init.vm), init.owner));
        });

    m_newPromiseCapabilityFunction.set(vm, this, JSFunction::create(vm, promiseOperationsNewPromiseCapabilityCodeGenerator(vm), this));
    m_functionProtoHasInstanceSymbolFunction.set(vm, this, hasInstanceSymbolFunction);
    m_throwTypeErrorGetterSetter.initLater(
        [] (const Initializer<GetterSetter>& init) {
            JSFunction* thrower = init.owner->throwTypeErrorFunction();
            GetterSetter* getterSetter = GetterSetter::create(init.vm, init.owner, thrower, thrower);
            init.set(getterSetter);
        });

    m_nullGetterFunction.set(vm, this, NullGetterFunction::create(vm, NullGetterFunction::createStructure(vm, this, m_functionPrototype.get())));
    m_nullSetterFunction.set(vm, this, NullSetterFunction::create(vm, NullSetterFunction::createStructure(vm, this, m_functionPrototype.get())));
    m_objectPrototype.set(vm, this, ObjectPrototype::create(vm, this, ObjectPrototype::createStructure(vm, this, jsNull())));
    GetterSetter* protoAccessor = GetterSetter::create(vm, this,
        JSFunction::create(vm, this, 0, makeString("get ", vm.propertyNames->underscoreProto.string()), globalFuncProtoGetter, UnderscoreProtoIntrinsic),
        JSFunction::create(vm, this, 0, makeString("set ", vm.propertyNames->underscoreProto.string()), globalFuncProtoSetter));
    m_objectPrototype->putDirectNonIndexAccessor(vm, vm.propertyNames->underscoreProto, protoAccessor, PropertyAttribute::Accessor | PropertyAttribute::DontEnum);
    m_functionPrototype->structure(vm)->setPrototypeWithoutTransition(vm, m_objectPrototype.get());
    m_objectStructureForObjectConstructor.set(vm, this, vm.structureCache.emptyObjectStructureForPrototype(this, m_objectPrototype.get(), JSFinalObject::defaultInlineCapacity()));
    m_objectProtoValueOfFunction.set(vm, this, jsCast<JSFunction*>(objectPrototype()->getDirect(vm, vm.propertyNames->valueOf)));
    
    JSFunction* thrower = JSFunction::create(vm, this, 0, String(), globalFuncThrowTypeErrorArgumentsCalleeAndCaller);
    GetterSetter* getterSetter = GetterSetter::create(vm, this, thrower, thrower);
    m_throwTypeErrorArgumentsCalleeAndCallerGetterSetter.set(vm, this, getterSetter);
    
    m_functionPrototype->initRestrictedProperties(exec, this);

    m_speciesGetterSetter.set(vm, this, GetterSetter::create(vm, this, JSFunction::create(vm, globalOperationsSpeciesGetterCodeGenerator(vm), this), nullptr));

    m_typedArrayProto.initLater(
        [] (const Initializer<JSTypedArrayViewPrototype>& init) {
            init.set(JSTypedArrayViewPrototype::create(init.vm, init.owner, JSTypedArrayViewPrototype::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get())));
            
            // Make sure that the constructor gets initialized, too.
            init.owner->m_typedArraySuperConstructor.get(init.owner);
        });
    m_typedArraySuperConstructor.initLater(
        [] (const Initializer<JSTypedArrayViewConstructor>& init) {
            JSTypedArrayViewPrototype* prototype = init.owner->m_typedArrayProto.get(init.owner);
            JSTypedArrayViewConstructor* constructor = JSTypedArrayViewConstructor::create(init.vm, init.owner, JSTypedArrayViewConstructor::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()), prototype, init.owner->m_speciesGetterSetter.get());
            prototype->putDirectWithoutTransition(init.vm, init.vm.propertyNames->constructor, constructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
            init.set(constructor);
        });
    
#define INIT_TYPED_ARRAY_LATER(type) \
    m_typedArray ## type.initLater( \
        [] (LazyClassStructure::Initializer& init) { \
            init.setPrototype(JS ## type ## ArrayPrototype::create(init.vm, init.global, JS ## type ## ArrayPrototype::createStructure(init.vm, init.global, init.global->m_typedArrayProto.get(init.global)))); \
            init.setStructure(JS ## type ## Array::createStructure(init.vm, init.global, init.prototype)); \
            init.setConstructor(JS ## type ## ArrayConstructor::create(init.vm, init.global, JS ## type ## ArrayConstructor::createStructure(init.vm, init.global, init.global->m_typedArraySuperConstructor.get(init.global)), init.prototype, #type "Array"_s, typedArrayConstructorAllocate ## type ## ArrayCodeGenerator(init.vm))); \
            init.global->putDirectWithoutTransition(init.vm, init.vm.propertyNames->builtinNames().type ## ArrayPrivateName(), init.constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \
        });
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(INIT_TYPED_ARRAY_LATER)
#undef INIT_TYPED_ARRAY_LATER
    
    m_typedArrayDataView.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.setPrototype(JSDataViewPrototype::create(init.vm, JSDataViewPrototype::createStructure(init.vm, init.global, init.global->m_objectPrototype.get())));
            init.setStructure(JSDataView::createStructure(init.vm, init.global, init.prototype));
            init.setConstructor(JSDataViewConstructor::create(init.vm, init.global, JSDataViewConstructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), init.prototype, "DataView"_s, nullptr));
        });
    
    m_lexicalEnvironmentStructure.set(vm, this, JSLexicalEnvironment::createStructure(vm, this));
    m_moduleEnvironmentStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSModuleEnvironment::createStructure(init.vm, init.owner));
        });
    m_strictEvalActivationStructure.set(vm, this, StrictEvalActivation::createStructure(vm, this, jsNull()));
    m_debuggerScopeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(DebuggerScope::createStructure(init.vm, init.owner));
        });
    m_withScopeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSWithScope::createStructure(init.vm, init.owner, jsNull()));
        });
    
    m_nullPrototypeObjectStructure.set(vm, this, JSFinalObject::createStructure(vm, this, jsNull(), JSFinalObject::defaultInlineCapacity()));
    
    m_callbackFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_directArgumentsStructure.set(vm, this, DirectArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_scopedArgumentsStructure.set(vm, this, ScopedArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_clonedArgumentsStructure.set(vm, this, ClonedArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_callbackConstructorStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackConstructor::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });
    m_callbackObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackObject<JSDestructibleObject>::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });

#if JSC_OBJC_API_ENABLED
    m_objcCallbackFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(ObjCCallbackFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_objcWrapperObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackObject<JSAPIWrapperObject>::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });
#endif
#ifdef JSC_GLIB_API_ENABLED
    m_glibCallbackFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCCallbackFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_glibWrapperObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackObject<JSAPIWrapperObject>::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });
#endif
    m_arrayPrototype.set(vm, this, ArrayPrototype::create(vm, this, ArrayPrototype::createStructure(vm, this, m_objectPrototype.get())));
    
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(UndecidedShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithUndecided));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(Int32Shape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithInt32));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(DoubleShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithDouble));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(ContiguousShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithContiguous));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(ArrayStorageShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithArrayStorage));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(SlowPutArrayStorageShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithSlowPutArrayStorage));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(CopyOnWriteArrayWithInt32)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), CopyOnWriteArrayWithInt32));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(CopyOnWriteArrayWithDouble)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), CopyOnWriteArrayWithDouble));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), CopyOnWriteArrayWithContiguous));
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i] = m_originalArrayStructureForIndexingShape[i];

    m_regExpPrototype.set(vm, this, RegExpPrototype::create(vm, this, RegExpPrototype::createStructure(vm, this, m_objectPrototype.get())));
    m_regExpStructure.set(vm, this, RegExpObject::createStructure(vm, this, m_regExpPrototype.get()));
    m_regExpMatchesArrayStructure.set(vm, this, createRegExpMatchesArrayStructure(vm, this));
    m_regExpMatchesArrayWithGroupsStructure.set(vm, this, createRegExpMatchesArrayWithGroupsStructure(vm, this));

    m_moduleRecordStructure.set(vm, this, JSModuleRecord::createStructure(vm, this, jsNull()));
    m_moduleNamespaceObjectStructure.set(vm, this, JSModuleNamespaceObject::createStructure(vm, this, jsNull()));
    {
        bool isCallable = false;
        m_proxyObjectStructure.set(vm, this, ProxyObject::createStructure(vm, this, jsNull(), isCallable));
        isCallable = true;
        m_callableProxyObjectStructure.set(vm, this, ProxyObject::createStructure(vm, this, jsNull(), isCallable));
    }
    m_proxyRevokeStructure.set(vm, this, ProxyRevoke::createStructure(vm, this, m_functionPrototype.get()));

    m_parseIntFunction.set(vm, this, JSFunction::create(vm, this, 2, vm.propertyNames->parseInt.string(), globalFuncParseInt, ParseIntIntrinsic));
    putDirectWithoutTransition(vm, vm.propertyNames->parseInt, m_parseIntFunction.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_parseFloatFunction.set(vm, this, JSFunction::create(vm, this, 1, vm.propertyNames->parseFloat.string(), globalFuncParseFloat, NoIntrinsic));
    putDirectWithoutTransition(vm, vm.propertyNames->parseFloat, m_parseFloatFunction.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    m_arrayBufferPrototype.set(vm, this, JSArrayBufferPrototype::create(vm, this, JSArrayBufferPrototype::createStructure(vm, this, m_objectPrototype.get()), ArrayBufferSharingMode::Default));
    m_arrayBufferStructure.set(vm, this, JSArrayBuffer::createStructure(vm, this, m_arrayBufferPrototype.get()));
#if ENABLE(SHARED_ARRAY_BUFFER)
    m_sharedArrayBufferPrototype.set(vm, this, JSArrayBufferPrototype::create(vm, this, JSArrayBufferPrototype::createStructure(vm, this, m_objectPrototype.get()), ArrayBufferSharingMode::Shared));
    m_sharedArrayBufferStructure.set(vm, this, JSArrayBuffer::createStructure(vm, this, m_sharedArrayBufferPrototype.get()));
#endif

    m_iteratorPrototype.set(vm, this, IteratorPrototype::create(vm, this, IteratorPrototype::createStructure(vm, this, m_objectPrototype.get())));
    m_asyncIteratorPrototype.set(vm, this, AsyncIteratorPrototype::create(vm, this, AsyncIteratorPrototype::createStructure(vm, this, m_objectPrototype.get())));

    m_generatorPrototype.set(vm, this, GeneratorPrototype::create(vm, this, GeneratorPrototype::createStructure(vm, this, m_iteratorPrototype.get())));
    m_asyncGeneratorPrototype.set(vm, this, AsyncGeneratorPrototype::create(vm, this, AsyncGeneratorPrototype::createStructure(vm, this, m_asyncIteratorPrototype.get())));

#define CREATE_PROTOTYPE_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase) do { \
        m_ ## lowerName ## Prototype.set(vm, this, capitalName##Prototype::create(vm, this, capitalName##Prototype::createStructure(vm, this, m_ ## prototypeBase ## Prototype.get()))); \
        m_ ## properName ## Structure.set(vm, this, instanceType::createStructure(vm, this, m_ ## lowerName ## Prototype.get())); \
    } while (0);
    
    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)

    if (UNLIKELY(Options::useBigInt()))
        FOR_BIG_INT_BUILTIN_TYPE_WITH_CONSTRUCTOR(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)

    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)
    
#undef CREATE_PROTOTYPE_FOR_SIMPLE_TYPE

#define CREATE_PROTOTYPE_FOR_LAZY_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase) \
    m_ ## properName ## Structure.initLater(\
        [] (LazyClassStructure::Initializer& init) { \
            init.setPrototype(capitalName##Prototype::create(init.vm, init.global, capitalName##Prototype::createStructure(init.vm, init.global, init.global->m_ ## prototypeBase ## Prototype.get()))); \
            init.setStructure(instanceType::createStructure(init.vm, init.global, init.prototype)); \
            init.setConstructor(capitalName ## Constructor::create(init.vm, capitalName ## Constructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), jsCast<capitalName ## Prototype*>(init.prototype), init.global->m_speciesGetterSetter.get())); \
        });
    
    FOR_EACH_LAZY_BUILTIN_TYPE(CREATE_PROTOTYPE_FOR_LAZY_TYPE)
    
#undef CREATE_PROTOTYPE_FOR_LAZY_TYPE
    
    // Constructors

    ObjectConstructor* objectConstructor = ObjectConstructor::create(vm, this, ObjectConstructor::createStructure(vm, this, m_functionPrototype.get()), m_objectPrototype.get());
    m_objectConstructor.set(vm, this, objectConstructor);

    JSFunction* throwTypeErrorFunction = JSFunction::create(vm, this, 0, String(), globalFuncThrowTypeError);
    m_throwTypeErrorFunction.set(vm, this, throwTypeErrorFunction);

    JSCell* functionConstructor = FunctionConstructor::create(vm, FunctionConstructor::createStructure(vm, this, m_functionPrototype.get()), m_functionPrototype.get());

    ArrayConstructor* arrayConstructor = ArrayConstructor::create(vm, this, ArrayConstructor::createStructure(vm, this, m_functionPrototype.get()), m_arrayPrototype.get(), m_speciesGetterSetter.get());
    m_arrayConstructor.set(vm, this, arrayConstructor);
    
    m_regExpConstructor.set(vm, this, RegExpConstructor::create(vm, RegExpConstructor::createStructure(vm, this, m_functionPrototype.get()), m_regExpPrototype.get(), m_speciesGetterSetter.get()));
    
    JSArrayBufferConstructor* arrayBufferConstructor = JSArrayBufferConstructor::create(vm, JSArrayBufferConstructor::createStructure(vm, this, m_functionPrototype.get()), m_arrayBufferPrototype.get(), m_speciesGetterSetter.get(), ArrayBufferSharingMode::Default);
    m_arrayBufferPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, arrayBufferConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));

#if ENABLE(SHARED_ARRAY_BUFFER)
    JSArrayBufferConstructor* sharedArrayBufferConstructor = nullptr;
    sharedArrayBufferConstructor = JSArrayBufferConstructor::create(vm, JSArrayBufferConstructor::createStructure(vm, this, m_functionPrototype.get()), m_sharedArrayBufferPrototype.get(), m_speciesGetterSetter.get(), ArrayBufferSharingMode::Shared);
    m_sharedArrayBufferPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, sharedArrayBufferConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));

    AtomicsObject* atomicsObject = AtomicsObject::create(vm, this, AtomicsObject::createStructure(vm, this, m_objectPrototype.get()));
#endif

#define CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase) \
capitalName ## Constructor* lowerName ## Constructor = capitalName ## Constructor::create(vm, capitalName ## Constructor::createStructure(vm, this, m_functionPrototype.get()), m_ ## lowerName ## Prototype.get(), m_speciesGetterSetter.get()); \
m_ ## lowerName ## Prototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, lowerName ## Constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE)
    BigIntConstructor* bigIntConstructor = nullptr;
    if (UNLIKELY(Options::useBigInt())) {
        bigIntConstructor = BigIntConstructor::create(vm, BigIntConstructor::createStructure(vm, this, m_functionPrototype.get()), m_bigIntPrototype.get(), m_speciesGetterSetter.get());
        m_bigIntPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, bigIntConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }
    
#undef CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE

    m_errorConstructor.set(vm, this, errorConstructor);
    m_promiseConstructor.set(vm, this, promiseConstructor);
    m_internalPromiseConstructor.set(vm, this, internalPromiseConstructor);
    
    m_nativeErrorPrototypeStructure.set(vm, this, NativeErrorPrototype::createStructure(vm, this, m_errorPrototype.get()));
    m_nativeErrorStructure.set(vm, this, NativeErrorConstructor::createStructure(vm, this, errorConstructor));
    m_evalErrorConstructor.initLater(
        [] (const Initializer<NativeErrorConstructor>& init) {
            init.set(NativeErrorConstructor::create(init.vm, init.owner, init.owner->m_nativeErrorStructure.get(), init.owner->m_nativeErrorPrototypeStructure.get(), "EvalError"_s));
        });
    m_rangeErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, m_nativeErrorStructure.get(), m_nativeErrorPrototypeStructure.get(), "RangeError"_s));
    m_referenceErrorConstructor.initLater(
        [] (const Initializer<NativeErrorConstructor>& init) {
            init.set(NativeErrorConstructor::create(init.vm, init.owner, init.owner->m_nativeErrorStructure.get(), init.owner->m_nativeErrorPrototypeStructure.get(), "ReferenceError"_s));
        });
    m_syntaxErrorConstructor.initLater(
        [] (const Initializer<NativeErrorConstructor>& init) {
            init.set(NativeErrorConstructor::create(init.vm, init.owner, init.owner->m_nativeErrorStructure.get(), init.owner->m_nativeErrorPrototypeStructure.get(), "SyntaxError"_s));
        });
    m_typeErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, m_nativeErrorStructure.get(), m_nativeErrorPrototypeStructure.get(), "TypeError"_s));
    m_URIErrorConstructor.initLater(
        [] (const Initializer<NativeErrorConstructor>& init) {
            init.set(NativeErrorConstructor::create(init.vm, init.owner, init.owner->m_nativeErrorStructure.get(), init.owner->m_nativeErrorPrototypeStructure.get(), "URIError"_s));
        });

    m_generatorFunctionPrototype.set(vm, this, GeneratorFunctionPrototype::create(vm, GeneratorFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    GeneratorFunctionConstructor* generatorFunctionConstructor = GeneratorFunctionConstructor::create(vm, GeneratorFunctionConstructor::createStructure(vm, this, functionConstructor), m_generatorFunctionPrototype.get());
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, generatorFunctionConstructor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorFunctionStructure.set(vm, this, JSGeneratorFunction::createStructure(vm, this, m_generatorFunctionPrototype.get()));

    m_generatorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_generatorFunctionPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->prototype, m_generatorPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    m_asyncFunctionPrototype.set(vm, this, AsyncFunctionPrototype::create(vm, AsyncFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    AsyncFunctionConstructor* asyncFunctionConstructor = AsyncFunctionConstructor::create(vm, AsyncFunctionConstructor::createStructure(vm, this, functionConstructor), m_asyncFunctionPrototype.get());
    m_asyncFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, asyncFunctionConstructor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_asyncFunctionStructure.set(vm, this, JSAsyncFunction::createStructure(vm, this, m_asyncFunctionPrototype.get()));

    m_asyncGeneratorFunctionPrototype.set(vm, this, AsyncGeneratorFunctionPrototype::create(vm, AsyncGeneratorFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    AsyncGeneratorFunctionConstructor* asyncGeneratorFunctionConstructor = AsyncGeneratorFunctionConstructor::create(vm, AsyncGeneratorFunctionConstructor::createStructure(vm, this, functionConstructor), m_asyncGeneratorFunctionPrototype.get());
    m_asyncGeneratorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, asyncGeneratorFunctionConstructor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_asyncGeneratorFunctionStructure.set(vm, this, JSAsyncGeneratorFunction::createStructure(vm, this, m_asyncGeneratorFunctionPrototype.get()));

    m_asyncGeneratorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_asyncGeneratorFunctionPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_asyncGeneratorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->prototype, m_asyncGeneratorPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    
    
    m_objectPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, objectConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_functionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, functionConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_arrayPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, arrayConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_regExpPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_regExpConstructor.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    putDirectWithoutTransition(vm, vm.propertyNames->Object, objectConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->Function, functionConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->Array, arrayConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->RegExp, m_regExpConstructor.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->RangeError, m_rangeErrorConstructor.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->TypeError, m_typeErrorConstructor.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().ObjectPrivateName(), objectConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().ArrayPrivateName(), arrayConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

    putDirectWithoutTransition(vm, vm.propertyNames->ArrayBuffer, arrayBufferConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
#if ENABLE(SHARED_ARRAY_BUFFER)
    putDirectWithoutTransition(vm, vm.propertyNames->SharedArrayBuffer, sharedArrayBufferConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(exec, "Atomics"), atomicsObject, static_cast<unsigned>(PropertyAttribute::DontEnum));
#endif

#define PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase) \
putDirectWithoutTransition(vm, vm.propertyNames-> jsName, lowerName ## Constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE)
    if (UNLIKELY(Options::useBigInt()))
        FOR_BIG_INT_BUILTIN_TYPE_WITH_CONSTRUCTOR(PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE)

#undef PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE
    m_iteratorResultObjectStructure.set(vm, this, createIteratorResultObjectStructure(vm, *this));
    
    m_evalFunction.set(vm, this, JSFunction::create(vm, this, 1, vm.propertyNames->eval.string(), globalFuncEval));
    putDirectWithoutTransition(vm, vm.propertyNames->eval, m_evalFunction.get(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    
#if ENABLE(INTL)
    IntlObject* intl = IntlObject::create(vm, this, IntlObject::createStructure(vm, this, m_objectPrototype.get()));
    putDirectWithoutTransition(vm, vm.propertyNames->Intl, intl, static_cast<unsigned>(PropertyAttribute::DontEnum));
#endif // ENABLE(INTL)
    ReflectObject* reflectObject = ReflectObject::create(vm, this, ReflectObject::createStructure(vm, this, m_objectPrototype.get()));
    putDirectWithoutTransition(vm, vm.propertyNames->Reflect, reflectObject, static_cast<unsigned>(PropertyAttribute::DontEnum));

    m_moduleLoader.initLater(
        [] (const Initializer<JSModuleLoader>& init) {
            auto catchScope = DECLARE_CATCH_SCOPE(init.vm);
            init.set(JSModuleLoader::create(init.owner->globalExec(), init.vm, init.owner, JSModuleLoader::createStructure(init.vm, init.owner, jsNull())));
            catchScope.releaseAssertNoException();
        });
    if (Options::exposeInternalModuleLoader())
        putDirectWithoutTransition(vm, vm.propertyNames->Loader, moduleLoader(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* builtinLog = JSFunction::create(vm, this, 1, vm.propertyNames->emptyIdentifier.string(), globalFuncBuiltinLog);
    JSFunction* builtinDescribe = JSFunction::create(vm, this, 1, vm.propertyNames->emptyIdentifier.string(), globalFuncBuiltinDescribe);

    JSFunction* privateFuncAbs = JSFunction::create(vm, this, 0, String(), mathProtoFuncAbs, AbsIntrinsic);
    JSFunction* privateFuncFloor = JSFunction::create(vm, this, 0, String(), mathProtoFuncFloor, FloorIntrinsic);
    JSFunction* privateFuncTrunc = JSFunction::create(vm, this, 0, String(), mathProtoFuncTrunc, TruncIntrinsic);

    JSFunction* privateFuncGetOwnPropertyNames = JSFunction::create(vm, this, 0, String(), objectConstructorGetOwnPropertyNames);
    JSFunction* privateFuncPropertyIsEnumerable = JSFunction::create(vm, this, 0, String(), globalFuncPropertyIsEnumerable);
    JSFunction* privateFuncImportModule = JSFunction::create(vm, this, 0, String(), globalFuncImportModule);
    JSFunction* privateFuncTypedArrayLength = JSFunction::create(vm, this, 0, String(), typedArrayViewPrivateFuncLength);
    JSFunction* privateFuncTypedArrayGetOriginalConstructor = JSFunction::create(vm, this, 0, String(), typedArrayViewPrivateFuncGetOriginalConstructor);
    JSFunction* privateFuncTypedArraySort = JSFunction::create(vm, this, 0, String(), typedArrayViewPrivateFuncSort);
    JSFunction* privateFuncIsTypedArrayView = JSFunction::create(vm, this, 0, String(), typedArrayViewPrivateFuncIsTypedArrayView, IsTypedArrayViewIntrinsic);
    JSFunction* privateFuncTypedArraySubarrayCreate = JSFunction::create(vm, this, 0, String(), typedArrayViewPrivateFuncSubarrayCreate);
    JSFunction* privateFuncIsBoundFunction = JSFunction::create(vm, this, 0, String(), isBoundFunction);
    JSFunction* privateFuncHasInstanceBoundFunction = JSFunction::create(vm, this, 0, String(), hasInstanceBoundFunction);
    JSFunction* privateFuncInstanceOf = JSFunction::create(vm, this, 0, String(), objectPrivateFuncInstanceOf);
    JSFunction* privateFuncThisTimeValue = JSFunction::create(vm, this, 0, String(), dateProtoFuncGetTime);
    JSFunction* privateFuncThisNumberValue = JSFunction::create(vm, this, 0, String(), numberProtoFuncValueOf);
    JSFunction* privateFuncIsArrayConstructor = JSFunction::create(vm, this, 0, String(), arrayConstructorPrivateFuncIsArrayConstructor);
    JSFunction* privateFuncIsArraySlow = JSFunction::create(vm, this, 0, String(), arrayConstructorPrivateFuncIsArraySlow);
    JSFunction* privateFuncConcatMemcpy = JSFunction::create(vm, this, 0, String(), arrayProtoPrivateFuncConcatMemcpy);
    JSFunction* privateFuncAppendMemcpy = JSFunction::create(vm, this, 0, String(), arrayProtoPrivateFuncAppendMemcpy);
    JSFunction* privateFuncMapBucketHead = JSFunction::create(vm, this, 0, String(), mapPrivateFuncMapBucketHead, JSMapBucketHeadIntrinsic);
    JSFunction* privateFuncMapBucketNext = JSFunction::create(vm, this, 0, String(), mapPrivateFuncMapBucketNext, JSMapBucketNextIntrinsic);
    JSFunction* privateFuncMapBucketKey = JSFunction::create(vm, this, 0, String(), mapPrivateFuncMapBucketKey, JSMapBucketKeyIntrinsic);
    JSFunction* privateFuncMapBucketValue = JSFunction::create(vm, this, 0, String(), mapPrivateFuncMapBucketValue, JSMapBucketValueIntrinsic);
    JSFunction* privateFuncSetBucketHead = JSFunction::create(vm, this, 0, String(), setPrivateFuncSetBucketHead, JSSetBucketHeadIntrinsic);
    JSFunction* privateFuncSetBucketNext = JSFunction::create(vm, this, 0, String(), setPrivateFuncSetBucketNext, JSSetBucketNextIntrinsic);
    JSFunction* privateFuncSetBucketKey = JSFunction::create(vm, this, 0, String(), setPrivateFuncSetBucketKey, JSSetBucketKeyIntrinsic);

    JSObject* regExpProtoFlagsGetterObject = getGetterById(exec, m_regExpPrototype.get(), vm.propertyNames->flags);
    catchScope.assertNoException();
    JSObject* regExpProtoGlobalGetterObject = getGetterById(exec, m_regExpPrototype.get(), vm.propertyNames->global);
    catchScope.assertNoException();
    m_regExpProtoGlobalGetter.set(vm, this, regExpProtoGlobalGetterObject);
    JSObject* regExpProtoIgnoreCaseGetterObject = getGetterById(exec, m_regExpPrototype.get(), vm.propertyNames->ignoreCase);
    catchScope.assertNoException();
    JSObject* regExpProtoMultilineGetterObject = getGetterById(exec, m_regExpPrototype.get(), vm.propertyNames->multiline);
    catchScope.assertNoException();
    JSObject* regExpProtoSourceGetterObject = getGetterById(exec, m_regExpPrototype.get(), vm.propertyNames->source);
    catchScope.assertNoException();
    JSObject* regExpProtoStickyGetterObject = getGetterById(exec, m_regExpPrototype.get(), vm.propertyNames->sticky);
    catchScope.assertNoException();
    JSObject* regExpProtoUnicodeGetterObject = getGetterById(exec, m_regExpPrototype.get(), vm.propertyNames->unicode);
    catchScope.assertNoException();
    m_regExpProtoUnicodeGetter.set(vm, this, regExpProtoUnicodeGetterObject);
    JSObject* builtinRegExpExec = asObject(m_regExpPrototype->getDirect(vm, vm.propertyNames->exec).asCell());
    m_regExpProtoExec.set(vm, this, builtinRegExpExec);
    JSObject* regExpSymbolReplace = asObject(m_regExpPrototype->getDirect(vm, vm.propertyNames->replaceSymbol).asCell());
    m_regExpProtoSymbolReplace.set(vm, this, regExpSymbolReplace);

#define CREATE_PRIVATE_GLOBAL_FUNCTION(name, code) JSFunction* name ## PrivateFunction = JSFunction::create(vm, code ## CodeGenerator(vm), this);
    JSC_FOREACH_BUILTIN_FUNCTION_PRIVATE_GLOBAL_NAME(CREATE_PRIVATE_GLOBAL_FUNCTION)
#undef CREATE_PRIVATE_GLOBAL_FUNCTION

    JSObject* arrayIteratorPrototype = ArrayIteratorPrototype::create(vm, this, ArrayIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    createArrayIteratorPrivateFunction->putDirect(vm, vm.propertyNames->prototype, arrayIteratorPrototype);

    JSObject* asyncFromSyncIteratorPrototype = AsyncFromSyncIteratorPrototype::create(vm, this, AsyncFromSyncIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    AsyncFromSyncIteratorConstructorPrivateFunction->putDirect(vm, vm.propertyNames->prototype, asyncFromSyncIteratorPrototype);

    JSObject* mapIteratorPrototype = MapIteratorPrototype::create(vm, this, MapIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    createMapIteratorPrivateFunction->putDirect(vm, vm.propertyNames->prototype, mapIteratorPrototype);

    JSObject* setIteratorPrototype = SetIteratorPrototype::create(vm, this, SetIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    createSetIteratorPrivateFunction->putDirect(vm, vm.propertyNames->prototype, setIteratorPrototype);

    GlobalPropertyInfo staticGlobals[] = {
#define INIT_PRIVATE_GLOBAL(name, code) GlobalPropertyInfo(vm.propertyNames->builtinNames().name ## PrivateName(), name ## PrivateFunction, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSC_FOREACH_BUILTIN_FUNCTION_PRIVATE_GLOBAL_NAME(INIT_PRIVATE_GLOBAL)
#undef INIT_PRIVATE_GLOBAL
        GlobalPropertyInfo(vm.propertyNames->NaN, jsNaN(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->Infinity, jsNumber(std::numeric_limits<double>::infinity()), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->undefinedKeyword, jsUndefined(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().getOwnPropertyNamesPrivateName(), privateFuncGetOwnPropertyNames, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().propertyIsEnumerablePrivateName(), privateFuncPropertyIsEnumerable, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().importModulePrivateName(), privateFuncImportModule, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().enqueueJobPrivateName(), JSFunction::create(vm, this, 0, String(), enqueueJob), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().ErrorPrivateName(), m_errorConstructor.get(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().RangeErrorPrivateName(), m_rangeErrorConstructor.get(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().TypeErrorPrivateName(), m_typeErrorConstructor.get(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().typedArrayLengthPrivateName(), privateFuncTypedArrayLength, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().typedArrayGetOriginalConstructorPrivateName(), privateFuncTypedArrayGetOriginalConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().typedArraySortPrivateName(), privateFuncTypedArraySort, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isTypedArrayViewPrivateName(), privateFuncIsTypedArrayView, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().typedArraySubarrayCreatePrivateName(), privateFuncTypedArraySubarrayCreate, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isBoundFunctionPrivateName(), privateFuncIsBoundFunction, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().hasInstanceBoundFunctionPrivateName(), privateFuncHasInstanceBoundFunction, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().instanceOfPrivateName(), privateFuncInstanceOf, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().BuiltinLogPrivateName(), builtinLog, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().BuiltinDescribePrivateName(), builtinDescribe, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().NumberPrivateName(), numberConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().RegExpPrivateName(), m_regExpConstructor.get(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().StringPrivateName(), stringConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().absPrivateName(), privateFuncAbs, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().floorPrivateName(), privateFuncFloor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().truncPrivateName(), privateFuncTrunc, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().PromisePrivateName(), promiseConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().ReflectPrivateName(), reflectObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().InternalPromisePrivateName(), internalPromiseConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),

        GlobalPropertyInfo(vm.propertyNames->builtinNames().repeatCharacterPrivateName(), JSFunction::create(vm, this, 2, String(), stringProtoFuncRepeatCharacter), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isArrayPrivateName(), arrayConstructor->getDirect(vm, vm.propertyNames->isArray), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isArraySlowPrivateName(), privateFuncIsArraySlow, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isArrayConstructorPrivateName(), privateFuncIsArrayConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().concatMemcpyPrivateName(), privateFuncConcatMemcpy, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().appendMemcpyPrivateName(), privateFuncAppendMemcpy, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),

        GlobalPropertyInfo(vm.propertyNames->builtinNames().hostPromiseRejectionTrackerPrivateName(), JSFunction::create(vm, this, 2, String(), globalFuncHostPromiseRejectionTracker), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().InspectorInstrumentationPrivateName(), InspectorInstrumentationObject::create(vm, this, InspectorInstrumentationObject::createStructure(vm, this, m_objectPrototype.get())), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().MapPrivateName(), mapConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().SetPrivateName(), setConstructor, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().thisTimeValuePrivateName(), privateFuncThisTimeValue, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().thisNumberValuePrivateName(), privateFuncThisNumberValue, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#if ENABLE(INTL)
        GlobalPropertyInfo(vm.propertyNames->builtinNames().CollatorPrivateName(), intl->getDirect(vm, vm.propertyNames->Collator), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().DateTimeFormatPrivateName(), intl->getDirect(vm, vm.propertyNames->DateTimeFormat), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().NumberFormatPrivateName(), intl->getDirect(vm, vm.propertyNames->NumberFormat), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().PluralRulesPrivateName(), intl->getDirect(vm, vm.propertyNames->PluralRules), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#endif // ENABLE(INTL)

        GlobalPropertyInfo(vm.propertyNames->builtinNames().isConstructorPrivateName(), JSFunction::create(vm, this, 1, String(), esSpecIsConstructor, NoIntrinsic), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),

        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpProtoFlagsGetterPrivateName(), regExpProtoFlagsGetterObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpProtoGlobalGetterPrivateName(), regExpProtoGlobalGetterObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpProtoIgnoreCaseGetterPrivateName(), regExpProtoIgnoreCaseGetterObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpProtoMultilineGetterPrivateName(), regExpProtoMultilineGetterObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpProtoSourceGetterPrivateName(), regExpProtoSourceGetterObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpProtoStickyGetterPrivateName(), regExpProtoStickyGetterObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpProtoUnicodeGetterPrivateName(), regExpProtoUnicodeGetterObject, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),

        // RegExp.prototype helpers.
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpBuiltinExecPrivateName(), builtinRegExpExec, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpCreatePrivateName(), JSFunction::create(vm, this, 2, String(), esSpecRegExpCreate, NoIntrinsic), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpMatchFastPrivateName(), JSFunction::create(vm, this, 1, String(), regExpProtoFuncMatchFast, RegExpMatchFastIntrinsic), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpSearchFastPrivateName(), JSFunction::create(vm, this, 1, String(), regExpProtoFuncSearchFast), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpSplitFastPrivateName(), JSFunction::create(vm, this, 2, String(), regExpProtoFuncSplitFast), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpPrototypeSymbolReplacePrivateName(), m_regExpPrototype->getDirect(vm, vm.propertyNames->replaceSymbol), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().regExpTestFastPrivateName(), JSFunction::create(vm, this, 1, String(), regExpProtoFuncTestFast, RegExpTestFastIntrinsic), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),

        // String.prototype helpers.
        GlobalPropertyInfo(vm.propertyNames->builtinNames().stringIncludesInternalPrivateName(), JSFunction::create(vm, this, 1, String(), builtinStringIncludesInternal), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().stringSplitFastPrivateName(), JSFunction::create(vm, this, 2, String(), stringProtoFuncSplitFast), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().stringSubstrInternalPrivateName(), JSFunction::create(vm, this, 2, String(), builtinStringSubstrInternal), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),

        // Function prototype helpers.
        GlobalPropertyInfo(vm.propertyNames->builtinNames().makeBoundFunctionPrivateName(), JSFunction::create(vm, this, 5, String(), makeBoundFunction), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().hasOwnLengthPropertyPrivateName(), JSFunction::create(vm, this, 1, String(), hasOwnLengthProperty), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),

        // Map and Set helpers.
        GlobalPropertyInfo(vm.propertyNames->builtinNames().mapBucketHeadPrivateName(), privateFuncMapBucketHead, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().mapBucketNextPrivateName(), privateFuncMapBucketNext, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().mapBucketKeyPrivateName(), privateFuncMapBucketKey, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().mapBucketValuePrivateName(), privateFuncMapBucketValue, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().setBucketHeadPrivateName(), privateFuncSetBucketHead, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().setBucketNextPrivateName(), privateFuncSetBucketNext, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().setBucketKeyPrivateName(), privateFuncSetBucketKey, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#if ENABLE(WEBASSEMBLY) && ENABLE(WEBASSEMBLY_STREAMING_API)
        // WebAssembly Streaming API
        GlobalPropertyInfo(vm.propertyNames->builtinNames().webAssemblyCompileStreamingInternalPrivateName(), JSFunction::create(vm, this, 1, String(), webAssemblyCompileStreamingInternal), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().webAssemblyInstantiateStreamingInternalPrivateName(), JSFunction::create(vm, this, 1, String(), webAssemblyInstantiateStreamingInternal), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#endif
#if !ASSERT_DISABLED
        GlobalPropertyInfo(vm.propertyNames->builtinNames().assertPrivateName(), JSFunction::create(vm, this, 1, String(), assertCall), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#endif
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
    
    m_specialPointers[Special::CallFunction] = m_callFunction.get();
    m_specialPointers[Special::ApplyFunction] = m_applyFunction.get();
    m_specialPointers[Special::ObjectConstructor] = objectConstructor;
    m_specialPointers[Special::ArrayConstructor] = arrayConstructor;

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::ThrowTypeErrorFunction)] = m_throwTypeErrorFunction.get();

    if (UNLIKELY(Options::useDollarVM()))
        exposeDollarVM(vm);

#if ENABLE(WEBASSEMBLY)
    if (Options::useWebAssembly()) {
        auto* webAssemblyPrototype = WebAssemblyPrototype::create(vm, this, WebAssemblyPrototype::createStructure(vm, this, m_objectPrototype.get()));
        m_webAssemblyStructure.set(vm, this, JSWebAssembly::createStructure(vm, this, webAssemblyPrototype));
        m_webAssemblyModuleRecordStructure.set(vm, this, WebAssemblyModuleRecord::createStructure(vm, this, m_objectPrototype.get()));
        m_webAssemblyFunctionStructure.set(vm, this, WebAssemblyFunction::createStructure(vm, this, m_functionPrototype.get()));
        m_webAssemblyWrapperFunctionStructure.set(vm, this, WebAssemblyWrapperFunction::createStructure(vm, this, m_functionPrototype.get()));
        m_webAssemblyToJSCalleeStructure.set(vm, this, WebAssemblyToJSCallee::createStructure(vm, this, jsNull()));
        auto* webAssembly = JSWebAssembly::create(vm, this, m_webAssemblyStructure.get());
        putDirectWithoutTransition(vm, Identifier::fromString(exec, "WebAssembly"), webAssembly, static_cast<unsigned>(PropertyAttribute::DontEnum));

#define CREATE_WEBASSEMBLY_CONSTRUCTOR(capitalName, lowerName, properName, instanceType, jsName, prototypeBase) do { \
        typedef capitalName ## Prototype Prototype; \
        typedef capitalName ## Constructor Constructor; \
        typedef JS ## capitalName JSObj; \
        auto* base = m_ ## prototypeBase ## Prototype.get(); \
        auto* prototype = Prototype::create(vm, this, Prototype::createStructure(vm, this, base)); \
        auto* structure = JSObj::createStructure(vm, this, prototype); \
        auto* constructor = Constructor::create(vm, Constructor::createStructure(vm, this, this->functionPrototype()), prototype); \
        prototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \
        m_ ## lowerName ## Prototype.set(vm, this, prototype); \
        m_ ## properName ## Structure.set(vm, this, structure); \
        webAssembly->putDirectWithoutTransition(vm, Identifier::fromString(this->globalExec(), #jsName), constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \
    } while (0);

        FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(CREATE_WEBASSEMBLY_CONSTRUCTOR)

#undef CREATE_WEBASSEMBLY_CONSTRUCTOR
    }
#endif // ENABLE(WEBASSEMBLY)

    {
        ExecState* exec = globalExec();

        auto setupAdaptiveWatchpoint = [&] (JSObject* base, const Identifier& ident) -> ObjectPropertyCondition {
            // Performing these gets should not throw.
            PropertySlot slot(base, PropertySlot::InternalMethodType::Get);
            bool result = base->getOwnPropertySlot(base, exec, ident, slot);
            ASSERT_UNUSED(result, result);
            catchScope.assertNoException();
            RELEASE_ASSERT(slot.isCacheableValue());
            JSValue functionValue = slot.getValue(exec, ident);
            catchScope.assertNoException();
            ASSERT(jsDynamicCast<JSFunction*>(vm, functionValue));

            ObjectPropertyCondition condition = generateConditionForSelfEquivalence(m_vm, nullptr, base, ident.impl());
            RELEASE_ASSERT(condition.requiredValue() == functionValue);

            bool isWatchable = condition.isWatchable(PropertyCondition::EnsureWatchability);
            RELEASE_ASSERT(isWatchable); // We allow this to install the necessary watchpoints.

            return condition;
        };

        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(arrayIteratorPrototype, m_vm.propertyNames->next);
            m_arrayIteratorPrototypeNext = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_arrayIteratorProtocolWatchpoint);
            m_arrayIteratorPrototypeNext->install(vm);
        }
        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this->arrayPrototype(), m_vm.propertyNames->iteratorSymbol);
            m_arrayPrototypeSymbolIteratorWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_arrayIteratorProtocolWatchpoint);
            m_arrayPrototypeSymbolIteratorWatchpoint->install(vm);
        }

        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(mapIteratorPrototype, m_vm.propertyNames->next);
            m_mapIteratorPrototypeNextWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_mapIteratorProtocolWatchpoint);
            m_mapIteratorPrototypeNextWatchpoint->install(vm);
        }
        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(m_mapPrototype.get(), m_vm.propertyNames->iteratorSymbol);
            m_mapPrototypeSymbolIteratorWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_mapIteratorProtocolWatchpoint);
            m_mapPrototypeSymbolIteratorWatchpoint->install(vm);
        }

        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(setIteratorPrototype, m_vm.propertyNames->next);
            m_setIteratorPrototypeNextWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_setIteratorProtocolWatchpoint);
            m_setIteratorPrototypeNextWatchpoint->install(vm);
        }
        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(m_setPrototype.get(), m_vm.propertyNames->iteratorSymbol);
            m_setPrototypeSymbolIteratorWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_setIteratorProtocolWatchpoint);
            m_setPrototypeSymbolIteratorWatchpoint->install(vm);
        }

        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(m_stringIteratorPrototype.get(), m_vm.propertyNames->next);
            m_stringIteratorPrototypeNextWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_stringIteratorProtocolWatchpoint);
            m_stringIteratorPrototypeNextWatchpoint->install(vm);
        }
        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(m_stringPrototype.get(), m_vm.propertyNames->iteratorSymbol);
            m_stringPrototypeSymbolIteratorWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_stringIteratorProtocolWatchpoint);
            m_stringPrototypeSymbolIteratorWatchpoint->install(vm);
        }

        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(m_mapPrototype.get(), m_vm.propertyNames->set);
            m_mapPrototypeSetWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_mapSetWatchpoint);
            m_mapPrototypeSetWatchpoint->install(vm);
        }

        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(m_setPrototype.get(), m_vm.propertyNames->add);
            m_setPrototypeAddWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_setAddWatchpoint);
            m_setPrototypeAddWatchpoint->install(vm);
        }

        {
            ObjectPropertyCondition condition = setupAdaptiveWatchpoint(numberPrototype(), m_vm.propertyNames->toString);
            m_numberPrototypeToStringWatchpoint = std::make_unique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(condition, m_numberToStringWatchpoint);
            m_numberPrototypeToStringWatchpoint->install(vm);
            m_numberProtoToStringFunction.set(vm, this, jsCast<JSFunction*>(numberPrototype()->getDirect(vm, vm.propertyNames->toString)));
        }
    }

    resetPrototype(vm, getPrototypeDirect(vm));
}

bool JSGlobalObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        RELEASE_AND_RETURN(scope, ordinarySetSlow(exec, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode()));

    bool shouldThrowReadOnlyError = slot.isStrictMode();
    bool ignoreReadOnlyErrors = false;
    bool putResult = false;
    bool done = symbolTablePutTouchWatchpointSet(thisObject, exec, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
    EXCEPTION_ASSERT((!!scope.exception() == (done && !putResult)) || !shouldThrowReadOnlyError);
    if (done)
        return putResult;
    RELEASE_AND_RETURN(scope, Base::put(thisObject, exec, propertyName, value, slot));
}

bool JSGlobalObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry);
    // silently ignore attempts to add accessors aliasing vars.
    if (descriptor.isAccessorDescriptor() && symbolTableGet(thisObject, propertyName, slot))
        return false;
    return Base::defineOwnProperty(thisObject, exec, propertyName, descriptor, shouldThrow);
}

void JSGlobalObject::addGlobalVar(const Identifier& ident)
{
    ConcurrentJSLocker locker(symbolTable()->m_lock);
    SymbolTableEntry entry = symbolTable()->get(locker, ident.impl());
    if (!entry.isNull())
        return;
    
    ScopeOffset offset = symbolTable()->takeNextScopeOffset(locker);
    SymbolTableEntry newEntry(VarOffset(offset), 0);
    newEntry.prepareToWatch();
    symbolTable()->add(locker, ident.impl(), WTFMove(newEntry));
    
    ScopeOffset offsetForAssert = addVariables(1, jsUndefined());
    RELEASE_ASSERT(offsetForAssert == offset);
}

void JSGlobalObject::addFunction(ExecState* exec, const Identifier& propertyName)
{
    VM& vm = exec->vm();
    VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
    methodTable(vm)->deleteProperty(this, exec, propertyName);
    addGlobalVar(propertyName);
}

void JSGlobalObject::setGlobalScopeExtension(JSScope* scope)
{
    m_globalScopeExtension.set(vm(), this, scope);
}

void JSGlobalObject::clearGlobalScopeExtension()
{
    m_globalScopeExtension.clear();
}

static inline JSObject* lastInPrototypeChain(VM& vm, JSObject* object)
{
    JSObject* o = object;
    while (o->getPrototypeDirect(vm).isObject())
        o = asObject(o->getPrototypeDirect(vm));
    return o;
}

// Private namespace for helpers for JSGlobalObject::haveABadTime()
namespace {

class GlobalObjectDependencyFinder : public MarkedBlock::VoidFunctor {
public:
    GlobalObjectDependencyFinder(VM& vm)
        : m_vm(vm)
    { }

    IterationStatus operator()(HeapCell*, HeapCell::Kind) const;

    void addDependency(JSGlobalObject* key, JSGlobalObject* dependent);
    HashSet<JSGlobalObject*>* dependentsFor(JSGlobalObject* key);

private:
    void visit(JSObject*);

    VM& m_vm;
    HashMap<JSGlobalObject*, HashSet<JSGlobalObject*>> m_dependencies;
};

inline void GlobalObjectDependencyFinder::addDependency(JSGlobalObject* key, JSGlobalObject* dependent)
{
    auto keyResult = m_dependencies.add(key, HashSet<JSGlobalObject*>());
    keyResult.iterator->value.add(dependent);
}

inline HashSet<JSGlobalObject*>* GlobalObjectDependencyFinder::dependentsFor(JSGlobalObject* key)
{
    auto iterator = m_dependencies.find(key);
    if (iterator == m_dependencies.end())
        return nullptr;
    return &iterator->value;
}

inline void GlobalObjectDependencyFinder::visit(JSObject* object)
{
    VM& vm = m_vm;

    if (!object->mayBePrototype())
        return;

    JSObject* current = object;
    JSGlobalObject* objectGlobalObject = object->globalObject(vm);
    do {
        JSValue prototypeValue = current->getPrototypeDirect(vm);
        if (prototypeValue.isNull())
            return;
        current = asObject(prototypeValue);

        JSGlobalObject* protoGlobalObject = current->globalObject(vm);
        if (protoGlobalObject != objectGlobalObject)
            addDependency(protoGlobalObject, objectGlobalObject);
    } while (true);
}

IterationStatus GlobalObjectDependencyFinder::operator()(HeapCell* cell, HeapCell::Kind kind) const
{
    if (isJSCellKind(kind) && static_cast<JSCell*>(cell)->isObject()) {
        // FIXME: This const_cast exists because this isn't a C++ lambda.
        // https://bugs.webkit.org/show_bug.cgi?id=159644
        const_cast<GlobalObjectDependencyFinder*>(this)->visit(jsCast<JSObject*>(static_cast<JSCell*>(cell)));
    }
    return IterationStatus::Continue;
}

enum class BadTimeFinderMode {
    SingleGlobal,
    MultipleGlobals
};

template<BadTimeFinderMode mode>
class ObjectsWithBrokenIndexingFinder : public MarkedBlock::VoidFunctor {
public:
    ObjectsWithBrokenIndexingFinder(VM&, Vector<JSObject*>&, JSGlobalObject*);
    ObjectsWithBrokenIndexingFinder(VM&, Vector<JSObject*>&, HashSet<JSGlobalObject*>&);

    bool needsMultiGlobalsScan() const { return m_needsMultiGlobalsScan; }
    IterationStatus operator()(HeapCell*, HeapCell::Kind) const;

private:
    IterationStatus visit(JSObject*);

    VM& m_vm;
    Vector<JSObject*>& m_foundObjects;
    JSGlobalObject* m_globalObject { nullptr }; // Only used for SingleBadTimeGlobal mode.
    HashSet<JSGlobalObject*>* m_globalObjects { nullptr }; // Only used for BadTimeGlobalGraph mode;
    bool m_needsMultiGlobalsScan { false };
};

template<>
ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::SingleGlobal>::ObjectsWithBrokenIndexingFinder(
    VM& vm, Vector<JSObject*>& foundObjects, JSGlobalObject* globalObject)
    : m_vm(vm)
    , m_foundObjects(foundObjects)
    , m_globalObject(globalObject)
{
}

template<>
ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::MultipleGlobals>::ObjectsWithBrokenIndexingFinder(
    VM& vm, Vector<JSObject*>& foundObjects, HashSet<JSGlobalObject*>& globalObjects)
    : m_vm(vm)
    , m_foundObjects(foundObjects)
    , m_globalObjects(&globalObjects)
{
}

inline bool hasBrokenIndexing(IndexingType type)
{
    return type && !hasSlowPutArrayStorage(type);
}

inline bool hasBrokenIndexing(JSObject* object)
{
    IndexingType type = object->indexingType();
    return hasBrokenIndexing(type);
}

template<BadTimeFinderMode mode>
inline IterationStatus ObjectsWithBrokenIndexingFinder<mode>::visit(JSObject* object)
{
    VM& vm = m_vm;

    // We only want to have a bad time in the affected global object, not in the entire
    // VM. But we have to be careful, since there may be objects that claim to belong to
    // a different global object that have prototypes from our global object.
    auto isInAffectedGlobalObject = [&] (JSObject* object) {
        JSGlobalObject* objectGlobalObject { nullptr };
        bool objectMayBePrototype { false };

        if (mode == BadTimeFinderMode::SingleGlobal) {
            objectGlobalObject = object->globalObject(vm);
            if (objectGlobalObject == m_globalObject)
                return true;

            objectMayBePrototype = object->mayBePrototype();
        }

        for (JSObject* current = object; ;) {
            JSGlobalObject* currentGlobalObject = current->globalObject(vm);
            if (mode == BadTimeFinderMode::SingleGlobal) {
                if (objectMayBePrototype && currentGlobalObject != objectGlobalObject)
                    m_needsMultiGlobalsScan = true;
                if (currentGlobalObject == m_globalObject)
                    return true;
            } else {
                if (m_globalObjects->contains(currentGlobalObject))
                    return true;
            }

            JSValue prototypeValue = current->getPrototypeDirect(vm);
            if (prototypeValue.isNull())
                return false;
            current = asObject(prototypeValue);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    if (JSFunction* function = jsDynamicCast<JSFunction*>(vm, object)) {
        if (FunctionRareData* rareData = function->rareData()) {
            // We only use this to cache JSFinalObjects. They do not start off with a broken indexing type.
            ASSERT(!(rareData->objectAllocationStructure() && hasBrokenIndexing(rareData->objectAllocationStructure()->indexingType())));

            if (Structure* structure = rareData->internalFunctionAllocationStructure()) {
                if (hasBrokenIndexing(structure->indexingType())) {
                    bool isRelevantGlobalObject =
                        (mode == BadTimeFinderMode::SingleGlobal
                            ? m_globalObject == structure->globalObject()
                            : m_globalObjects->contains(structure->globalObject()))
                        || (structure->hasMonoProto() && !structure->storedPrototype().isNull() && isInAffectedGlobalObject(asObject(structure->storedPrototype())));
                    if (mode == BadTimeFinderMode::SingleGlobal && m_needsMultiGlobalsScan)
                        return IterationStatus::Done; // Bailing early and let the MultipleGlobals path handle everything.
                    if (isRelevantGlobalObject)
                        rareData->clearInternalFunctionAllocationProfile();
                }
            }
        }
    }

    // Run this filter first, since it's cheap, and ought to filter out a lot of objects.
    if (!hasBrokenIndexing(object))
        return IterationStatus::Continue;

    if (isInAffectedGlobalObject(object))
        m_foundObjects.append(object);

    if (mode == BadTimeFinderMode::SingleGlobal && m_needsMultiGlobalsScan)
        return IterationStatus::Done; // Bailing early and let the MultipleGlobals path handle everything.

    return IterationStatus::Continue;
}

template<BadTimeFinderMode mode>
IterationStatus ObjectsWithBrokenIndexingFinder<mode>::operator()(HeapCell* cell, HeapCell::Kind kind) const
{
    if (isJSCellKind(kind) && static_cast<JSCell*>(cell)->isObject()) {
        // FIXME: This const_cast exists because this isn't a C++ lambda.
        // https://bugs.webkit.org/show_bug.cgi?id=159644
        return const_cast<ObjectsWithBrokenIndexingFinder*>(this)->visit(jsCast<JSObject*>(static_cast<JSCell*>(cell)));
    }
    return IterationStatus::Continue;
}

} // end private namespace for helpers for JSGlobalObject::haveABadTime()

void JSGlobalObject::fireWatchpointAndMakeAllArrayStructuresSlowPut(VM& vm)
{
    if (isHavingABadTime())
        return;

    // Make sure that all allocations or indexed storage transitions that are inlining
    // the assumption that it's safe to transition to a non-SlowPut array storage don't
    // do so anymore.
    m_havingABadTimeWatchpoint->fireAll(vm, "Having a bad time");
    ASSERT(isHavingABadTime()); // The watchpoint is what tells us that we're having a bad time.
    
    // Make sure that all JSArray allocations that load the appropriate structure from
    // this object now load a structure that uses SlowPut.
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i].set(vm, this, originalArrayStructureForIndexingType(ArrayWithSlowPutArrayStorage));

    // Same for any special array structures.
    Structure* slowPutStructure;
    slowPutStructure = createRegExpMatchesArraySlowPutStructure(vm, this);
    m_regExpMatchesArrayStructure.set(vm, this, slowPutStructure);
    slowPutStructure = createRegExpMatchesArrayWithGroupsSlowPutStructure(vm, this);
    m_regExpMatchesArrayWithGroupsStructure.set(vm, this, slowPutStructure);
    slowPutStructure = ClonedArguments::createSlowPutStructure(vm, this, m_objectPrototype.get());
    m_clonedArgumentsStructure.set(vm, this, slowPutStructure);
};

void JSGlobalObject::haveABadTime(VM& vm)
{
    ASSERT(&vm == &this->vm());
    
    if (isHavingABadTime())
        return;

    vm.structureCache.clear(); // We may be caching array structures in here.

    DeferGC deferGC(vm.heap);

    // Consider the following objects and prototype chains:
    //    O (of global G1) -> A (of global G1)
    //    B (of global G2) where G2 has a bad time
    //
    // If we set B as the prototype of A, G1 will need to have a bad time.
    // See comments in Structure::mayInterceptIndexedAccesses() for why.
    //
    // Now, consider the following objects and prototype chains:
    //    O1 (of global G1) -> A1 (of global G1) -> B1 (of global G2)
    //    O2 (of global G2) -> A2 (of global G2)
    //    B2 (of global G3) where G3 has a bad time.
    //
    // G1 and G2 does not have a bad time, but G3 already has a bad time.
    // If we set B2 as the prototype of A2, then G2 needs to have a bad time.
    // Note that by induction, G1 also now needs to have a bad time because of
    // O1 -> A1 -> B1.
    //
    // We describe this as global G1 being affected by global G2, and G2 by G3.
    // Similarly, we say that G1 is dependent on G2, and G2 on G3.
    // Hence, when G3 has a bad time, we need to ensure that all globals that
    // are transitively dependent on it also have a bad time (G2 and G1 in this
    // example).
    //
    // Apart from clearing the VM structure cache above, there are 2 more things
    // that we have to do when globals have a bad time:
    // 1. For each affected global:
    //    a. Fire its HaveABadTime watchpoint.
    //    b. Convert all of its array structures to SlowPutArrayStorage.
    // 2. Make sure that all affected objects  switch to the slow kind of
    //    indexed storage. An object is considered to be affected if it has
    //    indexed storage and has a prototype object which may have indexed
    //    accessors. If the prototype object belongs to a global having a bad
    //    time, then the prototype object is considered to possibly have indexed
    //    accessors. See comments in Structure::mayInterceptIndexedAccesses()
    //    for details.
    //
    // Note: step 1 must be completed before step 2 because step 2 relies on
    // the HaveABadTime watchpoint having already been fired on all affected
    // globals.
    //
    // In the common case, only this global will start having a bad time here,
    // and no other globals are affected by it. So, we first proceed on this assumption
    // with a simpler ObjectsWithBrokenIndexingFinder scan to find heap objects
    // affected by this global that need to be converted to SlowPutArrayStorage.
    // We'll also have the finder check for the presence of other global objects
    // depending on this one.
    //
    // If we do discover other globals depending on this one, we'll abort this
    // first ObjectsWithBrokenIndexingFinder scan because it will be insufficient
    // to find all affected objects that need to be converted to SlowPutArrayStorage.
    // It also does not make dependent globals have a bad time. Instead, we'll
    // take a more comprehensive approach of first creating a dependency graph
    // between globals, and then using that graph to determine all affected
    // globals and objects. With that, we can make all affected globals have a
    // bad time, and convert all affected objects to SlowPutArrayStorage.

    fireWatchpointAndMakeAllArrayStructuresSlowPut(vm); // Step 1 above.
    
    Vector<JSObject*> foundObjects;
    ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::SingleGlobal> finder(vm, foundObjects, this);
    {
        HeapIterationScope iterationScope(vm.heap);
        vm.heap.objectSpace().forEachLiveCell(iterationScope, finder); // Attempt step 2 above.
    }

    if (finder.needsMultiGlobalsScan()) {
        foundObjects.clear();

        // Find all globals that will also have a bad time as a side effect of
        // this global having a bad time.
        GlobalObjectDependencyFinder dependencies(vm);
        {
            HeapIterationScope iterationScope(vm.heap);
            vm.heap.objectSpace().forEachLiveCell(iterationScope, dependencies);
        }

        HashSet<JSGlobalObject*> globalsHavingABadTime;
        Deque<JSGlobalObject*> globals;

        globals.append(this);
        while (!globals.isEmpty()) {
            JSGlobalObject* global = globals.takeFirst();
            global->fireWatchpointAndMakeAllArrayStructuresSlowPut(vm); // Step 1 above.
            auto result = globalsHavingABadTime.add(global);
            if (result.isNewEntry) {
                if (HashSet<JSGlobalObject*>* dependents = dependencies.dependentsFor(global)) {
                    for (JSGlobalObject* dependentGlobal : *dependents)
                        globals.append(dependentGlobal);
                }
            }
        }

        ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::MultipleGlobals> finder(vm, foundObjects, globalsHavingABadTime);
        {
            HeapIterationScope iterationScope(vm.heap);
            vm.heap.objectSpace().forEachLiveCell(iterationScope, finder); // Step 2 above.
        }
    }

    while (!foundObjects.isEmpty()) {
        JSObject* object = asObject(foundObjects.last());
        foundObjects.removeLast();
        ASSERT(hasBrokenIndexing(object));
        object->switchToSlowPutArrayStorage(vm);
    }
}

// Set prototype, and also insert the object prototype at the end of the chain.
void JSGlobalObject::resetPrototype(VM& vm, JSValue prototype)
{
    setPrototypeDirect(vm, prototype);

    JSObject* oldLastInPrototypeChain = lastInPrototypeChain(vm, this);
    JSObject* objectPrototype = m_objectPrototype.get();
    if (oldLastInPrototypeChain != objectPrototype)
        oldLastInPrototypeChain->setPrototypeDirect(vm, objectPrototype);

    // Whenever we change the prototype of the global object, we need to create a new JSProxy with the correct prototype.
    setGlobalThis(vm, JSProxy::create(vm, JSProxy::createStructure(vm, this, prototype, PureForwardingProxyType), this));
}

void JSGlobalObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{ 
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_globalThis);

    visitor.append(thisObject->m_globalLexicalEnvironment);
    visitor.append(thisObject->m_globalScopeExtension);
    visitor.append(thisObject->m_globalCallee);
    visitor.append(thisObject->m_stackOverflowFrameCallee);
    visitor.append(thisObject->m_regExpConstructor);
    visitor.append(thisObject->m_errorConstructor);
    visitor.append(thisObject->m_nativeErrorPrototypeStructure);
    visitor.append(thisObject->m_nativeErrorStructure);
    thisObject->m_evalErrorConstructor.visit(visitor);
    visitor.append(thisObject->m_rangeErrorConstructor);
    thisObject->m_referenceErrorConstructor.visit(visitor);
    thisObject->m_syntaxErrorConstructor.visit(visitor);
    visitor.append(thisObject->m_typeErrorConstructor);
    thisObject->m_URIErrorConstructor.visit(visitor);
    visitor.append(thisObject->m_objectConstructor);
    visitor.append(thisObject->m_promiseConstructor);

    visitor.append(thisObject->m_nullGetterFunction);
    visitor.append(thisObject->m_nullSetterFunction);

    visitor.append(thisObject->m_parseIntFunction);
    visitor.append(thisObject->m_parseFloatFunction);
    visitor.append(thisObject->m_evalFunction);
    visitor.append(thisObject->m_callFunction);
    visitor.append(thisObject->m_applyFunction);
    visitor.append(thisObject->m_throwTypeErrorFunction);
    thisObject->m_arrayProtoToStringFunction.visit(visitor);
    thisObject->m_arrayProtoValuesFunction.visit(visitor);
    thisObject->m_initializePromiseFunction.visit(visitor);
    thisObject->m_iteratorProtocolFunction.visit(visitor);
    thisObject->m_promiseResolveFunction.visit(visitor);
    visitor.append(thisObject->m_objectProtoValueOfFunction);
    visitor.append(thisObject->m_numberProtoToStringFunction);
    visitor.append(thisObject->m_newPromiseCapabilityFunction);
    visitor.append(thisObject->m_functionProtoHasInstanceSymbolFunction);
    thisObject->m_throwTypeErrorGetterSetter.visit(visitor);
    visitor.append(thisObject->m_throwTypeErrorArgumentsCalleeAndCallerGetterSetter);
    thisObject->m_moduleLoader.visit(visitor);

    visitor.append(thisObject->m_objectPrototype);
    visitor.append(thisObject->m_functionPrototype);
    visitor.append(thisObject->m_arrayPrototype);
    visitor.append(thisObject->m_errorPrototype);
    visitor.append(thisObject->m_iteratorPrototype);
    visitor.append(thisObject->m_generatorFunctionPrototype);
    visitor.append(thisObject->m_generatorPrototype);
    visitor.append(thisObject->m_asyncFunctionPrototype);
    visitor.append(thisObject->m_asyncGeneratorPrototype);
    visitor.append(thisObject->m_asyncIteratorPrototype);
    visitor.append(thisObject->m_asyncGeneratorFunctionPrototype);

    thisObject->m_debuggerScopeStructure.visit(visitor);
    thisObject->m_withScopeStructure.visit(visitor);
    visitor.append(thisObject->m_strictEvalActivationStructure);
    visitor.append(thisObject->m_lexicalEnvironmentStructure);
    thisObject->m_moduleEnvironmentStructure.visit(visitor);
    visitor.append(thisObject->m_directArgumentsStructure);
    visitor.append(thisObject->m_scopedArgumentsStructure);
    visitor.append(thisObject->m_clonedArgumentsStructure);
    visitor.append(thisObject->m_objectStructureForObjectConstructor);
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        visitor.append(thisObject->m_originalArrayStructureForIndexingShape[i]);
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        visitor.append(thisObject->m_arrayStructureForIndexingShapeDuringAllocation[i]);
    thisObject->m_callbackConstructorStructure.visit(visitor);
    thisObject->m_callbackFunctionStructure.visit(visitor);
    thisObject->m_callbackObjectStructure.visit(visitor);
#if JSC_OBJC_API_ENABLED
    thisObject->m_objcCallbackFunctionStructure.visit(visitor);
    thisObject->m_objcWrapperObjectStructure.visit(visitor);
#endif
#ifdef JSC_GLIB_API_ENABLED
    thisObject->m_glibCallbackFunctionStructure.visit(visitor);
    thisObject->m_glibWrapperObjectStructure.visit(visitor);
#endif
    visitor.append(thisObject->m_nullPrototypeObjectStructure);
    visitor.append(thisObject->m_errorStructure);
    visitor.append(thisObject->m_calleeStructure);

    visitor.append(thisObject->m_hostFunctionStructure);
    auto visitFunctionStructures = [&] (FunctionStructures& structures) {
        visitor.append(structures.arrowFunctionStructure);
        visitor.append(structures.sloppyFunctionStructure);
        visitor.append(structures.strictFunctionStructure);
    };
    visitFunctionStructures(thisObject->m_builtinFunctions);
    visitFunctionStructures(thisObject->m_ordinaryFunctions);

    thisObject->m_customGetterSetterFunctionStructure.visit(visitor);
    thisObject->m_boundFunctionStructure.visit(visitor);
    visitor.append(thisObject->m_getterSetterStructure);
    thisObject->m_nativeStdFunctionStructure.visit(visitor);
    visitor.append(thisObject->m_bigIntObjectStructure);
    visitor.append(thisObject->m_symbolObjectStructure);
    visitor.append(thisObject->m_regExpStructure);
    visitor.append(thisObject->m_generatorFunctionStructure);
    visitor.append(thisObject->m_asyncFunctionStructure);
    visitor.append(thisObject->m_asyncGeneratorFunctionStructure);
    visitor.append(thisObject->m_iteratorResultObjectStructure);
    visitor.append(thisObject->m_regExpMatchesArrayStructure);
    visitor.append(thisObject->m_regExpMatchesArrayWithGroupsStructure);
    visitor.append(thisObject->m_moduleRecordStructure);
    visitor.append(thisObject->m_moduleNamespaceObjectStructure);
    visitor.append(thisObject->m_proxyObjectStructure);
    visitor.append(thisObject->m_callableProxyObjectStructure);
    visitor.append(thisObject->m_proxyRevokeStructure);
    
    visitor.append(thisObject->m_arrayBufferPrototype);
    visitor.append(thisObject->m_arrayBufferStructure);
#if ENABLE(SHARED_ARRAY_BUFFER)
    visitor.append(thisObject->m_sharedArrayBufferPrototype);
    visitor.append(thisObject->m_sharedArrayBufferStructure);
#endif

#define VISIT_SIMPLE_TYPE(CapitalName, lowerName, properName, instanceType, jsName, prototypeBase) do { \
        visitor.append(thisObject->m_ ## lowerName ## Prototype); \
        visitor.append(thisObject->m_ ## properName ## Structure); \
    } while (0);

    FOR_EACH_SIMPLE_BUILTIN_TYPE(VISIT_SIMPLE_TYPE)
    if (UNLIKELY(Options::useBigInt()))
        FOR_BIG_INT_BUILTIN_TYPE_WITH_CONSTRUCTOR(VISIT_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(VISIT_SIMPLE_TYPE)
    
#if ENABLE(WEBASSEMBLY)
    visitor.append(thisObject->m_webAssemblyStructure);
    visitor.append(thisObject->m_webAssemblyModuleRecordStructure);
    visitor.append(thisObject->m_webAssemblyFunctionStructure);
    visitor.append(thisObject->m_webAssemblyWrapperFunctionStructure);
    visitor.append(thisObject->m_webAssemblyToJSCalleeStructure);
    FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(VISIT_SIMPLE_TYPE)
#endif // ENABLE(WEBASSEMBLY)

#undef VISIT_SIMPLE_TYPE

#define VISIT_LAZY_TYPE(CapitalName, lowerName, properName, instanceType, jsName, prototypeBase) \
    thisObject->m_ ## properName ## Structure.visit(visitor);
    
    FOR_EACH_LAZY_BUILTIN_TYPE(VISIT_LAZY_TYPE)

#undef VISIT_LAZY_TYPE

    for (unsigned i = NumberOfTypedArrayTypes; i--;)
        thisObject->lazyTypedArrayStructure(indexToTypedArrayType(i)).visit(visitor);
    
    visitor.append(thisObject->m_speciesGetterSetter);
    thisObject->m_typedArrayProto.visit(visitor);
    thisObject->m_typedArraySuperConstructor.visit(visitor);
}

ExecState* JSGlobalObject::globalExec()
{
    return CallFrame::create(m_globalCallFrame);
}

void JSGlobalObject::exposeDollarVM(VM& vm)
{
    if (hasOwnProperty(globalExec(), vm.propertyNames->builtinNames().dollarVMPrivateName()))
        return;

    JSDollarVM* dollarVM = JSDollarVM::create(vm, JSDollarVM::createStructure(vm, this, m_objectPrototype.get()));

    GlobalPropertyInfo extraStaticGlobals[] = {
        GlobalPropertyInfo(vm.propertyNames->builtinNames().dollarVMPrivateName(), dollarVM, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };
    addStaticGlobals(extraStaticGlobals, WTF_ARRAY_LENGTH(extraStaticGlobals));

    putDirect(vm, Identifier::fromString(globalExec(), "$vm"), dollarVM, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

void JSGlobalObject::addStaticGlobals(GlobalPropertyInfo* globals, int count)
{
    ScopeOffset startOffset = addVariables(count, jsUndefined());

    for (int i = 0; i < count; ++i) {
        GlobalPropertyInfo& global = globals[i];
        // This `configurable = false` is necessary condition for static globals,
        // otherwise lexical bindings can change the result of GlobalVar queries too.
        // We won't be able to declare a global lexical variable with the sanem name to
        // the static globals because configurable = false.
        ASSERT(global.attributes & PropertyAttribute::DontDelete);
        
        WatchpointSet* watchpointSet = nullptr;
        WriteBarrierBase<Unknown>* variable = nullptr;
        {
            ConcurrentJSLocker locker(symbolTable()->m_lock);
            ScopeOffset offset = symbolTable()->takeNextScopeOffset(locker);
            RELEASE_ASSERT(offset == startOffset + i);
            SymbolTableEntry newEntry(VarOffset(offset), global.attributes);
            newEntry.prepareToWatch();
            watchpointSet = newEntry.watchpointSet();
            symbolTable()->add(locker, global.identifier.impl(), WTFMove(newEntry));
            variable = &variableAt(offset);
        }
        symbolTablePutTouchWatchpointSet(vm(), this, global.identifier, global.value, variable, watchpointSet);
    }
}

bool JSGlobalObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    if (Base::getOwnPropertySlot(object, exec, propertyName, slot))
        return true;
    return symbolTableGet(jsCast<JSGlobalObject*>(object), propertyName, slot);
}

void JSGlobalObject::clearRareData(JSCell* cell)
{
    jsCast<JSGlobalObject*>(cell)->m_rareData = nullptr;
}

void slowValidateCell(JSGlobalObject* globalObject)
{
    RELEASE_ASSERT(globalObject->isGlobalObject());
    ASSERT_GC_OBJECT_INHERITS(globalObject, JSGlobalObject::info());
}

void JSGlobalObject::setRemoteDebuggingEnabled(bool enabled)
{
#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->setRemoteDebuggingAllowed(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

bool JSGlobalObject::remoteDebuggingEnabled() const
{
#if ENABLE(REMOTE_INSPECTOR)
    return m_inspectorDebuggable->remoteDebuggingAllowed();
#else
    return false;
#endif
}

void JSGlobalObject::setName(const String& name)
{
    m_name = name;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->update();
#endif
}

# if ENABLE(INTL)
static void addMissingScriptLocales(HashSet<String>& availableLocales)
{
    if (availableLocales.contains("pa-Arab-PK"))
        availableLocales.add("pa-PK"_s);
    if (availableLocales.contains("zh-Hans-CN"))
        availableLocales.add("zh-CN"_s);
    if (availableLocales.contains("zh-Hant-HK"))
        availableLocales.add("zh-HK"_s);
    if (availableLocales.contains("zh-Hans-SG"))
        availableLocales.add("zh-SG"_s);
    if (availableLocales.contains("zh-Hant-TW"))
        availableLocales.add("zh-TW"_s);
}

const HashSet<String>& JSGlobalObject::intlCollatorAvailableLocales()
{
    if (m_intlCollatorAvailableLocales.isEmpty()) {
        int32_t count = ucol_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = convertICULocaleToBCP47LanguageTag(ucol_getAvailable(i));
            if (!locale.isEmpty())
                m_intlCollatorAvailableLocales.add(locale);
        }
        addMissingScriptLocales(m_intlCollatorAvailableLocales);
    }
    return m_intlCollatorAvailableLocales;
}

const HashSet<String>& JSGlobalObject::intlDateTimeFormatAvailableLocales()
{
    if (m_intlDateTimeFormatAvailableLocales.isEmpty()) {
        int32_t count = udat_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = convertICULocaleToBCP47LanguageTag(udat_getAvailable(i));
            if (!locale.isEmpty())
                m_intlDateTimeFormatAvailableLocales.add(locale);
        }
        addMissingScriptLocales(m_intlDateTimeFormatAvailableLocales);
    }
    return m_intlDateTimeFormatAvailableLocales;
}

const HashSet<String>& JSGlobalObject::intlNumberFormatAvailableLocales()
{
    if (m_intlNumberFormatAvailableLocales.isEmpty()) {
        int32_t count = unum_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = convertICULocaleToBCP47LanguageTag(unum_getAvailable(i));
            if (!locale.isEmpty())
                m_intlNumberFormatAvailableLocales.add(locale);
        }
        addMissingScriptLocales(m_intlNumberFormatAvailableLocales);
    }
    return m_intlNumberFormatAvailableLocales;
}

const HashSet<String>& JSGlobalObject::intlPluralRulesAvailableLocales()
{
    if (m_intlPluralRulesAvailableLocales.isEmpty()) {
        int32_t count = uloc_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = convertICULocaleToBCP47LanguageTag(uloc_getAvailable(i));
            if (!locale.isEmpty())
                m_intlPluralRulesAvailableLocales.add(locale);
        }
        addMissingScriptLocales(m_intlPluralRulesAvailableLocales);
    }
    return m_intlPluralRulesAvailableLocales;
}
#endif // ENABLE(INTL)

void JSGlobalObject::notifyLexicalBindingShadowing(VM& vm, const IdentifierSet& set)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
#if ENABLE(DFG_JIT)
    for (const auto& key : set)
        ensureReferencedPropertyWatchpointSet(key.get()).fireAll(vm, "Lexical binding shadows the existing global properties");
#endif
    vm.heap.codeBlockSet().iterate([&] (CodeBlock* codeBlock) {
        if (codeBlock->globalObject() != this)
            return;
        codeBlock->notifyLexicalBindingShadowing(vm, set);
        scope.assertNoException();
    });
    scope.release();
}

void JSGlobalObject::queueMicrotask(Ref<Microtask>&& task)
{
    if (globalObjectMethodTable()->queueTaskToEventLoop) {
        globalObjectMethodTable()->queueTaskToEventLoop(*this, WTFMove(task));
        return;
    }

    vm().queueMicrotask(*this, WTFMove(task));
}

bool JSGlobalObject::hasDebugger() const
{ 
    return m_debugger;
}

bool JSGlobalObject::hasInteractiveDebugger() const 
{ 
    return m_debugger && m_debugger->isInteractivelyDebugging();
}

#if ENABLE(DFG_JIT)
WatchpointSet* JSGlobalObject::getReferencedPropertyWatchpointSet(UniquedStringImpl* uid)
{
    return m_referencedGlobalPropertyWatchpointSets.get(uid);
}

WatchpointSet& JSGlobalObject::ensureReferencedPropertyWatchpointSet(UniquedStringImpl* uid)
{
    return m_referencedGlobalPropertyWatchpointSets.ensure(uid, [] {
        return WatchpointSet::create(IsWatched);
    }).iterator->value.get();
}
#endif

JSGlobalObject* JSGlobalObject::create(VM& vm, Structure* structure)
{
    JSGlobalObject* globalObject = new (NotNull, allocateCell<JSGlobalObject>(vm.heap)) JSGlobalObject(vm, structure);
    globalObject->finishCreation(vm);
    return globalObject;
}

void JSGlobalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    structure(vm)->setGlobalObject(vm, this);
    m_runtimeFlags = m_globalObjectMethodTable->javaScriptRuntimeFlags(this);
    init(vm);
    setGlobalThis(vm, JSProxy::create(vm, JSProxy::createStructure(vm, this, getPrototypeDirect(vm), PureForwardingProxyType), this));
    ASSERT(type() == GlobalObjectType);
}

void JSGlobalObject::finishCreation(VM& vm, JSObject* thisValue)
{
    Base::finishCreation(vm);
    structure(vm)->setGlobalObject(vm, this);
    m_runtimeFlags = m_globalObjectMethodTable->javaScriptRuntimeFlags(this);
    init(vm);
    setGlobalThis(vm, thisValue);
    ASSERT(type() == GlobalObjectType);
}

#ifdef JSC_GLIB_API_ENABLED
void JSGlobalObject::setWrapperMap(std::unique_ptr<WrapperMap>&& map)
{
    m_wrapperMap = WTFMove(map);
}
#endif

} // namespace JSC
