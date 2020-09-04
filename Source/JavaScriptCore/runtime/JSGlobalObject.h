/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
#include "JSArrayBufferPrototype.h"
#include "JSClassRef.h"
#include "JSGlobalLexicalEnvironment.h"
#include "JSPromise.h"
#include "JSSegmentedVariableObject.h"
#include "JSWeakObjectMapRefInternal.h"
#include "LazyProperty.h"
#include "LazyClassStructure.h"
#include "NumberPrototype.h"
#include "ParserModes.h"
#include "RegExpGlobalData.h"
#include "RuntimeFlags.h"
#include "StringPrototype.h"
#include "SymbolPrototype.h"
#include "VM.h"
#include "Watchpoint.h"
#include <JavaScriptCore/JSBase.h>
#include <array>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>

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
class IndirectEvalExecutable;
class InputCursor;
class IntlObject;
class IntlCollator;
class JSArrayBuffer;
class JSArrayBufferPrototype;
class JSCallee;
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
class RegExpConstructor;
class RegExpPrototype;
class SetIteratorPrototype;
class SetPrototype;
class SourceCode;
class SourceOrigin;
class UnlinkedModuleProgramCodeBlock;
class VariableEnvironment;
struct ActivationStackNode;
struct HashTable;
enum class LinkTimeConstant : int32_t;

#ifdef JSC_GLIB_API_ENABLED
class WrapperMap;
#endif

template<typename Watchpoint> class ObjectPropertyChangeAdaptiveWatchpoint;

constexpr bool typeExposedByDefault = true;

#define DEFINE_STANDARD_BUILTIN(macro, upperName, lowerName) macro(upperName, lowerName, lowerName, JS ## upperName, upperName, object, typeExposedByDefault)

#define FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(macro) \
    macro(String, string, stringObject, StringObject, String, object, typeExposedByDefault) \
    macro(JSPromise, promise, promise, JSPromise, Promise, object, typeExposedByDefault) \
    macro(BigInt, bigInt, bigIntObject, BigIntObject, BigInt, object, Options::useBigInt()) \
    macro(WeakObjectRef, weakObjectRef, weakObjectRef, JSWeakObjectRef, WeakRef, object, Options::useWeakRefs()) \
    macro(FinalizationRegistry, finalizationRegistry, finalizationRegistry, JSFinalizationRegistry, FinalizationRegistry, object, Options::useWeakRefs()) \


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
    macro(WebAssemblyCompileError, webAssemblyCompileError, webAssemblyCompileError, JSWebAssemblyCompileError, CompileError, error, typeExposedByDefault) \
    macro(WebAssemblyGlobal,       webAssemblyGlobal,       webAssemblyGlobal,       JSWebAssemblyGlobal,       Global,       object, typeExposedByDefault) \
    macro(WebAssemblyInstance,     webAssemblyInstance,     webAssemblyInstance,     JSWebAssemblyInstance,     Instance,     object, typeExposedByDefault) \
    macro(WebAssemblyLinkError,    webAssemblyLinkError,    webAssemblyLinkError,    JSWebAssemblyLinkError,    LinkError,    error, typeExposedByDefault) \
    macro(WebAssemblyMemory,       webAssemblyMemory,       webAssemblyMemory,       JSWebAssemblyMemory,       Memory,       object, typeExposedByDefault) \
    macro(WebAssemblyModule,       webAssemblyModule,       webAssemblyModule,       JSWebAssemblyModule,       Module,       object, typeExposedByDefault) \
    macro(WebAssemblyRuntimeError, webAssemblyRuntimeError, webAssemblyRuntimeError, JSWebAssemblyRuntimeError, RuntimeError, error, typeExposedByDefault) \
    macro(WebAssemblyTable,        webAssemblyTable,        webAssemblyTable,        JSWebAssemblyTable,        Table,        object, typeExposedByDefault)
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

struct GlobalObjectMethodTable {
    typedef bool (*SupportsRichSourceInfoFunctionPtr)(const JSGlobalObject*);
    SupportsRichSourceInfoFunctionPtr supportsRichSourceInfo;

    typedef bool (*ShouldInterruptScriptFunctionPtr)(const JSGlobalObject*);
    ShouldInterruptScriptFunctionPtr shouldInterruptScript;

    typedef RuntimeFlags (*JavaScriptRuntimeFlagsFunctionPtr)(const JSGlobalObject*);
    JavaScriptRuntimeFlagsFunctionPtr javaScriptRuntimeFlags;

    typedef void (*QueueTaskToEventLoopFunctionPtr)(JSGlobalObject&, Ref<Microtask>&&);
    QueueTaskToEventLoopFunctionPtr queueTaskToEventLoop;

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

    typedef JSValue (*ModuleLoaderEvaluatePtr)(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, JSValue);
    ModuleLoaderEvaluatePtr moduleLoaderEvaluate;

    typedef void (*PromiseRejectionTrackerPtr)(JSGlobalObject*, JSPromise*, JSPromiseRejectionOperation);
    PromiseRejectionTrackerPtr promiseRejectionTracker;

    typedef void (*ReportUncaughtExceptionAtEventLoopPtr)(JSGlobalObject*, Exception*);
    ReportUncaughtExceptionAtEventLoopPtr reportUncaughtExceptionAtEventLoop;

    typedef String (*DefaultLanguageFunctionPtr)();
    DefaultLanguageFunctionPtr defaultLanguage;

    typedef void (*CompileStreamingPtr)(JSGlobalObject*, JSPromise*, JSValue);
    CompileStreamingPtr compileStreaming;

    typedef void (*InstantiateStreamingPtr)(JSGlobalObject*, JSPromise*, JSValue, JSObject*);
    InstantiateStreamingPtr instantiateStreaming;
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
    VM* m_vm;

// Our hashtable code-generator tries to access these properties, so we make them public.
// However, we'd like it better if they could be protected.
public:
    template<typename T> using Initializer = typename LazyProperty<JSGlobalObject, T>::Initializer;

    Register m_deprecatedCallFrameForDebugger[CallFrame::headerSizeInRegisters];
    
    WriteBarrier<JSObject> m_globalThis;

    WriteBarrier<JSGlobalLexicalEnvironment> m_globalLexicalEnvironment;
    WriteBarrier<JSScope> m_globalScopeExtension;
    WriteBarrier<JSCallee> m_globalCallee;
    WriteBarrier<JSCallee> m_stackOverflowFrameCallee;

    LazyClassStructure m_evalErrorStructure;
    LazyClassStructure m_rangeErrorStructure;
    LazyClassStructure m_referenceErrorStructure;
    LazyClassStructure m_syntaxErrorStructure;
    LazyClassStructure m_typeErrorStructure;
    LazyClassStructure m_URIErrorStructure;
    LazyClassStructure m_aggregateErrorStructure;

    WriteBarrier<ObjectConstructor> m_objectConstructor;
    WriteBarrier<ArrayConstructor> m_arrayConstructor;
    WriteBarrier<RegExpConstructor> m_regExpConstructor;
    WriteBarrier<FunctionConstructor> m_functionConstructor;
    WriteBarrier<JSPromiseConstructor> m_promiseConstructor;
    WriteBarrier<JSInternalPromiseConstructor> m_internalPromiseConstructor;

    LazyProperty<JSGlobalObject, IntlCollator> m_defaultCollator;
    LazyProperty<JSGlobalObject, Structure> m_collatorStructure;
    LazyProperty<JSGlobalObject, Structure> m_dateTimeFormatStructure;
    LazyProperty<JSGlobalObject, Structure> m_displayNamesStructure;
    LazyProperty<JSGlobalObject, Structure> m_localeStructure;
    LazyProperty<JSGlobalObject, Structure> m_numberFormatStructure;
    LazyProperty<JSGlobalObject, Structure> m_pluralRulesStructure;
    LazyProperty<JSGlobalObject, Structure> m_relativeTimeFormatStructure;
    LazyProperty<JSGlobalObject, Structure> m_segmentIteratorStructure;
    LazyProperty<JSGlobalObject, Structure> m_segmenterStructure;
    LazyProperty<JSGlobalObject, Structure> m_segmentsStructure;

    WriteBarrier<NullGetterFunction> m_nullGetterFunction;
    WriteBarrier<NullSetterFunction> m_nullSetterFunction;
    WriteBarrier<NullSetterFunction> m_nullSetterStrictFunction;

    LazyProperty<JSGlobalObject, JSFunction> m_parseIntFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_parseFloatFunction;

    LazyProperty<JSGlobalObject, JSFunction> m_objectProtoToStringFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_arrayProtoToStringFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_arrayProtoValuesFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_evalFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_iteratorProtocolFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_promiseResolveFunction;
    LazyProperty<JSGlobalObject, JSFunction> m_numberProtoToStringFunction;
    WriteBarrier<JSFunction> m_objectProtoValueOfFunction;
    WriteBarrier<JSFunction> m_functionProtoHasInstanceSymbolFunction;
    LazyProperty<JSGlobalObject, GetterSetter> m_throwTypeErrorGetterSetter;
    WriteBarrier<JSObject> m_regExpProtoSymbolReplace;
    WriteBarrier<GetterSetter> m_throwTypeErrorArgumentsCalleeAndCallerGetterSetter;

    LazyProperty<JSGlobalObject, JSModuleLoader> m_moduleLoader;

    WriteBarrier<ObjectPrototype> m_objectPrototype;
    WriteBarrier<FunctionPrototype> m_functionPrototype;
    WriteBarrier<ArrayPrototype> m_arrayPrototype;
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
    WriteBarrier<Structure> m_lexicalEnvironmentStructure;
    LazyProperty<JSGlobalObject, Structure> m_moduleEnvironmentStructure;
    WriteBarrier<Structure> m_directArgumentsStructure;
    WriteBarrier<Structure> m_scopedArgumentsStructure;
    WriteBarrier<Structure> m_clonedArgumentsStructure;

    WriteBarrier<Structure> m_objectStructureForObjectConstructor;
        
    // Lists the actual structures used for having these particular indexing shapes.
    WriteBarrier<Structure> m_originalArrayStructureForIndexingShape[NumberOfArrayIndexingModes];
    // Lists the structures we should use during allocation for these particular indexing shapes.
    // These structures will differ from the originals list above when we are having a bad time.
    WriteBarrier<Structure> m_arrayStructureForIndexingShapeDuringAllocation[NumberOfArrayIndexingModes];

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
    WriteBarrier<Structure> m_nullPrototypeObjectStructure;
    WriteBarrier<Structure> m_calleeStructure;

    WriteBarrier<Structure> m_hostFunctionStructure;

    struct FunctionStructures {
        WriteBarrier<Structure> arrowFunctionStructure;
        WriteBarrier<Structure> sloppyFunctionStructure;
        WriteBarrier<Structure> strictFunctionStructure;
    };
    FunctionStructures m_builtinFunctions;
    FunctionStructures m_ordinaryFunctions;

    LazyProperty<JSGlobalObject, Structure> m_boundFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_customGetterSetterFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_nativeStdFunctionStructure;
    PropertyOffset m_functionNameOffset;
    WriteBarrier<Structure> m_regExpStructure;
    WriteBarrier<AsyncFunctionPrototype> m_asyncFunctionPrototype;
    WriteBarrier<AsyncGeneratorFunctionPrototype> m_asyncGeneratorFunctionPrototype;
    WriteBarrier<Structure> m_asyncFunctionStructure;
    WriteBarrier<Structure> m_asyncGeneratorFunctionStructure;
    WriteBarrier<Structure> m_generatorFunctionStructure;
    WriteBarrier<Structure> m_generatorStructure;
    WriteBarrier<Structure> m_asyncGeneratorStructure;
    WriteBarrier<Structure> m_arrayIteratorStructure;
    WriteBarrier<Structure> m_mapIteratorStructure;
    WriteBarrier<Structure> m_setIteratorStructure;
    LazyProperty<JSGlobalObject, Structure> m_iteratorResultObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_dataPropertyDescriptorObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_accessorPropertyDescriptorObjectStructure;
    WriteBarrier<Structure> m_regExpMatchesArrayStructure;
    LazyProperty<JSGlobalObject, Structure> m_moduleRecordStructure;
    LazyProperty<JSGlobalObject, Structure> m_moduleNamespaceObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_proxyObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_callableProxyObjectStructure;
    LazyProperty<JSGlobalObject, Structure> m_proxyRevokeStructure;
#if ENABLE(SHARED_ARRAY_BUFFER)
    WriteBarrier<JSArrayBufferPrototype> m_sharedArrayBufferPrototype;
    WriteBarrier<Structure> m_sharedArrayBufferStructure;
#endif

#define DEFINE_STORAGE_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    WriteBarrier<capitalName ## Prototype> m_ ## lowerName ## Prototype; \
    WriteBarrier<Structure> m_ ## properName ## Structure;

#define DEFINE_STORAGE_FOR_LAZY_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    LazyClassStructure m_ ## properName ## Structure;

    FOR_EACH_SIMPLE_BUILTIN_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE)
    
#if ENABLE(WEBASSEMBLY)
    LazyProperty<JSGlobalObject, Structure> m_webAssemblyModuleRecordStructure;
    LazyProperty<JSGlobalObject, Structure> m_webAssemblyFunctionStructure;
    LazyProperty<JSGlobalObject, Structure> m_jsToWasmICCalleeStructure;
    LazyProperty<JSGlobalObject, Structure> m_webAssemblyWrapperFunctionStructure;
    FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(DEFINE_STORAGE_FOR_LAZY_TYPE)
#endif // ENABLE(WEBASSEMBLY)

    FOR_EACH_LAZY_BUILTIN_TYPE(DEFINE_STORAGE_FOR_LAZY_TYPE)

#undef DEFINE_STORAGE_FOR_SIMPLE_TYPE
#undef DEFINE_STORAGE_FOR_LAZY_TYPE

    WriteBarrier<GetterSetter> m_speciesGetterSetter;
    
    LazyProperty<JSGlobalObject, JSTypedArrayViewPrototype> m_typedArrayProto;
    LazyProperty<JSGlobalObject, JSTypedArrayViewConstructor> m_typedArraySuperConstructor;
    
#define DECLARE_TYPED_ARRAY_TYPE_STRUCTURE(name) LazyClassStructure m_typedArray ## name;
    FOR_EACH_TYPED_ARRAY_TYPE(DECLARE_TYPED_ARRAY_TYPE_STRUCTURE)
#undef DECLARE_TYPED_ARRAY_TYPE_STRUCTURE

    Vector<LazyProperty<JSGlobalObject, JSCell>> m_linkTimeConstants;

    String m_name;

    Strong<JSObject> m_unhandledRejectionCallback;

    Debugger* m_debugger;

#if ENABLE(REMOTE_INSPECTOR)
    std::unique_ptr<Inspector::JSGlobalObjectInspectorController> m_inspectorController;
    std::unique_ptr<JSGlobalObjectDebuggable> m_inspectorDebuggable;
#endif

    RefPtr<WatchpointSet> m_masqueradesAsUndefinedWatchpoint;
    RefPtr<WatchpointSet> m_havingABadTimeWatchpoint;
    RefPtr<WatchpointSet> m_varInjectionWatchpoint;

    std::unique_ptr<JSGlobalObjectRareData> m_rareData;

    WeakRandom m_weakRandom;
    RegExpGlobalData m_regExpGlobalData;

    // If this hasn't been invalidated, it means the array iterator protocol
    // is not observable to user code yet.
    InlineWatchpointSet m_arrayIteratorProtocolWatchpointSet;
    InlineWatchpointSet m_mapIteratorProtocolWatchpointSet;
    InlineWatchpointSet m_setIteratorProtocolWatchpointSet;
    InlineWatchpointSet m_stringIteratorProtocolWatchpointSet;
    InlineWatchpointSet m_mapSetWatchpointSet;
    InlineWatchpointSet m_setAddWatchpointSet;
    InlineWatchpointSet m_arraySpeciesWatchpointSet;
    InlineWatchpointSet m_arrayJoinWatchpointSet;
    InlineWatchpointSet m_numberToStringWatchpointSet;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayConstructorSpeciesWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayPrototypeConstructorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayPrototypeJoinWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_arrayIteratorPrototypeNext;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_mapPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_mapIteratorPrototypeNextWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_setPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_setIteratorPrototypeNextWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_stringPrototypeSymbolIteratorWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_stringIteratorPrototypeNextWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_mapPrototypeSetWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_setPrototypeAddWatchpoint;
    std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>> m_numberPrototypeToStringWatchpoint;

public:
    JSCallee* stackOverflowFrameCallee() const { return m_stackOverflowFrameCallee.get(); }

    InlineWatchpointSet& arrayIteratorProtocolWatchpointSet() { return m_arrayIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& mapIteratorProtocolWatchpointSet() { return m_mapIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& setIteratorProtocolWatchpointSet() { return m_setIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& stringIteratorProtocolWatchpointSet() { return m_stringIteratorProtocolWatchpointSet; }
    InlineWatchpointSet& mapSetWatchpointSet() { return m_mapSetWatchpointSet; }
    InlineWatchpointSet& setAddWatchpointSet() { return m_setAddWatchpointSet; }
    InlineWatchpointSet& arraySpeciesWatchpointSet() { return m_arraySpeciesWatchpointSet; }
    InlineWatchpointSet& arrayJoinWatchpointSet() { return m_arrayJoinWatchpointSet; }
    InlineWatchpointSet& numberToStringWatchpointSet()
    {
        RELEASE_ASSERT(Options::useJIT());
        return m_numberToStringWatchpointSet;
    }

    bool isArrayPrototypeIteratorProtocolFastAndNonObservable();
    bool isMapPrototypeIteratorProtocolFastAndNonObservable();
    bool isSetPrototypeIteratorProtocolFastAndNonObservable();
    bool isStringPrototypeIteratorProtocolFastAndNonObservable();
    bool isMapPrototypeSetFastAndNonObservable();
    bool isSetPrototypeAddFastAndNonObservable();

#if ENABLE(DFG_JIT)
    using ReferencedGlobalPropertyWatchpointSets = HashMap<RefPtr<UniquedStringImpl>, Ref<WatchpointSet>, IdentifierRepHash>;
    ReferencedGlobalPropertyWatchpointSets m_referencedGlobalPropertyWatchpointSets;
    ConcurrentJSLock m_referencedGlobalPropertyWatchpointSetsLock;
#endif

    bool m_evalEnabled { true };
    bool m_webAssemblyEnabled { true };
    unsigned m_globalLexicalBindingEpoch { 1 };
    String m_evalDisabledErrorMessage;
    String m_webAssemblyDisabledErrorMessage;
    RuntimeFlags m_runtimeFlags;
    ConsoleClient* m_consoleClient { nullptr };
    Optional<unsigned> m_stackTraceLimit;

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
    // Do we realy need OverridesAnyFormOfGetPropertyNames here?
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=212954
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable | OverridesGetOwnPropertySlot | OverridesAnyFormOfGetPropertyNames | IsImmutablePrototypeExoticObject;

    static constexpr bool needsDestruction = true;
    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.globalObjectSpace<mode>();
    }

    JS_EXPORT_PRIVATE static JSGlobalObject* create(VM&, Structure*);

    DECLARE_EXPORT_INFO;

    bool hasDebugger() const;
    bool hasInteractiveDebugger() const;
    const RuntimeFlags& runtimeFlags() const { return m_runtimeFlags; }

#if ENABLE(DFG_JIT)
    WatchpointSet* getReferencedPropertyWatchpointSet(UniquedStringImpl*);
    WatchpointSet& ensureReferencedPropertyWatchpointSet(UniquedStringImpl*);
#endif

    Optional<unsigned> stackTraceLimit() const { return m_stackTraceLimit; }
    void setStackTraceLimit(Optional<unsigned> value) { m_stackTraceLimit = value; }

protected:
    JS_EXPORT_PRIVATE explicit JSGlobalObject(VM&, Structure*, const GlobalObjectMethodTable* = nullptr);

    JS_EXPORT_PRIVATE void finishCreation(VM&);

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSObject*);

    void addGlobalVar(const Identifier&);

public:
    JS_EXPORT_PRIVATE ~JSGlobalObject();
    JS_EXPORT_PRIVATE static void destroy(JSCell*);

    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

    JS_EXPORT_PRIVATE static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    JS_EXPORT_PRIVATE static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    JS_EXPORT_PRIVATE static void defineGetter(JSObject*, JSGlobalObject*, PropertyName, JSObject* getterFunc, unsigned attributes);
    JS_EXPORT_PRIVATE static void defineSetter(JSObject*, JSGlobalObject*, PropertyName, JSObject* setterFunc, unsigned attributes);
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
    JSFunction* arrayProtoToStringFunction() const { return m_arrayProtoToStringFunction.get(this); }
    JSFunction* arrayProtoValuesFunction() const { return m_arrayProtoValuesFunction.get(this); }
    JSFunction* arrayProtoValuesFunctionConcurrently() const { return m_arrayProtoValuesFunction.getConcurrently(); }
    JSFunction* iteratorProtocolFunction() const { return m_iteratorProtocolFunction.get(this); }
    JSFunction* newPromiseCapabilityFunction() const;
    JSFunction* promiseResolveFunction() const { return m_promiseResolveFunction.get(this); }
    JSFunction* resolvePromiseFunction() const;
    JSFunction* rejectPromiseFunction() const;
    JSFunction* promiseProtoThenFunction() const;
    JSFunction* objectProtoValueOfFunction() const { return m_objectProtoValueOfFunction.get(); }
    JSFunction* numberProtoToStringFunction() const { return m_numberProtoToStringFunction.getInitializedOnMainThread(this); }
    JSFunction* functionProtoHasInstanceSymbolFunction() const { return m_functionProtoHasInstanceSymbolFunction.get(); }
    JSFunction* regExpProtoExecFunction() const;
    JSObject* regExpProtoSymbolReplaceFunction() const { return m_regExpProtoSymbolReplace.get(); }
    GetterSetter* regExpProtoGlobalGetter() const;
    GetterSetter* regExpProtoUnicodeGetter() const;
    GetterSetter* throwTypeErrorArgumentsCalleeAndCallerGetterSetter()
    {
        return m_throwTypeErrorArgumentsCalleeAndCallerGetterSetter.get();
    }
    
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
    Structure* customGetterSetterFunctionStructure() const { return m_customGetterSetterFunctionStructure.get(this); }
    Structure* nativeStdFunctionStructure() const { return m_nativeStdFunctionStructure.get(this); }
    PropertyOffset functionNameOffset() const { return m_functionNameOffset; }
    Structure* numberObjectStructure() const { return m_numberObjectStructure.get(this); }
    Structure* regExpStructure() const { return m_regExpStructure.get(); }
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
    Structure* moduleRecordStructure() const { return m_moduleRecordStructure.get(this); }
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
    Structure* numberFormatStructure() { return m_numberFormatStructure.get(this); }
    Structure* localeStructure() { return m_localeStructure.get(this); }
    Structure* pluralRulesStructure() { return m_pluralRulesStructure.get(this); }
    Structure* relativeTimeFormatStructure() { return m_relativeTimeFormatStructure.get(this); }
    Structure* segmentIteratorStructure() { return m_segmentIteratorStructure.get(this); }
    Structure* segmenterStructure() { return m_segmenterStructure.get(this); }
    Structure* segmentsStructure() { return m_segmentsStructure.get(this); }

    JS_EXPORT_PRIVATE void setRemoteDebuggingEnabled(bool);
    JS_EXPORT_PRIVATE bool remoteDebuggingEnabled() const;

    void setIsITML();

    RegExpGlobalData& regExpGlobalData() { return m_regExpGlobalData; }
    static ptrdiff_t regExpGlobalDataOffset() { return OBJECT_OFFSETOF(JSGlobalObject, m_regExpGlobalData); }

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::JSGlobalObjectInspectorController& inspectorController() const { return *m_inspectorController.get(); }
    JSGlobalObjectDebuggable& inspectorDebuggable() { return *m_inspectorDebuggable.get(); }
#endif

    void bumpGlobalLexicalBindingEpoch(VM&);
    unsigned globalLexicalBindingEpoch() const { return m_globalLexicalBindingEpoch; }
    static ptrdiff_t globalLexicalBindingEpochOffset() { return OBJECT_OFFSETOF(JSGlobalObject, m_globalLexicalBindingEpoch); }
    unsigned* addressOfGlobalLexicalBindingEpoch() { return &m_globalLexicalBindingEpoch; }

    void setConsoleClient(ConsoleClient* consoleClient) { m_consoleClient = consoleClient; }
    ConsoleClient* consoleClient() const { return m_consoleClient; }

    void setName(const String&);
    const String& name() const { return m_name; }

    void setUnhandledRejectionCallback(VM& vm, JSObject* function) { m_unhandledRejectionCallback.set(vm, function); }
    JSObject* unhandledRejectionCallback() const { return m_unhandledRejectionCallback.get(); }

    static void reportUncaughtExceptionAtEventLoop(JSGlobalObject*, Exception*);

    JSObject* arrayBufferConstructor() const { return m_arrayBufferStructure.constructor(this); }

    JSObject* arrayBufferPrototype(ArrayBufferSharingMode sharingMode) const
    {
        switch (sharingMode) {
        case ArrayBufferSharingMode::Default:
            return m_arrayBufferStructure.prototype(this);
#if ENABLE(SHARED_ARRAY_BUFFER)
        case ArrayBufferSharingMode::Shared:
            return m_sharedArrayBufferPrototype.get();
#else
        default:
            return m_arrayBufferStructure.prototype(this);
#endif
        }
    }
    Structure* arrayBufferStructure(ArrayBufferSharingMode sharingMode) const
    {
        switch (sharingMode) {
        case ArrayBufferSharingMode::Default:
            return m_arrayBufferStructure.get(this);
#if ENABLE(SHARED_ARRAY_BUFFER)
        case ArrayBufferSharingMode::Shared:
            return m_sharedArrayBufferStructure.get();
#else
        default:
            return m_arrayBufferStructure.get(this);
#endif
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
    
    Structure* typedArrayStructure(TypedArrayType type) const
    {
        return lazyTypedArrayStructure(type).get(this);
    }
    Structure* typedArrayStructureConcurrently(TypedArrayType type) const
    {
        return lazyTypedArrayStructure(type).getConcurrently();
    }
    bool isOriginalTypedArrayStructure(Structure* structure)
    {
        TypedArrayType type = structure->classInfo()->typedArrayStorageType;
        if (type == NotTypedArray)
            return false;
        return typedArrayStructureConcurrently(type) == structure;
    }

    JSObject* typedArrayConstructor(TypedArrayType type) const
    {
        return lazyTypedArrayStructure(type).constructor(this);
    }

    JSCell* linkTimeConstant(LinkTimeConstant value) const
    {
        JSCell* result = m_linkTimeConstants[static_cast<unsigned>(value)].getInitializedOnMainThread(this);
        ASSERT(result);
        return result;
    }

    WatchpointSet* masqueradesAsUndefinedWatchpoint() { return m_masqueradesAsUndefinedWatchpoint.get(); }
    WatchpointSet* havingABadTimeWatchpoint() { return m_havingABadTimeWatchpoint.get(); }
    WatchpointSet* varInjectionWatchpoint() { return m_varInjectionWatchpoint.get(); }
        
    bool isHavingABadTime() const
    {
        return m_havingABadTimeWatchpoint->hasBeenInvalidated();
    }
        
    void haveABadTime(VM&);
        
    bool objectPrototypeIsSane();
    bool arrayPrototypeChainIsSane();
    bool stringPrototypeChainIsSane();

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

    JS_EXPORT_PRIVATE CallFrame* deprecatedCallFrameForDebugger();

    static bool supportsRichSourceInfo(const JSGlobalObject*) { return true; }

    static bool shouldInterruptScript(const JSGlobalObject*) { return true; }
    static bool shouldInterruptScriptBeforeTimeout(const JSGlobalObject*) { return false; }
    static RuntimeFlags javaScriptRuntimeFlags(const JSGlobalObject*) { return RuntimeFlags(); }

    JS_EXPORT_PRIVATE void queueMicrotask(Ref<Microtask>&&);

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

    void tryInstallArraySpeciesWatchpoint();
    void installNumberPrototypeWatchpoint(NumberPrototype*);
    void installMapPrototypeWatchpoint(MapPrototype*);
    void installSetPrototypeWatchpoint(SetPrototype*);

protected:
    struct GlobalPropertyInfo {
        GlobalPropertyInfo(const Identifier& i, JSValue v, unsigned a)
            : identifier(i)
            , value(v)
            , attributes(a)
        {
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
    void fixupPrototypeChainWithObjectPrototype(VM&);

    JS_EXPORT_PRIVATE static void clearRareData(JSCell*);

    bool m_needsSiteSpecificQuirks { false };
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
    if (hasInteractiveDebugger() || Options::forceDebuggerBytecodeGeneration())
        codeGenerationMode.add(CodeGenerationMode::Debugger);
    if (vm().typeProfiler())
        codeGenerationMode.add(CodeGenerationMode::TypeProfiler);
    if (vm().controlFlowProfiler())
        codeGenerationMode.add(CodeGenerationMode::ControlFlowProfiler);
    return codeGenerationMode;
}

} // namespace JSC
