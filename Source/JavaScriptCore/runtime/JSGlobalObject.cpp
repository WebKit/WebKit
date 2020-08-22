/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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

#include "AggregateError.h"
#include "AggregateErrorConstructor.h"
#include "AggregateErrorPrototype.h"
#include "ArrayConstructor.h"
#include "ArrayIteratorPrototype.h"
#include "ArrayPrototype.h"
#include "AsyncFromSyncIteratorPrototype.h"
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
#include "ConsoleObject.h"
#include "DateConstructor.h"
#include "DatePrototype.h"
#include "Debugger.h"
#include "DebuggerScope.h"
#include "DirectArguments.h"
#include "ErrorConstructor.h"
#include "ErrorPrototype.h"
#include "FinalizationRegistryConstructor.h"
#include "FinalizationRegistryPrototype.h"
#include "FunctionConstructor.h"
#include "FunctionPrototype.h"
#include "GeneratorFunctionConstructor.h"
#include "GeneratorFunctionPrototype.h"
#include "GeneratorPrototype.h"
#include "GetterSetter.h"
#include "HeapIterationScope.h"
#include "InspectorInstrumentationObject.h"
#include "IntlCollator.h"
#include "IntlCollatorPrototype.h"
#include "IntlDateTimeFormat.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlDisplayNames.h"
#include "IntlDisplayNamesPrototype.h"
#include "IntlLocale.h"
#include "IntlLocalePrototype.h"
#include "IntlNumberFormat.h"
#include "IntlNumberFormatPrototype.h"
#include "IntlObject.h"
#include "IntlPluralRules.h"
#include "IntlPluralRulesPrototype.h"
#include "IntlRelativeTimeFormat.h"
#include "IntlRelativeTimeFormatPrototype.h"
#include "IntlSegmentIterator.h"
#include "IntlSegmentIteratorPrototype.h"
#include "IntlSegmenter.h"
#include "IntlSegmenterPrototype.h"
#include "IntlSegments.h"
#include "IntlSegmentsPrototype.h"
#include "IteratorPrototype.h"
#include "JSAPIWrapperObject.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferConstructor.h"
#include "JSArrayBufferPrototype.h"
#include "JSArrayIterator.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGenerator.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSBoundFunction.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSCustomGetterSetterFunction.h"
#include "JSDataView.h"
#include "JSDataViewPrototype.h"
#include "JSDollarVM.h"
#include "JSFinalizationRegistry.h"
#include "JSFunction.h"
#include "JSGenerator.h"
#include "JSGeneratorFunction.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSGenericTypedArrayViewPrototypeInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSInternalPromisePrototype.h"
#include "JSLexicalEnvironment.h"
#include "JSMap.h"
#include "JSMapIterator.h"
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
#include "JSSetIterator.h"
#include "JSStringIterator.h"
#include "JSTypedArrayConstructors.h"
#include "JSTypedArrayPrototypes.h"
#include "JSTypedArrayViewConstructor.h"
#include "JSTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"
#include "JSWeakMap.h"
#include "JSWeakObjectRef.h"
#include "JSWeakSet.h"
#include "JSWebAssembly.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyGlobal.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyRuntimeError.h"
#include "JSWebAssemblyTable.h"
#include "JSWithScope.h"
#include "LazyClassStructureInlines.h"
#include "LazyPropertyInlines.h"
#include "LinkTimeConstant.h"
#include "MapConstructor.h"
#include "MapIteratorPrototype.h"
#include "MapPrototype.h"
#include "MarkedSpaceInlines.h"
#include "MathObject.h"
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
#include "ProxyConstructor.h"
#include "ProxyObject.h"
#include "ProxyRevoke.h"
#include "ReflectObject.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "RegExpStringIteratorPrototype.h"
#include "SamplingProfiler.h"
#include "ScopedArguments.h"
#include "SetConstructor.h"
#include "SetIteratorPrototype.h"
#include "SetPrototype.h"
#include "StrictEvalActivation.h"
#include "StringConstructor.h"
#include "StringIteratorPrototype.h"
#include "StringPrototype.h"
#include "SymbolConstructor.h"
#include "SymbolObject.h"
#include "SymbolPrototype.h"
#include "WasmCapabilities.h"
#include "WeakMapConstructor.h"
#include "WeakMapPrototype.h"
#include "WeakObjectRefConstructor.h"
#include "WeakObjectRefPrototype.h"
#include "WeakSetConstructor.h"
#include "WeakSetPrototype.h"
#include "WebAssemblyCompileErrorConstructor.h"
#include "WebAssemblyCompileErrorPrototype.h"
#include "WebAssemblyFunction.h"
#include "WebAssemblyGlobalConstructor.h"
#include "WebAssemblyGlobalPrototype.h"
#include "WebAssemblyInstanceConstructor.h"
#include "WebAssemblyInstancePrototype.h"
#include "WebAssemblyLinkErrorConstructor.h"
#include "WebAssemblyLinkErrorPrototype.h"
#include "WebAssemblyMemoryConstructor.h"
#include "WebAssemblyMemoryPrototype.h"
#include "WebAssemblyModuleConstructor.h"
#include "WebAssemblyModulePrototype.h"
#include "WebAssemblyModuleRecord.h"
#include "WebAssemblyRuntimeErrorConstructor.h"
#include "WebAssemblyRuntimeErrorPrototype.h"
#include "WebAssemblyTableConstructor.h"
#include "WebAssemblyTablePrototype.h"
#include <wtf/RandomNumber.h>

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectDebuggable.h"
#include "JSGlobalObjectInspectorController.h"
#endif

#ifdef JSC_GLIB_API_ENABLED
#include "JSCCallbackFunction.h"
#include "JSCWrapperMap.h"
#endif

namespace JSC {

#define CHECK_FEATURE_FLAG_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
static_assert(std::is_same_v<std::remove_cv_t<decltype(featureFlag)>, bool> || std::is_same_v<std::remove_cv_t<decltype(featureFlag)>, bool&>);

FOR_EACH_SIMPLE_BUILTIN_TYPE(CHECK_FEATURE_FLAG_TYPE)
FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(CHECK_FEATURE_FLAG_TYPE)
FOR_EACH_LAZY_BUILTIN_TYPE(CHECK_FEATURE_FLAG_TYPE)

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

static JSValue createReflectProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return ReflectObject::create(vm, global, ReflectObject::createStructure(vm, global, global->objectPrototype()));
}

static JSValue createConsoleProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return ConsoleObject::create(vm, global, ConsoleObject::createStructure(vm, global, constructEmptyObject(global)));
}

static EncodedJSValue JSC_HOST_CALL makeBoundFunction(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* target = asObject(callFrame->uncheckedArgument(0));
    JSValue boundThis = callFrame->uncheckedArgument(1);
    JSValue boundArgs = callFrame->uncheckedArgument(2);
    double length = callFrame->uncheckedArgument(3).asNumber();
    JSString* nameString = asString(callFrame->uncheckedArgument(4));

    RELEASE_AND_RETURN(scope, JSValue::encode(JSBoundFunction::create(vm, globalObject, target, boundThis, boundArgs.isCell() ? jsCast<JSImmutableButterfly*>(boundArgs) : nullptr, length, nameString)));
}

static EncodedJSValue JSC_HOST_CALL hasOwnLengthProperty(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* target = asObject(callFrame->uncheckedArgument(0));
    JSFunction* function = jsDynamicCast<JSFunction*>(vm, target);
    if (function && function->canAssumeNameAndLengthAreOriginal(vm)) {
#if ASSERT_ENABLED
        bool result = target->hasOwnProperty(globalObject, vm.propertyNames->length);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(result);
#endif
        return JSValue::encode(jsBoolean(true));
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(target->hasOwnProperty(globalObject, vm.propertyNames->length))));
}

// FIXME: use a bytecode or intrinsic for creating a private symbol.
// https://bugs.webkit.org/show_bug.cgi?id=212782
static EncodedJSValue JSC_HOST_CALL createPrivateSymbol(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    return JSValue::encode(Symbol::create(vm, PrivateSymbolImpl::createNullSymbol().get()));
}

#if ASSERT_ENABLED
static EncodedJSValue JSC_HOST_CALL assertCall(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    RELEASE_ASSERT(callFrame->argument(0).isBoolean());
    if (callFrame->argument(0).asBoolean())
        return JSValue::encode(jsUndefined());

    bool iteratedOnce = false;
    CodeBlock* codeBlock = nullptr;
    unsigned line;
    callFrame->iterate(globalObject->vm(), [&] (StackVisitor& visitor) {
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
#endif // ASSERT_ENABLED

#if ENABLE(SAMPLING_PROFILER)
static EncodedJSValue JSC_HOST_CALL enableSamplingProfiler(JSGlobalObject* globalObject, CallFrame*)
{
    SamplingProfiler* profiler = globalObject->vm().samplingProfiler();
    if (!profiler)
        profiler = &globalObject->vm().ensureSamplingProfiler(Stopwatch::create());
    profiler->start();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL disableSamplingProfiler(JSGlobalObject* globalObject, CallFrame*)
{
    SamplingProfiler* profiler = globalObject->vm().samplingProfiler();
    if (!profiler)
        profiler = &globalObject->vm().ensureSamplingProfiler(Stopwatch::create());

    {
        auto locker = holdLock(profiler->getLock());
        profiler->pause(locker);
    }

    return JSValue::encode(jsUndefined());
}
#endif

static EncodedJSValue JSC_HOST_CALL enableSuperSampler(JSGlobalObject*, CallFrame*)
{
    enableSuperSampler();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL disableSuperSampler(JSGlobalObject*, CallFrame*)
{
    disableSuperSampler();
    return JSValue::encode(jsUndefined());
}

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
    &reportUncaughtExceptionAtEventLoop,
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
  eval                  JSGlobalObject::m_evalFunction               DontEnum|CellProperty
  globalThis            JSGlobalObject::m_globalThis                 DontEnum|CellProperty
  parseInt              JSGlobalObject::m_parseIntFunction           DontEnum|CellProperty
  parseFloat            JSGlobalObject::m_parseFloatFunction         DontEnum|CellProperty
  ArrayBuffer           JSGlobalObject::m_arrayBufferStructure       DontEnum|ClassStructure
  EvalError             JSGlobalObject::m_evalErrorStructure         DontEnum|ClassStructure
  RangeError            JSGlobalObject::m_rangeErrorStructure        DontEnum|ClassStructure
  ReferenceError        JSGlobalObject::m_referenceErrorStructure    DontEnum|ClassStructure
  SyntaxError           JSGlobalObject::m_syntaxErrorStructure       DontEnum|ClassStructure
  TypeError             JSGlobalObject::m_typeErrorStructure         DontEnum|ClassStructure
  URIError              JSGlobalObject::m_URIErrorStructure          DontEnum|ClassStructure
  AggregateError        JSGlobalObject::m_aggregateErrorStructure    DontEnum|ClassStructure
  Proxy                 createProxyProperty                          DontEnum|PropertyCallback
  Reflect               createReflectProperty                        DontEnum|PropertyCallback
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
  Error                 JSGlobalObject::m_errorStructure             DontEnum|ClassStructure
  Boolean               JSGlobalObject::m_booleanObjectStructure     DontEnum|ClassStructure
  Map                   JSGlobalObject::m_mapStructure               DontEnum|ClassStructure
  Number                JSGlobalObject::m_numberObjectStructure      DontEnum|ClassStructure
  Set                   JSGlobalObject::m_setStructure               DontEnum|ClassStructure
  Symbol                JSGlobalObject::m_symbolObjectStructure      DontEnum|ClassStructure
  WeakMap               JSGlobalObject::m_weakMapStructure           DontEnum|ClassStructure
  WeakSet               JSGlobalObject::m_weakSetStructure           DontEnum|ClassStructure
@end
*/

static EncodedJSValue JSC_HOST_CALL enqueueJob(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();

    JSValue job = callFrame->argument(0);
    JSValue argument0 = callFrame->argument(1);
    JSValue argument1 = callFrame->argument(2);
    JSValue argument2 = callFrame->argument(3);

    globalObject->queueMicrotask(createJSMicrotask(vm, job, argument0, argument1, argument2));

    return JSValue::encode(jsUndefined());
}

JSGlobalObject::JSGlobalObject(VM& vm, Structure* structure, const GlobalObjectMethodTable* globalObjectMethodTable)
    : Base(vm, structure, nullptr)
    , m_vm(&vm)
    , m_linkTimeConstants(numberOfLinkTimeConstants)
    , m_masqueradesAsUndefinedWatchpoint(WatchpointSet::create(IsWatched))
    , m_havingABadTimeWatchpoint(WatchpointSet::create(IsWatched))
    , m_varInjectionWatchpoint(WatchpointSet::create(IsWatched))
    , m_weakRandom(Options::forceWeakRandomSeed() ? Options::forcedWeakRandomSeed() : static_cast<unsigned>(randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0)))
    , m_arrayIteratorProtocolWatchpointSet(IsWatched)
    , m_mapIteratorProtocolWatchpointSet(IsWatched)
    , m_setIteratorProtocolWatchpointSet(IsWatched)
    , m_stringIteratorProtocolWatchpointSet(IsWatched)
    , m_mapSetWatchpointSet(IsWatched)
    , m_setAddWatchpointSet(IsWatched)
    , m_arraySpeciesWatchpointSet(ClearWatchpoint)
    , m_arrayJoinWatchpointSet(IsWatched)
    , m_numberToStringWatchpointSet(IsWatched)
    , m_runtimeFlags()
    , m_stackTraceLimit(Options::defaultErrorStackTraceLimit())
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

static GetterSetter* getGetterById(JSGlobalObject* globalObject, JSObject* base, const Identifier& ident)
{
    VM& vm = globalObject->vm();
    JSValue baseValue = JSValue(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);
    baseValue.getPropertySlot(globalObject, ident, slot);
    return jsCast<GetterSetter*>(slot.getPureResult());
}

static ObjectPropertyCondition setupAdaptiveWatchpoint(JSGlobalObject* globalObject, JSObject* base, const Identifier& ident)
{
    // Performing these gets should not throw.
    VM& vm = globalObject->vm();
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    PropertySlot slot(base, PropertySlot::InternalMethodType::Get);
    bool result = base->getOwnPropertySlot(base, globalObject, ident, slot);
    ASSERT_UNUSED(result, result);
    catchScope.assertNoException();
    RELEASE_ASSERT(slot.isCacheableValue());
    JSValue functionValue = slot.getValue(globalObject, ident);
    catchScope.assertNoException();
    ASSERT(jsDynamicCast<JSFunction*>(vm, functionValue));

    ObjectPropertyCondition condition = generateConditionForSelfEquivalence(vm, nullptr, base, ident.impl());
    RELEASE_ASSERT(condition.requiredValue() == functionValue);

    bool isWatchable = condition.isWatchable(PropertyCondition::EnsureWatchability);
    RELEASE_ASSERT(isWatchable); // We allow this to install the necessary watchpoints.

    return condition;
}

template<ErrorType errorType>
void JSGlobalObject::initializeErrorConstructor(LazyClassStructure::Initializer& init)
{
    init.setPrototype(NativeErrorPrototype::create(init.vm, NativeErrorPrototype::createStructure(init.vm, this, m_errorStructure.prototype(this)), errorTypeName(errorType)));
    init.setStructure(ErrorInstance::createStructure(init.vm, this, init.prototype));
    init.setConstructor(NativeErrorConstructor<errorType>::create(init.vm, NativeErrorConstructor<errorType>::createStructure(init.vm, this, m_errorStructure.constructor(this)), jsCast<NativeErrorPrototype*>(init.prototype)));
}

void JSGlobalObject::initializeAggregateErrorConstructor(LazyClassStructure::Initializer& init)
{
    init.setPrototype(AggregateErrorPrototype::create(init.vm, AggregateErrorPrototype::createStructure(init.vm, this, m_errorStructure.prototype(this))));
    init.setStructure(AggregateError::createStructure(init.vm, this, init.prototype));
    init.setConstructor(AggregateErrorConstructor::create(init.vm, AggregateErrorConstructor::createStructure(init.vm, this, m_errorStructure.constructor(this)), jsCast<AggregateErrorPrototype*>(init.prototype)));
}

void JSGlobalObject::init(VM& vm)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    Base::setStructure(vm, Structure::toCacheableDictionaryTransition(vm, structure(vm)));

    m_debugger = nullptr;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorController = makeUnique<Inspector::JSGlobalObjectInspectorController>(*this);
    m_inspectorDebuggable = makeUnique<JSGlobalObjectDebuggable>(*this);
    m_inspectorDebuggable->init();
    m_consoleClient = m_inspectorController->consoleClient();
#endif

    m_functionPrototype.set(vm, this, FunctionPrototype::create(vm, FunctionPrototype::createStructure(vm, this, jsNull()))); // The real prototype will be set once ObjectPrototype is created.
    m_calleeStructure.set(vm, this, JSCallee::createStructure(vm, this, jsNull()));

    m_globalLexicalEnvironment.set(vm, this, JSGlobalLexicalEnvironment::create(vm, JSGlobalLexicalEnvironment::createStructure(vm, this), this));

    // Need to create the callee structure (above) before creating the callee.
    JSCallee* globalCallee = JSCallee::create(vm, this, globalScope());
    m_globalCallee.set(vm, this, globalCallee);

    CallFrame::initDeprecatedCallFrameForDebugger(JSGlobalObject::deprecatedCallFrameForDebugger(), globalCallee);

    JSCallee* stackOverflowFrameCallee = JSCallee::create(vm, this, globalScope());
    m_stackOverflowFrameCallee.set(vm, this, stackOverflowFrameCallee);

    m_hostFunctionStructure.set(vm, this, JSFunction::createStructure(vm, this, m_functionPrototype.get()));

    auto initFunctionStructures = [&] (FunctionStructures& structures) {
        structures.strictFunctionStructure.set(vm, this, JSStrictFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.sloppyFunctionStructure.set(vm, this, JSSloppyFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.arrowFunctionStructure.set(vm, this, JSArrowFunction::createStructure(vm, this, m_functionPrototype.get()));
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
    m_nativeStdFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSNativeStdFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    JSFunction* callFunction = nullptr;
    JSFunction* applyFunction = nullptr;
    JSFunction* hasInstanceSymbolFunction = nullptr;
    m_functionPrototype->addFunctionProperties(vm, this, &callFunction, &applyFunction, &hasInstanceSymbolFunction);
    m_arrayProtoToStringFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 0, init.vm.propertyNames->toString.string(), arrayProtoFuncToString, NoIntrinsic));
        });
    m_arrayProtoValuesFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 0, init.vm.propertyNames->builtinNames().valuesPublicName().string(), arrayProtoFuncValues, ArrayValuesIntrinsic));
        });

    m_iteratorProtocolFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, iteratorHelpersPerformIterationCodeGenerator(init.vm), init.owner));
        });

    m_promiseResolveFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, promiseConstructorResolveCodeGenerator(init.vm), init.owner));
        });

    m_numberProtoToStringFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, init.vm.propertyNames->toString.string(), numberProtoFuncToString, NumberPrototypeToStringIntrinsic));
        });

    m_functionProtoHasInstanceSymbolFunction.set(vm, this, hasInstanceSymbolFunction);
    m_throwTypeErrorGetterSetter.initLater(
        [] (const Initializer<GetterSetter>& init) {
            JSFunction* thrower = init.owner->throwTypeErrorFunction();
            GetterSetter* getterSetter = GetterSetter::create(init.vm, init.owner, thrower, thrower);
            init.set(getterSetter);
        });

    m_nullGetterFunction.set(vm, this, NullGetterFunction::create(vm, NullGetterFunction::createStructure(vm, this, m_functionPrototype.get())));
    Structure* nullSetterFunctionStructure = NullSetterFunction::createStructure(vm, this, m_functionPrototype.get());
    m_nullSetterFunction.set(vm, this, NullSetterFunction::create(vm, nullSetterFunctionStructure, ECMAMode::sloppy()));
    m_nullSetterStrictFunction.set(vm, this, NullSetterFunction::create(vm, nullSetterFunctionStructure, ECMAMode::strict()));
    m_objectPrototype.set(vm, this, ObjectPrototype::create(vm, this, ObjectPrototype::createStructure(vm, this, jsNull())));
    // We have to manually set this here because we make it a prototype without transition below.
    m_objectPrototype.get()->didBecomePrototype();
    GetterSetter* protoAccessor = GetterSetter::create(vm, this,
        JSFunction::create(vm, this, 0, makeString("get ", vm.propertyNames->underscoreProto.string()), globalFuncProtoGetter, UnderscoreProtoIntrinsic),
        JSFunction::create(vm, this, 0, makeString("set ", vm.propertyNames->underscoreProto.string()), globalFuncProtoSetter));
    m_objectPrototype->putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->underscoreProto, protoAccessor, PropertyAttribute::Accessor | PropertyAttribute::DontEnum);
    m_functionPrototype->structure(vm)->setPrototypeWithoutTransition(vm, m_objectPrototype.get());
    m_objectStructureForObjectConstructor.set(vm, this, vm.structureCache.emptyObjectStructureForPrototype(this, m_objectPrototype.get(), JSFinalObject::defaultInlineCapacity()));
    m_objectProtoValueOfFunction.set(vm, this, jsCast<JSFunction*>(objectPrototype()->getDirect(vm, vm.propertyNames->valueOf)));
    
    JSFunction* thrower = JSFunction::create(vm, this, 0, emptyString(), globalFuncThrowTypeErrorArgumentsCalleeAndCaller);
    thrower->freeze(vm);
    GetterSetter* getterSetter = GetterSetter::create(vm, this, thrower, thrower);
    m_throwTypeErrorArgumentsCalleeAndCallerGetterSetter.set(vm, this, getterSetter);
    
    m_functionPrototype->initRestrictedProperties(vm, this);

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
            init.global->putDirect(init.vm, init.vm.propertyNames->builtinNames().type ## ArrayPrivateName(), init.constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \
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
    m_strictEvalActivationStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(StrictEvalActivation::createStructure(init.vm, init.owner, jsNull()));
        });
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
            init.set(JSCallbackObject<JSNonFinalObject>::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
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

    m_moduleRecordStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSModuleRecord::createStructure(init.vm, init.owner, jsNull()));
        });
    m_moduleNamespaceObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSModuleNamespaceObject::createStructure(init.vm, init.owner, jsNull()));
        });
    m_proxyObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            bool isCallable = false;
            init.set(ProxyObject::createStructure(init.vm, init.owner, jsNull(), isCallable));
        });
    m_callableProxyObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            bool isCallable = true;
            init.set(ProxyObject::createStructure(init.vm, init.owner, jsNull(), isCallable));
        });
    m_proxyRevokeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(ProxyRevoke::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });

    m_parseIntFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 2, init.vm.propertyNames->parseInt.string(), globalFuncParseInt, ParseIntIntrinsic));
        });
    m_parseFloatFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 1, init.vm.propertyNames->parseFloat.string(), globalFuncParseFloat, NoIntrinsic));
        });
    
#if ENABLE(SHARED_ARRAY_BUFFER)
    m_sharedArrayBufferPrototype.set(vm, this, JSArrayBufferPrototype::create(vm, this, JSArrayBufferPrototype::createStructure(vm, this, m_objectPrototype.get()), ArrayBufferSharingMode::Shared));
    m_sharedArrayBufferStructure.set(vm, this, JSArrayBuffer::createStructure(vm, this, m_sharedArrayBufferPrototype.get()));
#endif

    m_iteratorPrototype.set(vm, this, IteratorPrototype::create(vm, this, IteratorPrototype::createStructure(vm, this, m_objectPrototype.get())));
    m_asyncIteratorPrototype.set(vm, this, AsyncIteratorPrototype::create(vm, this, AsyncIteratorPrototype::createStructure(vm, this, m_objectPrototype.get())));

    m_generatorPrototype.set(vm, this, GeneratorPrototype::create(vm, this, GeneratorPrototype::createStructure(vm, this, m_iteratorPrototype.get())));
    m_asyncGeneratorPrototype.set(vm, this, AsyncGeneratorPrototype::create(vm, this, AsyncGeneratorPrototype::createStructure(vm, this, m_asyncIteratorPrototype.get())));

    auto* arrayIteratorPrototype = ArrayIteratorPrototype::create(vm, this, ArrayIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_arrayIteratorPrototype.set(vm, this, arrayIteratorPrototype);
    m_arrayIteratorStructure.set(vm, this, JSArrayIterator::createStructure(vm, this, arrayIteratorPrototype));

    auto* mapIteratorPrototype = MapIteratorPrototype::create(vm, this, MapIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_mapIteratorPrototype.set(vm, this, mapIteratorPrototype);
    m_mapIteratorStructure.set(vm, this, JSMapIterator::createStructure(vm, this, mapIteratorPrototype));

    auto* setIteratorPrototype = SetIteratorPrototype::create(vm, this, SetIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_setIteratorPrototype.set(vm, this, setIteratorPrototype);
    m_setIteratorStructure.set(vm, this, JSSetIterator::createStructure(vm, this, setIteratorPrototype));

    JSFunction* defaultPromiseThen = JSFunction::create(vm, promisePrototypeThenCodeGenerator(vm), this);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::defaultPromiseThen)].set(vm, this, defaultPromiseThen);

#define CREATE_PROTOTYPE_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) { \
        m_ ## lowerName ## Prototype.set(vm, this, capitalName##Prototype::create(vm, this, capitalName##Prototype::createStructure(vm, this, m_ ## prototypeBase ## Prototype.get()))); \
        m_ ## properName ## Structure.set(vm, this, instanceType::createStructure(vm, this, m_ ## lowerName ## Prototype.get())); \
    }
    
    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)
    
#undef CREATE_PROTOTYPE_FOR_SIMPLE_TYPE

#define CREATE_PROTOTYPE_FOR_LAZY_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) {  \
    m_ ## properName ## Structure.initLater(\
        [] (LazyClassStructure::Initializer& init) { \
            init.setPrototype(capitalName##Prototype::create(init.vm, init.global, capitalName##Prototype::createStructure(init.vm, init.global, init.global->m_ ## prototypeBase ## Prototype.get()))); \
            init.setStructure(instanceType::createStructure(init.vm, init.global, init.prototype)); \
            init.setConstructor(capitalName ## Constructor::create(init.vm, capitalName ## Constructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), jsCast<capitalName ## Prototype*>(init.prototype), init.global->m_speciesGetterSetter.get())); \
        }); \
    }
    
    FOR_EACH_LAZY_BUILTIN_TYPE(CREATE_PROTOTYPE_FOR_LAZY_TYPE)
    
    // Constructors

    ObjectConstructor* objectConstructor = ObjectConstructor::create(vm, this, ObjectConstructor::createStructure(vm, this, m_functionPrototype.get()), m_objectPrototype.get());
    m_objectConstructor.set(vm, this, objectConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Object)].set(vm, this, objectConstructor);

    JSFunction* throwTypeErrorFunction = JSFunction::create(vm, this, 0, String(), globalFuncThrowTypeError);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::throwTypeErrorFunction)].set(vm, this, throwTypeErrorFunction);

    FunctionConstructor* functionConstructor = FunctionConstructor::create(vm, FunctionConstructor::createStructure(vm, this, m_functionPrototype.get()), m_functionPrototype.get());
    m_functionConstructor.set(vm, this, functionConstructor);

    ArrayConstructor* arrayConstructor = ArrayConstructor::create(vm, this, ArrayConstructor::createStructure(vm, this, m_functionPrototype.get()), m_arrayPrototype.get(), m_speciesGetterSetter.get());
    m_arrayConstructor.set(vm, this, arrayConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Array)].set(vm, this, arrayConstructor);
    
    RegExpConstructor* regExpConstructor = RegExpConstructor::create(vm, RegExpConstructor::createStructure(vm, this, m_functionPrototype.get()), m_regExpPrototype.get(), m_speciesGetterSetter.get());
    m_regExpConstructor.set(vm, this, regExpConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::RegExp)].set(vm, this, regExpConstructor);
    m_regExpGlobalData.cachedResult().record(vm, this, nullptr, jsEmptyString(vm), MatchResult(0, 0));
    
#if ENABLE(SHARED_ARRAY_BUFFER)
    JSSharedArrayBufferConstructor* sharedArrayBufferConstructor = nullptr;
    sharedArrayBufferConstructor = JSSharedArrayBufferConstructor::create(vm, JSSharedArrayBufferConstructor::createStructure(vm, this, m_functionPrototype.get()), m_sharedArrayBufferPrototype.get(), m_speciesGetterSetter.get());
    m_sharedArrayBufferPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, sharedArrayBufferConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));

    AtomicsObject* atomicsObject = AtomicsObject::create(vm, this, AtomicsObject::createStructure(vm, this, m_objectPrototype.get()));
#endif

#define CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
capitalName ## Constructor* lowerName ## Constructor = featureFlag ? capitalName ## Constructor::create(vm, capitalName ## Constructor::createStructure(vm, this, m_functionPrototype.get()), m_ ## lowerName ## Prototype.get(), m_speciesGetterSetter.get()) : nullptr; \
    if (featureFlag) \
        m_ ## lowerName ## Prototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, lowerName ## Constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE)
    
#undef CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE

    m_promiseConstructor.set(vm, this, promiseConstructor);
    m_internalPromiseConstructor.set(vm, this, internalPromiseConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Promise)].set(vm, this, promiseConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::InternalPromise)].set(vm, this, internalPromiseConstructor);
    
    m_evalErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::EvalError>(init);
        });
    m_rangeErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::RangeError>(init);
        });
    m_referenceErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::ReferenceError>(init);
        });
    m_syntaxErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::SyntaxError>(init);
        });
    m_typeErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::TypeError>(init);
        });
    m_URIErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::URIError>(init);
        });
    m_aggregateErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeAggregateErrorConstructor(init);
        });

    m_generatorFunctionPrototype.set(vm, this, GeneratorFunctionPrototype::create(vm, GeneratorFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    GeneratorFunctionConstructor* generatorFunctionConstructor = GeneratorFunctionConstructor::create(vm, GeneratorFunctionConstructor::createStructure(vm, this, functionConstructor), m_generatorFunctionPrototype.get());
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, generatorFunctionConstructor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorFunctionStructure.set(vm, this, JSGeneratorFunction::createStructure(vm, this, m_generatorFunctionPrototype.get()));

    m_generatorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_generatorFunctionPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->prototype, m_generatorPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorStructure.set(vm, this, JSGenerator::createStructure(vm, this, m_generatorPrototype.get()));

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
    m_asyncGeneratorStructure.set(vm, this, JSAsyncGenerator::createStructure(vm, this, m_asyncGeneratorPrototype.get()));
    
    m_objectPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, objectConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_functionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, functionConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_arrayPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, arrayConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_regExpPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, regExpConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    putDirectWithoutTransition(vm, vm.propertyNames->Object, objectConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->Function, functionConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->Array, arrayConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->RegExp, regExpConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));

#if ENABLE(SHARED_ARRAY_BUFFER)
    putDirectWithoutTransition(vm, vm.propertyNames->SharedArrayBuffer, sharedArrayBufferConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "Atomics"), atomicsObject, static_cast<unsigned>(PropertyAttribute::DontEnum));
#endif

#define PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    if (featureFlag) \
        putDirectWithoutTransition(vm, vm.propertyNames-> jsName, lowerName ## Constructor, static_cast<unsigned>(PropertyAttribute::DontEnum));


    FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE)

#undef PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE
    m_iteratorResultObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(createIteratorResultObjectStructure(init.vm, *init.owner));
        });
    m_dataPropertyDescriptorObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(createDataPropertyDescriptorObjectStructure(init.vm, *init.owner));
        });
    m_accessorPropertyDescriptorObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(createAccessorPropertyDescriptorObjectStructure(init.vm, *init.owner));
        });
    
    m_evalFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 1, init.vm.propertyNames->eval.string(), globalFuncEval, NoIntrinsic));
        });

    m_collatorStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlCollatorPrototype* collatorPrototype = IntlCollatorPrototype::create(init.vm, globalObject, IntlCollatorPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlCollator::createStructure(init.vm, globalObject, collatorPrototype));
        });
    m_dateTimeFormatStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlDateTimeFormatPrototype* dateTimeFormatPrototype = IntlDateTimeFormatPrototype::create(init.vm, globalObject, IntlDateTimeFormatPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlDateTimeFormat::createStructure(init.vm, globalObject, dateTimeFormatPrototype));
        });
    m_displayNamesStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlDisplayNamesPrototype* displayNamesPrototype = IntlDisplayNamesPrototype::create(init.vm, IntlDisplayNamesPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlDisplayNames::createStructure(init.vm, globalObject, displayNamesPrototype));
        });
    m_localeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlLocalePrototype* localePrototype = IntlLocalePrototype::create(init.vm, IntlLocalePrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlLocale::createStructure(init.vm, globalObject, localePrototype));
        });
    m_numberFormatStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlNumberFormatPrototype* numberFormatPrototype = IntlNumberFormatPrototype::create(init.vm, globalObject, IntlNumberFormatPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlNumberFormat::createStructure(init.vm, globalObject, numberFormatPrototype));
        });
    m_pluralRulesStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlPluralRulesPrototype* pluralRulesPrototype = IntlPluralRulesPrototype::create(init.vm, globalObject, IntlPluralRulesPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlPluralRules::createStructure(init.vm, globalObject, pluralRulesPrototype));
        });
    m_relativeTimeFormatStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlRelativeTimeFormatPrototype* relativeTimeFormatPrototype = IntlRelativeTimeFormatPrototype::create(init.vm, IntlRelativeTimeFormatPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlRelativeTimeFormat::createStructure(init.vm, globalObject, relativeTimeFormatPrototype));
        });
    m_segmentIteratorStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlSegmentIteratorPrototype* segmentIteratorPrototype = IntlSegmentIteratorPrototype::create(init.vm, IntlSegmentIteratorPrototype::createStructure(init.vm, globalObject, globalObject->iteratorPrototype()));
            init.set(IntlSegmentIterator::createStructure(init.vm, globalObject, segmentIteratorPrototype));
        });
    m_segmenterStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlSegmenterPrototype* segmenterPrototype = IntlSegmenterPrototype::create(init.vm, IntlSegmenterPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlSegmenter::createStructure(init.vm, globalObject, segmenterPrototype));
        });
    m_segmentsStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlSegmentsPrototype* segmentsPrototype = IntlSegmentsPrototype::create(init.vm, globalObject, IntlSegmentsPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlSegments::createStructure(init.vm, globalObject, segmentsPrototype));
        });
    m_defaultCollator.initLater(
        [] (const Initializer<IntlCollator>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            VM& vm = init.vm;
            auto scope = DECLARE_CATCH_SCOPE(vm);
            IntlCollator* collator = IntlCollator::create(vm, globalObject->collatorStructure());
            collator->initializeCollator(globalObject, jsUndefined(), jsUndefined());
            scope.releaseAssertNoException();
            init.set(collator);
        });

    IntlObject* intl = IntlObject::create(vm, this, IntlObject::createStructure(vm, this, m_objectPrototype.get()));
    putDirectWithoutTransition(vm, vm.propertyNames->Intl, intl, static_cast<unsigned>(PropertyAttribute::DontEnum));

    m_moduleLoader.initLater(
        [] (const Initializer<JSModuleLoader>& init) {
            auto catchScope = DECLARE_CATCH_SCOPE(init.vm);
            init.set(JSModuleLoader::create(init.owner, init.vm, JSModuleLoader::createStructure(init.vm, init.owner, jsNull())));
            catchScope.releaseAssertNoException();
        });
    if (Options::exposeInternalModuleLoader())
        putDirectWithoutTransition(vm, vm.propertyNames->Loader, moduleLoader(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    GetterSetter* regExpProtoFlagsGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->flags);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoFlagsGetter)].set(vm, this, regExpProtoFlagsGetter);
    GetterSetter* regExpProtoGlobalGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->global);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoGlobalGetter)].set(vm, this, regExpProtoGlobalGetter);
    GetterSetter* regExpProtoIgnoreCaseGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->ignoreCase);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoIgnoreCaseGetter)].set(vm, this, regExpProtoIgnoreCaseGetter);
    GetterSetter* regExpProtoMultilineGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->multiline);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoMultilineGetter)].set(vm, this, regExpProtoMultilineGetter);
    GetterSetter* regExpProtoSourceGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->source);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoSourceGetter)].set(vm, this, regExpProtoSourceGetter);
    GetterSetter* regExpProtoStickyGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->sticky);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoStickyGetter)].set(vm, this, regExpProtoStickyGetter);
    GetterSetter* regExpProtoUnicodeGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->unicode);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoUnicodeGetter)].set(vm, this, regExpProtoUnicodeGetter);
    JSFunction* regExpSymbolReplace = jsCast<JSFunction*>(m_regExpPrototype->getDirect(vm, vm.propertyNames->replaceSymbol));
    m_regExpProtoSymbolReplace.set(vm, this, regExpSymbolReplace);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpBuiltinExec)].set(vm, this, jsCast<JSFunction*>(m_regExpPrototype->getDirect(vm, vm.propertyNames->exec)));
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpPrototypeSymbolMatch)].set(vm, this, m_regExpPrototype->getDirect(vm, vm.propertyNames->matchSymbol).asCell());
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpPrototypeSymbolReplace)].set(vm, this, m_regExpPrototype->getDirect(vm, vm.propertyNames->replaceSymbol).asCell());

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isArray)].set(vm, this, arrayConstructor->getDirect(vm, vm.propertyNames->isArray).asCell());
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::callFunction)].set(vm, this, callFunction);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::applyFunction)].set(vm, this, applyFunction);

    {
        JSValue hasOwnPropertyFunction = jsCast<JSFunction*>(objectPrototype()->get(this, vm.propertyNames->hasOwnProperty));
        catchScope.assertNoException();
        RELEASE_ASSERT(!!jsDynamicCast<JSFunction*>(vm, hasOwnPropertyFunction));
        m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::hasOwnPropertyFunction)].set(vm, this, jsCast<JSFunction*>(hasOwnPropertyFunction));
    }

#define INIT_PRIVATE_GLOBAL(funcName, code) \
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::funcName)].initLater([] (const Initializer<JSCell>& init) { \
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner); \
            init.set(JSFunction::create(init.vm, code ## CodeGenerator(init.vm), globalObject)); \
        });
    JSC_FOREACH_BUILTIN_FUNCTION_PRIVATE_GLOBAL_NAME(INIT_PRIVATE_GLOBAL)
#undef INIT_PRIVATE_GLOBAL

    // FIXME: Initializing them lazily.
    // https://bugs.webkit.org/show_bug.cgi?id=203795
    JSObject* asyncFromSyncIteratorPrototype = AsyncFromSyncIteratorPrototype::create(vm, this, AsyncFromSyncIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    jsCast<JSObject*>(linkTimeConstant(LinkTimeConstant::AsyncFromSyncIterator))->putDirect(vm, vm.propertyNames->prototype, asyncFromSyncIteratorPrototype);

    JSObject* regExpStringIteratorPrototype = RegExpStringIteratorPrototype::create(vm, this, RegExpStringIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    jsCast<JSObject*>(linkTimeConstant(LinkTimeConstant::RegExpStringIterator))->putDirect(vm, vm.propertyNames->prototype, regExpStringIteratorPrototype);

    // Map and Set helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Set)].initLater([] (const Initializer<JSCell>& init) {
            init.set(jsCast<JSGlobalObject*>(init.owner)->setConstructor());
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapBucketHead)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), mapPrivateFuncMapBucketHead, JSMapBucketHeadIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapBucketNext)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), mapPrivateFuncMapBucketNext, JSMapBucketNextIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapBucketKey)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), mapPrivateFuncMapBucketKey, JSMapBucketKeyIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapBucketValue)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), mapPrivateFuncMapBucketValue, JSMapBucketValueIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setBucketHead)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), setPrivateFuncSetBucketHead, JSSetBucketHeadIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setBucketNext)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), setPrivateFuncSetBucketNext, JSSetBucketNextIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setBucketKey)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), setPrivateFuncSetBucketKey, JSSetBucketKeyIntrinsic));
        });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::importModule)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), globalFuncImportModule));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::propertyIsEnumerable)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), globalFuncPropertyIsEnumerable));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::ownKeys)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), globalFuncOwnKeys));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::enqueueJob)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "enqueueJob"_s, enqueueJob));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::makeTypeError)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), globalFuncMakeTypeError));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::AggregateError)].initLater([] (const Initializer<JSCell>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            init.set(globalObject->m_aggregateErrorStructure.constructor(globalObject));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::typedArrayLength)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), typedArrayViewPrivateFuncLength));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::typedArrayGetOriginalConstructor)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), typedArrayViewPrivateFuncGetOriginalConstructor));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::typedArraySort)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), typedArrayViewPrivateFuncSort));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isTypedArrayView)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), typedArrayViewPrivateFuncIsTypedArrayView, IsTypedArrayViewIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::typedArraySubarrayCreate)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), typedArrayViewPrivateFuncSubarrayCreate));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isBoundFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), isBoundFunction));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::hasInstanceBoundFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), hasInstanceBoundFunction));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::instanceOf)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), objectPrivateFuncInstanceOf));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::BuiltinLog)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), globalFuncBuiltinLog));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::BuiltinDescribe)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), globalFuncBuiltinDescribe));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::trunc)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), mathProtoFuncTrunc, TruncIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::repeatCharacter)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), stringProtoFuncRepeatCharacter));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::arraySpeciesCreate)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), arrayProtoFuncSpeciesCreate));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isArraySlow)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), arrayConstructorPrivateFuncIsArraySlow));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::concatMemcpy)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), arrayProtoPrivateFuncConcatMemcpy));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::appendMemcpy)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), arrayProtoPrivateFuncAppendMemcpy));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::hostPromiseRejectionTracker)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), globalFuncHostPromiseRejectionTracker));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::InspectorInstrumentation)].initLater([] (const Initializer<JSCell>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            init.set(InspectorInstrumentationObject::create(init.vm, globalObject, InspectorInstrumentationObject::createStructure(init.vm, globalObject, globalObject->m_objectPrototype.get())));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::thisTimeValue)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), dateProtoFuncGetTime, DatePrototypeGetTimeIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::sameValue)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), objectConstructorIs, ObjectIsIntrinsic));
        });

    // RegExp.prototype helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpCreate)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), esSpecRegExpCreate, NoIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isRegExp)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), esSpecIsRegExp, NoIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpMatchFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), regExpProtoFuncMatchFast, RegExpMatchFastIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpSearchFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), regExpProtoFuncSearchFast));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpSplitFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), regExpProtoFuncSplitFast));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpTestFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), regExpProtoFuncTestFast, RegExpTestFastIntrinsic));
        });

    // String.prototype helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringIncludesInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), builtinStringIncludesInternal));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringIndexOfInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), builtinStringIndexOfInternal));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringSplitFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), stringProtoFuncSplitFast));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringSubstringInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, String(), builtinStringSubstringInternal));
        });

    // Function prototype helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::makeBoundFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 5, String(), makeBoundFunction));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::hasOwnLengthProperty)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), hasOwnLengthProperty));
        });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::dateTimeFormat)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), globalFuncDateTimeFormat));
        });

    // PrivateSymbols / PrivateNames
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::createPrivateSymbol)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, String(), createPrivateSymbol));
        });

#if ENABLE(WEBASSEMBLY) && ENABLE(WEBASSEMBLY_STREAMING_API)
    // WebAssembly Streaming API
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::webAssemblyCompileStreamingInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), webAssemblyCompileStreamingInternal));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::webAssemblyInstantiateStreamingInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, String(), webAssemblyInstantiateStreamingInternal));
        });
#endif

    if (Options::exposeProfilersOnGlobalObject()) {
#if ENABLE(SAMPLING_PROFILER)
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__enableSamplingProfiler"), JSFunction::create(vm, this, 1, String(), enableSamplingProfiler), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__disableSamplingProfiler"), JSFunction::create(vm, this, 1, String(), disableSamplingProfiler), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
#endif
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__enableSuperSampler"), JSFunction::create(vm, this, 1, String(), enableSuperSampler), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__disableSuperSampler"), JSFunction::create(vm, this, 1, String(), disableSuperSampler), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    }

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(vm.propertyNames->NaN, jsNaN(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->Infinity, jsNumber(std::numeric_limits<double>::infinity()), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->undefinedKeyword, jsUndefined(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#if ASSERT_ENABLED
        GlobalPropertyInfo(vm.propertyNames->builtinNames().assertPrivateName(), JSFunction::create(vm, this, 1, String(), assertCall), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#endif
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
    
    if (UNLIKELY(Options::useDollarVM()))
        exposeDollarVM(vm);

#if ENABLE(WEBASSEMBLY)
    if (Wasm::isSupported()) {
        m_webAssemblyModuleRecordStructure.initLater(
            [] (const Initializer<Structure>& init) {
                init.set(WebAssemblyModuleRecord::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
            });
        m_webAssemblyFunctionStructure.initLater(
            [] (const Initializer<Structure>& init) {
                init.set(WebAssemblyFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
            });
        m_jsToWasmICCalleeStructure.initLater(
            [] (const Initializer<Structure>& init) {
                init.set(JSToWasmICCallee::createStructure(init.vm, init.owner, jsNull()));
            });
        m_webAssemblyWrapperFunctionStructure.initLater(
            [] (const Initializer<Structure>& init) {
                init.set(WebAssemblyWrapperFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
            });
        auto* webAssembly = JSWebAssembly::create(vm, this, JSWebAssembly::createStructure(vm, this, m_objectPrototype.get()));
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "WebAssembly"), webAssembly, static_cast<unsigned>(PropertyAttribute::DontEnum));

#define CREATE_WEBASSEMBLY_PROTOTYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    if (featureFlag) {\
        m_ ## properName ## Structure.initLater(\
            [] (LazyClassStructure::Initializer& init) { \
                init.setPrototype(capitalName##Prototype::create(init.vm, init.global, capitalName##Prototype::createStructure(init.vm, init.global, init.global->prototypeBase ## Prototype()))); \
                init.setStructure(instanceType::createStructure(init.vm, init.global, init.prototype)); \
                auto* constructorPrototype = strcmp(#prototypeBase, "error") == 0 ? init.global->m_errorStructure.constructor(init.global) : init.global->functionPrototype(); \
                init.setConstructor(capitalName ## Constructor::create(init.vm, capitalName ## Constructor::createStructure(init.vm, init.global, constructorPrototype), jsCast<capitalName ## Prototype*>(init.prototype))); \
            }); \
    }

        FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(CREATE_WEBASSEMBLY_PROTOTYPE)

#undef CREATE_WEBASSEMBLY_CONSTRUCTOR
    }
#endif // ENABLE(WEBASSEMBLY)

#undef CREATE_PROTOTYPE_FOR_LAZY_TYPE


    {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, arrayIteratorPrototype, vm.propertyNames->next);
        m_arrayIteratorPrototypeNext = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_arrayIteratorProtocolWatchpointSet);
        m_arrayIteratorPrototypeNext->install(vm);
    }
    {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, this->arrayPrototype(), vm.propertyNames->iteratorSymbol);
        m_arrayPrototypeSymbolIteratorWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_arrayIteratorProtocolWatchpointSet);
        m_arrayPrototypeSymbolIteratorWatchpoint->install(vm);
    }
    {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, this->arrayPrototype(), vm.propertyNames->join);
        m_arrayPrototypeJoinWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_arrayJoinWatchpointSet);
        m_arrayPrototypeJoinWatchpoint->install(vm);
    }

    {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, mapIteratorPrototype, vm.propertyNames->next);
        m_mapIteratorPrototypeNextWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_mapIteratorProtocolWatchpointSet);
        m_mapIteratorPrototypeNextWatchpoint->install(vm);
    }
    {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, setIteratorPrototype, vm.propertyNames->next);
        m_setIteratorPrototypeNextWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_setIteratorProtocolWatchpointSet);
        m_setIteratorPrototypeNextWatchpoint->install(vm);
    }
    {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, m_stringIteratorPrototype.get(), vm.propertyNames->next);
        m_stringIteratorPrototypeNextWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_stringIteratorProtocolWatchpointSet);
        m_stringIteratorPrototypeNextWatchpoint->install(vm);
    }
    {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, m_stringPrototype.get(), vm.propertyNames->iteratorSymbol);
        m_stringPrototypeSymbolIteratorWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_stringIteratorProtocolWatchpointSet);
        m_stringPrototypeSymbolIteratorWatchpoint->install(vm);
    }

    // Unfortunately, the prototype objects of the builtin objects can be touched from concurrent compilers. So eagerly initialize them only if we use JIT.
    if (Options::useJIT()) {
        this->booleanPrototype();
        this->numberPrototype();
        this->symbolPrototype();
    }

    fixupPrototypeChainWithObjectPrototype(vm);
}

bool JSGlobalObject::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        RELEASE_AND_RETURN(scope, ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode()));

    bool shouldThrowReadOnlyError = slot.isStrictMode();
    bool ignoreReadOnlyErrors = false;
    bool putResult = false;
    bool done = symbolTablePutTouchWatchpointSet(thisObject, globalObject, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
    EXCEPTION_ASSERT((!!scope.exception() == (done && !putResult)) || !shouldThrowReadOnlyError);
    if (done)
        return putResult;
    RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
}

bool JSGlobalObject::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::VMInquiry, &vm);
    // silently ignore attempts to add accessors aliasing vars.
    if (descriptor.isAccessorDescriptor() && symbolTableGet(thisObject, propertyName, slot))
        return false;
    slot.disallowVMEntry.reset();
    return Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow);
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

void JSGlobalObject::addFunction(JSGlobalObject* globalObject, const Identifier& propertyName)
{
    VM& vm = globalObject->vm();
    VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
    JSCell::deleteProperty(this, globalObject, propertyName);
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
                        rareData->clearInternalFunctionAllocationProfile("have a bad time breaking internal function allocation");
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

    // Make sure that all JSArray allocations that load the appropriate structure from
    // this object now load a structure that uses SlowPut.
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i].set(vm, this, originalArrayStructureForIndexingType(ArrayWithSlowPutArrayStorage));

    // Same for any special array structures.
    Structure* slowPutStructure;
    slowPutStructure = createRegExpMatchesArraySlowPutStructure(vm, this);
    m_regExpMatchesArrayStructure.set(vm, this, slowPutStructure);
    slowPutStructure = ClonedArguments::createSlowPutStructure(vm, this, m_objectPrototype.get());
    m_clonedArgumentsStructure.set(vm, this, slowPutStructure);

    // Make sure that all allocations or indexed storage transitions that are inlining
    // the assumption that it's safe to transition to a non-SlowPut array storage don't
    // do so anymore.
    // Note: we are deliberately firing the watchpoint here at the end only after
    // making all the array structures SlowPut. This ensures that the concurrent
    // JIT threads will always get the SlowPut versions of the structures if
    // isHavingABadTime() returns true. The concurrent JIT relies on this.
    m_havingABadTimeWatchpoint->fireAll(vm, "Having a bad time");
    ASSERT(isHavingABadTime()); // The watchpoint is what tells us that we're having a bad time.
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

void JSGlobalObject::fixupPrototypeChainWithObjectPrototype(VM& vm)
{
    JSObject* oldLastInPrototypeChain = lastInPrototypeChain(vm, this);
    JSObject* objectPrototype = m_objectPrototype.get();
    if (oldLastInPrototypeChain != objectPrototype)
        oldLastInPrototypeChain->setPrototypeDirect(vm, objectPrototype);
}

// Set prototype, and also insert the object prototype at the end of the chain.
void JSGlobalObject::resetPrototype(VM& vm, JSValue prototype)
{
    if (getPrototypeDirect(vm) == prototype)
        return;
    setPrototypeDirect(vm, prototype);
    fixupPrototypeChainWithObjectPrototype(vm);
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
    thisObject->m_evalErrorStructure.visit(visitor);
    thisObject->m_rangeErrorStructure.visit(visitor);
    thisObject->m_referenceErrorStructure.visit(visitor);
    thisObject->m_syntaxErrorStructure.visit(visitor);
    thisObject->m_typeErrorStructure.visit(visitor);
    thisObject->m_URIErrorStructure.visit(visitor);
    thisObject->m_aggregateErrorStructure.visit(visitor);
    visitor.append(thisObject->m_arrayConstructor);
    visitor.append(thisObject->m_regExpConstructor);
    visitor.append(thisObject->m_objectConstructor);
    visitor.append(thisObject->m_functionConstructor);
    visitor.append(thisObject->m_promiseConstructor);
    visitor.append(thisObject->m_internalPromiseConstructor);

    thisObject->m_defaultCollator.visit(visitor);
    thisObject->m_collatorStructure.visit(visitor);
    thisObject->m_dateTimeFormatStructure.visit(visitor);
    thisObject->m_displayNamesStructure.visit(visitor);
    thisObject->m_numberFormatStructure.visit(visitor);
    thisObject->m_localeStructure.visit(visitor);
    thisObject->m_pluralRulesStructure.visit(visitor);
    thisObject->m_relativeTimeFormatStructure.visit(visitor);
    thisObject->m_segmentIteratorStructure.visit(visitor);
    thisObject->m_segmenterStructure.visit(visitor);
    thisObject->m_segmentsStructure.visit(visitor);

    visitor.append(thisObject->m_nullGetterFunction);
    visitor.append(thisObject->m_nullSetterFunction);
    visitor.append(thisObject->m_nullSetterStrictFunction);

    thisObject->m_parseIntFunction.visit(visitor);
    thisObject->m_parseFloatFunction.visit(visitor);
    thisObject->m_arrayProtoToStringFunction.visit(visitor);
    thisObject->m_arrayProtoValuesFunction.visit(visitor);
    thisObject->m_evalFunction.visit(visitor);
    thisObject->m_iteratorProtocolFunction.visit(visitor);
    thisObject->m_promiseResolveFunction.visit(visitor);
    visitor.append(thisObject->m_objectProtoValueOfFunction);
    thisObject->m_numberProtoToStringFunction.visit(visitor);
    visitor.append(thisObject->m_functionProtoHasInstanceSymbolFunction);
    thisObject->m_throwTypeErrorGetterSetter.visit(visitor);
    visitor.append(thisObject->m_regExpProtoSymbolReplace);
    visitor.append(thisObject->m_throwTypeErrorArgumentsCalleeAndCallerGetterSetter);
    thisObject->m_moduleLoader.visit(visitor);

    visitor.append(thisObject->m_objectPrototype);
    visitor.append(thisObject->m_functionPrototype);
    visitor.append(thisObject->m_arrayPrototype);
    visitor.append(thisObject->m_iteratorPrototype);
    visitor.append(thisObject->m_generatorFunctionPrototype);
    visitor.append(thisObject->m_generatorPrototype);
    visitor.append(thisObject->m_arrayIteratorPrototype);
    visitor.append(thisObject->m_mapIteratorPrototype);
    visitor.append(thisObject->m_setIteratorPrototype);
    visitor.append(thisObject->m_asyncFunctionPrototype);
    visitor.append(thisObject->m_asyncGeneratorPrototype);
    visitor.append(thisObject->m_asyncIteratorPrototype);
    visitor.append(thisObject->m_asyncGeneratorFunctionPrototype);

    thisObject->m_debuggerScopeStructure.visit(visitor);
    thisObject->m_withScopeStructure.visit(visitor);
    thisObject->m_strictEvalActivationStructure.visit(visitor);
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
    thisObject->m_nativeStdFunctionStructure.visit(visitor);
    visitor.append(thisObject->m_regExpStructure);
    visitor.append(thisObject->m_generatorFunctionStructure);
    visitor.append(thisObject->m_asyncFunctionStructure);
    visitor.append(thisObject->m_asyncGeneratorFunctionStructure);
    visitor.append(thisObject->m_generatorStructure);
    visitor.append(thisObject->m_asyncGeneratorStructure);
    visitor.append(thisObject->m_arrayIteratorStructure);
    visitor.append(thisObject->m_mapIteratorStructure);
    visitor.append(thisObject->m_setIteratorStructure);
    thisObject->m_iteratorResultObjectStructure.visit(visitor);
    thisObject->m_dataPropertyDescriptorObjectStructure.visit(visitor);
    thisObject->m_accessorPropertyDescriptorObjectStructure.visit(visitor);
    visitor.append(thisObject->m_regExpMatchesArrayStructure);
    thisObject->m_moduleRecordStructure.visit(visitor);
    thisObject->m_moduleNamespaceObjectStructure.visit(visitor);
    thisObject->m_proxyObjectStructure.visit(visitor);
    thisObject->m_callableProxyObjectStructure.visit(visitor);
    thisObject->m_proxyRevokeStructure.visit(visitor);

    for (auto& property : thisObject->m_linkTimeConstants)
        property.visit(visitor);
    
#if ENABLE(SHARED_ARRAY_BUFFER)
    visitor.append(thisObject->m_sharedArrayBufferPrototype);
    visitor.append(thisObject->m_sharedArrayBufferStructure);
#endif

#define VISIT_SIMPLE_TYPE(CapitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) { \
        visitor.append(thisObject->m_ ## lowerName ## Prototype); \
        visitor.append(thisObject->m_ ## properName ## Structure); \
    }

    FOR_EACH_SIMPLE_BUILTIN_TYPE(VISIT_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(VISIT_SIMPLE_TYPE)

#define VISIT_LAZY_TYPE(CapitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) \
        thisObject->m_ ## properName ## Structure.visit(visitor);

    FOR_EACH_LAZY_BUILTIN_TYPE(VISIT_LAZY_TYPE)

#if ENABLE(WEBASSEMBLY)
    thisObject->m_webAssemblyModuleRecordStructure.visit(visitor);
    thisObject->m_webAssemblyFunctionStructure.visit(visitor);
    thisObject->m_jsToWasmICCalleeStructure.visit(visitor);
    thisObject->m_webAssemblyWrapperFunctionStructure.visit(visitor);
    FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(VISIT_LAZY_TYPE)
#endif // ENABLE(WEBASSEMBLY)

#undef VISIT_SIMPLE_TYPE
#undef VISIT_LAZY_TYPE

    for (unsigned i = NumberOfTypedArrayTypes; i--;)
        thisObject->lazyTypedArrayStructure(indexToTypedArrayType(i)).visit(visitor);
    
    visitor.append(thisObject->m_speciesGetterSetter);
    thisObject->m_typedArrayProto.visit(visitor);
    thisObject->m_typedArraySuperConstructor.visit(visitor);
    thisObject->m_regExpGlobalData.visitAggregate(visitor);
}

CallFrame* JSGlobalObject::deprecatedCallFrameForDebugger()
{
    return CallFrame::create(m_deprecatedCallFrameForDebugger);
}

void JSGlobalObject::exposeDollarVM(VM& vm)
{
    RELEASE_ASSERT(g_jscConfig.restrictedOptionsEnabled && Options::useDollarVM());
    if (hasOwnProperty(this, vm.propertyNames->builtinNames().dollarVMPrivateName()))
        return;

    JSDollarVM* dollarVM = JSDollarVM::create(vm, JSDollarVM::createStructure(vm, this, m_objectPrototype.get()));

    GlobalPropertyInfo extraStaticGlobals[] = {
        GlobalPropertyInfo(vm.propertyNames->builtinNames().dollarVMPrivateName(), dollarVM, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };
    addStaticGlobals(extraStaticGlobals, WTF_ARRAY_LENGTH(extraStaticGlobals));

    putDirect(vm, Identifier::fromString(vm, "$vm"), dollarVM, static_cast<unsigned>(PropertyAttribute::DontEnum));
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

bool JSGlobalObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    if (Base::getOwnPropertySlot(object, globalObject, propertyName, slot))
        return true;
    return symbolTableGet(jsCast<JSGlobalObject*>(object), propertyName, slot);
}

void JSGlobalObject::clearRareData(JSCell* cell)
{
    jsCast<JSGlobalObject*>(cell)->m_rareData = nullptr;
}

void JSGlobalObject::tryInstallArraySpeciesWatchpoint()
{
    RELEASE_ASSERT(!m_arrayPrototypeConstructorWatchpoint);
    RELEASE_ASSERT(!m_arrayConstructorSpeciesWatchpoint);

    VM& vm = this->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // First we need to make sure that the Array.prototype.constructor property points to Array
    // and that Array[Symbol.species] is the primordial GetterSetter.
    ArrayPrototype* arrayPrototype = this->arrayPrototype();

    // We only initialize once so flattening the structures does not have any real cost.
    Structure* prototypeStructure = arrayPrototype->structure(vm);
    if (prototypeStructure->isDictionary())
        prototypeStructure = prototypeStructure->flattenDictionaryStructure(vm, arrayPrototype);
    RELEASE_ASSERT(!prototypeStructure->isDictionary());

    ArrayConstructor* arrayConstructor = this->arrayConstructor();

    auto invalidateWatchpoint = [&] {
        m_arraySpeciesWatchpointSet.invalidate(vm, StringFireDetail("Was not able to set up array species watchpoint."));
    };

    PropertySlot constructorSlot(arrayPrototype, PropertySlot::InternalMethodType::VMInquiry, &vm);
    arrayPrototype->getOwnPropertySlot(arrayPrototype, this, vm.propertyNames->constructor, constructorSlot);
    scope.assertNoException();
    if (constructorSlot.slotBase() != arrayPrototype
        || !constructorSlot.isCacheableValue()
        || constructorSlot.getValue(this, vm.propertyNames->constructor) != arrayConstructor) {
        invalidateWatchpoint();
        return;
    }

    Structure* constructorStructure = arrayConstructor->structure(vm);
    if (constructorStructure->isDictionary())
        constructorStructure = constructorStructure->flattenDictionaryStructure(vm, arrayConstructor);

    PropertySlot speciesSlot(arrayConstructor, PropertySlot::InternalMethodType::VMInquiry, &vm);
    arrayConstructor->getOwnPropertySlot(arrayConstructor, this, vm.propertyNames->speciesSymbol, speciesSlot);
    scope.assertNoException();
    if (speciesSlot.slotBase() != arrayConstructor
        || !speciesSlot.isCacheableGetter()
        || speciesSlot.getterSetter() != speciesGetterSetter()) {
        invalidateWatchpoint();
        return;
    }

    // Now we need to setup the watchpoints to make sure these conditions remain valid.
    prototypeStructure->startWatchingPropertyForReplacements(vm, constructorSlot.cachedOffset());
    constructorStructure->startWatchingPropertyForReplacements(vm, speciesSlot.cachedOffset());

    ObjectPropertyCondition constructorCondition = ObjectPropertyCondition::equivalence(vm, arrayPrototype, arrayPrototype, vm.propertyNames->constructor.impl(), arrayConstructor);
    ObjectPropertyCondition speciesCondition = ObjectPropertyCondition::equivalence(vm, arrayPrototype, arrayConstructor, vm.propertyNames->speciesSymbol.impl(), speciesGetterSetter());

    if (!constructorCondition.isWatchable() || !speciesCondition.isWatchable()) {
        invalidateWatchpoint();
        return;
    }

    // We only watch this from the DFG, and the DFG makes sure to only start watching if the watchpoint is in the IsWatched state.
    RELEASE_ASSERT(!m_arraySpeciesWatchpointSet.isBeingWatched());
    m_arraySpeciesWatchpointSet.touch(vm, "Set up array species watchpoint.");

    m_arrayPrototypeConstructorWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, constructorCondition, m_arraySpeciesWatchpointSet);
    m_arrayPrototypeConstructorWatchpoint->install(vm);

    m_arrayConstructorSpeciesWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, speciesCondition, m_arraySpeciesWatchpointSet);
    m_arrayConstructorSpeciesWatchpoint->install(vm);
}

void JSGlobalObject::installNumberPrototypeWatchpoint(NumberPrototype* numberPrototype)
{
    VM& vm = this->vm();
    ASSERT(m_numberToStringWatchpointSet.isStillValid());
    ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, numberPrototype, vm.propertyNames->toString);
    m_numberPrototypeToStringWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_numberToStringWatchpointSet);
    m_numberPrototypeToStringWatchpoint->install(vm);
}

void JSGlobalObject::installMapPrototypeWatchpoint(MapPrototype* mapPrototype)
{
    VM& vm = this->vm();
    if (m_mapIteratorProtocolWatchpointSet.isStillValid()) {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, mapPrototype, vm.propertyNames->iteratorSymbol);
        m_mapPrototypeSymbolIteratorWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_mapIteratorProtocolWatchpointSet);
        m_mapPrototypeSymbolIteratorWatchpoint->install(vm);
    }
    ASSERT(m_mapSetWatchpointSet.isStillValid());
    ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, mapPrototype, vm.propertyNames->set);
    m_mapPrototypeSetWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_mapSetWatchpointSet);
    m_mapPrototypeSetWatchpoint->install(vm);
}

void JSGlobalObject::installSetPrototypeWatchpoint(SetPrototype* setPrototype)
{
    VM& vm = this->vm();
    if (m_setIteratorProtocolWatchpointSet.isStillValid()) {
        ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, setPrototype, vm.propertyNames->iteratorSymbol);
        m_setPrototypeSymbolIteratorWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_setIteratorProtocolWatchpointSet);
        m_setPrototypeSymbolIteratorWatchpoint->install(vm);
    }
    ASSERT(m_setAddWatchpointSet.isStillValid());
    ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, setPrototype, vm.propertyNames->add);
    m_setPrototypeAddWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_setAddWatchpointSet);
    m_setPrototypeAddWatchpoint->install(vm);
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

void JSGlobalObject::setIsITML()
{
#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->setIsITML();
#endif
}

void JSGlobalObject::setName(const String& name)
{
    m_name = name;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->update();
#endif
}

void JSGlobalObject::bumpGlobalLexicalBindingEpoch(VM& vm)
{
    if (++m_globalLexicalBindingEpoch == Options::thresholdForGlobalLexicalBindingEpoch()) {
        // Since the epoch overflows, we should rewrite all the CodeBlock to adjust to the newly started generation.
        m_globalLexicalBindingEpoch = 1;
        vm.heap.codeBlockSet().iterate([&] (CodeBlock* codeBlock) {
            if (codeBlock->globalObject() != this)
                return;
            codeBlock->notifyLexicalBindingUpdate();
        });
    }
}

void JSGlobalObject::queueMicrotask(Ref<Microtask>&& task)
{
    if (globalObjectMethodTable()->queueTaskToEventLoop) {
        globalObjectMethodTable()->queueTaskToEventLoop(*this, WTFMove(task));
        return;
    }

    vm().queueMicrotask(*this, WTFMove(task));
}

void JSGlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject*, Exception* exception)
{
    dataLogLn("Uncaught Exception at run loop: ", exception->value());
}

void JSGlobalObject::setDebugger(Debugger* debugger)
{
    m_debugger = debugger;
    if (debugger)
        vm().ensureShadowChicken();
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
    ConcurrentJSLocker locker(m_referencedGlobalPropertyWatchpointSetsLock);
    return m_referencedGlobalPropertyWatchpointSets.get(uid);
}

WatchpointSet& JSGlobalObject::ensureReferencedPropertyWatchpointSet(UniquedStringImpl* uid)
{
    ConcurrentJSLocker locker(m_referencedGlobalPropertyWatchpointSetsLock);
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
