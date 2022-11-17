/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "ArrayAllocationProfile.h"
#include "ArrayBufferSharingMode.h"
#include "BigIntPrototype.h"
#include "BooleanPrototype.h"
#include "ErrorType.h"
#include "ExceptionHelpers.h"
#include "GetVM.h"
#include "InternalFunction.h"
#include "JSArray.h"
#include "JSClassRef.h"
#include "JSGlobalLexicalEnvironment.h"
#include "JSPromise.h"
#include "JSSegmentedVariableObject.h"
#include "JSWeakObjectMapRefInternal.h"
#include "LazyClassStructure.h"
#include "LazyProperty.h"
#include "NumberPrototype.h"
#include "ParserModes.h"
#include "RegExpGlobalData.h"
#include "RuntimeFlags.h"
#include "StringPrototype.h"
#include "StructureCache.h"
#include "SymbolPrototype.h"
#include "VM.h"
#include "Watchpoint.h"
#include "WeakGCSet.h"
#include <JavaScriptCore/JSBase.h>
#include <array>
#include <wtf/FixedVector.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/JSGlobalObjectAdditions.h>
#else
#define JS_GLOBAL_OBJECT_ADDITIONS_1
#define JS_GLOBAL_OBJECT_ADDITIONS_2
#define JS_GLOBAL_OBJECT_ADDITIONS_3
#define JS_GLOBAL_OBJECT_ADDITIONS_4
#endif

struct OpaqueJSClass;
struct OpaqueJSClassContextData;
OBJC_CLASS JSWrapperMap;

namespace Inspector {
class JSGlobalObjectInspectorController;
}

namespace JSC {
class ArrayConstructor;
class ArrayPrototype;
class ArrayIteratorPrototype;
class AsyncIteratorPrototype;
class AsyncFunctionPrototype;
class AsyncGeneratorPrototype;
class AsyncGeneratorFunctionPrototype;
class BooleanPrototype;
class ChainedWatchpoint;
class ConsoleClient;
class Debugger;
class ErrorConstructor;
class ErrorPrototype;
class EvalCodeBlock;
class EvalExecutable;
class FunctionConstructor;
class FunctionPrototype;
class GeneratorPrototype;
class GeneratorFunctionPrototype;
class GetterSetter;
class GlobalCodeBlock;
class ImportMap;
class IndirectEvalExecutable;
class InputCursor;
class IntlObject;
class IntlCollator;
class JSArrayBuffer;
class JSArrayBufferPrototype;
class JSCallee;
class JSCustomGetterFunction;
class JSCustomSetterFunction;
class JSGlobalObjectDebuggable;
class JSInternalPromise;
class JSModuleLoader;
class JSModuleRecord;
class JSPromise;
class JSPromiseConstructor;
class JSPromisePrototype;
class JSSharedArrayBuffer;
class JSSharedArrayBufferPrototype;
class JSTypedArrayViewConstructor;
class JSTypedArrayViewPrototype;
class DirectEvalExecutable;
class LLIntOffsetsExtractor;
class MapIteratorPrototype;
class MapPrototype;
class Microtask;
class ModuleLoader;
class ModuleProgramExecutable;
class NativeErrorConstructorBase;
class NullGetterFunction;
class NullSetterFunction;
class ObjectConstructor;
class ProgramCodeBlock;
class ProgramExecutable;
class ShadowRealmConstructor;
class ShadowRealmPrototype;
class RegExpConstructor;
class RegExpPrototype;
class SetIteratorPrototype;
class SetPrototype;
class SourceCode;
class SourceOrigin;
class StringConstructor;
class UnlinkedModuleProgramCodeBlock;
class VariableEnvironment;
struct ActivationStackNode;
struct HashTable;
enum class LinkTimeConstant : int32_t;

#ifdef JSC_GLIB_API_ENABLED
class WrapperMap;
#endif

template<typename Watchpoint> class ObjectPropertyChangeAdaptiveWatchpoint;
class ObjectAdaptiveStructureWatchpoint;

constexpr bool typeExposedByDefault = true;

#define DEFINE_STANDARD_BUILTIN(macro, upperName, lowerName) macro(upperName, lowerName, lowerName, JS ## upperName, upperName, object, typeExposedByDefault)

#define FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(macro) \
    macro(String, string, stringObject, StringObject, String, object, typeExposedByDefault) \
    macro(JSPromise, promise, promise, JSPromise, Promise, object, typeExposedByDefault) \
    macro(BigInt, bigInt, bigIntObject, BigIntObject, BigInt, object, typeExposedByDefault) \
    macro(WeakObjectRef, weakObjectRef, weakObjectRef, JSWeakObjectRef, WeakRef, object, typeExposedByDefault) \
    macro(FinalizationRegistry, finalizationRegistry, finalizationRegistry, JSFinalizationRegistry, FinalizationRegistry, object, typeExposedByDefault) \


#define FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(macro) \
    macro(StringIterator, stringIterator, stringIterator, JSStringIterator, StringIterator, iterator, typeExposedByDefault) \

#define FOR_EACH_SIMPLE_BUILTIN_TYPE(macro) \
    FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(macro) \
    macro(JSInternalPromise, internalPromise, internalPromise, JSInternalPromise, InternalPromise, object, typeExposedByDefault) \

#define FOR_EACH_LAZY_BUILTIN_TYPE_WITH_DECLARATION(macro) \
    macro(Boolean, boolean, booleanObject, BooleanObject, Boolean, object, typeExposedByDefault) \
    macro(Date, date, date, DateInstance, Date, object, typeExposedByDefault) \
    macro(Error, error, error, ErrorInstance, Error, object, typeExposedByDefault) \
    macro(Map, map, map, JSMap, Map, object, typeExposedByDefault) \
    macro(Number, number, numberObject, NumberObject, Number, object, typeExposedByDefault) \
    macro(Set, set, set, JSSet, Set, object, typeExposedByDefault) \
    macro(Symbol, symbol, symbolObject, SymbolObject, Symbol, object, typeExposedByDefault) \
    DEFINE_STANDARD_BUILTIN(macro, WeakMap, weakMap) \
    DEFINE_STANDARD_BUILTIN(macro, WeakSet, weakSet) \

#define FOR_EACH_LAZY_BUILTIN_TYPE(macro) \
    FOR_EACH_LAZY_BUILTIN_TYPE_WITH_DECLARATION(macro) \
    macro(JSArrayBuffer, arrayBuffer, arrayBuffer, JSArrayBuffer, ArrayBuffer, object, typeExposedByDefault) \

#if ENABLE(WEBASSEMBLY)
#define FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(macro) \
    macro(WebAssemblyArray,        webAssemblyArray,        webAssemblyArray,        JSWebAssemblyArray,        Array,        object, Options::useWebAssemblyGC()) \
    macro(WebAssemblyCompileError, webAssemblyCompileError, webAssemblyCompileError, ErrorInstance,             CompileError, error,  typeExposedByDefault) \
    macro(WebAssemblyException,    webAssemblyException,    webAssemblyException,    JSWebAssemblyException,    Exception,    object, typeExposedByDefault) \
    macro(WebAssemblyGlobal,       webAssemblyGlobal,       webAssemblyGlobal,       JSWebAssemblyGlobal,       Global,       object, typeExposedByDefault) \
    macro(WebAssemblyInstance,     webAssemblyInstance,     webAssemblyInstance,     JSWebAssemblyInstance,     Instance,     object, typeExposedByDefault) \
    macro(WebAssemblyLinkError,    webAssemblyLinkError,    webAssemblyLinkError,    ErrorInstance,             LinkError,    error,  typeExposedByDefault) \
    macro(WebAssemblyMemory,       webAssemblyMemory,       webAssemblyMemory,       JSWebAssemblyMemory,       Memory,       object, typeExposedByDefault) \
    macro(WebAssemblyModule,       webAssemblyModule,       webAssemblyModule,       JSWebAssemblyModule,       Module,       object, typeExposedByDefault) \
    macro(WebAssemblyRuntimeError, webAssemblyRuntimeError, webAssemblyRuntimeError, ErrorInstance,             RuntimeError, error,  typeExposedByDefault) \
    macro(WebAssemblyStruct,       webAssemblyStruct,       webAssemblyStruct,       JSWebAssemblyStruct,       Struct,       object, Options::useWebAssemblyGC()) \
    macro(WebAssemblyTable,        webAssemblyTable,        webAssemblyTable,        JSWebAssemblyTable,        Table,        object, typeExposedByDefault) \
    macro(WebAssemblyTag,          webAssemblyTag,          webAssemblyTag,          JSWebAssemblyTag,          Tag,          object, typeExposedByDefault) 
#else
#define FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(macro)
#endif // ENABLE(WEBASSEMBLY)

#define DECLARE_SIMPLE_BUILTIN_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    class JS ## capitalName; \
    class capitalName ## Prototype; \
    class capitalName ## Constructor;

class IteratorPrototype;
FOR_EACH_SIMPLE_BUILTIN_TYPE(DECLARE_SIMPLE_BUILTIN_TYPE)
FOR_EACH_LAZY_BUILTIN_TYPE_WITH_DECLARATION(DECLARE_SIMPLE_BUILTIN_TYPE)
FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DECLARE_SIMPLE_BUILTIN_TYPE)
FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(DECLARE_SIMPLE_BUILTIN_TYPE)

#undef DECLARE_SIMPLE_BUILTIN_TYPE

enum class JSPromiseRejectionOperation : unsigned {
    Reject, // When a promise is rejected without any handlers.
    Handle, // When a handler is added to a rejected promise for the first time.
};

enum class ScriptExecutionStatus {
    Running,
    Suspended,
    Stopped,
};

struct GlobalObjectMethodTable {
    typedef bool (*SupportsRichSourceInfoFunctionPtr)(const JSGlobalObject*);
    SupportsRichSourceInfoFunctionPtr supportsRichSourceInfo;

    typedef bool (*ShouldInterruptScriptFunctionPtr)(const JSGlobalObject*);
    ShouldInterruptScriptFunctionPtr shouldInterruptScript;

    typedef RuntimeFlags (*JavaScriptRuntimeFlagsFunctionPtr)(const JSGlobalObject*);
    JavaScriptRuntimeFlagsFunctionPtr javaScriptRuntimeFlags;

    typedef void (*QueueMicrotaskToEventLoopFunctionPtr)(JSGlobalObject&, Ref<Microtask>&&);
    QueueMicrotaskToEventLoopFunctionPtr queueMicrotaskToEventLoop;

    typedef bool (*ShouldInterruptScriptBeforeTimeoutPtr)(const JSGlobalObject*);
    ShouldInterruptScriptBeforeTimeoutPtr shouldInterruptScriptBeforeTimeout;

    typedef JSInternalPromise* (*ModuleLoaderImportModulePtr)(JSGlobalObject*, JSModuleLoader*, JSString*, JSValue, const SourceOrigin&);
    ModuleLoaderImportModulePtr moduleLoaderImportModule;

    typedef Identifier (*ModuleLoaderResolvePtr)(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, JSValue);
    ModuleLoaderResolvePtr moduleLoaderResolve;

    typedef JSInternalPromise* (*ModuleLoaderFetchPtr)(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, JSValue);
    ModuleLoaderFetchPtr moduleLoaderFetch;

    typedef JSObject* (*ModuleLoaderCreateImportMetaPropertiesPtr)(JSGlobalObject*, JSModuleLoader*, JSValue, JSModuleRecord*, JSValue);
    ModuleLoaderCreateImportMetaPropertiesPtr moduleLoaderCreateImportMetaProperties;

    typedef JSValue (*ModuleLoaderEvaluatePtr)(JSGlobalObject*, JSModuleLoader*, JSValue key, JSValue moduleRecordValue, JSValue scriptFetcher, JSValue awaitedValue, JSValue resumeMode);
    ModuleLoaderEvaluatePtr moduleLoaderEvaluate;

    typedef void (*PromiseRejectionTrackerPtr)(JSGlobalObject*, JSPromise*, JSPromiseRejectionOperation);
    PromiseRejectionTrackerPtr promiseRejectionTracker;

    typedef void (*ReportUncaughtExceptionAtEventLoopPtr)(JSGlobalObject*, Exception*);
    ReportUncaughtExceptionAtEventLoopPtr reportUncaughtExceptionAtEventLoop;

    // For most contexts this is just the global object. For JSDOMWindow, however, this is the JSDocument.
    typedef JSObject* (*CurrentScriptExecutionOwnerPtr)(JSGlobalObject*);
    CurrentScriptExecutionOwnerPtr currentScriptExecutionOwner;

    typedef ScriptExecutionStatus (*ScriptExecutionStatusPtr)(JSGlobalObject*, JSObject* scriptExecutionOwner);
    ScriptExecutionStatusPtr scriptExecutionStatus;
    
    typedef void (*ReportViolationForUnsafeEvalPtr)(JSGlobalObject*, JSString*);
    ReportViolationForUnsafeEvalPtr reportViolationForUnsafeEval;

    typedef String (*DefaultLanguageFunctionPtr)();
    DefaultLanguageFunctionPtr defaultLanguage;

    typedef JSPromise* (*CompileStreamingPtr)(JSGlobalObject*, JSValue);
    CompileStreamingPtr compileStreaming;

    typedef JSPromise* (*InstantiateStreamingPtr)(JSGlobalObject*, JSValue, JSObject*);
    InstantiateStreamingPtr instantiateStreaming;

    typedef JSGlobalObject* (*DeriveShadowRealmGlobalFunctionPtr)(JSGlobalObject*);
    DeriveShadowRealmGlobalFunctionPtr deriveShadowRealmGlobalObject;
};

class JSGlobalObject : public JSSegmentedVariableObject {
private:
    typedef HashSet<RefPtr<OpaqueJSWeakObjectMap>> WeakMapSet;
    typedef HashMap<OpaqueJSClass*, std::unique_ptr<OpaqueJSClassContextData>> OpaqueJSClassDataMap;

    struct JSGlobalObjectRareData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        JSGlobalObjectRareData()
            : profileGroup(0)
        {
        }

        WeakMapSet weakMaps;
        unsigned profileGroup;
        
        OpaqueJSClassDataMap opaqueJSClassData;
    };

    // m_vm must be a pointer (instead of a reference) because the JSCLLIntOffsetsExtractor
    // cannot handle it being a reference.
    VM* const m_vm;

// Our hashtable code-generator tries to access these properties, so we make them public.
// However, we'd like it better if they could be protected.
public:
    template<typename T> using Initializer = typename LazyProperty<JSGlobalObject, T>::Initializer;

    WriteBarrier<JSObject> m_globalThis;

    WriteBarrier<JSGlobalLexicalEnvironment> m_globalLexicalEnvironment;
    WriteBarrier<JSScope> m_globalScopeExtension;
    WriteBarrier<JSCallee> m_globalCallee;
    WriteBarrier<JSCallee> m_stackOverflowFrameCallee;

    JS_GLOBAL_OBJECT_ADDITIONS_1;

    LazyClassStructure m_evalErrorStructure;
    LazyClassStructure m_rangeErrorStructure;
    LazyClassStructure m_referenceErrorStructure;
    LazyClassStructure m_syntaxErrorStructure;
    LazyClassStructure m_typeErrorStructure;
    LazyClassStructure m_URIErrorStructure;
    LazyClassStructure m_aggregateErrorStructure;

    WriteBarrier<ObjectConstructor> m_objectConstructor;
    WriteBarrier<ArrayConstructor> m_arrayConstructor;
    WriteBarrier<ShadowRealmConstructor> m_shadowRealmConstructor;
    WriteBarrier<RegExpConstructor> m_regExpConstructor;
    WriteBarrier<FunctionConstructor> m_functionConstructor;
    WriteBarrier<JSPromiseConstructor> m_promiseConstructor;
    WriteBarrier<JSInternalPromiseConstructor> m_internalPromiseConstructor;
    WriteBarrier<StringConstructor> m_stringConstructor;

    LazyProperty<JSGlobalObject, IntlCollator> m_defaultCollator;
    LazyProperty<JSGlobalObject, Structure> m_collatorStructure;
    LazyProperty<JSGlobalObject, Structure> m_displayNamesStructure;
    LazyProperty<JSGlobalObject, Structure> m_durationFormatStructure;
    LazyProperty<JSGlobalObject, Structure> m_listFormatStructure;
    LazyProperty<JSGlobalObject, Structure> m_localeStructure;
    LazyProperty<JSGlobalObject, Structure> m_pluralRulesStructure;
    LazyProperty<JSGlobalObject, Structure> m_relativeTimeFormatStructure;
    LazyProperty<JSGlobalObject, Structure> m_segmentIteratorStructure;
    LazyProperty<JSGlobalObject, Structure> m_segmenterStructure;
    LazyProperty<JSGlobalObject, Structure> m_segmentsStructure;
    LazyClassStructure m_dateTimeFormatStructure;
    LazyClassStructure m_numberFormatStructure;

    LazyProperty<JSGlobalObject, Structure> m_calendarStructure;
    LazyProperty<JSGlobalObject, Structure> m_durationStructure;
    LazyProperty<JSGlobalObject, Structure> m_instantStructure;
    LazyProperty<JSGlobalObject, Structure> m_plainDateStructure;
    LazyProperty<JSGlobalObject, Structure> m_plainDateTimeStructure;
    LazyProperty<JSGlobalObject, Structure> m_plainTimeStructure;
    LazyProperty<JSGlobalObject, Structure> m_timeZoneStructure;

    WriteBarrier<NullGetterFunction> m_nullGetterFunction;
    WriteBarrier<NullSetterFunction> m_nullSetterFunction;
    WriteBarrier<NullSetterFunction> m_nullSetterStrictFunction;

    LazyProperty<JSGlobalObject, JSFunction> m_parseIntFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_parseFloatFunction;

    LazyProperty<JSGlobalObject, JSFunction> m_objectProtoToStringFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_arrayProtoToStringFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_arrayProtoValuesFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_evalFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_promiseResolveFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_numberProtoToStringFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_typedArrayProtoSort;
    WriteBarrier<JSFunction> m_objectProtoValueOfFunction;
    WriteBarrier<JSFunction> m_functionProtoHasInstanceSymbolFunction;
    WriteBarrier<JSObject> m_regExpProtoSymbolReplace;
    LazyProperty<JSGlobalObject, GetterSetter> m_throwTypeErrorArgumentsCalleeGetterSetter;

    LazyProperty<JSGlobalObject, JSModuleLoader> m_moduleLoader;

    WriteBarrier<ObjectPrototype> m_objectPrototype;
    WriteBarrier<FunctionPrototype> m_functionPrototype;
    WriteBarrier<ArrayPrototype> m_arrayPrototype;
    WriteBarrier<ShadowRealmPrototype> m_shadowRealmPrototype;
    WriteBarrier<RegExpPrototype> m_regExpPrototype;
    WriteBarrier<IteratorPrototype> m_iteratorPrototype;
    WriteBarrier<AsyncIteratorPrototype> m_asyncIteratorPrototype;
    WriteBarrier<GeneratorFunctionPrototype> m_generatorFunctionPrototype;
    WriteBarrier<GeneratorPrototype> m_generatorPrototype;
    WriteBarrier<AsyncGeneratorPrototype> m_asyncGeneratorPrototype;
    WriteBarrier<ArrayIteratorPrototype> m_arrayIteratorPrototype;
    WriteBarrier<MapIteratorPrototype> m_mapIteratorPrototype;
    WriteBarrier<SetIteratorPrototype> m_setIteratorPrototype;

    LazyProperty<JSGlobalObject, Structure> m_debuggerScopeStructure;
    LazyProperty<JSGlobalObject, Structure> m_withScopeStructure;
    LazyProperty<JSGlobalObject, Structure> m_strictEvalActivationStructure;
    LazyProperty<JSGlobalObject, Structure> m_moduleEnvironmentStructure;
    LazyProperty<JSGlobalObject, Structure> m_callbackConstructorStructure;
    LazyProperty<JSGlobalObject, Structure> m_callbackFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_callbackObjectStructure;
#if JSC_OBJC_API_ENABLED
    LazyProperty<JSGlobalObject, Structure> m_objcCallbackFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_objcWrapperObjectStructure;
#endif
#ifdef JSC_GLIB_API_ENABLED
    LazyProperty<JSGlobalObject, Structure> m_glibCallbackFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_glibWrapperObjectStructure;
#endif

    WriteBarrierStructureID m_lexicalEnvironmentStructure;
    WriteBarrierStructureID m_directArgumentsStructure;
    WriteBarrierStructureID m_scopedArgumentsStructure;
    WriteBarrierStructureID m_clonedArgumentsStructure;

    WriteBarrierStructureID m_objectStructureForObjectConstructor;

    // Lists the actual structures used for having these particular indexing shapes.
    WriteBarrierStructureID m_originalArrayStructureForIndexingShape[NumberOfArrayIndexingModes];
    // Lists the structures we should use during allocation for these particular indexing shapes.
    // These structures will differ from the originals list above when we are having a bad time.
    WriteBarrierStructureID m_arrayStructureForIndexingShapeDuringAllocation[NumberOfArrayIndexingModes];

    WriteBarrierStructureID m_nullPrototypeObjectStructure;
    WriteBarrierStructureID m_calleeStructure;

    WriteBarrierStructureID m_hostFunctionStructure;

    struct FunctionStructures {
        WriteBarrierStructureID arrowFunctionStructure;
        WriteBarrierStructureID sloppyFunctionStructure;
        WriteBarrierStructureID strictFunctionStructure;
    };
    FunctionStructures m_builtinFunctions;
    FunctionStructures m_ordinaryFunctions;

    WriteBarrierStructureID m_shadowRealmObjectStructure;
    WriteBarrierStructureID m_regExpStructure;

    WriteBarrierStructureID m_asyncFunctionStructure;
    WriteBarrierStructureID m_asyncGeneratorFunctionStructure;
    WriteBarrierStructureID m_generatorFunctionStructure;
    WriteBarrierStructureID m_generatorStructure;
    WriteBarrierStructureID m_asyncGeneratorStructure;
    WriteBarrierStructureID m_arrayIteratorStructure;
    WriteBarrierStructureID m_mapIteratorStructure;
    WriteBarrierStructureID m_setIteratorStructure;
    WriteBarrierStructureID m_regExpMatchesArrayStructure;
    WriteBarrierStructureID m_regExpMatchesArrayWithIndicesStructure;
    WriteBarrierStructureID m_regExpMatchesIndicesArrayStructure;

    LazyProperty<JSGlobalObject, Structure> m_boundFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_customGetterFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_customSetterFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_nativeStdFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_remoteFunctionStructure;
    WriteBarrier<AsyncFunctionPrototype> m_asyncFunctionPrototype;
    WriteBarrier<AsyncGeneratorFunctionPrototype> m_asyncGeneratorFunctionPrototype;
    LazyProperty<JSGlobalObject, Structure> m_iteratorResultObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_dataPropertyDescriptorObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_accessorPropertyDescriptorObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_moduleRecordStructure;
    LazyProperty<JSGlobalObject, Structure> m_syntheticModuleRecordStructure;
    LazyProperty<JSGlobalObject, Structure> m_moduleNamespaceObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_proxyObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_callableProxyObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_proxyRevokeStructure;
    LazyClassStructure m_sharedArrayBufferStructure;

#define DEFINE_STORAGE_FOR_SIMPLE_TYPE_PROTOTYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    WriteBarrier<capitalName ## Prototype> m_ ## lowerName ## Prototype;

#define DEFINE_STORAGE_FOR_SIMPLE_TYPE_STRUCTURE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    WriteBarrierStructureID m_ ## properName ## Structure;

#define DEFINE_STORAGE_FOR_LAZY_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    LazyClassStructure m_ ## properName ## Structure;

    FOR_EACH_SIMPLE_BUILTIN_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE_STRUCTURE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE_STRUCTURE)
    FOR_EACH_SIMPLE_BUILTIN_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE_PROTOTYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE_PROTOTYPE)
    
#if ENABLE(WEBASSEMBLY)
    LazyProperty<JSGlobalObject, Structure> m_webAssemblyModuleRecordStructure;
    LazyProperty<JSGlobalObject, Structure> m_webAssemblyFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_jsToWasmICCalleeStructure;
    LazyProperty<JSGlobalObject, Structure> m_webAssemblyWrapperFunctionStructure;
    FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(DEFINE_STORAGE_FOR_LAZY_TYPE)
#endif // ENABLE(WEBASSEMBLY)

    FOR_EACH_LAZY_BUILTIN_TYPE(DEFINE_STORAGE_FOR_LAZY_TYPE)

#undef DEFINE_STORAGE_FOR_SIMPLE_TYPE_PROTOTYPE
#undef DEFINE_STORAGE_FOR_SIMPLE_TYPE_STRUCTURE
#undef DEFINE_STORAGE_FOR_LAZY_TYPE

    WriteBarrier<GetterSetter> m_speciesGetterSetter;
    
    LazyProperty<JSGlobalObject, JSTypedArrayViewPrototype> m_typedArrayProto;
    LazyProperty<JSGlobalObject, JSTypedArrayViewConstructor> m_typedArraySuperConstructor;
    
#define DECLARE_TYPED_ARRAY_TYPE_STRUCTURE(name) \
    LazyClassStructure m_typedArray ## name; \
    LazyProperty<JSGlobalObject, Structure> m_resizableOrGrowableSharedTypedArray ## name ## Structure;
    FOR_EACH_TYPED_ARRAY_TYPE(DECLARE_TYPED_ARRAY_TYPE_STRUCTURE)
#undef DECLARE_TYPED_ARRAY_TYPE_STRUCTURE

    LazyProperty<JSGlobalObject, JSInternalPromise> m_importMapStatusPromise;

    FixedVector<LazyProperty<JSGlobalObject, JSCell>> m_linkTimeConstants;

    StructureCache m_structureCache;

    String m_name;

    Strong<JSObject> m_unhandledRejectionCallback;

    Debugger* m_debugger;

#if ENABLE(REMOTE_INSPECTOR)
    // FIXME: <http://webkit.org/b/246237> Local inspection should be controlled by `inspectable` API.
    std::unique_ptr<Inspector::JSGlobalObjectInspectorController> m_inspectorController;
    std::unique_ptr<JSGlobalObjectDebuggable> m_inspectorDebuggable;
#endif

    Ref<WatchpointSet> m_masqueradesAsUndefinedWatchpointSet;
    Ref<WatchpointSet> m_havingABadTimeWatchpointSet;
    Ref<WatchpointSet> m_varInjectionWatchpointSet;
    Ref<WatchpointSet> m_varReadOnlyWatchpointSet;
    Ref<WatchpointSet> m_regExpRecompiledWatchpointSet;

    std::unique_ptr<JSGlobalObjectRareData> m_rareData;

    WeakRandom m_weakRandom;
    RegExpGlobalData m_regExpGlobalData;

    // If this hasn't been invalidated, it means the array iterator protocol
    // is not observable to user code yet.
    InlineWatchpointSet m_arrayIteratorProtocolWatchpointSet { IsWatched };
    InlineWatchpointSet m_mapIteratorProtocolWatchpointSet { IsWatched };
    InlineWatchpointSet m_setIteratorProtocolWatchpointSet { IsWatched };
    InlineWatchpointSet m_stringIteratorProtocolWatchpointSet { IsWatched };
    InlineWatchpointSet m_stringSymbolReplaceWatchpointSet { IsWatched };
    InlineWatchpointSet m_regExpPrimordialPropertiesWatchpointSet { IsWatched };
    InlineWatchpointSet m_mapSetWatchpointSet { IsWatched };
    InlineWatchpointSet m_setAddWatchpointSet { IsWatched };
    InlineWatchpointSet m_arraySpeciesWatchpointSet { ClearWatchpoint };
    InlineWatchpointSet m_arrayJoinWatchpointSet { IsWatched };
    InlineWatchpointSet m_arrayPrototypeChainIsSaneWatchpointSet { IsWatched };
    InlineWatchpointSet m_objectPrototypeChainIsSaneWatchpointSet { IsWatched };
    InlineWatchpointSet m_stringPrototypeChainIsSaneWatchpointSet { IsWatched };
    InlineWatchpointSet m_numberToStringWatchpointSet { IsWatched };
    InlineWatchpointSet m_structureCacheClearedWatchpointSet { IsWatched };
    InlineWatchpointSet m_arrayBufferSpeciesWatchpointSet { ClearWatchpoint };
    InlineWatchpointSet m_sharedArrayBufferSpeciesWatchpointSet { ClearWatchpoint };
    InlineWatchpointSet m_typedArrayConstructorSpeciesWatchpointSet { IsWatched };
    InlineWatchpointSet m_typedArrayPrototypeIteratorProtocolWatchpointSet { IsWatched };
#define DECLARE_TYPED_ARRAY_TYPE_SPECIES_WATCHPOINT_SET(name) \
    InlineWatchpointSet m_typedArray ## name ## SpeciesWatchpointSet { ClearWatchpoint }; \
    InlineWatchpointSet m_typedArray ## name ## IteratorProtocolWatchpointSet { ClearWatchpoint };
    FOR_EACH_TYPED_ARRAY_TYPE(DECLARE_TYPED_ARRAY_TYPE_SPECIES_WATCHPOINT_SET)
#undef DECLARE_TYPED_ARRAY_TYPE_SPECIES_WATCHPOINT_SET

    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayConstructorSpeciesWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayPrototypeConstructorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayPrototypeJoinWatchpoint;
    std::unique_ptr<ObjectAdaptiveStructureWatchpoint> m_arrayPrototypeAbsenceOfIndexedPropertiesWatchpoint;
    std::unique_ptr<ObjectAdaptiveStructureWatchpoint> m_stringPrototypeAbsenceOfIndexedPropertiesWatchpoint;
    std::unique_ptr<ObjectAdaptiveStructureWatchpoint> m_objectPrototypeAbsenceOfIndexedPropertiesWatchpoint;
    std::unique_ptr<ChainedWatchpoint> m_objectPrototypeAbsenceOfIndexedPropertiesWatchpointForArray;
    std::unique_ptr<ChainedWatchpoint> m_objectPrototypeAbsenceOfIndexedPropertiesWatchpointForString;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayIteratorPrototypeNext;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_mapPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_mapIteratorPrototypeNextWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_setPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_setIteratorPrototypeNextWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_stringPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_stringIteratorPrototypeNextWatchpoint;
    std::unique_ptr<ObjectAdaptiveStructureWatchpoint> m_stringPrototypeSymbolReplaceMissWatchpoint;
    std::unique_ptr<ObjectAdaptiveStructureWatchpoint> m_objectPrototypeSymbolReplaceMissWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_regExpPrototypeExecWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_regExpPrototypeGlobalWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_regExpPrototypeUnicodeWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_regExpPrototypeSymbolReplaceWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_mapPrototypeSetWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_setPrototypeAddWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_numberPrototypeToStringWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayBufferConstructorSpeciesWatchpoints[2];
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayBufferPrototypeConstructorWatchpoints[2];
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_typedArrayConstructorSpeciesWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_typedArrayPrototypeSymbolIteratorWatchpoint;
#define DECLARE_TYPED_ARRAY_TYPE_WATCHPOINT(name) \
    std::unique_ptr<ObjectAdaptiveStructureWatchpoint> m_typedArray ## name ## ConstructorSpeciesAbsenceWatchpoint; \
    std::unique_ptr<ObjectAdaptiveStructureWatchpoint> m_typedArray ## name ## PrototypeSymbolIteratorAbsenceWatchpoint; \
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_typedArray ## name ## PrototypeConstructorWatchpoint;
    FOR_EACH_TYPED_ARRAY_TYPE(DECLARE_TYPED_ARRAY_TYPE_WATCHPOINT)
#undef DECLARE_TYPED_ARRAY_TYPE_WATCHPOINT

    std::unique_ptr<ObjectAdaptiveStructureWatchpoint>& typedArrayConstructorSpeciesAbsenceWatchpoint(TypedArrayType type)
    {
        switch (type) {
        case NotTypedArray:
            RELEASE_ASSERT_NOT_REACHED();
            return m_typedArrayInt8ConstructorSpeciesAbsenceWatchpoint;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## ConstructorSpeciesAbsenceWatchpoint;
            FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8ConstructorSpeciesAbsenceWatchpoint;
    }

    std::unique_ptr<ObjectAdaptiveStructureWatchpoint>& typedArrayPrototypeSymbolIteratorAbsenceWatchpoint(TypedArrayType type)
    {
        switch (type) {
        case NotTypedArray:
            RELEASE_ASSERT_NOT_REACHED();
            return m_typedArrayInt8PrototypeSymbolIteratorAbsenceWatchpoint;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## PrototypeSymbolIteratorAbsenceWatchpoint;
            FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8PrototypeSymbolIteratorAbsenceWatchpoint;
    }

    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>& typedArrayPrototypeConstructorWatchpoint(TypedArrayType type)
    {
        switch (type) {
        case NotTypedArray:
            RELEASE_ASSERT_NOT_REACHED();
            return m_typedArrayInt8PrototypeConstructorWatchpoint;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## PrototypeConstructorWatchpoint;
            FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8PrototypeConstructorWatchpoint;
    }

public:
    JSCallee* stackOverflowFrameCallee() const { return m_stackOverflowFrameCallee.get(); }

    InlineWatchpointSet& arrayIteratorProtocolWatchpointSet() { return m_arrayIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& mapIteratorProtocolWatchpointSet() { return m_mapIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& setIteratorProtocolWatchpointSet() { return m_setIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& stringIteratorProtocolWatchpointSet() { return m_stringIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& stringSymbolReplaceWatchpointSet() { return m_stringSymbolReplaceWatchpointSet; }
    InlineWatchpointSet& regExpPrimordialPropertiesWatchpointSet() { return m_regExpPrimordialPropertiesWatchpointSet; }
    InlineWatchpointSet& mapSetWatchpointSet() { return m_mapSetWatchpointSet; }
    InlineWatchpointSet& setAddWatchpointSet() { return m_setAddWatchpointSet; }
    InlineWatchpointSet& arraySpeciesWatchpointSet() { return m_arraySpeciesWatchpointSet; }
    InlineWatchpointSet& arrayPrototypeChainIsSaneWatchpointSet() { return m_arrayPrototypeChainIsSaneWatchpointSet; }
    InlineWatchpointSet& objectPrototypeChainIsSaneWatchpointSet() { return m_objectPrototypeChainIsSaneWatchpointSet; }
    InlineWatchpointSet& stringPrototypeChainIsSaneWatchpointSet() { return m_stringPrototypeChainIsSaneWatchpointSet; }
    InlineWatchpointSet& arrayJoinWatchpointSet() { return m_arrayJoinWatchpointSet; }
    InlineWatchpointSet& numberToStringWatchpointSet()
    {
        RELEASE_ASSERT(Options::useJIT());
        return m_numberToStringWatchpointSet;
    }
    InlineWatchpointSet& structureCacheClearedWatchpointSet() { return m_structureCacheClearedWatchpointSet; }
    InlineWatchpointSet& arrayBufferSpeciesWatchpointSet(ArrayBufferSharingMode sharingMode)
    {
        switch (sharingMode) {
        case ArrayBufferSharingMode::Default:
            return m_arrayBufferSpeciesWatchpointSet;
        case ArrayBufferSharingMode::Shared:
            return m_sharedArrayBufferSpeciesWatchpointSet;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_arrayBufferSpeciesWatchpointSet;
    }

    InlineWatchpointSet& typedArraySpeciesWatchpointSet(TypedArrayType type)
    {
        switch (type) {
        case NotTypedArray:
            RELEASE_ASSERT_NOT_REACHED();
            return m_typedArrayInt8SpeciesWatchpointSet;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## SpeciesWatchpointSet;
            FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8SpeciesWatchpointSet;
    }
    InlineWatchpointSet& typedArrayIteratorProtocolWatchpointSet(TypedArrayType type)
    {
        switch (type) {
        case NotTypedArray:
            RELEASE_ASSERT_NOT_REACHED();
            return m_typedArrayInt8IteratorProtocolWatchpointSet;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## IteratorProtocolWatchpointSet;
            FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8IteratorProtocolWatchpointSet;
    }
    InlineWatchpointSet& typedArrayConstructorSpeciesWatchpointSet() { return m_typedArrayConstructorSpeciesWatchpointSet; }
    InlineWatchpointSet& typedArrayPrototypeIteratorProtocolWatchpointSet() { return m_typedArrayPrototypeIteratorProtocolWatchpointSet; }

    bool isArrayPrototypeIteratorProtocolFastAndNonObservable();
    bool isTypedArrayPrototypeIteratorProtocolFastAndNonObservable(TypedArrayType);
    bool isMapPrototypeIteratorProtocolFastAndNonObservable();
    bool isSetPrototypeIteratorProtocolFastAndNonObservable();
    bool isStringPrototypeIteratorProtocolFastAndNonObservable();
    bool isMapPrototypeSetFastAndNonObservable();
    bool isSetPrototypeAddFastAndNonObservable();
    bool isArgumentsPrototypeIteratorProtocolFastAndNonObservable();

#if ENABLE(DFG_JIT)
    using ReferencedGlobalPropertyWatchpointSets = HashMap<RefPtr<UniquedStringImpl>, Ref<WatchpointSet>, IdentifierRepHash>;
    ReferencedGlobalPropertyWatchpointSets m_referencedGlobalPropertyWatchpointSets;
    ConcurrentJSLock m_referencedGlobalPropertyWatchpointSetsLock;
#endif

    bool m_evalEnabled { true };
    bool m_webAssemblyEnabled { true };
    bool m_needsSiteSpecificQuirks { false };
    unsigned m_globalLexicalBindingEpoch { 1 };
    String m_evalDisabledErrorMessage;
    String m_webAssemblyDisabledErrorMessage;
    RuntimeFlags m_runtimeFlags;
    WeakPtr<ConsoleClient> m_consoleClient;
    std::optional<unsigned> m_stackTraceLimit;

    template<typename T>
    struct WeakCustomGetterOrSetterHash {
        static unsigned hash(const Weak<T>&);
        static bool equal(const Weak<T>&, const Weak<T>&);
        static unsigned hash(const PropertyName&, typename T::CustomFunctionPointer, const ClassInfo*);

        static constexpr bool safeToCompareToEmptyOrDeleted = false;
    };

    WeakGCSet<JSCustomGetterFunction, WeakCustomGetterOrSetterHash<JSCustomGetterFunction>> m_customGetterFunctionSet;
    WeakGCSet<JSCustomSetterFunction, WeakCustomGetterOrSetterHash<JSCustomSetterFunction>> m_customSetterFunctionSet;

    WeakGCSet<JSCustomGetterFunction, WeakCustomGetterOrSetterHash<JSCustomGetterFunction>>& customGetterFunctionSet() { return m_customGetterFunctionSet; }
    WeakGCSet<JSCustomSetterFunction, WeakCustomGetterOrSetterHash<JSCustomSetterFunction>>& customSetterFunctionSet() { return m_customSetterFunctionSet; }

    Ref<ImportMap> m_importMap;

#if ASSERT_ENABLED
    const JSGlobalObject* m_globalObjectAtDebuggerEntry { nullptr };
#endif

    static JS_EXPORT_PRIVATE const GlobalObjectMethodTable s_globalObjectMethodTable;
    const GlobalObjectMethodTable* m_globalObjectMethodTable;

    void createRareDataIfNeeded()
    {
        if (m_rareData)
            return;
        m_rareData = makeUnique<JSGlobalObjectRareData>();
    }
        
public:
    using Base = JSSegmentedVariableObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable | OverridesGetOwnPropertySlot | OverridesPut | IsImmutablePrototypeExoticObject;

    static constexpr bool needsDestruction = true;
    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.globalObjectSpace<mode>();
    }

    JS_EXPORT_PRIVATE static JSGlobalObject* create(VM&, Structure*);
    JS_EXPORT_PRIVATE static JSGlobalObject* createWithCustomMethodTable(VM&, Structure*, const GlobalObjectMethodTable*);

    DECLARE_EXPORT_INFO;

    bool hasDebugger() const { return m_debugger; }
    bool hasInteractiveDebugger() const;
    const RuntimeFlags& runtimeFlags() const { return m_runtimeFlags; }

#if ENABLE(DFG_JIT)
    WatchpointSet* getReferencedPropertyWatchpointSet(UniquedStringImpl*);
    WatchpointSet& ensureReferencedPropertyWatchpointSet(UniquedStringImpl*);
#endif

    std::optional<unsigned> stackTraceLimit() const { return m_stackTraceLimit; }
    void setStackTraceLimit(std::optional<unsigned> value) { m_stackTraceLimit = value; }

protected:
    JS_EXPORT_PRIVATE explicit JSGlobalObject(VM&, Structure*, const GlobalObjectMethodTable* = nullptr);

    JS_EXPORT_PRIVATE void finishCreation(VM&);

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSObject*);

    void addGlobalVar(const Identifier&);

public:
    JS_EXPORT_PRIVATE ~JSGlobalObject();
    JS_EXPORT_PRIVATE static void destroy(JSCell*);

    DECLARE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE);

    JS_EXPORT_PRIVATE static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    JS_EXPORT_PRIVATE static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

    void addVar(JSGlobalObject* globalObject, const Identifier& propertyName)
    {
        if (!hasOwnProperty(globalObject, propertyName))
            addGlobalVar(propertyName);
    }
    void addFunction(JSGlobalObject*, const Identifier&);

    JSScope* globalScope() { return m_globalLexicalEnvironment.get(); }
    JSGlobalLexicalEnvironment* globalLexicalEnvironment() { return m_globalLexicalEnvironment.get(); }

    JSScope* globalScopeExtension() { return m_globalScopeExtension.get(); }
    void setGlobalScopeExtension(JSScope*);
    void clearGlobalScopeExtension();

    JSCallee* globalCallee() { return m_globalCallee.get(); }

    // The following accessors return pristine values, even if a script 
    // replaces the global object's associated property.

    GetterSetter* speciesGetterSetter() const { return m_speciesGetterSetter.get(); }

    ArrayConstructor* arrayConstructor() const { return m_arrayConstructor.get(); }
    RegExpConstructor* regExpConstructor() const { return m_regExpConstructor.get(); }
    ObjectConstructor* objectConstructor() const { return m_objectConstructor.get(); }
    FunctionConstructor* functionConstructor() const { return m_functionConstructor.get(); }
    JSPromiseConstructor* promiseConstructor() const { return m_promiseConstructor.get(); }
    JSInternalPromiseConstructor* internalPromiseConstructor() const { return m_internalPromiseConstructor.get(); }

    IntlCollator* defaultCollator() const { return m_defaultCollator.get(this); }

    NullGetterFunction* nullGetterFunction() const { return m_nullGetterFunction.get(); }
    NullSetterFunction* nullSetterFunction() const { return m_nullSetterFunction.get(); }
    NullSetterFunction* nullSetterStrictFunction() const { return m_nullSetterStrictFunction.get(); }

    JSFunction* parseIntFunction() const { return m_parseIntFunction.get(this); }
    JSFunction* parseFloatFunction() const { return m_parseFloatFunction.get(this); }

    JSFunction* evalFunction() const { return m_evalFunction.get(this); }
    JSFunction* throwTypeErrorFunction() const;
    JSFunction* objectProtoToStringFunction() const { return m_objectProtoToStringFunction.get(this); }
    JSFunction* objectProtoToStringFunctionConcurrently() const { return m_objectProtoToStringFunction.getConcurrently(); }
    JSFunction* arrayProtoToStringFunction() const { return m_arrayProtoToStringFunction.get(this); }
    JSFunction* arrayProtoValuesFunction() const { return m_arrayProtoValuesFunction.get(this); }
    JSFunction* arrayProtoValuesFunctionConcurrently() const { return m_arrayProtoValuesFunction.getConcurrently(); }
    JSFunction* iteratorProtocolFunction() const;
    JSFunction* newPromiseCapabilityFunction() const;
    JSFunction* promiseResolveFunction() const { return m_promiseResolveFunction.get(this); }
    JSFunction* resolvePromiseFunction() const;
    JSFunction* rejectPromiseFunction() const;
    JSFunction* promiseProtoThenFunction() const;
    JSFunction* performPromiseThenFunction() const;
    JSFunction* objectProtoValueOfFunction() const { return m_objectProtoValueOfFunction.get(); }
    JSFunction* numberProtoToStringFunction() const { return m_numberProtoToStringFunction.getInitializedOnMainThread(this); }
    JSFunction* functionProtoHasInstanceSymbolFunction() const { return m_functionProtoHasInstanceSymbolFunction.get(); }
    JSFunction* regExpProtoExecFunction() const;
    JSFunction* typedArrayProtoSort() const { return m_typedArrayProtoSort.get(this); }
    JSFunction* stringProtoSubstringFunction() const;
    JSObject* regExpProtoSymbolReplaceFunction() const { return m_regExpProtoSymbolReplace.get(); }
    GetterSetter* regExpProtoGlobalGetter() const;
    GetterSetter* regExpProtoUnicodeGetter() const;
    GetterSetter* throwTypeErrorArgumentsCalleeGetterSetter() const { return m_throwTypeErrorArgumentsCalleeGetterSetter.get(this); }
    
    JSModuleLoader* moduleLoader() const { return m_moduleLoader.get(this); }

    ObjectPrototype* objectPrototype() const { return m_objectPrototype.get(); }
    FunctionPrototype* functionPrototype() const { return m_functionPrototype.get(); }
    ArrayPrototype* arrayPrototype() const { return m_arrayPrototype.get(); }
    JSObject* booleanPrototype() const { return m_booleanObjectStructure.prototypeInitializedOnMainThread(this); }
    StringPrototype* stringPrototype() const { return m_stringPrototype.get(); }
    JSObject* numberPrototype() const { return m_numberObjectStructure.prototypeInitializedOnMainThread(this); }
    BigIntPrototype* bigIntPrototype() const { return m_bigIntPrototype.get(); }
    JSObject* datePrototype() const { return m_dateStructure.prototype(this); }
    JSObject* symbolPrototype() const { return m_symbolObjectStructure.prototypeInitializedOnMainThread(this); }
    ShadowRealmPrototype* shadowRealmPrototype() const { return m_shadowRealmPrototype.get(); }
    RegExpPrototype* regExpPrototype() const { return m_regExpPrototype.get(); }
    JSObject* errorPrototype() const { return m_errorStructure.prototype(this); }
    IteratorPrototype* iteratorPrototype() const { return m_iteratorPrototype.get(); }
    AsyncIteratorPrototype* asyncIteratorPrototype() const { return m_asyncIteratorPrototype.get(); }
    GeneratorFunctionPrototype* generatorFunctionPrototype() const { return m_generatorFunctionPrototype.get(); }
    GeneratorPrototype* generatorPrototype() const { return m_generatorPrototype.get(); }
    AsyncFunctionPrototype* asyncFunctionPrototype() const { return m_asyncFunctionPrototype.get(); }
    ArrayIteratorPrototype* arrayIteratorPrototype() const { return m_arrayIteratorPrototype.get(); }
    MapIteratorPrototype* mapIteratorPrototype() const { return m_mapIteratorPrototype.get(); }
    SetIteratorPrototype* setIteratorPrototype() const { return m_setIteratorPrototype.get(); }
    JSObject* mapPrototype() const { return m_mapStructure.prototype(this); }
    // Workaround for the name conflict between JSCell::setPrototype.
    JSObject* jsSetPrototype() const { return m_setStructure.prototype(this); }
    JSPromisePrototype* promisePrototype() const { return m_promisePrototype.get(); }
    AsyncGeneratorPrototype* asyncGeneratorPrototype() const { return m_asyncGeneratorPrototype.get(); }
    AsyncGeneratorFunctionPrototype* asyncGeneratorFunctionPrototype() const { return m_asyncGeneratorFunctionPrototype.get(); }

    Structure* debuggerScopeStructure() const { return m_debuggerScopeStructure.get(this); }
    Structure* withScopeStructure() const { return m_withScopeStructure.get(this); }
    Structure* strictEvalActivationStructure() const { return m_strictEvalActivationStructure.get(this); }
    Structure* activationStructure() const { return m_lexicalEnvironmentStructure.get(); }
    Structure* moduleEnvironmentStructure() const { return m_moduleEnvironmentStructure.get(this); }
    Structure* directArgumentsStructure() const { return m_directArgumentsStructure.get(); }
    Structure* scopedArgumentsStructure() const { return m_scopedArgumentsStructure.get(); }
    Structure* clonedArgumentsStructure() const { return m_clonedArgumentsStructure.get(); }
    Structure* objectStructureForObjectConstructor() const { return m_objectStructureForObjectConstructor.get(); }
    Structure* originalArrayStructureForIndexingType(IndexingType indexingType) const
    {
        ASSERT(indexingType & IsArray);
        return m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(indexingType)].get();
    }
    Structure* arrayStructureForIndexingTypeDuringAllocation(IndexingType indexingType) const
    {
        ASSERT(indexingType & IsArray);
        return m_arrayStructureForIndexingShapeDuringAllocation[arrayIndexFromIndexingType(indexingType)].get();
    }
    Structure* arrayStructureForIndexingTypeDuringAllocation(JSGlobalObject* globalObject, IndexingType indexingType, JSValue newTarget) const;
    Structure* arrayStructureForProfileDuringAllocation(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, JSValue newTarget) const
    {
        return arrayStructureForIndexingTypeDuringAllocation(globalObject, ArrayAllocationProfile::selectIndexingTypeFor(profile), newTarget);
    }
        
    bool isOriginalArrayStructure(Structure* structure)
    {
        return originalArrayStructureForIndexingType(structure->indexingMode() | IsArray) == structure;
    }
        
    Structure* booleanObjectStructure() const { return m_booleanObjectStructure.get(this); }
    Structure* callbackConstructorStructure() const { return m_callbackConstructorStructure.get(this); }
    Structure* callbackFunctionStructure() const { return m_callbackFunctionStructure.get(this); }
    Structure* callbackObjectStructure() const { return m_callbackObjectStructure.get(this); }
#if JSC_OBJC_API_ENABLED
    Structure* objcCallbackFunctionStructure() const { return m_objcCallbackFunctionStructure.get(this); }
    Structure* objcWrapperObjectStructure() const { return m_objcWrapperObjectStructure.get(this); }
#endif
#ifdef JSC_GLIB_API_ENABLED
    Structure* glibCallbackFunctionStructure() const { return m_glibCallbackFunctionStructure.get(this); }
    Structure* glibWrapperObjectStructure() const { return m_glibWrapperObjectStructure.get(this); }
#endif
    Structure* dateStructure() const { return m_dateStructure.get(this); }
    Structure* symbolObjectStructure() const { return m_symbolObjectStructure.get(this); }
    Structure* nullPrototypeObjectStructure() const { return m_nullPrototypeObjectStructure.get(); }
    Structure* errorStructure() const { return m_errorStructure.get(this); }
    Structure* errorStructure(ErrorType errorType) const
    {
        switch (errorType) {
        case ErrorType::Error:
            return errorStructure();
        case ErrorType::EvalError:
            return m_evalErrorStructure.get(this);
        case ErrorType::RangeError:
            return m_rangeErrorStructure.get(this);
        case ErrorType::ReferenceError:
            return m_referenceErrorStructure.get(this);
        case ErrorType::SyntaxError:
            return m_syntaxErrorStructure.get(this);
        case ErrorType::TypeError:
            return m_typeErrorStructure.get(this);
        case ErrorType::URIError:
            return m_URIErrorStructure.get(this);
        case ErrorType::AggregateError:
            return m_aggregateErrorStructure.get(this);
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    template<ErrorType errorType> Structure* errorStructureWithErrorType() const { return errorStructure(errorType); }

    Structure* calleeStructure() const { return m_calleeStructure.get(); }
    Structure* hostFunctionStructure() const { return m_hostFunctionStructure.get(); }

    Structure* arrowFunctionStructure(bool isBuiltin) const
    {
        if (isBuiltin)
            return m_builtinFunctions.arrowFunctionStructure.get();
        return m_ordinaryFunctions.arrowFunctionStructure.get();
    }
    Structure* sloppyFunctionStructure(bool isBuiltin) const
    {
        if (isBuiltin)
            return m_builtinFunctions.sloppyFunctionStructure.get();
        return m_ordinaryFunctions.sloppyFunctionStructure.get();
    }
    Structure* strictFunctionStructure(bool isBuiltin) const
    {
        if (isBuiltin)
            return m_builtinFunctions.strictFunctionStructure.get();
        return m_ordinaryFunctions.strictFunctionStructure.get();
    }

    Structure* boundFunctionStructure() const { return m_boundFunctionStructure.get(this); }
    Structure* customGetterFunctionStructure() const { return m_customGetterFunctionStructure.get(this); }
    Structure* customSetterFunctionStructure() const { return m_customSetterFunctionStructure.get(this); }
    Structure* nativeStdFunctionStructure() const { return m_nativeStdFunctionStructure.get(this); }
    Structure* numberObjectStructure() const { return m_numberObjectStructure.get(this); }
    Structure* regExpStructure() const { return m_regExpStructure.get(); }
    Structure* shadowRealmStructure() const { return m_shadowRealmObjectStructure.get(); }
    Structure* generatorStructure() const { return m_generatorStructure.get(); }
    Structure* asyncGeneratorStructure() const { return m_asyncGeneratorStructure.get(); }
    Structure* generatorFunctionStructure() const { return m_generatorFunctionStructure.get(); }
    Structure* asyncFunctionStructure() const { return m_asyncFunctionStructure.get(); }
    Structure* asyncGeneratorFunctionStructure() const { return m_asyncGeneratorFunctionStructure.get(); }
    Structure* arrayIteratorStructure() const { return m_arrayIteratorStructure.get(); }    
    Structure* mapIteratorStructure() const { return m_mapIteratorStructure.get(); }
    Structure* setIteratorStructure() const { return m_setIteratorStructure.get(); }
    Structure* stringObjectStructure() const { return m_stringObjectStructure.get(); }
    Structure* iteratorResultObjectStructure() const { return m_iteratorResultObjectStructure.get(this); }
    Structure* dataPropertyDescriptorObjectStructure() const { return m_dataPropertyDescriptorObjectStructure.get(this); }
    Structure* accessorPropertyDescriptorObjectStructure() const { return m_accessorPropertyDescriptorObjectStructure.get(this); }
    Structure* regExpMatchesArrayStructure() const { return m_regExpMatchesArrayStructure.get(); }
    Structure* regExpMatchesArrayWithIndicesStructure() const { return m_regExpMatchesArrayWithIndicesStructure.get(); }
    Structure* regExpMatchesIndicesArrayStructure() const { return m_regExpMatchesIndicesArrayStructure.get(); }
    Structure* remoteFunctionStructure() const { return m_remoteFunctionStructure.get(this); }
    Structure* moduleRecordStructure() const { return m_moduleRecordStructure.get(this); }
    Structure* syntheticModuleRecordStructure() const { return m_syntheticModuleRecordStructure.get(this); }
    Structure* moduleNamespaceObjectStructure() const { return m_moduleNamespaceObjectStructure.get(this); }
    Structure* proxyObjectStructure() const { return m_proxyObjectStructure.get(this); }
    Structure* callableProxyObjectStructure() const { return m_callableProxyObjectStructure.get(this); }
    Structure* proxyRevokeStructure() const { return m_proxyRevokeStructure.get(this); }
    Structure* restParameterStructure() const { return arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous); }
    Structure* originalRestParameterStructure() const { return originalArrayStructureForIndexingType(ArrayWithContiguous); }
#if ENABLE(WEBASSEMBLY)
    Structure* webAssemblyModuleRecordStructure() const { return m_webAssemblyModuleRecordStructure.get(this); }
    Structure* webAssemblyFunctionStructure() const { return m_webAssemblyFunctionStructure.get(this); }
    Structure* jsToWasmICCalleeStructure() const { return m_jsToWasmICCalleeStructure.get(this); }
    Structure* webAssemblyWrapperFunctionStructure() const { return m_webAssemblyWrapperFunctionStructure.get(this); }
#endif // ENABLE(WEBASSEMBLY)
    Structure* collatorStructure() { return m_collatorStructure.get(this); }
    Structure* dateTimeFormatStructure() { return m_dateTimeFormatStructure.get(this); }
    Structure* displayNamesStructure() { return m_displayNamesStructure.get(this); }
    Structure* durationFormatStructure() { return m_durationFormatStructure.get(this); }
    Structure* listFormatStructure() { return m_listFormatStructure.get(this); }
    Structure* numberFormatStructure() { return m_numberFormatStructure.get(this); }
    Structure* localeStructure() { return m_localeStructure.get(this); }
    Structure* pluralRulesStructure() { return m_pluralRulesStructure.get(this); }
    Structure* relativeTimeFormatStructure() { return m_relativeTimeFormatStructure.get(this); }
    Structure* segmentIteratorStructure() { return m_segmentIteratorStructure.get(this); }
    Structure* segmenterStructure() { return m_segmenterStructure.get(this); }
    Structure* segmentsStructure() { return m_segmentsStructure.get(this); }

    JSObject* dateTimeFormatConstructor() { return m_dateTimeFormatStructure.constructor(this); }
    JSObject* dateTimeFormatPrototype() { return m_dateTimeFormatStructure.prototype(this); }
    JSObject* numberFormatConstructor() { return m_numberFormatStructure.constructor(this); }
    JSObject* numberFormatPrototype() { return m_numberFormatStructure.prototype(this); }

    Structure* calendarStructure() { return m_calendarStructure.get(this); }
    Structure* durationStructure() { return m_durationStructure.get(this); }
    Structure* instantStructure() { return m_instantStructure.get(this); }
    Structure* plainDateStructure() { return m_plainDateStructure.get(this); }
    Structure* plainDateTimeStructure() { return m_plainDateTimeStructure.get(this); }
    Structure* plainTimeStructure() { return m_plainTimeStructure.get(this); }
    Structure* timeZoneStructure() { return m_timeZoneStructure.get(this); }

    JS_EXPORT_PRIVATE void setInspectable(bool);
    JS_EXPORT_PRIVATE bool inspectable() const;

    void setIsITML();

    RegExpGlobalData& regExpGlobalData() { return m_regExpGlobalData; }
    static ptrdiff_t regExpGlobalDataOffset() { return OBJECT_OFFSETOF(JSGlobalObject, m_regExpGlobalData); }

    static ptrdiff_t offsetOfGlobalThis() { return OBJECT_OFFSETOF(JSGlobalObject, m_globalThis); }
    static ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(JSGlobalObject, m_vm); }
    static ptrdiff_t offsetOfGlobalLexicalEnvironment() { return OBJECT_OFFSETOF(JSGlobalObject, m_globalLexicalEnvironment); }
    static ptrdiff_t offsetOfGlobalLexicalBindingEpoch() { return OBJECT_OFFSETOF(JSGlobalObject, m_globalLexicalBindingEpoch); }
    static ptrdiff_t offsetOfVarInjectionWatchpoint() { return OBJECT_OFFSETOF(JSGlobalObject, m_varInjectionWatchpointSet); }
    static ptrdiff_t offsetOfVarReadOnlyWatchpoint() { return OBJECT_OFFSETOF(JSGlobalObject, m_varReadOnlyWatchpointSet); }
    static ptrdiff_t offsetOfFunctionProtoHasInstanceSymbolFunction() { return OBJECT_OFFSETOF(JSGlobalObject, m_functionProtoHasInstanceSymbolFunction); }

#if ENABLE(REMOTE_INSPECTOR)
    // FIXME: <http://webkit.org/b/246237> Local inspection should be controlled by `inspectable` API.
    Inspector::JSGlobalObjectInspectorController& inspectorController() const { return *m_inspectorController.get(); }
    JSGlobalObjectDebuggable& inspectorDebuggable() { return *m_inspectorDebuggable.get(); }
#endif

    void bumpGlobalLexicalBindingEpoch(VM&);
    unsigned globalLexicalBindingEpoch() const { return m_globalLexicalBindingEpoch; }
    static ptrdiff_t globalLexicalBindingEpochOffset() { return OBJECT_OFFSETOF(JSGlobalObject, m_globalLexicalBindingEpoch); }
    unsigned* addressOfGlobalLexicalBindingEpoch() { return &m_globalLexicalBindingEpoch; }

    JS_EXPORT_PRIVATE void setConsoleClient(WeakPtr<ConsoleClient>&&);
    WeakPtr<ConsoleClient> consoleClient() const { return m_consoleClient; }

    void setName(const String&);
    const String& name() const { return m_name; }

    StructureCache& structureCache() { return m_structureCache; }

    void setUnhandledRejectionCallback(VM& vm, JSObject* function) { m_unhandledRejectionCallback.set(vm, function); }
    JSObject* unhandledRejectionCallback() const { return m_unhandledRejectionCallback.get(); }

    static void reportUncaughtExceptionAtEventLoop(JSGlobalObject*, Exception*);
    static JSObject* currentScriptExecutionOwner(JSGlobalObject* global) { return global; }
    static ScriptExecutionStatus scriptExecutionStatus(JSGlobalObject*, JSObject*) { return ScriptExecutionStatus::Running; }
    static void reportViolationForUnsafeEval(JSGlobalObject*, JSString*) { }

    JSObject* arrayBufferPrototype(ArrayBufferSharingMode sharingMode) const
    {
        switch (sharingMode) {
        case ArrayBufferSharingMode::Default:
            return m_arrayBufferStructure.prototype(this);
        case ArrayBufferSharingMode::Shared:
            return m_sharedArrayBufferStructure.prototype(this);
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
    Structure* arrayBufferStructure(ArrayBufferSharingMode sharingMode) const
    {
        switch (sharingMode) {
        case ArrayBufferSharingMode::Default:
            return m_arrayBufferStructure.get(this);
        case ArrayBufferSharingMode::Shared:
            return m_sharedArrayBufferStructure.get(this);
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
    template<ArrayBufferSharingMode sharingMode> Structure* arrayBufferStructureWithSharingMode() const { return arrayBufferStructure(sharingMode); }
    JSObject* arrayBufferConstructor(ArrayBufferSharingMode sharingMode) const
    {
        switch (sharingMode) {
        case ArrayBufferSharingMode::Default:
            return m_arrayBufferStructure.constructor(this);
        case ArrayBufferSharingMode::Shared:
            return m_sharedArrayBufferStructure.constructor(this);
        }
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }


#define DEFINE_ACCESSORS_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    Structure* properName ## Structure() { return m_ ## properName ## Structure.get(); }

    FOR_EACH_SIMPLE_BUILTIN_TYPE(DEFINE_ACCESSORS_FOR_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DEFINE_ACCESSORS_FOR_SIMPLE_TYPE)

#undef DEFINE_ACCESSORS_FOR_SIMPLE_TYPE

#define DEFINE_ACCESSORS_FOR_LAZY_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    Structure* properName ## Structure() { return m_ ## properName ## Structure.get(this); } \
    JSObject* properName ## Constructor() { return m_ ## properName ## Structure.constructor(this); }

    FOR_EACH_LAZY_BUILTIN_TYPE(DEFINE_ACCESSORS_FOR_LAZY_TYPE)
    FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(DEFINE_ACCESSORS_FOR_LAZY_TYPE)

#undef DEFINE_ACCESSORS_FOR_LAZY_TYPE

    LazyClassStructure& lazyTypedArrayStructure(TypedArrayType type)
    {
        switch (type) {
        case NotTypedArray:
            RELEASE_ASSERT_NOT_REACHED();
            return m_typedArrayInt8;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name;
            FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8;
    }
    const LazyClassStructure& lazyTypedArrayStructure(TypedArrayType type) const
    {
        return const_cast<const LazyClassStructure&>(const_cast<JSGlobalObject*>(this)->lazyTypedArrayStructure(type));
    }
    LazyProperty<JSGlobalObject, Structure>& lazyResizableOrGrowableSharedTypedArrayStructure(TypedArrayType type)
    {
        switch (type) {
        case NotTypedArray:
            RELEASE_ASSERT_NOT_REACHED();
            return m_resizableOrGrowableSharedTypedArrayInt8Structure;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_resizableOrGrowableSharedTypedArray ## name ## Structure;
            FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
        }
        RELEASE_ASSERT_NOT_REACHED();
        return m_resizableOrGrowableSharedTypedArrayInt8Structure;
    }
    const LazyProperty<JSGlobalObject, Structure>& lazyResizableOrGrowableSharedTypedArrayStructure(TypedArrayType type) const
    {
        return const_cast<const LazyProperty<JSGlobalObject, Structure>&>(const_cast<JSGlobalObject*>(this)->lazyResizableOrGrowableSharedTypedArrayStructure(type));
    }

    Structure* typedArrayStructure(TypedArrayType type, bool isResizableOrGrowableShared) const
    {
        if (isResizableOrGrowableShared)
            return lazyResizableOrGrowableSharedTypedArrayStructure(type).get(this);
        return lazyTypedArrayStructure(type).get(this);
    }
    Structure* typedArrayStructureConcurrently(TypedArrayType type, bool isResizableOrGrowableShared) const
    {
        if (isResizableOrGrowableShared)
            return lazyResizableOrGrowableSharedTypedArrayStructure(type).getConcurrently();
        return lazyTypedArrayStructure(type).getConcurrently();
    }
    bool isOriginalTypedArrayStructure(Structure* structure, bool isResizableOrGrowableShared)
    {
        TypedArrayType type = typedArrayType(structure->typeInfo().type());
        if (type == NotTypedArray)
            return false;
        return typedArrayStructureConcurrently(type, isResizableOrGrowableShared) == structure;
    }
    template<TypedArrayType type> Structure* typedArrayStructureWithTypedArrayType() const { return typedArrayStructure(type, /* isResizableOrGrowableShared */ false); }
    template<TypedArrayType type> Structure* resizableOrGrowableSharedTypedArrayStructureWithTypedArrayType() const { return typedArrayStructure(type, /* isResizableOrGrowableShared */ true); }

    JSObject* typedArrayConstructor(TypedArrayType type) const
    {
        return lazyTypedArrayStructure(type).constructor(this);
    }

    JSObject* typedArrayPrototype(TypedArrayType type) const
    {
        return lazyTypedArrayStructure(type).prototype(this);
    }

    JSCell* linkTimeConstant(LinkTimeConstant value) const
    {
        JSCell* result = m_linkTimeConstants[static_cast<unsigned>(value)].getInitializedOnMainThread(this);
        ASSERT(result);
        return result;
    }

    WatchpointSet& masqueradesAsUndefinedWatchpointSet() { return m_masqueradesAsUndefinedWatchpointSet.get(); }
    WatchpointSet& havingABadTimeWatchpointSet() { return m_havingABadTimeWatchpointSet.get(); }
    WatchpointSet& varInjectionWatchpointSet() { return m_varInjectionWatchpointSet.get(); }
    WatchpointSet& varReadOnlyWatchpointSet() { return m_varReadOnlyWatchpointSet.get(); }
    WatchpointSet& regExpRecompiledWatchpointSet() { return m_regExpRecompiledWatchpointSet.get(); }
        
    bool isHavingABadTime() const
    {
        return m_havingABadTimeWatchpointSet->hasBeenInvalidated();
    }
        
    void haveABadTime(VM&);
    void clearStructureCache(VM&);
        
    static bool objectPrototypeIsSaneConcurrently(Structure* objectPrototypeStructure);
    bool arrayPrototypeChainIsSaneConcurrently(Structure* arrayPrototypeStructure, Structure* objectPrototypeStructure);
    bool stringPrototypeChainIsSaneConcurrently(Structure* stringPrototypeStructure, Structure* objectPrototypeStructure);
    bool objectPrototypeChainIsSane();
    bool arrayPrototypeChainIsSane();
    bool stringPrototypeChainIsSane();

    bool isRegExpRecompiled() const
    {
        return m_regExpRecompiledWatchpointSet->hasBeenInvalidated();
    }

    void setProfileGroup(unsigned value) { createRareDataIfNeeded(); m_rareData->profileGroup = value; }
    unsigned profileGroup() const
    { 
        if (!m_rareData)
            return 0;
        return m_rareData->profileGroup;
    }

    Debugger* debugger() const { return m_debugger; }
    void setDebugger(Debugger*);

    const GlobalObjectMethodTable* globalObjectMethodTable() const { return m_globalObjectMethodTable; }

    static bool supportsRichSourceInfo(const JSGlobalObject*) { return true; }

    static JSGlobalObject* deriveShadowRealmGlobalObject(JSGlobalObject* globalObject)
    {
        auto& vm = globalObject->vm();
        JSGlobalObject* result = JSGlobalObject::createWithCustomMethodTable(vm, JSGlobalObject::createStructureForShadowRealm(vm, jsNull()), globalObject->globalObjectMethodTable());
        return result;
    }

    static bool shouldInterruptScript(const JSGlobalObject*) { return true; }
    static bool shouldInterruptScriptBeforeTimeout(const JSGlobalObject*) { return false; }
    static RuntimeFlags javaScriptRuntimeFlags(const JSGlobalObject*) { return RuntimeFlags(); }

    JS_EXPORT_PRIVATE void queueMicrotask(Ref<Microtask>&&);
    JS_EXPORT_PRIVATE void queueMicrotask(JSValue job, JSValue, JSValue, JSValue, JSValue);

    static void reportViolationForUnsafeEval(const JSGlobalObject*, JSString*) { }

    bool evalEnabled() const { return m_evalEnabled; }
    bool webAssemblyEnabled() const { return m_webAssemblyEnabled; }
    const String& evalDisabledErrorMessage() const { return m_evalDisabledErrorMessage; }
    const String& webAssemblyDisabledErrorMessage() const { return m_webAssemblyDisabledErrorMessage; }
    void setEvalEnabled(bool enabled, const String& errorMessage = String())
    {
        m_evalEnabled = enabled;
        m_evalDisabledErrorMessage = errorMessage;
    }
    void setWebAssemblyEnabled(bool enabled, const String& errorMessage = String())
    {
        m_webAssemblyEnabled = enabled;
        m_webAssemblyDisabledErrorMessage = errorMessage;
    }

#if ASSERT_ENABLED
    const JSGlobalObject* globalObjectAtDebuggerEntry() const { return m_globalObjectAtDebuggerEntry; }
    void setGlobalObjectAtDebuggerEntry(const JSGlobalObject* globalObject) { m_globalObjectAtDebuggerEntry = globalObject; }
#endif

    void resetPrototype(VM&, JSValue prototype);

    VM& vm() const { return *m_vm; }
    JSObject* globalThis() const;
    WriteBarrier<JSObject>* addressOfGlobalThis() { return &m_globalThis; }
    OptionSet<CodeGenerationMode> defaultCodeGenerationMode() const;

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        Structure* result = Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
        result->setTransitionWatchpointIsLikelyToBeFired(true);
        return result;
    }
    static Structure* createStructureForShadowRealm(VM& vm, JSValue prototype)
    {
        Structure* result = Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags & ~IsImmutablePrototypeExoticObject), info());
        result->setTransitionWatchpointIsLikelyToBeFired(true);
        return result;
    }

    void registerWeakMap(OpaqueJSWeakObjectMap* map)
    {
        createRareDataIfNeeded();
        m_rareData->weakMaps.add(map);
    }

    void unregisterWeakMap(OpaqueJSWeakObjectMap* map)
    {
        if (m_rareData)
            m_rareData->weakMaps.remove(map);
    }

    OpaqueJSClassDataMap& opaqueJSClassData()
    {
        createRareDataIfNeeded();
        return m_rareData->opaqueJSClassData;
    }

    static ptrdiff_t weakRandomOffset() { return OBJECT_OFFSETOF(JSGlobalObject, m_weakRandom); }
    double weakRandomNumber() { return m_weakRandom.get(); }
    unsigned weakRandomInteger() { return m_weakRandom.getUint32(); }
    WeakRandom& weakRandom() { return m_weakRandom; }

    bool needsSiteSpecificQuirks() const { return m_needsSiteSpecificQuirks; }
    JS_EXPORT_PRIVATE void exposeDollarVM(VM&);

#if JSC_OBJC_API_ENABLED
    JSWrapperMap* wrapperMap() const { return m_wrapperMap.get(); }
    void setWrapperMap(JSWrapperMap* map) { m_wrapperMap = map; }
#endif
#ifdef JSC_GLIB_API_ENABLED
    WrapperMap* wrapperMap() const { return m_wrapperMap.get(); }
    void setWrapperMap(std::unique_ptr<WrapperMap>&&);
#endif

    void installArraySpeciesWatchpoint();
    void installSaneChainWatchpoints();
    void installNumberPrototypeWatchpoint(NumberPrototype*);
    void installMapPrototypeWatchpoint(MapPrototype*);
    void installSetPrototypeWatchpoint(SetPrototype*);
    void tryInstallArrayBufferSpeciesWatchpoint(ArrayBufferSharingMode);
    void tryInstallTypedArraySpeciesWatchpoint(TypedArrayType);
    void installTypedArrayIteratorProtocolWatchpoint(JSObject* prototype, TypedArrayType);
    void installTypedArrayConstructorSpeciesWatchpoint(JSTypedArrayViewConstructor*);
    void installTypedArrayPrototypeIteratorProtocolWatchpoint(JSTypedArrayViewPrototype*);

    const ImportMap& importMap() const { return m_importMap.get(); }
    ImportMap& importMap() { return m_importMap.get(); }
    JSInternalPromise* importMapStatusPromise() const
    {
        if (m_importMapStatusPromise.isInitialized())
            return m_importMapStatusPromise.get(this);
        return nullptr;
    }
    JS_EXPORT_PRIVATE void registerImportMap() const;
    JS_EXPORT_PRIVATE bool isAcquiringImportMaps() const;
    JS_EXPORT_PRIVATE void setAcquiringImportMaps();
    JS_EXPORT_PRIVATE void setPendingImportMaps();
    JS_EXPORT_PRIVATE void clearPendingImportMaps();

protected:
    enum class HasSpeciesProperty : bool { Yes, No };
    template<typename SpeciesWatchpoint>
    void tryInstallSpeciesWatchpoint(JSObject* prototype, JSObject* constructor, std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>& constructorWatchpoint, std::unique_ptr<SpeciesWatchpoint>&, InlineWatchpointSet&, HasSpeciesProperty);

    struct GlobalPropertyInfo {
        GlobalPropertyInfo(const Identifier& i, JSValue v, unsigned a)
            : identifier(i)
            , value(v)
            , attributes(a)
        {
            ASSERT(Thread::current().stack().contains(this));
        }

        const Identifier identifier;
        JSValue value;
        unsigned attributes;
    };
    JS_EXPORT_PRIVATE void addStaticGlobals(GlobalPropertyInfo*, int count);

    void setNeedsSiteSpecificQuirks(bool needQuirks) { m_needsSiteSpecificQuirks = needQuirks; }

private:
    friend class LLIntOffsetsExtractor;

    void fireWatchpointAndMakeAllArrayStructuresSlowPut(VM&);
    void setGlobalThis(VM&, JSObject* globalThis);

    template<ErrorType errorType>
    void initializeErrorConstructor(LazyClassStructure::Initializer&);

    void initializeAggregateErrorConstructor(LazyClassStructure::Initializer&);

    JS_EXPORT_PRIVATE void init(VM&);
    void initStaticGlobals(VM&);
    void fixupPrototypeChainWithObjectPrototype(VM&);

    JS_EXPORT_PRIVATE static void clearRareData(JSCell*);

#if JSC_OBJC_API_ENABLED
    RetainPtr<JSWrapperMap> m_wrapperMap;
#endif
#ifdef JSC_GLIB_API_ENABLED
    std::unique_ptr<WrapperMap> m_wrapperMap;
#endif
};

inline JSArray* constructEmptyArray(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, unsigned initialLength = 0, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure;
    if (initialLength >= MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)
        structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(globalObject, ArrayWithArrayStorage, newTarget);
    else
        structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSArray* result = JSArray::tryCreate(vm, structure, initialLength);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    return ArrayAllocationProfile::updateLastAllocationFor(profile, result);
}

inline JSArray* constructArray(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const ArgList& values, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArray(globalObject, structure, values));
}

inline JSArray* constructArray(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const JSValue* values, unsigned length, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArray(globalObject, structure, values, length));
}

inline JSArray* constructArrayNegativeIndexed(JSGlobalObject* globalObject, ArrayAllocationProfile* profile, const JSValue* values, unsigned length, JSValue newTarget = JSValue())
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    Structure* structure = globalObject->arrayStructureForProfileDuringAllocation(globalObject, profile, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArrayNegativeIndexed(globalObject, structure, values, length));
}

inline JSObject* JSScope::globalThis()
{ 
    return globalObject()->globalThis();
}

inline JSObject* JSGlobalObject::globalThis() const
{ 
    return m_globalThis.get();
}

inline OptionSet<CodeGenerationMode> JSGlobalObject::defaultCodeGenerationMode() const
{
    OptionSet<CodeGenerationMode> codeGenerationMode;
    if (hasInteractiveDebugger() || Options::forceDebuggerBytecodeGeneration() || Options::debuggerTriggersBreakpointException())
        codeGenerationMode.add(CodeGenerationMode::Debugger);
    if (vm().typeProfiler())
        codeGenerationMode.add(CodeGenerationMode::TypeProfiler);
    if (vm().controlFlowProfiler())
        codeGenerationMode.add(CodeGenerationMode::ControlFlowProfiler);
    return codeGenerationMode;
}

} // namespace JSC
