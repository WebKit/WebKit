/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007, 2008, 2009, 2014, 2015 Apple Inc. All rights reserved.
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

#ifndef JSGlobalObject_h
#define JSGlobalObject_h

#include "ArrayAllocationProfile.h"
#include "JSArray.h"
#include "JSArrayBufferPrototype.h"
#include "JSClassRef.h"
#include "JSGlobalLexicalEnvironment.h"
#include "JSProxy.h"
#include "JSSegmentedVariableObject.h"
#include "JSWeakObjectMapRefInternal.h"
#include "NumberPrototype.h"
#include "RuntimeFlags.h"
#include "SpecialPointer.h"
#include "StringPrototype.h"
#include "SymbolPrototype.h"
#include "TemplateRegistry.h"
#include "VM.h"
#include "Watchpoint.h"
#include <JavaScriptCore/JSBase.h>
#include <array>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>

struct OpaqueJSClass;
struct OpaqueJSClassContextData;

namespace Inspector {
class JSGlobalObjectInspectorController;
}

namespace JSC {

class ArrayPrototype;
class BooleanPrototype;
class ConsoleClient;
class Debugger;
class ErrorConstructor;
class ErrorPrototype;
class EvalCodeBlock;
class EvalExecutable;
class FunctionCodeBlock;
class FunctionExecutable;
class FunctionPrototype;
class GeneratorPrototype;
class GeneratorFunctionPrototype;
class GetterSetter;
class GlobalCodeBlock;
class InputCursor;
class JSGlobalObjectDebuggable;
class JSInternalPromise;
class JSPromise;
class JSPromiseConstructor;
class JSPromisePrototype;
class JSStack;
class LLIntOffsetsExtractor;
class Microtask;
class ModuleLoaderObject;
class ModuleProgramExecutable;
class NativeErrorConstructor;
class NullGetterFunction;
class NullSetterFunction;
class ObjectConstructor;
class ProgramCodeBlock;
class ProgramExecutable;
class RegExpConstructor;
class RegExpPrototype;
class SourceCode;
class UnlinkedModuleProgramCodeBlock;
class VariableEnvironment;
enum class ThisTDZMode;
struct ActivationStackNode;
struct HashTable;

#define DEFINE_STANDARD_BUILTIN(macro, upperName, lowerName) macro(upperName, lowerName, lowerName, JS ## upperName, upperName)

#define FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(macro) \
    macro(Set, set, set, JSSet, Set) \
    macro(Map, map, map, JSMap, Map) \
    macro(Date, date, date, DateInstance, Date) \
    macro(String, string, stringObject, StringObject, String) \
    macro(Symbol, symbol, symbolObject, SymbolObject, Symbol) \
    macro(Boolean, boolean, booleanObject, BooleanObject, Boolean) \
    macro(Number, number, numberObject, NumberObject, Number) \
    macro(Error, error, error, ErrorInstance, Error) \
    macro(JSPromise, promise, promise, JSPromise, Promise) \
    macro(JSArrayBuffer, arrayBuffer, arrayBuffer, JSArrayBuffer, ArrayBuffer) \
    DEFINE_STANDARD_BUILTIN(macro, WeakMap, weakMap) \
    DEFINE_STANDARD_BUILTIN(macro, WeakSet, weakSet) \

#define FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(macro) \
    DEFINE_STANDARD_BUILTIN(macro, ArrayIterator, arrayIterator) \
    DEFINE_STANDARD_BUILTIN(macro, MapIterator, mapIterator) \
    DEFINE_STANDARD_BUILTIN(macro, SetIterator, setIterator) \
    DEFINE_STANDARD_BUILTIN(macro, StringIterator, stringIterator) \

#define FOR_EACH_BUILTIN_ITERATOR_TYPE(macro) \
    DEFINE_STANDARD_BUILTIN(macro, Iterator, iterator) \
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(macro) \

#define FOR_EACH_SIMPLE_BUILTIN_TYPE(macro) \
    FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(macro) \
    macro(JSInternalPromise, internalPromise, internalPromise, JSInternalPromise, InternalPromise) \

#define DECLARE_SIMPLE_BUILTIN_TYPE(capitalName, lowerName, properName, instanceType, jsName) \
    class JS ## capitalName; \
    class capitalName ## Prototype; \
    class capitalName ## Constructor;

class IteratorPrototype;
FOR_EACH_SIMPLE_BUILTIN_TYPE(DECLARE_SIMPLE_BUILTIN_TYPE)
FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DECLARE_SIMPLE_BUILTIN_TYPE)

#undef DECLARE_SIMPLE_BUILTIN_TYPE

typedef Vector<ExecState*, 16> ExecStateStack;

struct GlobalObjectMethodTable {
    typedef bool (*AllowsAccessFromFunctionPtr)(const JSGlobalObject*, ExecState*);
    AllowsAccessFromFunctionPtr allowsAccessFrom;

    typedef bool (*SupportsProfilingFunctionPtr)(const JSGlobalObject*); 
    SupportsProfilingFunctionPtr supportsProfiling;

    typedef bool (*SupportsRichSourceInfoFunctionPtr)(const JSGlobalObject*);
    SupportsRichSourceInfoFunctionPtr supportsRichSourceInfo;

    typedef bool (*ShouldInterruptScriptFunctionPtr)(const JSGlobalObject*);
    ShouldInterruptScriptFunctionPtr shouldInterruptScript;

    typedef RuntimeFlags (*JavaScriptRuntimeFlagsFunctionPtr)(const JSGlobalObject*);
    JavaScriptRuntimeFlagsFunctionPtr javaScriptRuntimeFlags;

    typedef void (*QueueTaskToEventLoopFunctionPtr)(const JSGlobalObject*, PassRefPtr<Microtask>);
    QueueTaskToEventLoopFunctionPtr queueTaskToEventLoop;

    typedef bool (*ShouldInterruptScriptBeforeTimeoutPtr)(const JSGlobalObject*);
    ShouldInterruptScriptBeforeTimeoutPtr shouldInterruptScriptBeforeTimeout;

    typedef JSInternalPromise* (*ModuleLoaderResolvePtr)(JSGlobalObject*, ExecState*, JSValue, JSValue);
    ModuleLoaderResolvePtr moduleLoaderResolve;

    typedef JSInternalPromise* (*ModuleLoaderFetchPtr)(JSGlobalObject*, ExecState*, JSValue);
    ModuleLoaderFetchPtr moduleLoaderFetch;

    typedef JSInternalPromise* (*ModuleLoaderTranslatePtr)(JSGlobalObject*, ExecState*, JSValue, JSValue);
    ModuleLoaderTranslatePtr moduleLoaderTranslate;

    typedef JSInternalPromise* (*ModuleLoaderInstantiatePtr)(JSGlobalObject*, ExecState*, JSValue, JSValue);
    ModuleLoaderInstantiatePtr moduleLoaderInstantiate;

    typedef JSValue (*ModuleLoaderEvaluatePtr)(JSGlobalObject*, ExecState*, JSValue, JSValue);
    ModuleLoaderEvaluatePtr moduleLoaderEvaluate;
};

class JSGlobalObject : public JSSegmentedVariableObject {
private:
    typedef HashSet<RefPtr<OpaqueJSWeakObjectMap>> WeakMapSet;
    typedef HashMap<OpaqueJSClass*, std::unique_ptr<OpaqueJSClassContextData>> OpaqueJSClassDataMap;

    struct JSGlobalObjectRareData {
        JSGlobalObjectRareData()
            : profileGroup(0)
        {
        }

        WeakMapSet weakMaps;
        unsigned profileGroup;
        
        OpaqueJSClassDataMap opaqueJSClassData;
    };

protected:
    Register m_globalCallFrame[JSStack::CallFrameHeaderSize];

    WriteBarrier<JSObject> m_globalThis;

    WriteBarrier<JSGlobalLexicalEnvironment> m_globalLexicalEnvironment;
    WriteBarrier<JSObject> m_globalCallee;
    WriteBarrier<RegExpConstructor> m_regExpConstructor;
    WriteBarrier<ErrorConstructor> m_errorConstructor;
    WriteBarrier<NativeErrorConstructor> m_evalErrorConstructor;
    WriteBarrier<NativeErrorConstructor> m_rangeErrorConstructor;
    WriteBarrier<NativeErrorConstructor> m_referenceErrorConstructor;
    WriteBarrier<NativeErrorConstructor> m_syntaxErrorConstructor;
    WriteBarrier<NativeErrorConstructor> m_typeErrorConstructor;
    WriteBarrier<NativeErrorConstructor> m_URIErrorConstructor;
    WriteBarrier<ObjectConstructor> m_objectConstructor;
    WriteBarrier<JSPromiseConstructor> m_promiseConstructor;
    WriteBarrier<JSInternalPromiseConstructor> m_internalPromiseConstructor;

    WriteBarrier<NullGetterFunction> m_nullGetterFunction;
    WriteBarrier<NullSetterFunction> m_nullSetterFunction;

    WriteBarrier<JSFunction> m_parseIntFunction;

    WriteBarrier<JSFunction> m_evalFunction;
    WriteBarrier<JSFunction> m_callFunction;
    WriteBarrier<JSFunction> m_applyFunction;
    WriteBarrier<JSFunction> m_definePropertyFunction;
    WriteBarrier<JSFunction> m_arrayProtoValuesFunction;
    WriteBarrier<JSFunction> m_initializePromiseFunction;
    WriteBarrier<JSFunction> m_newPromiseCapabilityFunction;
    WriteBarrier<JSFunction> m_functionProtoHasInstanceSymbolFunction;
    WriteBarrier<GetterSetter> m_throwTypeErrorGetterSetter;

    WriteBarrier<ModuleLoaderObject> m_moduleLoader;

    WriteBarrier<ObjectPrototype> m_objectPrototype;
    WriteBarrier<FunctionPrototype> m_functionPrototype;
    WriteBarrier<ArrayPrototype> m_arrayPrototype;
    WriteBarrier<RegExpPrototype> m_regExpPrototype;
    WriteBarrier<IteratorPrototype> m_iteratorPrototype;
    WriteBarrier<GeneratorFunctionPrototype> m_generatorFunctionPrototype;
    WriteBarrier<GeneratorPrototype> m_generatorPrototype;

    WriteBarrier<Structure> m_debuggerScopeStructure;
    WriteBarrier<Structure> m_withScopeStructure;
    WriteBarrier<Structure> m_strictEvalActivationStructure;
    WriteBarrier<Structure> m_lexicalEnvironmentStructure;
    WriteBarrier<Structure> m_moduleEnvironmentStructure;
    WriteBarrier<Structure> m_directArgumentsStructure;
    WriteBarrier<Structure> m_scopedArgumentsStructure;
    WriteBarrier<Structure> m_outOfBandArgumentsStructure;
        
    // Lists the actual structures used for having these particular indexing shapes.
    WriteBarrier<Structure> m_originalArrayStructureForIndexingShape[NumberOfIndexingShapes];
    // Lists the structures we should use during allocation for these particular indexing shapes.
    // These structures will differ from the originals list above when we are having a bad time.
    WriteBarrier<Structure> m_arrayStructureForIndexingShapeDuringAllocation[NumberOfIndexingShapes];

    WriteBarrier<Structure> m_callbackConstructorStructure;
    WriteBarrier<Structure> m_callbackFunctionStructure;
    WriteBarrier<Structure> m_callbackObjectStructure;
    WriteBarrier<Structure> m_propertyNameIteratorStructure;
#if JSC_OBJC_API_ENABLED
    WriteBarrier<Structure> m_objcCallbackFunctionStructure;
    WriteBarrier<Structure> m_objcWrapperObjectStructure;
#endif
    WriteBarrier<Structure> m_nullPrototypeObjectStructure;
    WriteBarrier<Structure> m_calleeStructure;
    WriteBarrier<Structure> m_functionStructure;
    WriteBarrier<Structure> m_boundFunctionStructure;
    WriteBarrier<Structure> m_arrowFunctionStructure;
    WriteBarrier<Structure> m_nativeStdFunctionStructure;
    WriteBarrier<Structure> m_namedFunctionStructure;
    PropertyOffset m_functionNameOffset;
    WriteBarrier<Structure> m_privateNameStructure;
    WriteBarrier<Structure> m_regExpStructure;
    WriteBarrier<Structure> m_generatorFunctionStructure;
    WriteBarrier<Structure> m_consoleStructure;
    WriteBarrier<Structure> m_dollarVMStructure;
    WriteBarrier<Structure> m_internalFunctionStructure;
    WriteBarrier<Structure> m_iteratorResultStructure;
    WriteBarrier<Structure> m_regExpMatchesArrayStructure;
    WriteBarrier<Structure> m_moduleRecordStructure;
    WriteBarrier<Structure> m_moduleNamespaceObjectStructure;
#if ENABLE(WEBASSEMBLY)
    WriteBarrier<Structure> m_wasmModuleStructure;
#endif

#define DEFINE_STORAGE_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName) \
    WriteBarrier<capitalName ## Prototype> m_ ## lowerName ## Prototype; \
    WriteBarrier<Structure> m_ ## properName ## Structure;

    FOR_EACH_SIMPLE_BUILTIN_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DEFINE_STORAGE_FOR_SIMPLE_TYPE)

#undef DEFINE_STORAGE_FOR_SIMPLE_TYPE

    struct TypedArrayData {
        WriteBarrier<JSObject> prototype;
        WriteBarrier<Structure> structure;
    };
    
    std::array<TypedArrayData, NUMBER_OF_TYPED_ARRAY_TYPES> m_typedArrays;

    JSCell* m_specialPointers[Special::TableSize]; // Special pointers used by the LLInt and JIT.
    JSCell* m_linkTimeConstants[LinkTimeConstantCount];

    String m_name;

    Debugger* m_debugger;

    VM& m_vm;

#if ENABLE(WEB_REPLAY)
    RefPtr<InputCursor> m_inputCursor;
#endif

#if ENABLE(REMOTE_INSPECTOR)
    std::unique_ptr<Inspector::JSGlobalObjectInspectorController> m_inspectorController;
    std::unique_ptr<JSGlobalObjectDebuggable> m_inspectorDebuggable;
#endif

#if ENABLE(INTL)
    HashSet<String> m_intlCollatorAvailableLocales;
    HashSet<String> m_intlDateTimeFormatAvailableLocales;
    HashSet<String> m_intlNumberFormatAvailableLocales;
#endif // ENABLE(INTL)

    RefPtr<WatchpointSet> m_masqueradesAsUndefinedWatchpoint;
    RefPtr<WatchpointSet> m_havingABadTimeWatchpoint;
    RefPtr<WatchpointSet> m_varInjectionWatchpoint;

    std::unique_ptr<JSGlobalObjectRareData> m_rareData;

    WeakRandom m_weakRandom;

    TemplateRegistry m_templateRegistry;

    bool m_evalEnabled;
    String m_evalDisabledErrorMessage;
    RuntimeFlags m_runtimeFlags;
    ConsoleClient* m_consoleClient;

    static JS_EXPORTDATA const GlobalObjectMethodTable s_globalObjectMethodTable;
    const GlobalObjectMethodTable* m_globalObjectMethodTable;

    void createRareDataIfNeeded()
    {
        if (m_rareData)
            return;
        m_rareData = std::make_unique<JSGlobalObjectRareData>();
    }
        
public:
    typedef JSSegmentedVariableObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetPropertyNames;

    static JSGlobalObject* create(VM& vm, Structure* structure)
    {
        JSGlobalObject* globalObject = new (NotNull, allocateCell<JSGlobalObject>(vm.heap)) JSGlobalObject(vm, structure);
        globalObject->finishCreation(vm);
        vm.heap.addFinalizer(globalObject, destroy);
        return globalObject;
    }

    DECLARE_EXPORT_INFO;

    bool hasDebugger() const { return m_debugger; }
    bool hasProfiler() const;
    const RuntimeFlags& runtimeFlags() const { return m_runtimeFlags; }

protected:
    JS_EXPORT_PRIVATE explicit JSGlobalObject(VM&, Structure*, const GlobalObjectMethodTable* = 0);

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        structure()->setGlobalObject(vm, this);
        m_runtimeFlags = m_globalObjectMethodTable->javaScriptRuntimeFlags(this);
        init(vm);
        setGlobalThis(vm, JSProxy::create(vm, JSProxy::createStructure(vm, this, prototype(), PureForwardingProxyType), this));
    }

    void finishCreation(VM& vm, JSObject* thisValue)
    {
        Base::finishCreation(vm);
        structure()->setGlobalObject(vm, this);
        m_runtimeFlags = m_globalObjectMethodTable->javaScriptRuntimeFlags(this);
        init(vm);
        setGlobalThis(vm, thisValue);
    }

    void addGlobalVar(const Identifier&);

public:
    JS_EXPORT_PRIVATE ~JSGlobalObject();
    JS_EXPORT_PRIVATE static void destroy(JSCell*);
    // We don't need a destructor because we use a finalizer instead.
    static const bool needsDestruction = false;

    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

    JS_EXPORT_PRIVATE static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
    bool hasOwnPropertyForWrite(ExecState*, PropertyName);
    JS_EXPORT_PRIVATE static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

    JS_EXPORT_PRIVATE static void defineGetter(JSObject*, ExecState*, PropertyName, JSObject* getterFunc, unsigned attributes);
    JS_EXPORT_PRIVATE static void defineSetter(JSObject*, ExecState*, PropertyName, JSObject* setterFunc, unsigned attributes);
    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

    void addVar(ExecState* exec, const Identifier& propertyName)
    {
        if (!hasProperty(exec, propertyName))
            addGlobalVar(propertyName);
    }
    void addFunction(ExecState*, const Identifier&);

    JSScope* globalScope() { return m_globalLexicalEnvironment.get(); }
    JSGlobalLexicalEnvironment* globalLexicalEnvironment() { return m_globalLexicalEnvironment.get(); }

    // The following accessors return pristine values, even if a script 
    // replaces the global object's associated property.

    RegExpConstructor* regExpConstructor() const { return m_regExpConstructor.get(); }

    ErrorConstructor* errorConstructor() const { return m_errorConstructor.get(); }
    ObjectConstructor* objectConstructor() const { return m_objectConstructor.get(); }
    JSPromiseConstructor* promiseConstructor() const { return m_promiseConstructor.get(); }
    JSInternalPromiseConstructor* internalPromiseConstructor() const { return m_internalPromiseConstructor.get(); }
    NativeErrorConstructor* evalErrorConstructor() const { return m_evalErrorConstructor.get(); }
    NativeErrorConstructor* rangeErrorConstructor() const { return m_rangeErrorConstructor.get(); }
    NativeErrorConstructor* referenceErrorConstructor() const { return m_referenceErrorConstructor.get(); }
    NativeErrorConstructor* syntaxErrorConstructor() const { return m_syntaxErrorConstructor.get(); }
    NativeErrorConstructor* typeErrorConstructor() const { return m_typeErrorConstructor.get(); }
    NativeErrorConstructor* URIErrorConstructor() const { return m_URIErrorConstructor.get(); }

    NullGetterFunction* nullGetterFunction() const { return m_nullGetterFunction.get(); }
    NullSetterFunction* nullSetterFunction() const { return m_nullSetterFunction.get(); }

    JSFunction* parseIntFunction() const { return m_parseIntFunction.get(); }

    JSFunction* evalFunction() const { return m_evalFunction.get(); }
    JSFunction* callFunction() const { return m_callFunction.get(); }
    JSFunction* applyFunction() const { return m_applyFunction.get(); }
    JSFunction* definePropertyFunction() const { return m_definePropertyFunction.get(); }
    JSFunction* arrayProtoValuesFunction() const { return m_arrayProtoValuesFunction.get(); }
    JSFunction* initializePromiseFunction() const { return m_initializePromiseFunction.get(); }
    JSFunction* newPromiseCapabilityFunction() const { return m_newPromiseCapabilityFunction.get(); }
    JSFunction* functionProtoHasInstanceSymbolFunction() const { return m_functionProtoHasInstanceSymbolFunction.get(); }
    GetterSetter* throwTypeErrorGetterSetter(VM& vm)
    {
        if (!m_throwTypeErrorGetterSetter)
            createThrowTypeError(vm);
        return m_throwTypeErrorGetterSetter.get();
    }

    ModuleLoaderObject* moduleLoader() const { return m_moduleLoader.get(); }

    ObjectPrototype* objectPrototype() const { return m_objectPrototype.get(); }
    FunctionPrototype* functionPrototype() const { return m_functionPrototype.get(); }
    ArrayPrototype* arrayPrototype() const { return m_arrayPrototype.get(); }
    BooleanPrototype* booleanPrototype() const { return m_booleanPrototype.get(); }
    StringPrototype* stringPrototype() const { return m_stringPrototype.get(); }
    SymbolPrototype* symbolPrototype() const { return m_symbolPrototype.get(); }
    NumberPrototype* numberPrototype() const { return m_numberPrototype.get(); }
    DatePrototype* datePrototype() const { return m_datePrototype.get(); }
    RegExpPrototype* regExpPrototype() const { return m_regExpPrototype.get(); }
    ErrorPrototype* errorPrototype() const { return m_errorPrototype.get(); }
    IteratorPrototype* iteratorPrototype() const { return m_iteratorPrototype.get(); }
    GeneratorFunctionPrototype* generatorFunctionPrototype() const { return m_generatorFunctionPrototype.get(); }
    GeneratorPrototype* generatorPrototype() const { return m_generatorPrototype.get(); }

    Structure* debuggerScopeStructure() const { return m_debuggerScopeStructure.get(); }
    Structure* withScopeStructure() const { return m_withScopeStructure.get(); }
    Structure* strictEvalActivationStructure() const { return m_strictEvalActivationStructure.get(); }
    Structure* activationStructure() const { return m_lexicalEnvironmentStructure.get(); }
    Structure* moduleEnvironmentStructure() const { return m_moduleEnvironmentStructure.get(); }
    Structure* directArgumentsStructure() const { return m_directArgumentsStructure.get(); }
    Structure* scopedArgumentsStructure() const { return m_scopedArgumentsStructure.get(); }
    Structure* outOfBandArgumentsStructure() const { return m_outOfBandArgumentsStructure.get(); }
    Structure* originalArrayStructureForIndexingType(IndexingType indexingType) const
    {
        ASSERT(indexingType & IsArray);
        return m_originalArrayStructureForIndexingShape[(indexingType & IndexingShapeMask) >> IndexingShapeShift].get();
    }
    Structure* arrayStructureForIndexingTypeDuringAllocation(IndexingType indexingType) const
    {
        ASSERT(indexingType & IsArray);
        return m_arrayStructureForIndexingShapeDuringAllocation[(indexingType & IndexingShapeMask) >> IndexingShapeShift].get();
    }
    Structure* arrayStructureForIndexingTypeDuringAllocation(VM& vm, IndexingType indexingType, JSValue prototype) const
    {
        return Structure::createSubclassStructure(vm, arrayStructureForIndexingTypeDuringAllocation(indexingType), prototype);
    }
    Structure* arrayStructureForProfileDuringAllocation(VM& vm, ArrayAllocationProfile* profile, JSValue prototype) const
    {
        return arrayStructureForIndexingTypeDuringAllocation(vm, ArrayAllocationProfile::selectIndexingTypeFor(profile), prototype);
    }
        
    bool isOriginalArrayStructure(Structure* structure)
    {
        return originalArrayStructureForIndexingType(structure->indexingType() | IsArray) == structure;
    }
        
    Structure* booleanObjectStructure() const { return m_booleanObjectStructure.get(); }
    Structure* callbackConstructorStructure() const { return m_callbackConstructorStructure.get(); }
    Structure* callbackFunctionStructure() const { return m_callbackFunctionStructure.get(); }
    Structure* callbackObjectStructure() const { return m_callbackObjectStructure.get(); }
    Structure* propertyNameIteratorStructure() const { return m_propertyNameIteratorStructure.get(); }
#if JSC_OBJC_API_ENABLED
    Structure* objcCallbackFunctionStructure() const { return m_objcCallbackFunctionStructure.get(); }
    Structure* objcWrapperObjectStructure() const { return m_objcWrapperObjectStructure.get(); }
#endif
    Structure* dateStructure() const { return m_dateStructure.get(); }
    Structure* nullPrototypeObjectStructure() const { return m_nullPrototypeObjectStructure.get(); }
    Structure* errorStructure() const { return m_errorStructure.get(); }
    Structure* calleeStructure() const { return m_calleeStructure.get(); }
    Structure* functionStructure() const { return m_functionStructure.get(); }
    Structure* boundFunctionStructure() const { return m_boundFunctionStructure.get(); }
    Structure* arrowFunctionStructure() const { return m_arrowFunctionStructure.get(); }
    Structure* nativeStdFunctionStructure() const { return m_nativeStdFunctionStructure.get(); }
    Structure* namedFunctionStructure() const { return m_namedFunctionStructure.get(); }
    PropertyOffset functionNameOffset() const { return m_functionNameOffset; }
    Structure* numberObjectStructure() const { return m_numberObjectStructure.get(); }
    Structure* privateNameStructure() const { return m_privateNameStructure.get(); }
    Structure* internalFunctionStructure() const { return m_internalFunctionStructure.get(); }
    Structure* mapStructure() const { return m_mapStructure.get(); }
    Structure* regExpStructure() const { return m_regExpStructure.get(); }
    Structure* generatorFunctionStructure() const { return m_generatorFunctionStructure.get(); }
    Structure* setStructure() const { return m_setStructure.get(); }
    Structure* stringObjectStructure() const { return m_stringObjectStructure.get(); }
    Structure* symbolObjectStructure() const { return m_symbolObjectStructure.get(); }
    Structure* iteratorResultStructure() const { return m_iteratorResultStructure.get(); }
    static ptrdiff_t iteratorResultStructureOffset() { return OBJECT_OFFSETOF(JSGlobalObject, m_iteratorResultStructure); }
    Structure* regExpMatchesArrayStructure() const { return m_regExpMatchesArrayStructure.get(); }
    Structure* moduleRecordStructure() const { return m_moduleRecordStructure.get(); }
    Structure* moduleNamespaceObjectStructure() const { return m_moduleNamespaceObjectStructure.get(); }
#if ENABLE(WEBASSEMBLY)
    Structure* wasmModuleStructure() const { return m_wasmModuleStructure.get(); }
#endif

    JS_EXPORT_PRIVATE void setRemoteDebuggingEnabled(bool);
    JS_EXPORT_PRIVATE bool remoteDebuggingEnabled() const;

#if ENABLE(WEB_REPLAY)
    JS_EXPORT_PRIVATE void setInputCursor(PassRefPtr<InputCursor>);
    InputCursor& inputCursor() const { return *m_inputCursor; }
#endif

#if ENABLE(REMOTE_INSPECTOR)
    Inspector::JSGlobalObjectInspectorController& inspectorController() const { return *m_inspectorController.get(); }
    JSGlobalObjectDebuggable& inspectorDebuggable() { return *m_inspectorDebuggable.get(); }
#endif

#if ENABLE(INTL)
    const HashSet<String>& intlCollatorAvailableLocales();
    const HashSet<String>& intlDateTimeFormatAvailableLocales();
    const HashSet<String>& intlNumberFormatAvailableLocales();
#endif // ENABLE(INTL)

    void setConsoleClient(ConsoleClient* consoleClient) { m_consoleClient = consoleClient; }
    ConsoleClient* consoleClient() const { return m_consoleClient; }

    void setName(const String&);
    const String& name() const { return m_name; }

    JSArrayBufferPrototype* arrayBufferPrototype() const { return m_arrayBufferPrototype.get(); }

#define DEFINE_ACCESSORS_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName) \
    Structure* properName ## Structure() { return m_ ## properName ## Structure.get(); }

    FOR_EACH_SIMPLE_BUILTIN_TYPE(DEFINE_ACCESSORS_FOR_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(DEFINE_ACCESSORS_FOR_SIMPLE_TYPE)

#undef DEFINE_ACCESSORS_FOR_SIMPLE_TYPE

    Structure* typedArrayStructure(TypedArrayType type) const
    {
        return m_typedArrays[toIndex(type)].structure.get();
    }
    bool isOriginalTypedArrayStructure(Structure* structure)
    {
        TypedArrayType type = structure->classInfo()->typedArrayStorageType;
        if (type == NotTypedArray)
            return false;
        return typedArrayStructure(type) == structure;
    }

    JSCell* actualPointerFor(Special::Pointer pointer)
    {
        ASSERT(pointer < Special::TableSize);
        return m_specialPointers[pointer];
    }
    JSCell* jsCellForLinkTimeConstant(LinkTimeConstant type)
    {
        unsigned index = static_cast<unsigned>(type);
        ASSERT(index < LinkTimeConstantCount);
        return m_linkTimeConstants[index];
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
    void setDebugger(Debugger* debugger) { m_debugger = debugger; }

    const GlobalObjectMethodTable* globalObjectMethodTable() const { return m_globalObjectMethodTable; }

    static bool allowsAccessFrom(const JSGlobalObject*, ExecState*) { return true; }
    static bool supportsProfiling(const JSGlobalObject*) { return false; }
    static bool supportsRichSourceInfo(const JSGlobalObject*) { return true; }

    JS_EXPORT_PRIVATE ExecState* globalExec();

    static bool shouldInterruptScript(const JSGlobalObject*) { return true; }
    static bool shouldInterruptScriptBeforeTimeout(const JSGlobalObject*) { return false; }
    static RuntimeFlags javaScriptRuntimeFlags(const JSGlobalObject*) { return RuntimeFlags(); }

    void queueMicrotask(PassRefPtr<Microtask>);

    bool evalEnabled() const { return m_evalEnabled; }
    const String& evalDisabledErrorMessage() const { return m_evalDisabledErrorMessage; }
    void setEvalEnabled(bool enabled, const String& errorMessage = String())
    {
        m_evalEnabled = enabled;
        m_evalDisabledErrorMessage = errorMessage;
    }

    void resetPrototype(VM&, JSValue prototype);

    VM& vm() const { return m_vm; }
    JSObject* globalThis() const;

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        Structure* result = Structure::create(vm, 0, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
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

    TemplateRegistry& templateRegistry() { return m_templateRegistry; }

    static ptrdiff_t weakRandomOffset() { return OBJECT_OFFSETOF(JSGlobalObject, m_weakRandom); }
    double weakRandomNumber() { return m_weakRandom.get(); }
    unsigned weakRandomInteger() { return m_weakRandom.getUint32(); }

    UnlinkedProgramCodeBlock* createProgramCodeBlock(CallFrame*, ProgramExecutable*, JSObject** exception);
    UnlinkedEvalCodeBlock* createEvalCodeBlock(CallFrame*, EvalExecutable*, ThisTDZMode, bool isArrowFunctionContext, const VariableEnvironment*);
    UnlinkedModuleProgramCodeBlock* createModuleProgramCodeBlock(CallFrame*, ModuleProgramExecutable*);

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

    JS_EXPORT_PRIVATE static JSC::JSValue toThis(JSC::JSCell*, JSC::ExecState*, ECMAMode);

private:
    friend class LLIntOffsetsExtractor;

    JS_EXPORT_PRIVATE void setGlobalThis(VM&, JSObject* globalThis);

    JS_EXPORT_PRIVATE void init(VM&);

    void createThrowTypeError(VM&);

    JS_EXPORT_PRIVATE static void clearRareData(JSCell*);
};

JSGlobalObject* asGlobalObject(JSValue);

inline JSGlobalObject* asGlobalObject(JSValue value)
{
    ASSERT(asObject(value)->isGlobalObject());
    return jsCast<JSGlobalObject*>(asObject(value));
}

inline bool JSGlobalObject::hasOwnPropertyForWrite(ExecState* exec, PropertyName propertyName)
{
    PropertySlot slot(this);
    if (Base::getOwnPropertySlot(this, exec, propertyName, slot))
        return true;
    bool slotIsWriteable;
    return symbolTableGet(this, propertyName, slot, slotIsWriteable);
}

inline JSArray* constructEmptyArray(ExecState* exec, ArrayAllocationProfile* profile, JSGlobalObject* globalObject, unsigned initialLength = 0, JSValue prototype = JSValue())
{
    VM& vm = exec->vm();
    Structure* structure;
    if (initialLength >= MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)
        structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(vm, ArrayWithArrayStorage, prototype);
    else
        structure = globalObject->arrayStructureForProfileDuringAllocation(vm, profile, prototype);

    return ArrayAllocationProfile::updateLastAllocationFor(profile, JSArray::create(vm, structure, initialLength));
}

inline JSArray* constructEmptyArray(ExecState* exec, ArrayAllocationProfile* profile, unsigned initialLength = 0, JSValue prototype = JSValue())
{
    return constructEmptyArray(exec, profile, exec->lexicalGlobalObject(), initialLength, prototype);
}
 
inline JSArray* constructArray(ExecState* exec, ArrayAllocationProfile* profile, JSGlobalObject* globalObject, const ArgList& values, JSValue prototype = JSValue())
{
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArray(exec, globalObject->arrayStructureForProfileDuringAllocation(exec->vm(), profile, prototype), values));
}

inline JSArray* constructArray(ExecState* exec, ArrayAllocationProfile* profile, const ArgList& values, JSValue prototype = JSValue())
{
    return constructArray(exec, profile, exec->lexicalGlobalObject(), values, prototype);
}

inline JSArray* constructArray(ExecState* exec, ArrayAllocationProfile* profile, JSGlobalObject* globalObject, const JSValue* values, unsigned length, JSValue prototype = JSValue())
{
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArray(exec, globalObject->arrayStructureForProfileDuringAllocation(exec->vm(), profile, prototype), values, length));
}

inline JSArray* constructArray(ExecState* exec, ArrayAllocationProfile* profile, const JSValue* values, unsigned length, JSValue prototype = JSValue())
{
    return constructArray(exec, profile, exec->lexicalGlobalObject(), values, length, prototype);
}

inline JSArray* constructArrayNegativeIndexed(ExecState* exec, ArrayAllocationProfile* profile, JSGlobalObject* globalObject, const JSValue* values, unsigned length, JSValue prototype = JSValue())
{
    return ArrayAllocationProfile::updateLastAllocationFor(profile, constructArrayNegativeIndexed(exec, globalObject->arrayStructureForProfileDuringAllocation(exec->vm(), profile, prototype), values, length));
}

inline JSArray* constructArrayNegativeIndexed(ExecState* exec, ArrayAllocationProfile* profile, const JSValue* values, unsigned length, JSValue prototype = JSValue())
{
    return constructArrayNegativeIndexed(exec, profile, exec->lexicalGlobalObject(), values, length, prototype);
}

inline JSObject* ExecState::globalThisValue() const
{
    return lexicalGlobalObject()->globalThis();
}

inline JSObject* JSScope::globalThis()
{ 
    return globalObject()->globalThis();
}

inline JSObject* JSGlobalObject::globalThis() const
{ 
    return m_globalThis.get();
}

} // namespace JSC

#endif // JSGlobalObject_h
