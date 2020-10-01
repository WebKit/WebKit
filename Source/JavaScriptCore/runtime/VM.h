/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "CallData.h"
#include "CodeSpecializationKind.h"
#include "CompleteSubspace.h"
#include "ConcurrentJSLock.h"
#include "DateInstanceCache.h"
#include "DeleteAllCodeEffort.h"
#include "DisallowVMEntry.h"
#include "ExceptionEventLocation.h"
#include "FunctionHasExecutedCache.h"
#include "FuzzerAgent.h"
#include "Heap.h"
#include "Integrity.h"
#include "Intrinsic.h"
#include "IsoCellSet.h"
#include "IsoSubspace.h"
#include "JSCJSValue.h"
#include "JSLock.h"
#include "MacroAssemblerCodeRef.h"
#include "Microtask.h"
#include "NumericStrings.h"
#include "SmallStrings.h"
#include "Strong.h"
#include "StructureCache.h"
#include "SubspaceAccess.h"
#include "ThunkGenerator.h"
#include "VMTraps.h"
#include "WasmContext.h"
#include "Watchpoint.h"
#include <wtf/BumpPointerAllocator.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/DateMath.h>
#include <wtf/Deque.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/Forward.h>
#include <wtf/Gigacage.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>
#include <wtf/StackBounds.h>
#include <wtf/StackPointer.h>
#include <wtf/Stopwatch.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/SymbolRegistry.h>
#include <wtf/text/WTFString.h>
#if ENABLE(REGEXP_TRACING)
#include <wtf/ListHashSet.h>
#endif

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
#include <wtf/StackTrace.h>
#endif

// Enable the Objective-C API for platforms with a modern runtime. This has to match exactly what we
// have in JSBase.h.
#if !defined(JSC_OBJC_API_ENABLED)
#if (defined(__clang__) && defined(__APPLE__) && (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)))
#define JSC_OBJC_API_ENABLED 1
#else
#define JSC_OBJC_API_ENABLED 0
#endif
#endif

namespace WTF {
class RunLoop;
class SimpleStats;
} // namespace WTF
using WTF::SimpleStats;

namespace JSC {

class BasicBlockLocation;
class BuiltinExecutables;
class BytecodeIntrinsicRegistry;
class CallFrame;
struct CheckpointOSRExitSideState;
class CodeBlock;
class CodeCache;
class CommonIdentifiers;
class CompactVariableMap;
class ConservativeRoots;
class ControlFlowProfiler;
class CustomGetterSetter;
class DOMAttributeGetterSetter;
class DateInstance;
class ErrorInstance;
class Exception;
class ExceptionScope;
class FastMallocAlignedMemoryAllocator;
class GigacageAlignedMemoryAllocator;
class HandleStack;
class HasOwnPropertyCache;
class HeapProfiler;
class Identifier;
class Interpreter;
class IntlCache;
class IntlCollator;
class IntlDateTimeFormat;
class IntlDisplayNames;
class IntlLocale;
class IntlNumberFormat;
class IntlPluralRules;
class IntlRelativeTimeFormat;
class IntlSegmentIterator;
class IntlSegmenter;
class IntlSegments;
class JSAPIGlobalObject;
class JSAPIWrapperGlobalObject;
class JSAPIWrapperObject;
class JSCCallbackFunction;
class JSCallbackConstructor;
class JSCustomGetterSetterFunction;
class JSDestructibleObjectHeapCellType;
class JSGlobalObject;
class JSGlobalLexicalEnvironment;
class JSModuleNamespaceObject;
class JSModuleRecord;
class JSNativeStdFunction;
class JSNonFinalObject;
class JSObject;
class JSPromise;
class JSPropertyNameEnumerator;
class JSRunLoopTimer;
class JSWeakMap;
class JSWeakSet;
class JSWebAssemblyCodeBlock;
class JSWebAssemblyGlobal;
class JSWebAssemblyInstance;
class JSWebAssemblyMemory;
class JSWebAssemblyModule;
class JSWebAssemblyTable;
class JITThunks;
class LLIntOffsetsExtractor;
class NativeExecutable;
class ObjCCallbackFunction;
class DeferredWorkTimer;
class RegExp;
class RegExpCache;
class Register;
class RegisterAtOffsetList;
#if ENABLE(SAMPLING_PROFILER)
class SamplingProfiler;
#endif
class ShadowChicken;
class ScriptExecutable;
class SourceProvider;
class SourceProviderCache;
class StackFrame;
class Structure;
#if ENABLE(REGEXP_TRACING)
class RegExp;
#endif
class Symbol;
class TypedArrayController;
class UnlinkedCodeBlock;
class UnlinkedEvalCodeBlock;
class UnlinkedFunctionExecutable;
class UnlinkedProgramCodeBlock;
class UnlinkedModuleProgramCodeBlock;
class VirtualRegister;
class VMEntryScope;
class TopLevelGlobalObjectScope;
class TypeProfiler;
class TypeProfilerLog;
class Watchdog;
class Watchpoint;
class WatchpointSet;
class WebAssemblyFunction;
class WebAssemblyModuleRecord;

class IsoHeapCellType;
template<typename CellType> class IsoInlinedHeapCellType;
template<typename Parent> class JSCallbackObject;

#if ENABLE(FTL_JIT)
namespace FTL {
class Thunks;
}
#endif // ENABLE(FTL_JIT)
namespace Profiler {
class Database;
}
namespace DOMJIT {
class Signature;
}

struct EntryFrame;
struct HashTable;
struct Instruction;
struct ValueProfile;

struct LocalTimeOffsetCache {
    LocalTimeOffsetCache()
        : start(0.0)
        , end(-1.0)
        , increment(0.0)
    {
    }

    void reset()
    {
        offset = LocalTimeOffset();
        start = 0.0;
        end = -1.0;
        increment = 0.0;
    }

    LocalTimeOffset offset;
    double start;
    double end;
    double increment;
};

class QueuedTask {
    WTF_MAKE_NONCOPYABLE(QueuedTask);
    WTF_MAKE_FAST_ALLOCATED;
public:
    void run();

    QueuedTask(VM& vm, JSGlobalObject* globalObject, Ref<Microtask>&& microtask)
        : m_globalObject(vm, globalObject)
        , m_microtask(WTFMove(microtask))
    {
    }

private:
    Strong<JSGlobalObject> m_globalObject;
    Ref<Microtask> m_microtask;
};

class ConservativeRoots;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(VM);

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif
struct ScratchBuffer {
    ScratchBuffer()
    {
        u.m_activeLength = 0;
    }

    static ScratchBuffer* create(size_t size)
    {
        ScratchBuffer* result = new (VMMalloc::malloc(ScratchBuffer::allocationSize(size))) ScratchBuffer;
        return result;
    }

    static size_t allocationSize(Checked<size_t> bufferSize) { return (sizeof(ScratchBuffer) + bufferSize).unsafeGet(); }
    void setActiveLength(size_t activeLength) { u.m_activeLength = activeLength; }
    size_t activeLength() const { return u.m_activeLength; };
    size_t* addressOfActiveLength() { return &u.m_activeLength; };
    void* dataBuffer() { return m_buffer; }

    union {
        size_t m_activeLength;
        double pad; // Make sure m_buffer is double aligned.
    } u;
#if CPU(MIPS) && (defined WTF_MIPS_ARCH_REV && WTF_MIPS_ARCH_REV == 2)
    alignas(8) void* m_buffer[0];
#else
    void* m_buffer[0];
#endif
};
#if COMPILER(MSVC)
#pragma warning(pop)
#endif

class VM : public ThreadSafeRefCounted<VM>, public DoublyLinkedListNode<VM> {
public:
    // WebCore has a one-to-one mapping of threads to VMs;
    // create() should only be called once
    // on a thread, this is the 'default' VM (it uses the
    // thread's default string uniquing table from Thread::current()).
    // API contexts created using the new context group aware interface
    // create APIContextGroup objects which require less locking of JSC
    // than the old singleton APIShared VM created for use by
    // the original API.
    enum VMType { Default, APIContextGroup, APIShared };

    struct ClientData {
        JS_EXPORT_PRIVATE virtual ~ClientData() = 0;
    };

    bool isSharedInstance() { return vmType == APIShared; }
    bool usingAPI() { return vmType != Default; }
    JS_EXPORT_PRIVATE static bool sharedInstanceExists();
    JS_EXPORT_PRIVATE static VM& sharedInstance();

    JS_EXPORT_PRIVATE static Ref<VM> create(HeapType = SmallHeap, WTF::RunLoop* = nullptr);
    JS_EXPORT_PRIVATE static RefPtr<VM> tryCreate(HeapType = SmallHeap, WTF::RunLoop* = nullptr);
    static Ref<VM> createContextGroup(HeapType = SmallHeap);
    JS_EXPORT_PRIVATE ~VM();

    Watchdog& ensureWatchdog();
    Watchdog* watchdog() { return m_watchdog.get(); }

    HeapProfiler* heapProfiler() const { return m_heapProfiler.get(); }
    JS_EXPORT_PRIVATE HeapProfiler& ensureHeapProfiler();

#if ENABLE(SAMPLING_PROFILER)
    SamplingProfiler* samplingProfiler() { return m_samplingProfiler.get(); }
    JS_EXPORT_PRIVATE SamplingProfiler& ensureSamplingProfiler(Ref<Stopwatch>&&);
#endif

    FuzzerAgent* fuzzerAgent() const { return m_fuzzerAgent.get(); }
    void setFuzzerAgent(std::unique_ptr<FuzzerAgent>&& fuzzerAgent)
    {
        RELEASE_ASSERT_WITH_MESSAGE(!m_fuzzerAgent, "Only one FuzzerAgent can be specified at a time.");
        m_fuzzerAgent = WTFMove(fuzzerAgent);
    }

    static unsigned numberOfIDs() { return s_numberOfIDs.load(); }
    unsigned id() const { return m_id; }
    bool isEntered() const { return !!entryScope; }

    inline CallFrame* topJSCallFrame() const;

    // Global object in which execution began.
    JS_EXPORT_PRIVATE JSGlobalObject* deprecatedVMEntryGlobalObject(JSGlobalObject*) const;

    WeakRandom& random() { return m_random; }
    Integrity::Random& integrityRandom() { return m_integrityRandom; }

private:
    unsigned nextID();

    static Atomic<unsigned> s_numberOfIDs;

    unsigned m_id;
    RefPtr<JSLock> m_apiLock;
    Ref<WTF::RunLoop> m_runLoop;

    WeakRandom m_random;
    Integrity::Random m_integrityRandom;

public:
    Heap heap;
    
    std::unique_ptr<FastMallocAlignedMemoryAllocator> fastMallocAllocator;
    std::unique_ptr<GigacageAlignedMemoryAllocator> primitiveGigacageAllocator;
    std::unique_ptr<GigacageAlignedMemoryAllocator> jsValueGigacageAllocator;

    std::unique_ptr<HeapCellType> auxiliaryHeapCellType;
    std::unique_ptr<HeapCellType> immutableButterflyHeapCellType;
    std::unique_ptr<HeapCellType> cellHeapCellType;
    std::unique_ptr<HeapCellType> destructibleCellHeapCellType;
    std::unique_ptr<IsoHeapCellType> apiGlobalObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> callbackConstructorHeapCellType;
    std::unique_ptr<IsoHeapCellType> callbackGlobalObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> callbackObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> dateInstanceHeapCellType;
    std::unique_ptr<IsoHeapCellType> errorInstanceHeapCellType;
    std::unique_ptr<IsoHeapCellType> finalizationRegistryCellType;
    std::unique_ptr<IsoHeapCellType> globalLexicalEnvironmentHeapCellType;
    std::unique_ptr<IsoHeapCellType> globalObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> injectedScriptHostSpaceHeapCellType;
    std::unique_ptr<IsoHeapCellType> javaScriptCallFrameHeapCellType;
    std::unique_ptr<IsoHeapCellType> jsModuleRecordHeapCellType;
    std::unique_ptr<IsoHeapCellType> moduleNamespaceObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> nativeStdFunctionHeapCellType;
    std::unique_ptr<IsoInlinedHeapCellType<JSString>> stringHeapCellType;
    std::unique_ptr<IsoHeapCellType> weakMapHeapCellType;
    std::unique_ptr<IsoHeapCellType> weakSetHeapCellType;
    std::unique_ptr<JSDestructibleObjectHeapCellType> destructibleObjectHeapCellType;
#if JSC_OBJC_API_ENABLED
    std::unique_ptr<IsoHeapCellType> apiWrapperObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> objCCallbackFunctionHeapCellType;
#endif
#ifdef JSC_GLIB_API_ENABLED
    std::unique_ptr<IsoHeapCellType> apiWrapperObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> callbackAPIWrapperGlobalObjectHeapCellType;
    std::unique_ptr<IsoHeapCellType> jscCallbackFunctionHeapCellType;
#endif
    std::unique_ptr<IsoHeapCellType> intlCollatorHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlDateTimeFormatHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlDisplayNamesHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlLocaleHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlNumberFormatHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlPluralRulesHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlRelativeTimeFormatHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlSegmentIteratorHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlSegmenterHeapCellType;
    std::unique_ptr<IsoHeapCellType> intlSegmentsHeapCellType;
#if ENABLE(WEBASSEMBLY)
    std::unique_ptr<IsoHeapCellType> webAssemblyCodeBlockHeapCellType;
    std::unique_ptr<IsoHeapCellType> webAssemblyFunctionHeapCellType;
    std::unique_ptr<IsoHeapCellType> webAssemblyGlobalHeapCellType;
    std::unique_ptr<IsoHeapCellType> webAssemblyInstanceHeapCellType;
    std::unique_ptr<IsoHeapCellType> webAssemblyMemoryHeapCellType;
    std::unique_ptr<IsoHeapCellType> webAssemblyModuleHeapCellType;
    std::unique_ptr<IsoHeapCellType> webAssemblyModuleRecordHeapCellType;
    std::unique_ptr<IsoHeapCellType> webAssemblyTableHeapCellType;
#endif
    
    CompleteSubspace primitiveGigacageAuxiliarySpace; // Typed arrays, strings, bitvectors, etc go here.
    CompleteSubspace jsValueGigacageAuxiliarySpace; // Butterflies, arrays of JSValues, etc go here.
    CompleteSubspace immutableButterflyJSValueGigacageAuxiliarySpace; // JSImmutableButterfly goes here.

    // We make cross-cutting assumptions about typed arrays being in the primitive Gigacage and butterflies
    // being in the JSValue gigacage. For some types, it's super obvious where they should go, and so we
    // can hardcode that fact. But sometimes it's not clear, so we abstract it by having a Gigacage::Kind
    // constant somewhere.
    // FIXME: Maybe it would be better if everyone abstracted this?
    // https://bugs.webkit.org/show_bug.cgi?id=175248
    ALWAYS_INLINE CompleteSubspace& gigacageAuxiliarySpace(Gigacage::Kind kind)
    {
        switch (kind) {
        case Gigacage::Primitive:
            return primitiveGigacageAuxiliarySpace;
        case Gigacage::JSValue:
            return jsValueGigacageAuxiliarySpace;
        case Gigacage::NumberOfKinds:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return primitiveGigacageAuxiliarySpace;
    }
    
    // Whenever possible, use subspaceFor<CellType>(vm) to get one of these subspaces.
    CompleteSubspace cellSpace;
    CompleteSubspace variableSizedCellSpace; // FIXME: This space is problematic because we have things in here like DirectArguments and ScopedArguments; those should be split into JSValueOOB cells and JSValueStrict auxiliaries. https://bugs.webkit.org/show_bug.cgi?id=182858
    CompleteSubspace destructibleObjectSpace;
    
    IsoSubspace arraySpace;
    IsoSubspace bigIntSpace;
    IsoSubspace calleeSpace;
    IsoSubspace clonedArgumentsSpace;
    IsoSubspace customGetterSetterSpace;
    IsoSubspace dateInstanceSpace;
    IsoSubspace domAttributeGetterSetterSpace;
    IsoSubspace exceptionSpace;
    IsoSubspace executableToCodeBlockEdgeSpace;
    IsoSubspace functionSpace;
    IsoSubspace getterSetterSpace;
    IsoSubspace globalLexicalEnvironmentSpace;
    IsoSubspace internalFunctionSpace;
    IsoSubspace jsProxySpace;
    IsoSubspace nativeExecutableSpace;
    IsoSubspace numberObjectSpace;
    IsoSubspace plainObjectSpace;
    IsoSubspace promiseSpace;
    IsoSubspace propertyNameEnumeratorSpace;
    IsoSubspace propertyTableSpace;
    IsoSubspace regExpSpace;
    IsoSubspace regExpObjectSpace;
    IsoSubspace ropeStringSpace;
    IsoSubspace scopedArgumentsSpace;
    IsoSubspace sparseArrayValueMapSpace;
    IsoSubspace stringSpace;
    IsoSubspace stringObjectSpace;
    IsoSubspace structureChainSpace;
    IsoSubspace structureRareDataSpace;
    IsoSubspace structureSpace;
    IsoSubspace symbolTableSpace;

#define DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(name) \
    template<SubspaceAccess mode> \
    IsoSubspace* name() \
    { \
        if (m_##name || mode == SubspaceAccess::Concurrently) \
            return m_##name.get(); \
        return name##Slow(); \
    } \
    JS_EXPORT_PRIVATE IsoSubspace* name##Slow(); \
    std::unique_ptr<IsoSubspace> m_##name;


#if JSC_OBJC_API_ENABLED
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(apiWrapperObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(objCCallbackFunctionSpace)
#endif
#ifdef JSC_GLIB_API_ENABLED
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(apiWrapperObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(jscCallbackFunctionSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(callbackAPIWrapperGlobalObjectSpace)
#endif
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(apiGlobalObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(apiValueWrapperSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(arrayBufferSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(arrayIteratorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(asyncGeneratorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(bigIntObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(booleanObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(boundFunctionSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(callbackConstructorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(callbackGlobalObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(callbackFunctionSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(callbackObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(customGetterSetterFunctionSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(dataViewSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(debuggerScopeSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(errorInstanceSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(float32ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(float64ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(functionRareDataSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(generatorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(globalObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(injectedScriptHostSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(int8ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(int16ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(int32ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(javaScriptCallFrameSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(jsModuleRecordSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(mapBucketSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(mapIteratorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(mapSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(moduleNamespaceObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(nativeStdFunctionSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(proxyObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(proxyRevokeSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(scopedArgumentsTableSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(scriptFetchParametersSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(scriptFetcherSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(setBucketSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(setIteratorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(setSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(strictEvalActivationSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(stringIteratorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(sourceCodeSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(symbolSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(symbolObjectSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(templateObjectDescriptorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(uint8ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(uint8ClampedArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(uint16ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(uint32ArraySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(unlinkedEvalCodeBlockSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(unlinkedFunctionCodeBlockSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(unlinkedModuleProgramCodeBlockSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(unlinkedProgramCodeBlockSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(finalizationRegistrySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(weakObjectRefSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(weakSetSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(weakMapSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(withScopeSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlCollatorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlDateTimeFormatSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlDisplayNamesSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlLocaleSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlNumberFormatSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlPluralRulesSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlRelativeTimeFormatSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlSegmentIteratorSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlSegmenterSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(intlSegmentsSpace)
#if ENABLE(WEBASSEMBLY)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(jsToWasmICCalleeSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyCodeBlockSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyFunctionSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyGlobalSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyInstanceSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyMemorySpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyModuleSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyModuleRecordSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyTableSpace)
    DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER(webAssemblyWrapperFunctionSpace)
#endif

#undef DYNAMIC_ISO_SUBSPACE_DEFINE_MEMBER
    
    IsoCellSet executableToCodeBlockEdgesWithConstraints;
    IsoCellSet executableToCodeBlockEdgesWithFinalizers;

#define DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER(name) \
    template<SubspaceAccess mode> \
    IsoSubspace* name() \
    { \
        if (auto* spaceAndSet = m_##name.get()) \
            return &spaceAndSet->space; \
        if (mode == SubspaceAccess::Concurrently) \
            return nullptr; \
        return name##Slow(); \
    } \
    IsoSubspace* name##Slow(); \
    std::unique_ptr<SpaceAndSet> m_##name;
    
    struct SpaceAndSet {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        IsoSubspace space;
        IsoCellSet set;
        
        template<typename... Arguments>
        SpaceAndSet(Arguments&&... arguments)
            : space(std::forward<Arguments>(arguments)...)
            , set(space)
        {
        }
        
        static IsoCellSet& setFor(Subspace& space)
        {
            return *bitwise_cast<IsoCellSet*>(
                bitwise_cast<char*>(&space) -
                OBJECT_OFFSETOF(SpaceAndSet, space) +
                OBJECT_OFFSETOF(SpaceAndSet, set));
        }
    };
    
    SpaceAndSet codeBlockSpace;

    template<typename Func>
    void forEachCodeBlockSpace(const Func& func)
    {
        // This should not include webAssemblyCodeBlockSpace because this is about subsclasses of
        // JSC::CodeBlock.
        func(codeBlockSpace);
    }

    DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER(evalExecutableSpace)
    DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER(moduleProgramExecutableSpace)
    SpaceAndSet functionExecutableSpace;
    SpaceAndSet programExecutableSpace;

    template<typename Func>
    void forEachScriptExecutableSpace(const Func& func)
    {
        if (m_evalExecutableSpace)
            func(*m_evalExecutableSpace);
        func(functionExecutableSpace);
        if (m_moduleProgramExecutableSpace)
            func(*m_moduleProgramExecutableSpace);
        func(programExecutableSpace);
    }

    SpaceAndSet unlinkedFunctionExecutableSpace;

#undef DYNAMIC_SPACE_AND_SET_DEFINE_MEMBER

    VMType vmType;
    ClientData* clientData;
    EntryFrame* topEntryFrame { nullptr };
    // NOTE: When throwing an exception while rolling back the call frame, this may be equal to
    // topEntryFrame.
    // FIXME: This should be a void*, because it might not point to a CallFrame.
    // https://bugs.webkit.org/show_bug.cgi?id=160441
    CallFrame* topCallFrame { nullptr };
#if ENABLE(WEBASSEMBLY)
    Wasm::Context wasmContext;
#endif
    Strong<Structure> structureStructure;
    Strong<Structure> structureRareDataStructure;
    Strong<Structure> terminatedExecutionErrorStructure;
    Strong<Structure> stringStructure;
    Strong<Structure> propertyNameEnumeratorStructure;
    Strong<Structure> getterSetterStructure;
    Strong<Structure> customGetterSetterStructure;
    Strong<Structure> domAttributeGetterSetterStructure;
    Strong<Structure> scopedArgumentsTableStructure;
    Strong<Structure> apiWrapperStructure;
    Strong<Structure> nativeExecutableStructure;
    Strong<Structure> evalExecutableStructure;
    Strong<Structure> programExecutableStructure;
    Strong<Structure> functionExecutableStructure;
#if ENABLE(WEBASSEMBLY)
    Strong<Structure> webAssemblyCodeBlockStructure;
#endif
    Strong<Structure> moduleProgramExecutableStructure;
    Strong<Structure> regExpStructure;
    Strong<Structure> symbolStructure;
    Strong<Structure> symbolTableStructure;
    Strong<Structure> immutableButterflyStructures[NumberOfCopyOnWriteIndexingModes];
    Strong<Structure> sourceCodeStructure;
    Strong<Structure> scriptFetcherStructure;
    Strong<Structure> scriptFetchParametersStructure;
    Strong<Structure> structureChainStructure;
    Strong<Structure> sparseArrayValueMapStructure;
    Strong<Structure> templateObjectDescriptorStructure;
    Strong<Structure> unlinkedFunctionExecutableStructure;
    Strong<Structure> unlinkedProgramCodeBlockStructure;
    Strong<Structure> unlinkedEvalCodeBlockStructure;
    Strong<Structure> unlinkedFunctionCodeBlockStructure;
    Strong<Structure> unlinkedModuleProgramCodeBlockStructure;
    Strong<Structure> propertyTableStructure;
    Strong<Structure> functionRareDataStructure;
    Strong<Structure> exceptionStructure;
    Strong<Structure> programCodeBlockStructure;
    Strong<Structure> moduleProgramCodeBlockStructure;
    Strong<Structure> evalCodeBlockStructure;
    Strong<Structure> functionCodeBlockStructure;
    Strong<Structure> hashMapBucketSetStructure;
    Strong<Structure> hashMapBucketMapStructure;
    Strong<Structure> bigIntStructure;
    Strong<Structure> executableToCodeBlockEdgeStructure;

    Strong<JSPropertyNameEnumerator> m_emptyPropertyNameEnumerator;

    Strong<JSCell> m_sentinelSetBucket;
    Strong<JSCell> m_sentinelMapBucket;

    Weak<NativeExecutable> m_fastBoundExecutable;
    Weak<NativeExecutable> m_fastCanConstructBoundExecutable;
    Weak<NativeExecutable> m_slowBoundExecutable;
    Weak<NativeExecutable> m_slowCanConstructBoundExecutable;

    Ref<DeferredWorkTimer> deferredWorkTimer;

    JSCell* currentlyDestructingCallbackObject;
    const ClassInfo* currentlyDestructingCallbackObjectClassInfo { nullptr };

    AtomStringTable* m_atomStringTable;
    WTF::SymbolRegistry m_symbolRegistry;
    CommonIdentifiers* propertyNames;
    const ArgList* emptyList;
    SmallStrings smallStrings;
    NumericStrings numericStrings;
    std::unique_ptr<SimpleStats> machineCodeBytesPerBytecodeWordForBaselineJIT;
    WeakGCMap<std::pair<CustomGetterSetter*, int>, JSCustomGetterSetterFunction> customGetterSetterFunctionMap;
    WeakGCMap<StringImpl*, JSString, PtrHash<StringImpl*>> stringCache;
    Strong<JSString> lastCachedString;

    AtomStringTable* atomStringTable() const { return m_atomStringTable; }
    WTF::SymbolRegistry& symbolRegistry() { return m_symbolRegistry; }

    Strong<JSBigInt> heapBigIntConstantOne;

    JSCell* sentinelSetBucket()
    {
        if (LIKELY(m_sentinelSetBucket))
            return m_sentinelSetBucket.get();
        return sentinelSetBucketSlow();
    }

    JSCell* sentinelMapBucket()
    {
        if (LIKELY(m_sentinelMapBucket))
            return m_sentinelMapBucket.get();
        return sentinelMapBucketSlow();
    }

    JSPropertyNameEnumerator* emptyPropertyNameEnumerator()
    {
        if (LIKELY(m_emptyPropertyNameEnumerator))
            return m_emptyPropertyNameEnumerator.get();
        return emptyPropertyNameEnumeratorSlow();
    }

    WeakGCMap<SymbolImpl*, Symbol, PtrHash<SymbolImpl*>> symbolImplToSymbolMap;

    enum class DeletePropertyMode {
        // Default behaviour of deleteProperty, matching the spec.
        Default,
        // This setting causes deleteProperty to force deletion of all
        // properties including those that are non-configurable (DontDelete).
        IgnoreConfigurable
    };

    DeletePropertyMode deletePropertyMode()
    {
        return m_deletePropertyMode;
    }

    class DeletePropertyModeScope {
    public:
        DeletePropertyModeScope(VM& vm, DeletePropertyMode mode)
            : m_vm(vm)
            , m_previousMode(vm.m_deletePropertyMode)
        {
            m_vm.m_deletePropertyMode = mode;
        }

        ~DeletePropertyModeScope()
        {
            m_vm.m_deletePropertyMode = m_previousMode;
        }

    private:
        VM& m_vm;
        DeletePropertyMode m_previousMode;
    };

    static JS_EXPORT_PRIVATE bool canUseAssembler();
    static bool isInMiniMode()
    {
        return !Options::useJIT() || Options::forceMiniVMMode();
    }

    static bool useUnlinkedCodeBlockJettisoning()
    {
        return Options::useUnlinkedCodeBlockJettisoning() || isInMiniMode();
    }

    static void computeCanUseJIT();

    SourceProviderCache* addSourceProviderCache(SourceProvider*);
    void clearSourceProviderCaches();

    StructureCache structureCache;

    typedef HashMap<RefPtr<SourceProvider>, RefPtr<SourceProviderCache>> SourceProviderCacheMap;
    SourceProviderCacheMap sourceProviderCacheMap;
    Interpreter* interpreter;
#if ENABLE(JIT)
    std::unique_ptr<JITThunks> jitStubs;
    MacroAssemblerCodeRef<JITThunkPtrTag> getCTIStub(ThunkGenerator);

#endif // ENABLE(JIT)
#if ENABLE(FTL_JIT)
    std::unique_ptr<FTL::Thunks> ftlThunks;
#endif
    NativeExecutable* getHostFunction(NativeFunction, NativeFunction constructor, const String& name);
    NativeExecutable* getHostFunction(NativeFunction, Intrinsic, NativeFunction constructor, const DOMJIT::Signature*, const String& name);

    NativeExecutable* getBoundFunction(bool isJSFunction, bool canConstruct);

    MacroAssemblerCodePtr<JSEntryPtrTag> getCTIInternalFunctionTrampolineFor(CodeSpecializationKind);

    static ptrdiff_t exceptionOffset()
    {
        return OBJECT_OFFSETOF(VM, m_exception);
    }

    static ptrdiff_t callFrameForCatchOffset()
    {
        return OBJECT_OFFSETOF(VM, callFrameForCatch);
    }

    static ptrdiff_t topEntryFrameOffset()
    {
        return OBJECT_OFFSETOF(VM, topEntryFrame);
    }

    static ptrdiff_t offsetOfEncodedHostCallReturnValue()
    {
        return OBJECT_OFFSETOF(VM, encodedHostCallReturnValue);
    }

    static ptrdiff_t offsetOfHeapBarrierThreshold()
    {
        return OBJECT_OFFSETOF(VM, heap) + OBJECT_OFFSETOF(Heap, m_barrierThreshold);
    }

    static ptrdiff_t offsetOfHeapMutatorShouldBeFenced()
    {
        return OBJECT_OFFSETOF(VM, heap) + OBJECT_OFFSETOF(Heap, m_mutatorShouldBeFenced);
    }

    void restorePreviousException(Exception* exception) { setException(exception); }

    void clearLastException() { m_lastException = nullptr; }

    CallFrame** addressOfCallFrameForCatch() { return &callFrameForCatch; }

    JSCell** addressOfException() { return reinterpret_cast<JSCell**>(&m_exception); }

    Exception* lastException() const { return m_lastException; }
    JSCell** addressOfLastException() { return reinterpret_cast<JSCell**>(&m_lastException); }

    // This should only be used for test or assertion code that wants to inspect
    // the pending exception without interfering with Throw/CatchScopes.
    Exception* exceptionForInspection() const { return m_exception; }

    void setFailNextNewCodeBlock() { m_failNextNewCodeBlock = true; }
    bool getAndClearFailNextNewCodeBlock()
    {
        bool result = m_failNextNewCodeBlock;
        m_failNextNewCodeBlock = false;
        return result;
    }
    
    ALWAYS_INLINE Structure* getStructure(StructureID id)
    {
        return heap.structureIDTable().get(decontaminate(id));
    }

    // FIXME: rdar://69036888: remove this function when no longer needed.
    ALWAYS_INLINE Structure* tryGetStructure(StructureID id)
    {
        return heap.structureIDTable().tryGet(decontaminate(id));
    }

    void* stackPointerAtVMEntry() const { return m_stackPointerAtVMEntry; }
    void setStackPointerAtVMEntry(void*);

    size_t softReservedZoneSize() const { return m_currentSoftReservedZoneSize; }
    size_t updateSoftReservedZoneSize(size_t softReservedZoneSize);
    
    static size_t committedStackByteCount();
    inline bool ensureStackCapacityFor(Register* newTopOfStack);

    void* stackLimit() { return m_stackLimit; }
    void* softStackLimit() { return m_softStackLimit; }
    void** addressOfSoftStackLimit() { return &m_softStackLimit; }
#if ENABLE(C_LOOP)
    void* cloopStackLimit() { return m_cloopStackLimit; }
    void setCLoopStackLimit(void* limit) { m_cloopStackLimit = limit; }
    JS_EXPORT_PRIVATE void* currentCLoopStackPointer() const;
#endif

    inline bool isSafeToRecurseSoft() const;
    bool isSafeToRecurse() const
    {
        return isSafeToRecurse(m_stackLimit);
    }

    void** addressOfLastStackTop() { return &m_lastStackTop; }
    void* lastStackTop() { return m_lastStackTop; }
    void setLastStackTop(void*);
    
    void firePrimitiveGigacageEnabledIfNecessary()
    {
        if (m_needToFirePrimitiveGigacageEnabled) {
            m_needToFirePrimitiveGigacageEnabled = false;
            m_primitiveGigacageEnabled.fireAll(*this, "Primitive gigacage disabled asynchronously");
        }
    }

    EncodedJSValue encodedHostCallReturnValue { };
    unsigned varargsLength;
    CallFrame* newCallFrameReturnValue;
    CallFrame* callFrameForCatch;
    void* targetMachinePCForThrow;
    const Instruction* targetInterpreterPCForThrow;
    uint32_t osrExitIndex;
    void* osrExitJumpDestination;
    bool isExecutingInRegExpJIT { false };

    // The threading protocol here is as follows:
    // - You can call scratchBufferForSize from any thread.
    // - You can only set the ScratchBuffer's activeLength from the main thread.
    // - You can only write to entries in the ScratchBuffer from the main thread.
    ScratchBuffer* scratchBufferForSize(size_t size);
    void clearScratchBuffers();

    EncodedJSValue* exceptionFuzzingBuffer(size_t size)
    {
        ASSERT(Options::useExceptionFuzz());
        if (!m_exceptionFuzzBuffer)
            m_exceptionFuzzBuffer = MallocPtr<EncodedJSValue, VMMalloc>::malloc(size);
        return m_exceptionFuzzBuffer.get();
    }

    void gatherScratchBufferRoots(ConservativeRoots&);

    static constexpr unsigned expectedMaxActiveSideStateCount = 4;
    void pushCheckpointOSRSideState(std::unique_ptr<CheckpointOSRExitSideState>&&);
    std::unique_ptr<CheckpointOSRExitSideState> popCheckpointOSRSideState(CallFrame* expectedFrame);
    void popAllCheckpointOSRSideStateUntil(CallFrame* targetFrame);
    bool hasCheckpointOSRSideState() const { return m_checkpointSideState.size(); }
    void scanSideState(ConservativeRoots&) const;

    unsigned disallowVMEntryCount { 0 };
    VMEntryScope* entryScope;

    JSObject* stringRecursionCheckFirstObject { nullptr };
    HashSet<JSObject*> stringRecursionCheckVisitedObjects;

    struct DateCache {
        DateInstanceCache dateInstanceCache;
        LocalTimeOffsetCache utcTimeOffsetCache;
        LocalTimeOffsetCache localTimeOffsetCache;

        String cachedDateString;
        double cachedDateStringValue;
    };
    DateCache dateCache;

    std::unique_ptr<Profiler::Database> m_perBytecodeProfiler;
    RefPtr<TypedArrayController> m_typedArrayController;
    RegExpCache* m_regExpCache;
    BumpPointerAllocator m_regExpAllocator;
    ConcurrentJSLock m_regExpAllocatorLock;

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    static constexpr size_t patternContextBufferSize = 8192; // Space allocated to save nested parenthesis context
    UniqueArray<char> m_regExpPatternContexBuffer;
    Lock m_regExpPatternContextLock;
    char* acquireRegExpPatternContexBuffer();
    void releaseRegExpPatternContexBuffer();
#else
    static constexpr size_t patternContextBufferSize = 0; // Space allocated to save nested parenthesis context
#endif

    Ref<CompactVariableMap> m_compactVariableMap;

    std::unique_ptr<HasOwnPropertyCache> m_hasOwnPropertyCache;
    ALWAYS_INLINE HasOwnPropertyCache* hasOwnPropertyCache() { return m_hasOwnPropertyCache.get(); }
    HasOwnPropertyCache* ensureHasOwnPropertyCache();

#if ENABLE(REGEXP_TRACING)
    typedef ListHashSet<RegExp*> RTTraceList;
    RTTraceList* m_rtTraceList;
#endif

    JS_EXPORT_PRIVATE void resetDateCache();

    RegExpCache* regExpCache() { return m_regExpCache; }
#if ENABLE(REGEXP_TRACING)
    void addRegExpToTrace(RegExp*);
#endif
    JS_EXPORT_PRIVATE void dumpRegExpTrace();

    bool isCollectorBusyOnCurrentThread() { return heap.isCurrentThreadBusy(); }

#if ENABLE(GC_VALIDATION)
    bool isInitializingObject() const; 
    void setInitializingObjectClass(const ClassInfo*);
#endif

    bool currentThreadIsHoldingAPILock() const { return m_apiLock->currentThreadIsHoldingLock(); }

    JSLock& apiLock() { return *m_apiLock; }
    CodeCache* codeCache() { return m_codeCache.get(); }
    IntlCache& intlCache() { return *m_intlCache; }

    JS_EXPORT_PRIVATE void whenIdle(Function<void()>&&);

    JS_EXPORT_PRIVATE void deleteAllCode(DeleteAllCodeEffort);
    JS_EXPORT_PRIVATE void deleteAllLinkedCode(DeleteAllCodeEffort);

    void shrinkFootprintWhenIdle();

    WatchpointSet* ensureWatchpointSetForImpureProperty(UniquedStringImpl*);
    
    // FIXME: Use AtomString once it got merged with Identifier.
    JS_EXPORT_PRIVATE void addImpureProperty(UniquedStringImpl*);
    
    InlineWatchpointSet& primitiveGigacageEnabled() { return m_primitiveGigacageEnabled; }

    BuiltinExecutables* builtinExecutables() { return m_builtinExecutables.get(); }

    bool enableTypeProfiler();
    bool disableTypeProfiler();
    TypeProfilerLog* typeProfilerLog() { return m_typeProfilerLog.get(); }
    TypeProfiler* typeProfiler() { return m_typeProfiler.get(); }
    JS_EXPORT_PRIVATE void dumpTypeProfilerData();

    FunctionHasExecutedCache* functionHasExecutedCache() { return &m_functionHasExecutedCache; }

    ControlFlowProfiler* controlFlowProfiler() { return m_controlFlowProfiler.get(); }
    bool enableControlFlowProfiler();
    bool disableControlFlowProfiler();

    void queueMicrotask(JSGlobalObject&, Ref<Microtask>&&);
    JS_EXPORT_PRIVATE void drainMicrotasks();
    void setOnEachMicrotaskTick(WTF::Function<void(VM&)>&& func) { m_onEachMicrotaskTick = WTFMove(func); }
    void finalizeSynchronousJSExecution() { ASSERT(currentThreadIsHoldingAPILock()); m_currentWeakRefVersion++; }
    uintptr_t currentWeakRefVersion() const { return m_currentWeakRefVersion; }

    void setGlobalConstRedeclarationShouldThrow(bool globalConstRedeclarationThrow) { m_globalConstRedeclarationShouldThrow = globalConstRedeclarationThrow; }
    ALWAYS_INLINE bool globalConstRedeclarationShouldThrow() const { return m_globalConstRedeclarationShouldThrow; }

    void setShouldBuildPCToCodeOriginMapping() { m_shouldBuildPCToCodeOriginMapping = true; }
    bool shouldBuilderPCToCodeOriginMapping() const { return m_shouldBuildPCToCodeOriginMapping; }

    BytecodeIntrinsicRegistry& bytecodeIntrinsicRegistry() { return *m_bytecodeIntrinsicRegistry; }
    
    ShadowChicken* shadowChicken() { return m_shadowChicken.get(); }
    void ensureShadowChicken();
    
    template<typename Func>
    void logEvent(CodeBlock*, const char* summary, const Func& func);

    Optional<RefPtr<Thread>> ownerThread() const { return m_apiLock->ownerThread(); }

    VMTraps& traps() { return m_traps; }

    void handleTraps(JSGlobalObject* globalObject, CallFrame* callFrame, VMTraps::Mask mask = VMTraps::Mask::allEventTypes()) { m_traps.handleTraps(globalObject, callFrame, mask); }

    bool needTrapHandling() { return m_traps.needTrapHandling(); }
    bool needTrapHandling(VMTraps::Mask mask) { return m_traps.needTrapHandling(mask); }
    void* needTrapHandlingAddress() { return m_traps.needTrapHandlingAddress(); }

    void notifyNeedDebuggerBreak() { m_traps.fireTrap(VMTraps::NeedDebuggerBreak); }
    void notifyNeedShellTimeoutCheck() { m_traps.fireTrap(VMTraps::NeedShellTimeoutCheck); }
    void notifyNeedTermination() { m_traps.fireTrap(VMTraps::NeedTermination); }
    void notifyNeedWatchdogCheck() { m_traps.fireTrap(VMTraps::NeedWatchdogCheck); }

    void promiseRejected(JSPromise*);

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    StackTrace* nativeStackTraceOfLastThrow() const { return m_nativeStackTraceOfLastThrow.get(); }
    Thread* throwingThread() const { return m_throwingThread.get(); }
    bool needExceptionCheck() const { return m_needExceptionCheck; }
#endif

    WTF::RunLoop& runLoop() const { return m_runLoop; }

    static void setCrashOnVMCreation(bool);

    class DeferExceptionScope {
    public:
        DeferExceptionScope(VM& vm)
            : m_savedException(vm.m_exception, nullptr)
            , m_savedLastException(vm.m_lastException, nullptr)
        {
        }

    private:
        SetForScope<Exception*> m_savedException;
        SetForScope<Exception*> m_savedLastException;
    };

    void addLoopHintExecutionCounter(const Instruction*);
    uint64_t* getLoopHintExecutionCounter(const Instruction*);
    void removeLoopHintExecutionCounter(const Instruction*);

private:
    friend class LLIntOffsetsExtractor;

    VM(VMType, HeapType, WTF::RunLoop* = nullptr, bool* success = nullptr);
    static VM*& sharedInstanceInternal();
    void createNativeThunk();

    JS_EXPORT_PRIVATE JSCell* sentinelSetBucketSlow();
    JS_EXPORT_PRIVATE JSCell* sentinelMapBucketSlow();
    JSPropertyNameEnumerator* emptyPropertyNameEnumeratorSlow();

    void updateStackLimits();

    bool isSafeToRecurse(void* stackLimit) const
    {
        void* curr = currentStackPointer();
        return curr >= stackLimit;
    }

    void setException(Exception* exception)
    {
        m_exception = exception;
        m_lastException = exception;
    }
    Exception* exception() const
    {
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
        m_needExceptionCheck = false;
#endif
        return m_exception;
    }
    void clearException()
    {
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
        m_needExceptionCheck = false;
        m_nativeStackTraceOfLastThrow = nullptr;
        m_throwingThread = nullptr;
#endif
        m_exception = nullptr;
    }

#if ENABLE(C_LOOP)
    bool ensureStackCapacityForCLoop(Register* newTopOfStack);
    bool isSafeToRecurseSoftCLoop() const;
#endif // ENABLE(C_LOOP)

    JS_EXPORT_PRIVATE Exception* throwException(JSGlobalObject*, Exception*);
    JS_EXPORT_PRIVATE Exception* throwException(JSGlobalObject*, JSValue);
    JS_EXPORT_PRIVATE Exception* throwException(JSGlobalObject*, JSObject*);

#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    void verifyExceptionCheckNeedIsSatisfied(unsigned depth, ExceptionEventLocation&);
#endif
    
    static void primitiveGigacageDisabledCallback(void*);
    void primitiveGigacageDisabled();

    void callPromiseRejectionCallback(Strong<JSPromise>&);
    void didExhaustMicrotaskQueue();

#if ENABLE(GC_VALIDATION)
    const ClassInfo* m_initializingObjectClass;
#endif

    void* m_stackPointerAtVMEntry;
    size_t m_currentSoftReservedZoneSize;
    void* m_stackLimit { nullptr };
    void* m_softStackLimit { nullptr };
#if ENABLE(C_LOOP)
    void* m_cloopStackLimit { nullptr };
#endif
    void* m_lastStackTop { nullptr };

    Exception* m_exception { nullptr };
    Exception* m_lastException { nullptr };
#if ENABLE(EXCEPTION_SCOPE_VERIFICATION)
    ExceptionScope* m_topExceptionScope { nullptr };
    ExceptionEventLocation m_simulatedThrowPointLocation;
    unsigned m_simulatedThrowPointRecursionDepth { 0 };
    mutable bool m_needExceptionCheck { false };
    std::unique_ptr<StackTrace> m_nativeStackTraceOfLastThrow;
    std::unique_ptr<StackTrace> m_nativeStackTraceOfLastSimulatedThrow;
    RefPtr<Thread> m_throwingThread;
#endif

public:
    bool didEnterVM { false };
private:
    bool m_failNextNewCodeBlock { false };
    DeletePropertyMode m_deletePropertyMode { DeletePropertyMode::Default };
    bool m_globalConstRedeclarationShouldThrow { true };
    bool m_shouldBuildPCToCodeOriginMapping { false };
    std::unique_ptr<CodeCache> m_codeCache;
    std::unique_ptr<IntlCache> m_intlCache;
    std::unique_ptr<BuiltinExecutables> m_builtinExecutables;
    HashMap<RefPtr<UniquedStringImpl>, RefPtr<WatchpointSet>> m_impurePropertyWatchpointSets;
    std::unique_ptr<TypeProfiler> m_typeProfiler;
    std::unique_ptr<TypeProfilerLog> m_typeProfilerLog;
    unsigned m_typeProfilerEnabledCount;
    bool m_needToFirePrimitiveGigacageEnabled { false };
    Lock m_scratchBufferLock;
    Vector<ScratchBuffer*> m_scratchBuffers;
    size_t m_sizeOfLastScratchBuffer { 0 };
    Vector<std::unique_ptr<CheckpointOSRExitSideState>, expectedMaxActiveSideStateCount> m_checkpointSideState;
    InlineWatchpointSet m_primitiveGigacageEnabled;
    FunctionHasExecutedCache m_functionHasExecutedCache;
    std::unique_ptr<ControlFlowProfiler> m_controlFlowProfiler;
    unsigned m_controlFlowProfilerEnabledCount;
    Deque<std::unique_ptr<QueuedTask>> m_microtaskQueue;
    MallocPtr<EncodedJSValue, VMMalloc> m_exceptionFuzzBuffer;
    VMTraps m_traps;
    RefPtr<Watchdog> m_watchdog;
    std::unique_ptr<HeapProfiler> m_heapProfiler;
#if ENABLE(SAMPLING_PROFILER)
    RefPtr<SamplingProfiler> m_samplingProfiler;
#endif
    std::unique_ptr<FuzzerAgent> m_fuzzerAgent;
    std::unique_ptr<ShadowChicken> m_shadowChicken;
    std::unique_ptr<BytecodeIntrinsicRegistry> m_bytecodeIntrinsicRegistry;

    // FIXME: We should remove handled promises from this list at GC flip. <https://webkit.org/b/201005>
    Vector<Strong<JSPromise>> m_aboutToBeNotifiedRejectedPromises;

    WTF::Function<void(VM&)> m_onEachMicrotaskTick;
    uintptr_t m_currentWeakRefVersion { 0 };

    Lock m_loopHintExecutionCountLock;
    HashMap<const Instruction*, std::pair<unsigned, std::unique_ptr<uint64_t>>> m_loopHintExecutionCounts;

    VM* m_prev; // Required by DoublyLinkedListNode.
    VM* m_next; // Required by DoublyLinkedListNode.

    // Friends for exception checking purpose only.
    friend class Heap;
    friend class CatchScope;
    friend class ExceptionScope;
    friend class JSDollarVMHelper;
    friend class ThrowScope;
    friend class VMTraps;
    friend class WTF::DoublyLinkedListNode<VM>;
};

#if ENABLE(GC_VALIDATION)
inline bool VM::isInitializingObject() const
{
    return !!m_initializingObjectClass;
}

inline void VM::setInitializingObjectClass(const ClassInfo* initializingObjectClass)
{
    m_initializingObjectClass = initializingObjectClass;
}
#endif

inline Heap* WeakSet::heap() const
{
    return &m_vm->heap;
}

#if !ENABLE(C_LOOP)
extern "C" void sanitizeStackForVMImpl(VM*);
#endif

JS_EXPORT_PRIVATE void sanitizeStackForVM(VM&);
void logSanitizeStack(VM&);

} // namespace JSC
