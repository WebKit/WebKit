/*
 * Copyright (C) 2007, 2008, 2009, 2014, 2015 Apple Inc. All rights reserved.
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
#include "BooleanConstructor.h"
#include "BooleanPrototype.h"
#include "BuiltinNames.h"
#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CodeCache.h"
#include "ConsolePrototype.h"
#include "DateConstructor.h"
#include "DatePrototype.h"
#include "Debugger.h"
#include "DebuggerScope.h"
#include "DirectArguments.h"
#include "Error.h"
#include "ErrorConstructor.h"
#include "ErrorPrototype.h"
#include "FunctionConstructor.h"
#include "FunctionPrototype.h"
#include "GeneratorFunctionConstructor.h"
#include "GeneratorFunctionPrototype.h"
#include "GeneratorPrototype.h"
#include "GetterSetter.h"
#include "HeapIterationScope.h"
#include "InspectorInstrumentationObject.h"
#include "Interpreter.h"
#include "IteratorPrototype.h"
#include "JSAPIWrapperObject.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferConstructor.h"
#include "JSArrayBufferPrototype.h"
#include "JSArrayIterator.h"
#include "JSArrowFunction.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSConsole.h"
#include "JSDataView.h"
#include "JSDataViewPrototype.h"
#include "JSDollarVM.h"
#include "JSDollarVMPrototype.h"
#include "JSFunction.h"
#include "JSGeneratorFunction.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSGenericTypedArrayViewPrototypeInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSInternalPromisePrototype.h"
#include "JSJob.h"
#include "JSLexicalEnvironment.h"
#include "JSLock.h"
#include "JSMap.h"
#include "JSMapIterator.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSNativeStdFunction.h"
#include "JSONObject.h"
#include "JSPromise.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSPropertyNameIterator.h"
#include "JSSet.h"
#include "JSSetIterator.h"
#include "JSStringIterator.h"
#include "JSTemplateRegistryKey.h"
#include "JSTypedArrayConstructors.h"
#include "JSTypedArrayPrototypes.h"
#include "JSTypedArrayViewConstructor.h"
#include "JSTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"
#include "JSWASMModule.h"
#include "JSWeakMap.h"
#include "JSWeakSet.h"
#include "JSWithScope.h"
#include "LegacyProfiler.h"
#include "Lookup.h"
#include "MapConstructor.h"
#include "MapIteratorPrototype.h"
#include "MapPrototype.h"
#include "MathObject.h"
#include "Microtask.h"
#include "ModuleLoaderObject.h"
#include "NativeErrorConstructor.h"
#include "NativeErrorPrototype.h"
#include "NullGetterFunction.h"
#include "NullSetterFunction.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"
#include "ObjCCallbackFunction.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "ParserError.h"
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
#include "SymbolPrototype.h"
#include "VariableWriteFireDetail.h"
#include "WeakGCMapInlines.h"
#include "WeakMapConstructor.h"
#include "WeakMapPrototype.h"
#include "WeakSetConstructor.h"
#include "WeakSetPrototype.h"

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

#if ENABLE(WEB_REPLAY)
#include "EmptyInputCursor.h"
#include "JSReplayInputs.h"
#endif

#include "JSGlobalObject.lut.h"

namespace JSC {

const ClassInfo JSGlobalObject::s_info = { "GlobalObject", &Base::s_info, &globalObjectTable, CREATE_METHOD_TABLE(JSGlobalObject) };

const GlobalObjectMethodTable JSGlobalObject::s_globalObjectMethodTable = { &allowsAccessFrom, &supportsProfiling, &supportsRichSourceInfo, &shouldInterruptScript, &javaScriptRuntimeFlags, nullptr, &shouldInterruptScriptBeforeTimeout, nullptr, nullptr, nullptr, nullptr, nullptr };

/* Source for JSGlobalObject.lut.h
@begin globalObjectTable
  parseFloat            globalFuncParseFloat            DontEnum|Function 1
  isNaN                 globalFuncIsNaN                 DontEnum|Function 1
  isFinite              globalFuncIsFinite              DontEnum|Function 1
  escape                globalFuncEscape                DontEnum|Function 1
  unescape              globalFuncUnescape              DontEnum|Function 1
  decodeURI             globalFuncDecodeURI             DontEnum|Function 1
  decodeURIComponent    globalFuncDecodeURIComponent    DontEnum|Function 1
  encodeURI             globalFuncEncodeURI             DontEnum|Function 1
  encodeURIComponent    globalFuncEncodeURIComponent    DontEnum|Function 1
@end
*/

static EncodedJSValue JSC_HOST_CALL getTemplateObject(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    ASSERT(thisValue.inherits(JSTemplateRegistryKey::info()));
    return JSValue::encode(exec->lexicalGlobalObject()->templateRegistry().getTemplateObject(exec, jsCast<JSTemplateRegistryKey*>(thisValue)->templateRegistryKey()));
}


static EncodedJSValue JSC_HOST_CALL enqueueJob(ExecState* exec)
{
    VM& vm = exec->vm();
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    JSValue job = exec->argument(0);
    JSValue arguments = exec->argument(1);
    ASSERT(arguments.inherits(JSArray::info()));

    globalObject->queueMicrotask(createJSJob(vm, job, jsCast<JSArray*>(arguments)));

    return JSValue::encode(jsUndefined());
}

JSGlobalObject::JSGlobalObject(VM& vm, Structure* structure, const GlobalObjectMethodTable* globalObjectMethodTable)
    : Base(vm, structure, 0)
    , m_vm(vm)
#if ENABLE(WEB_REPLAY)
    , m_inputCursor(EmptyInputCursor::create())
#endif
    , m_masqueradesAsUndefinedWatchpoint(adoptRef(new WatchpointSet(IsWatched)))
    , m_havingABadTimeWatchpoint(adoptRef(new WatchpointSet(IsWatched)))
    , m_varInjectionWatchpoint(adoptRef(new WatchpointSet(IsWatched)))
    , m_weakRandom(Options::forceWeakRandomSeed() ? Options::forcedWeakRandomSeed() : static_cast<unsigned>(randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0)))
    , m_templateRegistry(vm)
    , m_evalEnabled(true)
    , m_runtimeFlags()
    , m_consoleClient(nullptr)
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

    if (LegacyProfiler* profiler = vm().enabledProfiler())
        profiler->stopProfiling(this);
}

void JSGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSGlobalObject*>(cell)->JSGlobalObject::~JSGlobalObject();
}

void JSGlobalObject::setGlobalThis(VM& vm, JSObject* globalThis)
{
    m_globalThis.set(vm, this, globalThis);
}

void JSGlobalObject::init(VM& vm)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());

    JSGlobalObject::globalExec()->init(0, 0, CallFrame::noCaller(), 0, 0);

    m_debugger = 0;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorController = std::make_unique<Inspector::JSGlobalObjectInspectorController>(*this);
    m_inspectorDebuggable = std::make_unique<JSGlobalObjectDebuggable>(*this);
    m_inspectorDebuggable->init();
    m_consoleClient = m_inspectorController->consoleClient();
#endif

    ExecState* exec = JSGlobalObject::globalExec();

    m_functionPrototype.set(vm, this, FunctionPrototype::create(vm, FunctionPrototype::createStructure(vm, this, jsNull()))); // The real prototype will be set once ObjectPrototype is created.
    m_calleeStructure.set(vm, this, JSCallee::createStructure(vm, this, jsNull()));

    m_globalLexicalEnvironment.set(vm, this, JSGlobalLexicalEnvironment::create(vm, JSGlobalLexicalEnvironment::createStructure(vm, this), this));
    // Need to create the callee structure (above) before creating the callee.
    m_globalCallee.set(vm, this, JSCallee::create(vm, this, globalScope()));
    exec->setCallee(m_globalCallee.get());

    m_functionStructure.set(vm, this, JSFunction::createStructure(vm, this, m_functionPrototype.get()));
    m_arrowFunctionStructure.set(vm, this, JSArrowFunction::createStructure(vm, this, m_functionPrototype.get()));
    m_boundFunctionStructure.set(vm, this, JSBoundFunction::createStructure(vm, this, m_functionPrototype.get()));
    m_nativeStdFunctionStructure.set(vm, this, JSNativeStdFunction::createStructure(vm, this, m_functionPrototype.get()));
    m_namedFunctionStructure.set(vm, this, Structure::addPropertyTransition(vm, m_functionStructure.get(), vm.propertyNames->name, DontDelete | ReadOnly | DontEnum, m_functionNameOffset));
    m_internalFunctionStructure.set(vm, this, InternalFunction::createStructure(vm, this, m_functionPrototype.get()));
    JSFunction* callFunction = 0;
    JSFunction* applyFunction = 0;
    m_functionPrototype->addFunctionProperties(exec, this, &callFunction, &applyFunction);
    m_callFunction.set(vm, this, callFunction);
    m_applyFunction.set(vm, this, applyFunction);
    m_arrayProtoValuesFunction.set(vm, this, JSFunction::create(vm, this, 0, vm.propertyNames->values.string(), arrayProtoFuncValues));
    m_initializePromiseFunction.set(vm, this, JSFunction::createBuiltinFunction(vm, promiseOperationsInitializePromiseCodeGenerator(vm), this));
    m_newPromiseCapabilityFunction.set(vm, this, JSFunction::createBuiltinFunction(vm, promiseOperationsNewPromiseCapabilityCodeGenerator(vm), this));
    m_nullGetterFunction.set(vm, this, NullGetterFunction::create(vm, NullGetterFunction::createStructure(vm, this, m_functionPrototype.get())));
    m_nullSetterFunction.set(vm, this, NullSetterFunction::create(vm, NullSetterFunction::createStructure(vm, this, m_functionPrototype.get())));
    m_objectPrototype.set(vm, this, ObjectPrototype::create(vm, this, ObjectPrototype::createStructure(vm, this, jsNull())));
    GetterSetter* protoAccessor = GetterSetter::create(vm, this);
    protoAccessor->setGetter(vm, this, JSFunction::create(vm, this, 0, String(), globalFuncProtoGetter));
    protoAccessor->setSetter(vm, this, JSFunction::create(vm, this, 0, String(), globalFuncProtoSetter));
    m_objectPrototype->putDirectNonIndexAccessor(vm, vm.propertyNames->underscoreProto, protoAccessor, Accessor | DontEnum);
    m_functionPrototype->structure()->setPrototypeWithoutTransition(vm, m_objectPrototype.get());

    JSTypedArrayViewPrototype* typedArrayProto = JSTypedArrayViewPrototype::create(vm, this, JSTypedArrayViewPrototype::createStructure(vm, this, m_objectPrototype.get()));

    m_typedArrays[toIndex(TypeInt8)].prototype.set(vm, this, JSInt8ArrayPrototype::create(vm, this, JSInt8ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeInt16)].prototype.set(vm, this, JSInt16ArrayPrototype::create(vm, this, JSInt16ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeInt32)].prototype.set(vm, this, JSInt32ArrayPrototype::create(vm, this, JSInt32ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeUint8)].prototype.set(vm, this, JSUint8ArrayPrototype::create(vm, this, JSUint8ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeUint8Clamped)].prototype.set(vm, this, JSUint8ClampedArrayPrototype::create(vm, this, JSUint8ClampedArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeUint16)].prototype.set(vm, this, JSUint16ArrayPrototype::create(vm, this, JSUint16ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeUint32)].prototype.set(vm, this, JSUint32ArrayPrototype::create(vm, this, JSUint32ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeFloat32)].prototype.set(vm, this, JSFloat32ArrayPrototype::create(vm, this, JSFloat32ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeFloat64)].prototype.set(vm, this, JSFloat64ArrayPrototype::create(vm, this, JSFloat64ArrayPrototype::createStructure(vm, this, typedArrayProto)));
    m_typedArrays[toIndex(TypeDataView)].prototype.set(vm, this, JSDataViewPrototype::create(vm, JSDataViewPrototype::createStructure(vm, this, m_objectPrototype.get())));
    
    m_typedArrays[toIndex(TypeInt8)].structure.set(vm, this, JSInt8Array::createStructure(vm, this, m_typedArrays[toIndex(TypeInt8)].prototype.get()));
    m_typedArrays[toIndex(TypeInt16)].structure.set(vm, this, JSInt16Array::createStructure(vm, this, m_typedArrays[toIndex(TypeInt16)].prototype.get()));
    m_typedArrays[toIndex(TypeInt32)].structure.set(vm, this, JSInt32Array::createStructure(vm, this, m_typedArrays[toIndex(TypeInt32)].prototype.get()));
    m_typedArrays[toIndex(TypeUint8)].structure.set(vm, this, JSUint8Array::createStructure(vm, this, m_typedArrays[toIndex(TypeUint8)].prototype.get()));
    m_typedArrays[toIndex(TypeUint8Clamped)].structure.set(vm, this, JSUint8ClampedArray::createStructure(vm, this, m_typedArrays[toIndex(TypeUint8Clamped)].prototype.get()));
    m_typedArrays[toIndex(TypeUint16)].structure.set(vm, this, JSUint16Array::createStructure(vm, this, m_typedArrays[toIndex(TypeUint16)].prototype.get()));
    m_typedArrays[toIndex(TypeUint32)].structure.set(vm, this, JSUint32Array::createStructure(vm, this, m_typedArrays[toIndex(TypeUint32)].prototype.get()));
    m_typedArrays[toIndex(TypeFloat32)].structure.set(vm, this, JSFloat32Array::createStructure(vm, this, m_typedArrays[toIndex(TypeFloat32)].prototype.get()));
    m_typedArrays[toIndex(TypeFloat64)].structure.set(vm, this, JSFloat64Array::createStructure(vm, this, m_typedArrays[toIndex(TypeFloat64)].prototype.get()));
    m_typedArrays[toIndex(TypeDataView)].structure.set(vm, this, JSDataView::createStructure(vm, this, m_typedArrays[toIndex(TypeDataView)].prototype.get()));
    
    m_lexicalEnvironmentStructure.set(vm, this, JSLexicalEnvironment::createStructure(vm, this));
    m_moduleEnvironmentStructure.set(vm, this, JSModuleEnvironment::createStructure(vm, this));
    m_strictEvalActivationStructure.set(vm, this, StrictEvalActivation::createStructure(vm, this, jsNull()));
    m_debuggerScopeStructure.set(m_vm, this, DebuggerScope::createStructure(m_vm, this));
    m_withScopeStructure.set(vm, this, JSWithScope::createStructure(vm, this, jsNull()));
    
    m_nullPrototypeObjectStructure.set(vm, this, JSFinalObject::createStructure(vm, this, jsNull(), JSFinalObject::defaultInlineCapacity()));
    
    m_callbackFunctionStructure.set(vm, this, JSCallbackFunction::createStructure(vm, this, m_functionPrototype.get()));
    m_directArgumentsStructure.set(vm, this, DirectArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_scopedArgumentsStructure.set(vm, this, ScopedArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_outOfBandArgumentsStructure.set(vm, this, ClonedArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_callbackConstructorStructure.set(vm, this, JSCallbackConstructor::createStructure(vm, this, m_objectPrototype.get()));
    m_callbackObjectStructure.set(vm, this, JSCallbackObject<JSDestructibleObject>::createStructure(vm, this, m_objectPrototype.get()));

#if JSC_OBJC_API_ENABLED
    m_objcCallbackFunctionStructure.set(vm, this, ObjCCallbackFunction::createStructure(vm, this, m_functionPrototype.get()));
    m_objcWrapperObjectStructure.set(vm, this, JSCallbackObject<JSAPIWrapperObject>::createStructure(vm, this, m_objectPrototype.get()));
#endif
    
    m_arrayPrototype.set(vm, this, ArrayPrototype::create(vm, this, ArrayPrototype::createStructure(vm, this, m_objectPrototype.get())));
    
    m_originalArrayStructureForIndexingShape[UndecidedShape >> IndexingShapeShift].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithUndecided));
    m_originalArrayStructureForIndexingShape[Int32Shape >> IndexingShapeShift].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithInt32));
    m_originalArrayStructureForIndexingShape[DoubleShape >> IndexingShapeShift].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithDouble));
    m_originalArrayStructureForIndexingShape[ContiguousShape >> IndexingShapeShift].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithContiguous));
    m_originalArrayStructureForIndexingShape[ArrayStorageShape >> IndexingShapeShift].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithArrayStorage));
    m_originalArrayStructureForIndexingShape[SlowPutArrayStorageShape >> IndexingShapeShift].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithSlowPutArrayStorage));
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i] = m_originalArrayStructureForIndexingShape[i];

    RegExp* emptyRegex = RegExp::create(vm, "", NoFlags);
    
    m_regExpPrototype.set(vm, this, RegExpPrototype::create(vm, RegExpPrototype::createStructure(vm, this, m_objectPrototype.get()), emptyRegex));
    m_regExpStructure.set(vm, this, RegExpObject::createStructure(vm, this, m_regExpPrototype.get()));
    m_regExpMatchesArrayStructure.set(vm, this, createRegExpMatchesArrayStructure(vm, *this));

    m_moduleRecordStructure.set(vm, this, JSModuleRecord::createStructure(vm, this, m_objectPrototype.get()));
    m_moduleNamespaceObjectStructure.set(vm, this, JSModuleNamespaceObject::createStructure(vm, this, jsNull()));
    
#if ENABLE(WEBASSEMBLY)
    m_wasmModuleStructure.set(vm, this, JSWASMModule::createStructure(vm, this));
#endif

    m_parseIntFunction.set(vm, this, JSFunction::create(vm, this, 2, vm.propertyNames->parseInt.string(), globalFuncParseInt, NoIntrinsic));
    putDirectWithoutTransition(vm, vm.propertyNames->parseInt, m_parseIntFunction.get(), DontEnum);

#define CREATE_PROTOTYPE_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName) \
m_ ## lowerName ## Prototype.set(vm, this, capitalName##Prototype::create(vm, this, capitalName##Prototype::createStructure(vm, this, m_objectPrototype.get()))); \
m_ ## properName ## Structure.set(vm, this, instanceType::createStructure(vm, this, m_ ## lowerName ## Prototype.get()));
    
    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)
    
#undef CREATE_PROTOTYPE_FOR_SIMPLE_TYPE

    m_iteratorPrototype.set(vm, this, IteratorPrototype::create(vm, this, IteratorPrototype::createStructure(vm, this, m_objectPrototype.get())));

#define CREATE_PROTOTYPE_FOR_DERIVED_ITERATOR_TYPE(capitalName, lowerName, properName, instanceType, jsName) \
m_ ## lowerName ## Prototype.set(vm, this, capitalName##Prototype::create(vm, this, capitalName##Prototype::createStructure(vm, this, m_iteratorPrototype.get()))); \
m_ ## properName ## Structure.set(vm, this, instanceType::createStructure(vm, this, m_ ## lowerName ## Prototype.get()));
    
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(CREATE_PROTOTYPE_FOR_DERIVED_ITERATOR_TYPE)
    m_propertyNameIteratorStructure.set(vm, this, JSPropertyNameIterator::createStructure(vm, this, m_iteratorPrototype.get()));
    m_generatorPrototype.set(vm, this, GeneratorPrototype::create(vm, this, GeneratorPrototype::createStructure(vm, this, m_iteratorPrototype.get())));
    
#undef CREATE_PROTOTYPE_FOR_DERIVED_ITERATOR_TYPE

    // Constructors
    
    ObjectConstructor* objectConstructor = ObjectConstructor::create(vm, this, ObjectConstructor::createStructure(vm, this, m_functionPrototype.get()), m_objectPrototype.get());
    m_objectConstructor.set(vm, this, objectConstructor);

    JSFunction* definePropertyFunction = m_objectConstructor->addDefineProperty(exec, this);
    m_definePropertyFunction.set(vm, this, definePropertyFunction);

    JSCell* functionConstructor = FunctionConstructor::create(vm, FunctionConstructor::createStructure(vm, this, m_functionPrototype.get()), m_functionPrototype.get());
    JSCell* arrayConstructor = ArrayConstructor::create(vm, ArrayConstructor::createStructure(vm, this, m_functionPrototype.get()), m_arrayPrototype.get());
    
    m_regExpConstructor.set(vm, this, RegExpConstructor::create(vm, RegExpConstructor::createStructure(vm, this, m_functionPrototype.get()), m_regExpPrototype.get()));
    
#define CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName) \
capitalName ## Constructor* lowerName ## Constructor = capitalName ## Constructor::create(vm, capitalName ## Constructor::createStructure(vm, this, m_functionPrototype.get()), m_ ## lowerName ## Prototype.get()); \
m_ ## lowerName ## Prototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, lowerName ## Constructor, DontEnum); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE)
    
#undef CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE
    
    m_errorConstructor.set(vm, this, errorConstructor);
    m_promiseConstructor.set(vm, this, promiseConstructor);
    m_internalPromiseConstructor.set(vm, this, internalPromiseConstructor);
    
    Structure* nativeErrorPrototypeStructure = NativeErrorPrototype::createStructure(vm, this, m_errorPrototype.get());
    Structure* nativeErrorStructure = NativeErrorConstructor::createStructure(vm, this, m_functionPrototype.get());
    m_evalErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("EvalError")));
    m_rangeErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("RangeError")));
    m_referenceErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("ReferenceError")));
    m_syntaxErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("SyntaxError")));
    m_typeErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("TypeError")));
    m_URIErrorConstructor.set(vm, this, NativeErrorConstructor::create(vm, this, nativeErrorStructure, nativeErrorPrototypeStructure, ASCIILiteral("URIError")));

    m_generatorFunctionPrototype.set(vm, this, GeneratorFunctionPrototype::create(vm, GeneratorFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    GeneratorFunctionConstructor* generatorFunctionConstructor = GeneratorFunctionConstructor::create(vm, GeneratorFunctionConstructor::createStructure(vm, this, functionConstructor), m_generatorFunctionPrototype.get());
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, generatorFunctionConstructor, DontEnum);
    m_generatorFunctionStructure.set(vm, this, JSGeneratorFunction::createStructure(vm, this, m_generatorFunctionPrototype.get()));

    m_generatorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_generatorFunctionPrototype.get(), DontEnum);
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->prototype, m_generatorPrototype.get(), DontEnum);
    
    m_objectPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, objectConstructor, DontEnum);
    m_functionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, functionConstructor, DontEnum);
    m_arrayPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, arrayConstructor, DontEnum);
    m_regExpPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_regExpConstructor.get(), DontEnum);
    
    putDirectWithoutTransition(vm, vm.propertyNames->Object, objectConstructor, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Function, functionConstructor, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Array, arrayConstructor, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->RegExp, m_regExpConstructor.get(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->EvalError, m_evalErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->RangeError, m_rangeErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->ReferenceError, m_referenceErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->SyntaxError, m_syntaxErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->TypeError, m_typeErrorConstructor.get(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->URIError, m_URIErrorConstructor.get(), DontEnum);
    
    
#define PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName) \
putDirectWithoutTransition(vm, vm.propertyNames-> jsName, lowerName ## Constructor, DontEnum); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE)

#undef PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE
    PrototypeMap& prototypeMap = vm.prototypeMap;
    Structure* iteratorResultStructure = prototypeMap.emptyObjectStructureForPrototype(m_objectPrototype.get(), JSFinalObject::defaultInlineCapacity());
    PropertyOffset offset;
    iteratorResultStructure = Structure::addPropertyTransition(vm, iteratorResultStructure, vm.propertyNames->done, 0, offset);
    iteratorResultStructure = Structure::addPropertyTransition(vm, iteratorResultStructure, vm.propertyNames->value, 0, offset);
    m_iteratorResultStructure.set(vm, this, iteratorResultStructure);
    
    m_evalFunction.set(vm, this, JSFunction::create(vm, this, 1, vm.propertyNames->eval.string(), globalFuncEval));
    putDirectWithoutTransition(vm, vm.propertyNames->eval, m_evalFunction.get(), DontEnum);
    
#if ENABLE(INTL)
    IntlObject* intl = IntlObject::create(vm, this, IntlObject::createStructure(vm, this, m_objectPrototype.get()));
    putDirectWithoutTransition(vm, vm.propertyNames->Intl, intl, DontEnum);
#endif // ENABLE(INTL)
    putDirectWithoutTransition(vm, vm.propertyNames->JSON, JSONObject::create(vm, JSONObject::createStructure(vm, this, m_objectPrototype.get())), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Math, MathObject::create(vm, this, MathObject::createStructure(vm, this, m_objectPrototype.get())), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Reflect, ReflectObject::create(vm, this, ReflectObject::createStructure(vm, this, m_objectPrototype.get())), DontEnum);

    JSTypedArrayViewConstructor* typedArraySuperConstructor = JSTypedArrayViewConstructor::create(vm, this, JSTypedArrayViewConstructor::createStructure(vm, this, m_functionPrototype.get()), typedArrayProto);
    typedArrayProto->putDirectWithoutTransition(vm, vm.propertyNames->constructor, typedArraySuperConstructor, DontEnum);

    std::array<InternalFunction*, NUMBER_OF_TYPED_ARRAY_TYPES> typedArrayConstructors;
    typedArrayConstructors[toIndex(TypeInt8)] = JSInt8ArrayConstructor::create(vm, this, JSInt8ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeInt8)].prototype.get(), ASCIILiteral("Int8Array"), typedArrayConstructorAllocateInt8ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeInt16)] = JSInt16ArrayConstructor::create(vm, this, JSInt16ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeInt16)].prototype.get(), ASCIILiteral("Int16Array"), typedArrayConstructorAllocateInt16ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeInt32)] = JSInt32ArrayConstructor::create(vm, this, JSInt32ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeInt32)].prototype.get(), ASCIILiteral("Int32Array"), typedArrayConstructorAllocateInt32ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeUint8)] = JSUint8ArrayConstructor::create(vm, this, JSUint8ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeUint8)].prototype.get(), ASCIILiteral("Uint8Array"), typedArrayConstructorAllocateUint8ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeUint8Clamped)] = JSUint8ClampedArrayConstructor::create(vm, this, JSUint8ClampedArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeUint8Clamped)].prototype.get(), ASCIILiteral("Uint8ClampedArray"), typedArrayConstructorAllocateUint8ClampedArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeUint16)] = JSUint16ArrayConstructor::create(vm, this, JSUint16ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeUint16)].prototype.get(), ASCIILiteral("Uint16Array"), typedArrayConstructorAllocateUint16ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeUint32)] = JSUint32ArrayConstructor::create(vm, this, JSUint32ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeUint32)].prototype.get(), ASCIILiteral("Uint32Array"), typedArrayConstructorAllocateUint32ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeFloat32)] = JSFloat32ArrayConstructor::create(vm, this, JSFloat32ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeFloat32)].prototype.get(), ASCIILiteral("Float32Array"), typedArrayConstructorAllocateFloat32ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeFloat64)] = JSFloat64ArrayConstructor::create(vm, this, JSFloat64ArrayConstructor::createStructure(vm, this, typedArraySuperConstructor), m_typedArrays[toIndex(TypeFloat64)].prototype.get(), ASCIILiteral("Float64Array"), typedArrayConstructorAllocateFloat64ArrayCodeGenerator(vm));
    typedArrayConstructors[toIndex(TypeDataView)] = JSDataViewConstructor::create(vm, this, JSDataViewConstructor::createStructure(vm, this, m_functionPrototype.get()), m_typedArrays[toIndex(TypeDataView)].prototype.get(), ASCIILiteral("DataView"), nullptr);
    
    for (unsigned typedArrayIndex = NUMBER_OF_TYPED_ARRAY_TYPES; typedArrayIndex--;) {
        m_typedArrays[typedArrayIndex].prototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, typedArrayConstructors[typedArrayIndex], DontEnum);
        putDirectWithoutTransition(vm, Identifier::fromString(exec, typedArrayConstructors[typedArrayIndex]->name(exec)), typedArrayConstructors[typedArrayIndex], DontEnum);
    }

    putDirectWithoutTransition(vm, vm.propertyNames->Int8ArrayPrivateName, typedArrayConstructors[toIndex(TypeInt8)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Int16ArrayPrivateName, typedArrayConstructors[toIndex(TypeInt16)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Int32ArrayPrivateName, typedArrayConstructors[toIndex(TypeInt32)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Uint8ArrayPrivateName, typedArrayConstructors[toIndex(TypeUint8)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Uint8ClampedArrayPrivateName, typedArrayConstructors[toIndex(TypeUint8Clamped)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Uint16ArrayPrivateName, typedArrayConstructors[toIndex(TypeUint16)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Uint32ArrayPrivateName, typedArrayConstructors[toIndex(TypeUint32)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Float32ArrayPrivateName, typedArrayConstructors[toIndex(TypeFloat32)], DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->Float64ArrayPrivateName, typedArrayConstructors[toIndex(TypeFloat64)], DontEnum);

    m_moduleLoader.set(vm, this, ModuleLoaderObject::create(vm, this, ModuleLoaderObject::createStructure(vm, this, m_objectPrototype.get())));
    if (Options::exposeInternalModuleLoader())
        putDirectWithoutTransition(vm, vm.propertyNames->Loader, m_moduleLoader.get(), DontEnum);

    JSFunction* builtinLog = JSFunction::create(vm, this, 1, vm.propertyNames->emptyIdentifier.string(), globalFuncBuiltinLog);

    JSFunction* privateFuncAbs = JSFunction::create(vm, this, 0, String(), mathProtoFuncAbs, AbsIntrinsic);
    JSFunction* privateFuncFloor = JSFunction::create(vm, this, 0, String(), mathProtoFuncFloor, FloorIntrinsic);
    JSFunction* privateFuncIsFinite = JSFunction::create(vm, this, 0, String(), globalFuncIsFinite);
    JSFunction* privateFuncIsNaN = JSFunction::create(vm, this, 0, String(), globalFuncIsNaN);

    JSFunction* privateFuncGetTemplateObject = JSFunction::create(vm, this, 0, String(), getTemplateObject);
    JSFunction* privateFuncToLength = JSFunction::createBuiltinFunction(vm, globalObjectToLengthCodeGenerator(vm), this);
    JSFunction* privateFuncToInteger = JSFunction::createBuiltinFunction(vm, globalObjectToIntegerCodeGenerator(vm), this);
    JSFunction* privateFuncTypedArrayLength = JSFunction::create(vm, this, 0, String(), typedArrayViewPrivateFuncLength);
    JSFunction* privateFuncTypedArraySort = JSFunction::create(vm, this, 0, String(), typedArrayViewPrivateFuncSort);

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(vm.propertyNames->NaN, jsNaN(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->Infinity, jsNumber(std::numeric_limits<double>::infinity()), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->undefinedKeyword, jsUndefined(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->undefinedPrivateName, jsUndefined(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->ObjectPrivateName, objectConstructor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->ownEnumerablePropertyKeysPrivateName, JSFunction::create(vm, this, 0, String(), ownEnumerablePropertyKeys), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->getTemplateObjectPrivateName, privateFuncGetTemplateObject, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->enqueueJobPrivateName, JSFunction::create(vm, this, 0, String(), enqueueJob), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->RangeErrorPrivateName, m_rangeErrorConstructor.get(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->TypeErrorPrivateName, m_typeErrorConstructor.get(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->typedArrayLengthPrivateName, privateFuncTypedArrayLength, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->typedArraySortPrivateName, privateFuncTypedArraySort, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->BuiltinLogPrivateName, builtinLog, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->ArrayPrivateName, arrayConstructor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->NumberPrivateName, numberConstructor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->StringPrivateName, stringConstructor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->absPrivateName, privateFuncAbs, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->floorPrivateName, privateFuncFloor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->isFinitePrivateName, privateFuncIsFinite, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->isNaNPrivateName, privateFuncIsNaN, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->arrayIterationKindKeyPrivateName, jsNumber(ArrayIterateKey), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->arrayIterationKindValuePrivateName, jsNumber(ArrayIterateValue), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->arrayIterationKindKeyValuePrivateName, jsNumber(ArrayIterateKeyValue), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().symbolIteratorPrivateName(), Symbol::create(vm, static_cast<SymbolImpl&>(*vm.propertyNames->iteratorSymbol.impl())), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->PromisePrivateName, promiseConstructor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->InternalPromisePrivateName, internalPromiseConstructor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->promisePendingPrivateName, jsNumber(static_cast<unsigned>(JSPromise::Status::Pending)), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().promiseFulfilledPrivateName(), jsNumber(static_cast<unsigned>(JSPromise::Status::Fulfilled)), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().promiseRejectedPrivateName(), jsNumber(static_cast<unsigned>(JSPromise::Status::Rejected)), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().toLengthPrivateName(), privateFuncToLength, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().toIntegerPrivateName(), privateFuncToInteger, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isObjectPrivateName(), JSFunction::createBuiltinFunction(vm, globalObjectIsObjectCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isDictionaryPrivateName(), JSFunction::createBuiltinFunction(vm, globalObjectIsDictionaryCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().isPromisePrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsIsPromiseCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().newPromiseReactionPrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsNewPromiseReactionCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().newPromiseCapabilityPrivateName(), m_newPromiseCapabilityFunction.get(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().triggerPromiseReactionsPrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsTriggerPromiseReactionsCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().rejectPromisePrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsRejectPromiseCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().fulfillPromisePrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsFulfillPromiseCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().createResolvingFunctionsPrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsCreateResolvingFunctionsCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().promiseReactionJobPrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsPromiseReactionJobCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().promiseResolveThenableJobPrivateName(), JSFunction::createBuiltinFunction(vm, promiseOperationsPromiseResolveThenableJobCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().InspectorInstrumentationPrivateName(), InspectorInstrumentationObject::create(vm, this, InspectorInstrumentationObject::createStructure(vm, this, m_objectPrototype.get())), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->MapPrivateName, mapConstructor, DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().generatorResumePrivateName(), JSFunction::createBuiltinFunction(vm, generatorPrototypeGeneratorResumeCodeGenerator(vm), this), DontEnum | DontDelete | ReadOnly),
#if ENABLE(INTL)
        GlobalPropertyInfo(vm.propertyNames->builtinNames().CollatorPrivateName(), intl->getDirect(vm, vm.propertyNames->Collator), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().DateTimeFormatPrivateName(), intl->getDirect(vm, vm.propertyNames->DateTimeFormat), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->builtinNames().NumberFormatPrivateName(), intl->getDirect(vm, vm.propertyNames->NumberFormat), DontEnum | DontDelete | ReadOnly),
#endif // ENABLE(INTL)
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
    
    m_specialPointers[Special::CallFunction] = m_callFunction.get();
    m_specialPointers[Special::ApplyFunction] = m_applyFunction.get();
    m_specialPointers[Special::ObjectConstructor] = objectConstructor;
    m_specialPointers[Special::ArrayConstructor] = arrayConstructor;

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::DefinePropertyFunction)] = m_definePropertyFunction.get();

    ConsolePrototype* consolePrototype = ConsolePrototype::create(vm, this, ConsolePrototype::createStructure(vm, this, m_objectPrototype.get()));
    m_consoleStructure.set(vm, this, JSConsole::createStructure(vm, this, consolePrototype));
    JSConsole* consoleObject = JSConsole::create(vm, m_consoleStructure.get());
    putDirectWithoutTransition(vm, Identifier::fromString(exec, "console"), consoleObject, DontEnum);

    if (UNLIKELY(Options::useDollarVM())) {
        JSDollarVMPrototype* dollarVMPrototype = JSDollarVMPrototype::create(vm, this, JSDollarVMPrototype::createStructure(vm, this, m_objectPrototype.get()));
        m_dollarVMStructure.set(vm, this, JSDollarVM::createStructure(vm, this, dollarVMPrototype));
        JSDollarVM* dollarVM = JSDollarVM::create(vm, m_dollarVMStructure.get());
        putDirectWithoutTransition(vm, Identifier::fromString(exec, "$vm"), dollarVM, DontEnum);
    }

    resetPrototype(vm, prototype());
}

void JSGlobalObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    bool shouldThrowReadOnlyError = slot.isStrictMode();
    bool ignoreReadOnlyErrors = false;
    if (symbolTablePutTouchWatchpointSet(thisObject, exec, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors))
        return;
    Base::put(thisObject, exec, propertyName, value, slot);
}

bool JSGlobalObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    PropertySlot slot(thisObject);
    // silently ignore attempts to add accessors aliasing vars.
    if (descriptor.isAccessorDescriptor() && symbolTableGet(thisObject, propertyName, slot))
        return false;
    return Base::defineOwnProperty(thisObject, exec, propertyName, descriptor, shouldThrow);
}

void JSGlobalObject::addGlobalVar(const Identifier& ident)
{
    ConcurrentJITLocker locker(symbolTable()->m_lock);
    SymbolTableEntry entry = symbolTable()->get(locker, ident.impl());
    if (!entry.isNull())
        return;
    
    ScopeOffset offset = symbolTable()->takeNextScopeOffset(locker);
    SymbolTableEntry newEntry(VarOffset(offset), 0);
    newEntry.prepareToWatch();
    symbolTable()->add(locker, ident.impl(), newEntry);
    
    ScopeOffset offsetForAssert = addVariables(1, jsUndefined());
    RELEASE_ASSERT(offsetForAssert == offset);
}

void JSGlobalObject::addFunction(ExecState* exec, const Identifier& propertyName)
{
    VM& vm = exec->vm();
    removeDirect(vm, propertyName); // Newly declared functions overwrite existing properties.
    addGlobalVar(propertyName);
}

static inline JSObject* lastInPrototypeChain(JSObject* object)
{
    JSObject* o = object;
    while (o->prototype().isObject())
        o = asObject(o->prototype());
    return o;
}

// Private namespace for helpers for JSGlobalObject::haveABadTime()
namespace {

class ObjectsWithBrokenIndexingFinder : public MarkedBlock::VoidFunctor {
public:
    ObjectsWithBrokenIndexingFinder(MarkedArgumentBuffer&, JSGlobalObject*);
    IterationStatus operator()(JSCell*);

private:
    void visit(JSCell*);

    MarkedArgumentBuffer& m_foundObjects;
    JSGlobalObject* m_globalObject;
};

ObjectsWithBrokenIndexingFinder::ObjectsWithBrokenIndexingFinder(
    MarkedArgumentBuffer& foundObjects, JSGlobalObject* globalObject)
    : m_foundObjects(foundObjects)
    , m_globalObject(globalObject)
{
}

inline bool hasBrokenIndexing(JSObject* object)
{
    // This will change if we have more indexing types.
    IndexingType type = object->indexingType();
    // This could be made obviously more efficient, but isn't made so right now, because
    // we expect this to be an unlikely slow path anyway.
    return hasUndecided(type) || hasInt32(type) || hasDouble(type) || hasContiguous(type) || hasArrayStorage(type);
}

inline void ObjectsWithBrokenIndexingFinder::visit(JSCell* cell)
{
    if (!cell->isObject())
        return;
    
    JSObject* object = asObject(cell);

    // Run this filter first, since it's cheap, and ought to filter out a lot of objects.
    if (!hasBrokenIndexing(object))
        return;
    
    // We only want to have a bad time in the affected global object, not in the entire
    // VM. But we have to be careful, since there may be objects that claim to belong to
    // a different global object that have prototypes from our global object.
    bool foundGlobalObject = false;
    for (JSObject* current = object; ;) {
        if (current->globalObject() == m_globalObject) {
            foundGlobalObject = true;
            break;
        }
        
        JSValue prototypeValue = current->prototype();
        if (prototypeValue.isNull())
            break;
        current = asObject(prototypeValue);
    }
    if (!foundGlobalObject)
        return;
    
    m_foundObjects.append(object);
}

IterationStatus ObjectsWithBrokenIndexingFinder::operator()(JSCell* cell)
{
    visit(cell);
    return IterationStatus::Continue;
}

} // end private namespace for helpers for JSGlobalObject::haveABadTime()

void JSGlobalObject::haveABadTime(VM& vm)
{
    ASSERT(&vm == &this->vm());
    
    if (isHavingABadTime())
        return;
    
    // Make sure that all allocations or indexed storage transitions that are inlining
    // the assumption that it's safe to transition to a non-SlowPut array storage don't
    // do so anymore.
    m_havingABadTimeWatchpoint->fireAll("Having a bad time");
    ASSERT(isHavingABadTime()); // The watchpoint is what tells us that we're having a bad time.
    
    // Make sure that all JSArray allocations that load the appropriate structure from
    // this object now load a structure that uses SlowPut.
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i].set(vm, this, originalArrayStructureForIndexingType(ArrayWithSlowPutArrayStorage));
    
    // Make sure that all objects that have indexed storage switch to the slow kind of
    // indexed storage.
    MarkedArgumentBuffer foundObjects; // Use MarkedArgumentBuffer because switchToSlowPutArrayStorage() may GC.
    ObjectsWithBrokenIndexingFinder finder(foundObjects, this);
    {
        HeapIterationScope iterationScope(vm.heap);
        vm.heap.objectSpace().forEachLiveCell(iterationScope, finder);
    }
    while (!foundObjects.isEmpty()) {
        JSObject* object = asObject(foundObjects.last());
        foundObjects.removeLast();
        ASSERT(hasBrokenIndexing(object));
        object->switchToSlowPutArrayStorage(vm);
    }
}

bool JSGlobalObject::objectPrototypeIsSane()
{
    return !hasIndexedProperties(m_objectPrototype->indexingType())
        && m_objectPrototype->prototype().isNull();
}

bool JSGlobalObject::arrayPrototypeChainIsSane()
{
    return !hasIndexedProperties(m_arrayPrototype->indexingType())
        && m_arrayPrototype->prototype() == m_objectPrototype.get()
        && objectPrototypeIsSane();
}

bool JSGlobalObject::stringPrototypeChainIsSane()
{
    return !hasIndexedProperties(m_stringPrototype->indexingType())
        && m_stringPrototype->prototype() == m_objectPrototype.get()
        && objectPrototypeIsSane();
}

void JSGlobalObject::createThrowTypeError(VM& vm)
{
    JSFunction* thrower = JSFunction::create(vm, this, 0, String(), globalFuncThrowTypeError);
    GetterSetter* getterSetter = GetterSetter::create(vm, this);
    getterSetter->setGetter(vm, this, thrower);
    getterSetter->setSetter(vm, this, thrower);
    m_throwTypeErrorGetterSetter.set(vm, this, getterSetter);
}

// Set prototype, and also insert the object prototype at the end of the chain.
void JSGlobalObject::resetPrototype(VM& vm, JSValue prototype)
{
    setPrototype(vm, prototype);

    JSObject* oldLastInPrototypeChain = lastInPrototypeChain(this);
    JSObject* objectPrototype = m_objectPrototype.get();
    if (oldLastInPrototypeChain != objectPrototype)
        oldLastInPrototypeChain->setPrototype(vm, objectPrototype);

    // Whenever we change the prototype of the global object, we need to create a new JSProxy with the correct prototype.
    setGlobalThis(vm, JSProxy::create(vm, JSProxy::createStructure(vm, this, prototype, PureForwardingProxyType), this));
}

void JSGlobalObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{ 
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_globalThis);

    visitor.append(&thisObject->m_globalLexicalEnvironment);
    visitor.append(&thisObject->m_globalCallee);
    visitor.append(&thisObject->m_regExpConstructor);
    visitor.append(&thisObject->m_errorConstructor);
    visitor.append(&thisObject->m_evalErrorConstructor);
    visitor.append(&thisObject->m_rangeErrorConstructor);
    visitor.append(&thisObject->m_referenceErrorConstructor);
    visitor.append(&thisObject->m_syntaxErrorConstructor);
    visitor.append(&thisObject->m_typeErrorConstructor);
    visitor.append(&thisObject->m_URIErrorConstructor);
    visitor.append(&thisObject->m_objectConstructor);
    visitor.append(&thisObject->m_promiseConstructor);
    visitor.append(&thisObject->m_internalPromiseConstructor);

    visitor.append(&thisObject->m_nullGetterFunction);
    visitor.append(&thisObject->m_nullSetterFunction);

    visitor.append(&thisObject->m_parseIntFunction);
    visitor.append(&thisObject->m_evalFunction);
    visitor.append(&thisObject->m_callFunction);
    visitor.append(&thisObject->m_applyFunction);
    visitor.append(&thisObject->m_definePropertyFunction);
    visitor.append(&thisObject->m_arrayProtoValuesFunction);
    visitor.append(&thisObject->m_initializePromiseFunction);
    visitor.append(&thisObject->m_newPromiseCapabilityFunction);
    visitor.append(&thisObject->m_throwTypeErrorGetterSetter);
    visitor.append(&thisObject->m_moduleLoader);

    visitor.append(&thisObject->m_objectPrototype);
    visitor.append(&thisObject->m_functionPrototype);
    visitor.append(&thisObject->m_arrayPrototype);
    visitor.append(&thisObject->m_errorPrototype);
    visitor.append(&thisObject->m_iteratorPrototype);
    visitor.append(&thisObject->m_generatorFunctionPrototype);
    visitor.append(&thisObject->m_generatorPrototype);

    visitor.append(&thisObject->m_debuggerScopeStructure);
    visitor.append(&thisObject->m_withScopeStructure);
    visitor.append(&thisObject->m_strictEvalActivationStructure);
    visitor.append(&thisObject->m_lexicalEnvironmentStructure);
    visitor.append(&thisObject->m_moduleEnvironmentStructure);
    visitor.append(&thisObject->m_directArgumentsStructure);
    visitor.append(&thisObject->m_scopedArgumentsStructure);
    visitor.append(&thisObject->m_outOfBandArgumentsStructure);
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        visitor.append(&thisObject->m_originalArrayStructureForIndexingShape[i]);
    for (unsigned i = 0; i < NumberOfIndexingShapes; ++i)
        visitor.append(&thisObject->m_arrayStructureForIndexingShapeDuringAllocation[i]);
    visitor.append(&thisObject->m_booleanObjectStructure);
    visitor.append(&thisObject->m_callbackConstructorStructure);
    visitor.append(&thisObject->m_callbackFunctionStructure);
    visitor.append(&thisObject->m_callbackObjectStructure);
    visitor.append(&thisObject->m_propertyNameIteratorStructure);
#if JSC_OBJC_API_ENABLED
    visitor.append(&thisObject->m_objcCallbackFunctionStructure);
    visitor.append(&thisObject->m_objcWrapperObjectStructure);
#endif
    visitor.append(&thisObject->m_nullPrototypeObjectStructure);
    visitor.append(&thisObject->m_errorStructure);
    visitor.append(&thisObject->m_calleeStructure);
    visitor.append(&thisObject->m_functionStructure);
    visitor.append(&thisObject->m_boundFunctionStructure);
    visitor.append(&thisObject->m_arrowFunctionStructure);
    visitor.append(&thisObject->m_nativeStdFunctionStructure);
    visitor.append(&thisObject->m_namedFunctionStructure);
    visitor.append(&thisObject->m_symbolObjectStructure);
    visitor.append(&thisObject->m_regExpStructure);
    visitor.append(&thisObject->m_generatorFunctionStructure);
    visitor.append(&thisObject->m_regExpMatchesArrayStructure);
    visitor.append(&thisObject->m_moduleRecordStructure);
    visitor.append(&thisObject->m_moduleNamespaceObjectStructure);
    visitor.append(&thisObject->m_consoleStructure);
    visitor.append(&thisObject->m_dollarVMStructure);
    visitor.append(&thisObject->m_internalFunctionStructure);
#if ENABLE(WEBASSEMBLY)
    visitor.append(&thisObject->m_wasmModuleStructure);
#endif

#define VISIT_SIMPLE_TYPE(CapitalName, lowerName, properName, instanceType, jsName) \
    visitor.append(&thisObject->m_ ## lowerName ## Prototype); \
    visitor.append(&thisObject->m_ ## properName ## Structure); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE(VISIT_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(VISIT_SIMPLE_TYPE)

#undef VISIT_SIMPLE_TYPE

    for (unsigned i = NUMBER_OF_TYPED_ARRAY_TYPES; i--;) {
        visitor.append(&thisObject->m_typedArrays[i].prototype);
        visitor.append(&thisObject->m_typedArrays[i].structure);
    }
}

JSValue JSGlobalObject::toThis(JSCell*, ExecState* exec, ECMAMode ecmaMode)
{
    if (ecmaMode == StrictMode)
        return jsUndefined();
    return exec->globalThisValue();
}

ExecState* JSGlobalObject::globalExec()
{
    return CallFrame::create(m_globalCallFrame);
}

void JSGlobalObject::addStaticGlobals(GlobalPropertyInfo* globals, int count)
{
    ScopeOffset startOffset = addVariables(count, jsUndefined());

    for (int i = 0; i < count; ++i) {
        GlobalPropertyInfo& global = globals[i];
        ASSERT(global.attributes & DontDelete);
        
        ScopeOffset offset;
        {
            ConcurrentJITLocker locker(symbolTable()->m_lock);
            offset = symbolTable()->takeNextScopeOffset(locker);
            RELEASE_ASSERT(offset = startOffset + i);
            SymbolTableEntry newEntry(VarOffset(offset), global.attributes);
            symbolTable()->add(locker, global.identifier.impl(), newEntry);
        }
        variableAt(offset).set(vm(), this, global.value);
    }
}

bool JSGlobalObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);
    if (getStaticFunctionSlot<Base>(exec, globalObjectTable, thisObject, propertyName, slot))
        return true;
    return symbolTableGet(thisObject, propertyName, slot);
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

UnlinkedProgramCodeBlock* JSGlobalObject::createProgramCodeBlock(CallFrame* callFrame, ProgramExecutable* executable, JSObject** exception)
{
    ParserError error;
    JSParserStrictMode strictMode = executable->isStrictMode() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    DebuggerMode debuggerMode = hasDebugger() ? DebuggerOn : DebuggerOff;
    ProfilerMode profilerMode = hasProfiler() ? ProfilerOn : ProfilerOff;
    UnlinkedProgramCodeBlock* unlinkedCodeBlock = vm().codeCache()->getProgramCodeBlock(
        vm(), executable, executable->source(), JSParserBuiltinMode::NotBuiltin, strictMode, 
        debuggerMode, profilerMode, error);

    if (hasDebugger())
        debugger()->sourceParsed(callFrame, executable->source().provider(), error.line(), error.message());

    if (error.isValid()) {
        *exception = error.toErrorObject(this, executable->source());
        return nullptr;
    }
    
    return unlinkedCodeBlock;
}

UnlinkedEvalCodeBlock* JSGlobalObject::createEvalCodeBlock(CallFrame* callFrame, EvalExecutable* executable, ThisTDZMode thisTDZMode, bool isArrowFunctionContext, const VariableEnvironment* variablesUnderTDZ)
{
    ParserError error;
    JSParserStrictMode strictMode = executable->isStrictMode() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    DebuggerMode debuggerMode = hasDebugger() ? DebuggerOn : DebuggerOff;
    ProfilerMode profilerMode = hasProfiler() ? ProfilerOn : ProfilerOff;
    UnlinkedEvalCodeBlock* unlinkedCodeBlock = vm().codeCache()->getEvalCodeBlock(
        vm(), executable, executable->source(), JSParserBuiltinMode::NotBuiltin, strictMode, thisTDZMode, isArrowFunctionContext, debuggerMode, profilerMode, error, variablesUnderTDZ);

    if (hasDebugger())
        debugger()->sourceParsed(callFrame, executable->source().provider(), error.line(), error.message());

    if (error.isValid()) {
        throwVMError(callFrame, error.toErrorObject(this, executable->source()));
        return nullptr;
    }

    return unlinkedCodeBlock;
}

UnlinkedModuleProgramCodeBlock* JSGlobalObject::createModuleProgramCodeBlock(CallFrame* callFrame, ModuleProgramExecutable* executable)
{
    ParserError error;
    DebuggerMode debuggerMode = hasDebugger() ? DebuggerOn : DebuggerOff;
    ProfilerMode profilerMode = hasProfiler() ? ProfilerOn : ProfilerOff;
    UnlinkedModuleProgramCodeBlock* unlinkedCodeBlock = vm().codeCache()->getModuleProgramCodeBlock(
        vm(), executable, executable->source(), JSParserBuiltinMode::NotBuiltin, debuggerMode, profilerMode, error);

    if (hasDebugger())
        debugger()->sourceParsed(callFrame, executable->source().provider(), error.line(), error.message());

    if (error.isValid()) {
        throwVMError(callFrame, error.toErrorObject(this, executable->source()));
        return nullptr;
    }

    return unlinkedCodeBlock;
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

#if ENABLE(WEB_REPLAY)
void JSGlobalObject::setInputCursor(PassRefPtr<InputCursor> prpCursor)
{
    m_inputCursor = prpCursor;
    ASSERT(m_inputCursor);

    InputCursor& cursor = inputCursor();
    // Save or set the random seed. This performed here rather than the constructor
    // to avoid threading the input cursor through all the abstraction layers.
    if (cursor.isCapturing())
        cursor.appendInput<SetRandomSeed>(m_weakRandom.seed());
    else if (cursor.isReplaying()) {
        if (SetRandomSeed* input = cursor.fetchInput<SetRandomSeed>())
            m_weakRandom.setSeed(static_cast<unsigned>(input->randomSeed()));
    }
}
#endif

void JSGlobalObject::setName(const String& name)
{
    m_name = name;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->update();
#endif
}

# if ENABLE(INTL)
const HashSet<String>& JSGlobalObject::intlCollatorAvailableLocales()
{
    if (m_intlCollatorAvailableLocales.isEmpty()) {
        int32_t count = ucol_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale(ucol_getAvailable(i));
            // Change from ICU format to BCP47.
            locale.replace('_', '-');
            m_intlCollatorAvailableLocales.add(locale);
        }
    }
    return m_intlCollatorAvailableLocales;
}

const HashSet<String>& JSGlobalObject::intlDateTimeFormatAvailableLocales()
{
    if (m_intlDateTimeFormatAvailableLocales.isEmpty()) {
        int32_t count = udat_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale(udat_getAvailable(i));
            // Change from ICU format to BCP47.
            locale.replace('_', '-');
            m_intlDateTimeFormatAvailableLocales.add(locale);
        }
    }
    return m_intlDateTimeFormatAvailableLocales;
}

const HashSet<String>& JSGlobalObject::intlNumberFormatAvailableLocales()
{
    if (m_intlNumberFormatAvailableLocales.isEmpty()) {
        int32_t count = unum_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale(unum_getAvailable(i));
            // Change from ICU format to BCP47.
            locale.replace('_', '-');
            m_intlNumberFormatAvailableLocales.add(locale);
        }
    }
    return m_intlNumberFormatAvailableLocales;
}
#endif // ENABLE(INTL)

void JSGlobalObject::queueMicrotask(PassRefPtr<Microtask> task)
{
    if (globalObjectMethodTable()->queueTaskToEventLoop) {
        globalObjectMethodTable()->queueTaskToEventLoop(this, task);
        return;
    }

    vm().queueMicrotask(this, task);
}

} // namespace JSC
