/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Bjoern Graf (bjoern.graf@gmail.com)
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

#include "ArrayBuffer.h"
#include "BigIntConstructor.h"
#include "BytecodeCacheError.h"
#include "CatchScope.h"
#include "CodeBlock.h"
#include "CodeCache.h"
#include "CompilerTimingScope.h"
#include "Completion.h"
#include "ConfigFile.h"
#include "DeferredWorkTimer.h"
#include "Disassembler.h"
#include "Exception.h"
#include "ExceptionHelpers.h"
#include "HeapSnapshotBuilder.h"
#include "InitializeThreading.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSArray.h"
#include "JSArrayBuffer.h"
#include "JSBigInt.h"
#include "JSFinalizationRegistry.h"
#include "JSFunction.h"
#include "JSInternalPromise.h"
#include "JSLock.h"
#include "JSNativeStdFunction.h"
#include "JSONObject.h"
#include "JSObjectInlines.h"
#include "JSSourceCode.h"
#include "JSString.h"
#include "JSTypedArrays.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyMemory.h"
#include "LLIntThunks.h"
#include "ObjectConstructor.h"
#include "ParserError.h"
#include "ProfilerDatabase.h"
#include "ReleaseHeapAccessScope.h"
#include "SamplingProfiler.h"
#include "StackVisitor.h"
#include "StructureInlines.h"
#include "SuperSampler.h"
#include "TestRunnerUtils.h"
#include "TypedArrayInlines.h"
#include "VMInspector.h"
#include "WasmCapabilities.h"
#include "WasmFaultSignalHandler.h"
#include "WasmMemory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <type_traits>
#include <wtf/CPUTime.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Scope.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/threads/Signals.h>

#if OS(WINDOWS)
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#if PLATFORM(COCOA)
#include <crt_externs.h>
#endif

#if HAVE(READLINE)
// readline/history.h has a Function typedef which conflicts with the WTF::Function template from WTF/Forward.h
// We #define it to something else to avoid this conflict.
#define Function ReadlineFunction
#include <readline/history.h>
#include <readline/readline.h>
#undef Function
#endif

#if COMPILER(MSVC)
#include <crtdbg.h>
#include <mmsystem.h>
#include <windows.h>
#endif

#if OS(DARWIN) && CPU(ARM_THUMB2)
#include <fenv.h>
#include <arm/arch.h>
#endif

#if OS(DARWIN)
#include <wtf/spi/darwin/ProcessMemoryFootprint.h>
#elif OS(LINUX)
#include <wtf/linux/ProcessMemoryFootprint.h>
#endif

#if OS(DARWIN) || OS(LINUX)
struct MemoryFootprint : ProcessMemoryFootprint {
    MemoryFootprint(const ProcessMemoryFootprint& src)
        : ProcessMemoryFootprint(src)
    {
    }
};
#else
struct MemoryFootprint {
    uint64_t current;
    uint64_t peak;

    static MemoryFootprint now()
    {
        return { 0L, 0L };
    }
    
    static void resetPeak()
    {
    }
};
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

using namespace JSC;

namespace {

#define EXIT_EXCEPTION 3

NO_RETURN_WITH_VALUE static void jscExit(int status)
{
    waitForAsynchronousDisassembly();
    
#if ENABLE(DFG_JIT)
    if (DFG::isCrashing()) {
        for (;;) {
#if OS(WINDOWS)
            Sleep(1000);
#else
            pause();
#endif
        }
    }
#endif // ENABLE(DFG_JIT)
    exit(status);
}

static unsigned asyncTestPasses { 0 };
static unsigned asyncTestExpectedPasses { 0 };

}

template<typename Vector>
static bool fillBufferWithContentsOfFile(const String& fileName, Vector& buffer);
static RefPtr<Uint8Array> fillBufferWithContentsOfFile(const String& fileName);

class CommandLine;
class GlobalObject;
class Workers;

template<typename Func>
int runJSC(const CommandLine&, bool isWorker, const Func&);
static void checkException(GlobalObject*, bool isLastFile, bool hasException, JSValue, const CommandLine&, bool& success);

class Message : public ThreadSafeRefCounted<Message> {
public:
    Message(ArrayBufferContents&&, int32_t);
    ~Message();
    
    ArrayBufferContents&& releaseContents() { return WTFMove(m_contents); }
    int32_t index() const { return m_index; }

private:
    ArrayBufferContents m_contents;
    int32_t m_index { 0 };
};

class Worker : public BasicRawSentinelNode<Worker> {
public:
    Worker(Workers&);
    ~Worker();
    
    void enqueue(const AbstractLocker&, RefPtr<Message>);
    RefPtr<Message> dequeue();
    
    static Worker& current();

private:
    static ThreadSpecific<Worker*>& currentWorker();

    Workers& m_workers;
    Deque<RefPtr<Message>> m_messages;
};

class Workers {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Workers);
public:
    Workers();
    ~Workers();
    
    template<typename Func>
    void broadcast(const Func&);
    
    void report(const String&);
    String tryGetReport();
    String getReport();
    
    static Workers& singleton();
    
private:
    friend class Worker;
    
    Lock m_lock;
    Condition m_condition;
    SentinelLinkedList<Worker, BasicRawSentinelNode<Worker>> m_workers;
    Deque<String> m_reports;
};


static EncodedJSValue JSC_HOST_CALL functionCreateGlobalObject(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionCreateHeapBigInt(JSGlobalObject*, CallFrame*);
#if USE(BIGINT32)
static EncodedJSValue JSC_HOST_CALL functionCreateBigInt32(JSGlobalObject*, CallFrame*);
#endif
static EncodedJSValue JSC_HOST_CALL functionUseBigInt32(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionIsBigInt32(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionIsHeapBigInt(JSGlobalObject*, CallFrame*);

static EncodedJSValue JSC_HOST_CALL functionPrintStdOut(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionPrintStdErr(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDebug(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDescribe(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDescribeArray(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionSleepSeconds(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionJSCStack(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionGCAndSweep(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionFullGC(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionEdenGC(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionHeapSize(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionCreateMemoryFootprint(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionResetMemoryPeak(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionAddressOf(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionVersion(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionRun(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionRunString(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionLoad(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionLoadString(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionReadFile(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionCheckSyntax(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionReadline(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionPreciseTime(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionNeverInlineFunction(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionNoDFG(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionNoFTL(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionNoOSRExitFuzzing(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionOptimizeNextInvocation(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionNumberOfDFGCompiles(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionCallerIsOMGCompiled(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionJSCOptions(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionReoptimizationRetryCount(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionTransferArrayBuffer(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionFailNextNewCodeBlock(JSGlobalObject*, CallFrame*);
static NO_RETURN_WITH_VALUE EncodedJSValue JSC_HOST_CALL functionQuit(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionFalse(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionUndefined1(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionUndefined2(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionIsInt32(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionIsPureNaN(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionEffectful42(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionIdentity(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionMakeMasquerader(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionCallMasquerader(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionHasCustomProperties(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDumpTypesForAllVariables(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDrainMicrotasks(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionSetTimeout(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionReleaseWeakRefs(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionFinalizationRegistryLiveCount(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionFinalizationRegistryDeadCount(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionIs32BitPlatform(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionCheckModuleSyntax(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionPlatformSupportsSamplingProfiler(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshot(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshotForGCDebugging(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionResetSuperSamplerState(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionEnsureArrayStorage(JSGlobalObject*, CallFrame*);
#if ENABLE(SAMPLING_PROFILER)
static EncodedJSValue JSC_HOST_CALL functionStartSamplingProfiler(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionSamplingProfilerStackTraces(JSGlobalObject*, CallFrame*);
#endif

static EncodedJSValue JSC_HOST_CALL functionMaxArguments(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionAsyncTestStart(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionAsyncTestPassed(JSGlobalObject*, CallFrame*);

#if ENABLE(WEBASSEMBLY)
static EncodedJSValue JSC_HOST_CALL functionWebAssemblyMemoryMode(JSGlobalObject*, CallFrame*);
#endif

#if ENABLE(SAMPLING_FLAGS)
static EncodedJSValue JSC_HOST_CALL functionSetSamplingFlags(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionClearSamplingFlags(JSGlobalObject*, CallFrame*);
#endif

static EncodedJSValue JSC_HOST_CALL functionGetRandomSeed(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionSetRandomSeed(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionIsRope(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionCallerSourceOrigin(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarCreateRealm(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarEvalScript(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentStart(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentReceiveBroadcast(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentReport(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentSleep(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentBroadcast(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentGetReport(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentLeaving(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentMonotonicNow(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionWaitForReport(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionHeapCapacity(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionFlashHeapAccess(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionDisableRichSourceInfo(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionMallocInALoop(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionTotalCompileTime(JSGlobalObject*, CallFrame*);

static EncodedJSValue JSC_HOST_CALL functionSetUnhandledRejectionCallback(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL functionAsDoubleNumber(JSGlobalObject*, CallFrame*);

struct Script {
    enum class StrictMode {
        Strict,
        Sloppy
    };

    enum class ScriptType {
        Script,
        Module
    };

    enum class CodeSource {
        File,
        CommandLine
    };

    StrictMode strictMode;
    CodeSource codeSource;
    ScriptType scriptType;
    char* argument;

    Script(StrictMode strictMode, CodeSource codeSource, ScriptType scriptType, char *argument)
        : strictMode(strictMode)
        , codeSource(codeSource)
        , scriptType(scriptType)
        , argument(argument)
    {
        if (strictMode == StrictMode::Strict)
            ASSERT(codeSource == CodeSource::File);
    }
};

class CommandLine {
public:
    CommandLine(int argc, char** argv)
    {
        parseArguments(argc, argv);
    }

    enum CommandLineForWorkersTag { CommandLineForWorkers };
    CommandLine(CommandLineForWorkersTag)
    {
    }

    Vector<Script> m_scripts;
    Vector<String> m_arguments;
    String m_profilerOutput;
    String m_uncaughtExceptionName;
    bool m_interactive { false };
    bool m_dump { false };
    bool m_module { false };
    bool m_exitCode { false };
    bool m_destroyVM { false };
    bool m_profile { false };
    bool m_treatWatchdogExceptionAsSuccess { false };
    bool m_alwaysDumpUncaughtException { false };
    bool m_dumpMemoryFootprint { false };
    bool m_dumpSamplingProfilerData { false };
    bool m_enableRemoteDebugging { false };

    void parseArguments(int, char**);
};

static const char interactivePrompt[] = ">>> ";

class StopWatch {
public:
    void start();
    void stop();
    long getElapsedMS(); // call stop() first

private:
    MonotonicTime m_startTime;
    MonotonicTime m_stopTime;
};

void StopWatch::start()
{
    m_startTime = MonotonicTime::now();
}

void StopWatch::stop()
{
    m_stopTime = MonotonicTime::now();
}

long StopWatch::getElapsedMS()
{
    return (m_stopTime - m_startTime).millisecondsAs<long>();
}

template<typename Vector>
static inline String stringFromUTF(const Vector& utf8)
{
    return String::fromUTF8WithLatin1Fallback(utf8.data(), utf8.size());
}

class GlobalObject final : public JSGlobalObject {
public:
    using Base = JSGlobalObject;

    static GlobalObject* create(VM& vm, Structure* structure, const Vector<String>& arguments)
    {
        GlobalObject* object = new (NotNull, allocateCell<GlobalObject>(vm.heap)) GlobalObject(vm, structure);
        object->finishCreation(vm, arguments);
        return object;
    }

    DECLARE_INFO;
    static const GlobalObjectMethodTable s_globalObjectMethodTable;

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        return Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    }

    static RuntimeFlags javaScriptRuntimeFlags(const JSGlobalObject*) { return RuntimeFlags::createAllEnabled(); }

private:
    GlobalObject(VM&, Structure*);

    static constexpr unsigned DontEnum = 0 | PropertyAttribute::DontEnum;

    void finishCreation(VM& vm, const Vector<String>& arguments)
    {
        Base::finishCreation(vm);
        JSC_TO_STRING_TAG_WITHOUT_TRANSITION();

        addFunction(vm, "debug", functionDebug, 1);
        addFunction(vm, "describe", functionDescribe, 1);
        addFunction(vm, "describeArray", functionDescribeArray, 1);
        addFunction(vm, "print", functionPrintStdOut, 1);
        addFunction(vm, "printErr", functionPrintStdErr, 1);
        addFunction(vm, "quit", functionQuit, 0);
        addFunction(vm, "gc", functionGCAndSweep, 0);
        addFunction(vm, "fullGC", functionFullGC, 0);
        addFunction(vm, "edenGC", functionEdenGC, 0);
        addFunction(vm, "gcHeapSize", functionHeapSize, 0);
        addFunction(vm, "MemoryFootprint", functionCreateMemoryFootprint, 0);
        addFunction(vm, "resetMemoryPeak", functionResetMemoryPeak, 0);
        addFunction(vm, "addressOf", functionAddressOf, 1);
        addFunction(vm, "version", functionVersion, 1);
        addFunction(vm, "run", functionRun, 1);
        addFunction(vm, "runString", functionRunString, 1);
        addFunction(vm, "load", functionLoad, 1);
        addFunction(vm, "loadString", functionLoadString, 1);
        addFunction(vm, "readFile", functionReadFile, 2);
        addFunction(vm, "read", functionReadFile, 2);
        addFunction(vm, "checkSyntax", functionCheckSyntax, 1);
        addFunction(vm, "sleepSeconds", functionSleepSeconds, 1);
        addFunction(vm, "jscStack", functionJSCStack, 1);
        addFunction(vm, "readline", functionReadline, 0);
        addFunction(vm, "preciseTime", functionPreciseTime, 0);
        addFunction(vm, "neverInlineFunction", functionNeverInlineFunction, 1);
        addFunction(vm, "noInline", functionNeverInlineFunction, 1);
        addFunction(vm, "noDFG", functionNoDFG, 1);
        addFunction(vm, "noFTL", functionNoFTL, 1);
        addFunction(vm, "noOSRExitFuzzing", functionNoOSRExitFuzzing, 1);
        addFunction(vm, "numberOfDFGCompiles", functionNumberOfDFGCompiles, 1);
        addFunction(vm, "callerIsOMGCompiled", functionCallerIsOMGCompiled, 0);
        addFunction(vm, "jscOptions", functionJSCOptions, 0);
        addFunction(vm, "optimizeNextInvocation", functionOptimizeNextInvocation, 1);
        addFunction(vm, "reoptimizationRetryCount", functionReoptimizationRetryCount, 1);
        addFunction(vm, "transferArrayBuffer", functionTransferArrayBuffer, 1);
        addFunction(vm, "failNextNewCodeBlock", functionFailNextNewCodeBlock, 1);
#if ENABLE(SAMPLING_FLAGS)
        addFunction(vm, "setSamplingFlags", functionSetSamplingFlags, 1);
        addFunction(vm, "clearSamplingFlags", functionClearSamplingFlags, 1);
#endif

        putDirectNativeFunction(vm, this, Identifier::fromString(vm, "OSRExit"), 0, functionUndefined1, OSRExitIntrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(vm, "isFinalTier"), 0, functionFalse, IsFinalTierIntrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(vm, "predictInt32"), 0, functionUndefined2, SetInt32HeapPredictionIntrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(vm, "isInt32"), 0, functionIsInt32, CheckInt32Intrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(vm, "isPureNaN"), 0, functionIsPureNaN, CheckInt32Intrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(vm, "fiatInt52"), 0, functionIdentity, FiatInt52Intrinsic, DontEnum);
        
        addFunction(vm, "effectful42", functionEffectful42, 0);
        addFunction(vm, "makeMasquerader", functionMakeMasquerader, 0);
        addFunction(vm, "hasCustomProperties", functionHasCustomProperties, 0);

        addFunction(vm, "createGlobalObject", functionCreateGlobalObject, 0);
        addFunction(vm, "createHeapBigInt", functionCreateHeapBigInt, 1);
#if USE(BIGINT32)
        addFunction(vm, "createBigInt32", functionCreateBigInt32, 1);
#endif
        addFunction(vm, "useBigInt32", functionUseBigInt32, 0);
        addFunction(vm, "isBigInt32", functionIsBigInt32, 1);
        addFunction(vm, "isHeapBigInt", functionIsHeapBigInt, 1);

        addFunction(vm, "dumpTypesForAllVariables", functionDumpTypesForAllVariables , 0);

        addFunction(vm, "drainMicrotasks", functionDrainMicrotasks, 0);
        addFunction(vm, "setTimeout", functionSetTimeout, 2);

        addFunction(vm, "releaseWeakRefs", functionReleaseWeakRefs, 0);
        addFunction(vm, "finalizationRegistryLiveCount", functionFinalizationRegistryLiveCount, 0);
        addFunction(vm, "finalizationRegistryDeadCount", functionFinalizationRegistryDeadCount, 0);
        
        addFunction(vm, "getRandomSeed", functionGetRandomSeed, 0);
        addFunction(vm, "setRandomSeed", functionSetRandomSeed, 1);
        addFunction(vm, "isRope", functionIsRope, 1);
        addFunction(vm, "callerSourceOrigin", functionCallerSourceOrigin, 0);

        addFunction(vm, "is32BitPlatform", functionIs32BitPlatform, 0);

        addFunction(vm, "checkModuleSyntax", functionCheckModuleSyntax, 1);

        addFunction(vm, "platformSupportsSamplingProfiler", functionPlatformSupportsSamplingProfiler, 0);
        addFunction(vm, "generateHeapSnapshot", functionGenerateHeapSnapshot, 0);
        addFunction(vm, "generateHeapSnapshotForGCDebugging", functionGenerateHeapSnapshotForGCDebugging, 0);
        addFunction(vm, "resetSuperSamplerState", functionResetSuperSamplerState, 0);
        addFunction(vm, "ensureArrayStorage", functionEnsureArrayStorage, 0);
#if ENABLE(SAMPLING_PROFILER)
        addFunction(vm, "startSamplingProfiler", functionStartSamplingProfiler, 0);
        addFunction(vm, "samplingProfilerStackTraces", functionSamplingProfilerStackTraces, 0);
#endif

        addFunction(vm, "maxArguments", functionMaxArguments, 0);

        addFunction(vm, "asyncTestStart", functionAsyncTestStart, 1);
        addFunction(vm, "asyncTestPassed", functionAsyncTestPassed, 1);

#if ENABLE(WEBASSEMBLY)
        addFunction(vm, "WebAssemblyMemoryMode", functionWebAssemblyMemoryMode, 1);
#endif

        if (!arguments.isEmpty()) {
            JSArray* array = constructEmptyArray(this, nullptr);
            for (size_t i = 0; i < arguments.size(); ++i)
                array->putDirectIndex(this, i, jsString(vm, arguments[i]));
            putDirect(vm, Identifier::fromString(vm, "arguments"), array, DontEnum);
        }

        putDirect(vm, Identifier::fromString(vm, "console"), jsUndefined(), DontEnum);
        
        Structure* plainObjectStructure = JSFinalObject::createStructure(vm, this, objectPrototype(), 0);
        
        JSObject* dollar = JSFinalObject::create(vm, plainObjectStructure);
        putDirect(vm, Identifier::fromString(vm, "$"), dollar, DontEnum);
        putDirect(vm, Identifier::fromString(vm, "$262"), dollar, DontEnum);
        
        addFunction(vm, dollar, "createRealm", functionDollarCreateRealm, 0);
        addFunction(vm, dollar, "detachArrayBuffer", functionTransferArrayBuffer, 1);
        addFunction(vm, dollar, "evalScript", functionDollarEvalScript, 1);
        
        dollar->putDirect(vm, Identifier::fromString(vm, "global"), this, DontEnum);
        dollar->putDirectCustomAccessor(vm, Identifier::fromString(vm, "IsHTMLDDA"),
            CustomGetterSetter::create(vm, [](JSGlobalObject* globalObject, EncodedJSValue, PropertyName) {
                return functionMakeMasquerader(globalObject, nullptr);
            }, nullptr),
            static_cast<unsigned>(PropertyAttribute::CustomValue)
        );

        JSObject* agent = JSFinalObject::create(vm, plainObjectStructure);
        dollar->putDirect(vm, Identifier::fromString(vm, "agent"), agent, DontEnum);
        
        // The test262 INTERPRETING.md document says that some of these functions are just in the main
        // thread and some are in the other threads. We just put them in all threads.
        addFunction(vm, agent, "start", functionDollarAgentStart, 1);
        addFunction(vm, agent, "receiveBroadcast", functionDollarAgentReceiveBroadcast, 1);
        addFunction(vm, agent, "report", functionDollarAgentReport, 1);
        addFunction(vm, agent, "sleep", functionDollarAgentSleep, 1);
        addFunction(vm, agent, "broadcast", functionDollarAgentBroadcast, 1);
        addFunction(vm, agent, "getReport", functionDollarAgentGetReport, 0);
        addFunction(vm, agent, "leaving", functionDollarAgentLeaving, 0);
        addFunction(vm, agent, "monotonicNow", functionDollarAgentMonotonicNow, 0);

        addFunction(vm, "waitForReport", functionWaitForReport, 0);

        addFunction(vm, "heapCapacity", functionHeapCapacity, 0);
        addFunction(vm, "flashHeapAccess", functionFlashHeapAccess, 0);

        addFunction(vm, "disableRichSourceInfo", functionDisableRichSourceInfo, 0);
        addFunction(vm, "mallocInALoop", functionMallocInALoop, 0);
        addFunction(vm, "totalCompileTime", functionTotalCompileTime, 0);

        addFunction(vm, "setUnhandledRejectionCallback", functionSetUnhandledRejectionCallback, 1);

        addFunction(vm, "asDoubleNumber", functionAsDoubleNumber, 1);

        if (Options::exposeCustomSettersOnGlobalObjectForTesting()) {
            {
                CustomGetterSetter* custom = CustomGetterSetter::create(vm, nullptr, testCustomAccessorSetter);
                Identifier identifier = Identifier::fromString(vm, "testCustomAccessorSetter");
                this->putDirectCustomAccessor(vm, identifier, custom, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::CustomAccessor);
            }

            {
                CustomGetterSetter* custom = CustomGetterSetter::create(vm, nullptr, testCustomValueSetter);
                Identifier identifier = Identifier::fromString(vm, "testCustomValueSetter");
                this->putDirectCustomAccessor(vm, identifier, custom, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::CustomValue);
            }
        }
    }

    static bool testCustomSetterImpl(JSGlobalObject* lexicalGlobalObject, GlobalObject* thisObject, EncodedJSValue encodedValue, const char* propertyName)
    {
        VM& vm = lexicalGlobalObject->vm();

        Identifier identifier = Identifier::fromString(vm, propertyName);
        thisObject->putDirect(vm, identifier, JSValue::decode(encodedValue), DontEnum);

        return true;
    }

    static bool testCustomAccessorSetter(JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, EncodedJSValue encodedValue)
    {
        VM& vm = lexicalGlobalObject->vm();
        RELEASE_ASSERT(JSValue::decode(thisValue).isCell());
        JSCell* thisCell = JSValue::decode(thisValue).asCell();
        RELEASE_ASSERT(thisCell->type() == PureForwardingProxyType);
        GlobalObject* thisObject = jsDynamicCast<GlobalObject*>(vm, jsCast<JSProxy*>(thisCell)->target());
        RELEASE_ASSERT(thisObject);
        return testCustomSetterImpl(lexicalGlobalObject, thisObject, encodedValue, "_testCustomAccessorSetter");
    }

    static bool testCustomValueSetter(JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, EncodedJSValue encodedValue)
    {
        VM& vm = lexicalGlobalObject->vm();
        RELEASE_ASSERT(JSValue::decode(thisValue).isCell());
        JSCell* thisCell = JSValue::decode(thisValue).asCell();
        GlobalObject* thisObject = jsDynamicCast<GlobalObject*>(vm, thisCell);
        RELEASE_ASSERT(thisObject);
        return testCustomSetterImpl(lexicalGlobalObject, thisObject, encodedValue, "_testCustomValueSetter");
    }
    
    void addFunction(VM& vm, JSObject* object, const char* name, NativeFunction function, unsigned arguments)
    {
        Identifier identifier = Identifier::fromString(vm, name);
        object->putDirect(vm, identifier, JSFunction::create(vm, this, arguments, identifier.string(), function), DontEnum);
    }

    void addFunction(VM& vm, const char* name, NativeFunction function, unsigned arguments)
    {
        addFunction(vm, this, name, function, arguments);
    }
    
    static JSInternalPromise* moduleLoaderImportModule(JSGlobalObject*, JSModuleLoader*, JSString*, JSValue, const SourceOrigin&);
    static Identifier moduleLoaderResolve(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, JSValue);
    static JSInternalPromise* moduleLoaderFetch(JSGlobalObject*, JSModuleLoader*, JSValue, JSValue, JSValue);
    static JSObject* moduleLoaderCreateImportMetaProperties(JSGlobalObject*, JSModuleLoader*, JSValue, JSModuleRecord*, JSValue);

    static void reportUncaughtExceptionAtEventLoop(JSGlobalObject*, Exception*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(GlobalObject, JSGlobalObject);

static bool supportsRichSourceInfo = true;
static bool shellSupportsRichSourceInfo(const JSGlobalObject*)
{
    return supportsRichSourceInfo;
}

const ClassInfo GlobalObject::s_info = { "global", &JSGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(GlobalObject) };
const GlobalObjectMethodTable GlobalObject::s_globalObjectMethodTable = {
    &shellSupportsRichSourceInfo,
    &shouldInterruptScript,
    &javaScriptRuntimeFlags,
    nullptr, // queueTaskToEventLoop
    &shouldInterruptScriptBeforeTimeout,
    &moduleLoaderImportModule,
    &moduleLoaderResolve,
    &moduleLoaderFetch,
    &moduleLoaderCreateImportMetaProperties,
    nullptr, // moduleLoaderEvaluate
    nullptr, // promiseRejectionTracker
    &reportUncaughtExceptionAtEventLoop,
    nullptr, // defaultLanguage
    nullptr, // compileStreaming
    nullptr, // instantinateStreaming
};

GlobalObject::GlobalObject(VM& vm, Structure* structure)
    : JSGlobalObject(vm, structure, &s_globalObjectMethodTable)
{
}

static UChar pathSeparator()
{
#if OS(WINDOWS)
    return '\\';
#else
    return '/';
#endif
}

static URL currentWorkingDirectory()
{
#if OS(WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364934.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#maxpath
    // The _MAX_PATH in Windows is 260. If the path of the current working directory is longer than that, _getcwd truncates the result.
    // And other I/O functions taking a path name also truncate it. To avoid this situation,
    //
    // (1). When opening the file in Windows for modules, we always use the abosolute path and add "\\?\" prefix to the path name.
    // (2). When retrieving the current working directory, use GetCurrentDirectory instead of _getcwd.
    //
    // In the path utility functions inside the JSC shell, we does not handle the UNC and UNCW including the network host name.
    DWORD bufferLength = ::GetCurrentDirectoryW(0, nullptr);
    if (!bufferLength)
        return { };
    // In Windows, wchar_t is the UTF-16LE.
    // https://msdn.microsoft.com/en-us/library/dd374081.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff381407.aspx
    Vector<wchar_t> buffer(bufferLength);
    DWORD lengthNotIncludingNull = ::GetCurrentDirectoryW(bufferLength, buffer.data());
    String directoryString(buffer.data(), lengthNotIncludingNull);
    // We don't support network path like \\host\share\<path name>.
    if (directoryString.startsWith("\\\\"))
        return { };

#else
    Vector<char> buffer(PATH_MAX);
    if (!getcwd(buffer.data(), PATH_MAX))
        return { };
    String directoryString = String::fromUTF8(buffer.data());
#endif
    if (directoryString.isEmpty())
        return { };

    // Add a trailing slash if needed so the URL resolves to a directory and not a file.
    if (directoryString[directoryString.length() - 1] != pathSeparator())
        directoryString = makeString(directoryString, pathSeparator());

    return URL::fileURLWithFileSystemPath(directoryString);
}

static URL absolutePath(const String& fileName)
{
    auto directoryName = currentWorkingDirectory();
    if (!directoryName.isValid())
        return URL::fileURLWithFileSystemPath(fileName);

    return URL(directoryName, fileName);
}

JSInternalPromise* GlobalObject::moduleLoaderImportModule(JSGlobalObject* globalObject, JSModuleLoader*, JSString* moduleNameValue, JSValue parameters, const SourceOrigin& sourceOrigin)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto reject = [&] (JSValue rejectionReason) {
        catchScope.clearException();
        promise->reject(globalObject, rejectionReason);
        catchScope.clearException();
        return promise;
    };

    auto referrer = sourceOrigin.url();
    auto specifier = moduleNameValue->value(globalObject);
    RETURN_IF_EXCEPTION(throwScope, nullptr);
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception()->value());

    if (!referrer.isLocalFile())
        return reject(createError(globalObject, makeString("Could not resolve the referrer's path '", referrer.string(), "', while trying to resolve module '", specifier, "'.")));

    if (!specifier.startsWith('/') && !specifier.startsWith("./") && !specifier.startsWith("../"))
        return reject(createTypeError(globalObject, makeString("Module specifier, '"_s, specifier, "' does not start with \"/\", \"./\", or \"../\". Referenced from: "_s, referrer.fileSystemPath())));

    URL moduleURL(referrer, specifier);
    if (!moduleURL.isLocalFile())
        return reject(createError(globalObject, makeString("Module url, '", moduleURL.string(), "' does not map to a local file.")));

    auto result = JSC::importModule(globalObject, Identifier::fromString(vm, moduleURL.string()), parameters, jsUndefined());
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception()->value());
    return result;
}

Identifier GlobalObject::moduleLoaderResolve(JSGlobalObject* globalObject, JSModuleLoader*, JSValue keyValue, JSValue referrerValue, JSValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    scope.releaseAssertNoException();
    const Identifier key = keyValue.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (key.isSymbol())
        return key;

    auto resolvePath = [&] (const URL& directoryURL) -> Identifier {
        String specifier = key.impl();
        if (!specifier.startsWith('/') && !specifier.startsWith("./") && !specifier.startsWith("../")) {
            throwTypeError(globalObject, scope, makeString("Module specifier, '"_s, specifier, "' does not start with \"/\", \"./\", or \"../\". Referenced from: "_s, directoryURL.fileSystemPath()));
            return { };
        }

        if (!directoryURL.isLocalFile()) {
            throwException(globalObject, scope, createError(globalObject, makeString("Could not resolve the referrer's path: ", directoryURL.string())));
            return { };
        }

        URL resolvedURL(directoryURL, specifier);
        if (!resolvedURL.isValid()) {
            throwException(globalObject, scope, createError(globalObject, makeString("Resolved module url is not valid: ", resolvedURL.string())));
            return { };
        }
        ASSERT(resolvedURL.isLocalFile());

        return Identifier::fromString(vm, resolvedURL.string());
    };

    if (referrerValue.isUndefined())
        return resolvePath(currentWorkingDirectory());

    const Identifier referrer = referrerValue.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (referrer.isSymbol())
        return resolvePath(currentWorkingDirectory());

    // If the referrer exists, we assume that the referrer is the correct file url.
    URL url = URL({ }, referrer.impl());
    ASSERT(url.isLocalFile());
    return resolvePath(url);
}

template<typename Vector>
static void convertShebangToJSComment(Vector& buffer)
{
    if (buffer.size() >= 2) {
        if (buffer[0] == '#' && buffer[1] == '!')
            buffer[0] = buffer[1] = '/';
    }
}

static RefPtr<Uint8Array> fillBufferWithContentsOfFile(FILE* file)
{
    if (fseek(file, 0, SEEK_END) == -1)
        return nullptr;
    long bufferCapacity = ftell(file);
    if (bufferCapacity == -1)
        return nullptr;
    if (fseek(file, 0, SEEK_SET) == -1)
        return nullptr;
    auto result = Uint8Array::tryCreate(bufferCapacity);
    if (!result)
        return nullptr;
    size_t readSize = fread(result->data(), 1, bufferCapacity, file);
    if (readSize != static_cast<size_t>(bufferCapacity))
        return nullptr;
    return result;
}

static RefPtr<Uint8Array> fillBufferWithContentsOfFile(const String& fileName)
{
    FILE* f = fopen(fileName.utf8().data(), "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.utf8().data());
        return nullptr;
    }

    RefPtr<Uint8Array> result = fillBufferWithContentsOfFile(f);
    fclose(f);

    return result;
}

template<typename Vector>
static bool fillBufferWithContentsOfFile(FILE* file, Vector& buffer)
{
    // We might have injected "use strict"; at the top.
    size_t initialSize = buffer.size();
    if (fseek(file, 0, SEEK_END) == -1)
        return false;
    long bufferCapacity = ftell(file);
    if (bufferCapacity == -1)
        return false;
    if (fseek(file, 0, SEEK_SET) == -1)
        return false;
    buffer.resize(bufferCapacity + initialSize);
    size_t readSize = fread(buffer.data() + initialSize, 1, buffer.size(), file);
    return readSize == buffer.size() - initialSize;
}

static bool fillBufferWithContentsOfFile(const String& fileName, Vector<char>& buffer)
{
    FILE* f = fopen(fileName.utf8().data(), "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.utf8().data());
        return false;
    }

    bool result = fillBufferWithContentsOfFile(f, buffer);
    fclose(f);

    return result;
}

static bool fetchScriptFromLocalFileSystem(const String& fileName, Vector<char>& buffer)
{
    if (!fillBufferWithContentsOfFile(fileName, buffer))
        return false;
    convertShebangToJSComment(buffer);
    return true;
}

class ShellSourceProvider final : public StringSourceProvider {
public:
    static Ref<ShellSourceProvider> create(const String& source, const SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, SourceProviderSourceType sourceType)
    {
        return adoptRef(*new ShellSourceProvider(source, sourceOrigin, WTFMove(sourceURL), startPosition, sourceType));
    }

    ~ShellSourceProvider() final
    {
        commitCachedBytecode();
    }

    RefPtr<CachedBytecode> cachedBytecode() const final
    {
        if (!m_cachedBytecode)
            loadBytecode();
        return m_cachedBytecode.copyRef();
    }

    void updateCache(const UnlinkedFunctionExecutable* executable, const SourceCode&, CodeSpecializationKind kind, const UnlinkedFunctionCodeBlock* codeBlock) const final
    {
        if (!cacheEnabled() || !m_cachedBytecode)
            return;
        BytecodeCacheError error;
        RefPtr<CachedBytecode> cachedBytecode = encodeFunctionCodeBlock(executable->vm(), codeBlock, error);
        if (cachedBytecode && !error.isValid())
            m_cachedBytecode->addFunctionUpdate(executable, kind, *cachedBytecode);
    }

    void cacheBytecode(const BytecodeCacheGenerator& generator) const final
    {
        if (!cacheEnabled())
            return;
        if (!m_cachedBytecode)
            m_cachedBytecode = CachedBytecode::create();
        auto update = generator();
        if (update)
            m_cachedBytecode->addGlobalUpdate(*update);
    }

    void commitCachedBytecode() const final
    {
        if (!cacheEnabled() || !m_cachedBytecode || !m_cachedBytecode->hasUpdates())
            return;

        auto clearBytecode = makeScopeExit([&] {
            m_cachedBytecode = nullptr;
        });

        String filename = cachePath();
        auto fd = FileSystem::openAndLockFile(filename, FileSystem::FileOpenMode::Write, {FileSystem::FileLockMode::Exclusive, FileSystem::FileLockMode::Nonblocking});
        if (!FileSystem::isHandleValid(fd))
            return;

        auto closeFD = makeScopeExit([&] {
            FileSystem::unlockAndCloseFile(fd);
        });

        long long fileSize;
        if (!FileSystem::getFileSize(fd, fileSize))
            return;

        size_t cacheFileSize;
        if (!WTF::convertSafely(fileSize, cacheFileSize) || cacheFileSize != m_cachedBytecode->size()) {
            // The bytecode cache has already been updated
            return;
        }

        if (!FileSystem::truncateFile(fd, m_cachedBytecode->sizeForUpdate()))
            return;

        m_cachedBytecode->commitUpdates([&] (off_t offset, const void* data, size_t size) {
            long long result = FileSystem::seekFile(fd, offset, FileSystem::FileSeekOrigin::Beginning);
            ASSERT_UNUSED(result, result != -1);
            size_t bytesWritten = static_cast<size_t>(FileSystem::writeToFile(fd, static_cast<const char*>(data), size));
            ASSERT_UNUSED(bytesWritten, bytesWritten == size);
        });
    }

private:
    String cachePath() const
    {
        if (!cacheEnabled())
            return static_cast<const char*>(nullptr);
        const char* cachePath = Options::diskCachePath();
        String filename = FileSystem::encodeForFileName(FileSystem::lastComponentOfPathIgnoringTrailingSlash(sourceOrigin().url().fileSystemPath()));
        return FileSystem::pathByAppendingComponent(cachePath, makeString(source().toString().hash(), '-', filename, ".bytecode-cache"));
    }

    void loadBytecode() const
    {
        if (!cacheEnabled())
            return;

        String filename = cachePath();
        if (filename.isNull())
            return;

        auto fd = FileSystem::openAndLockFile(filename, FileSystem::FileOpenMode::Read, {FileSystem::FileLockMode::Shared, FileSystem::FileLockMode::Nonblocking});
        if (!FileSystem::isHandleValid(fd))
            return;

        auto closeFD = makeScopeExit([&] {
            FileSystem::unlockAndCloseFile(fd);
        });

        bool success;
        FileSystem::MappedFileData mappedFileData(fd, FileSystem::MappedFileMode::Private, success);

        if (!success)
            return;

        m_cachedBytecode = CachedBytecode::create(WTFMove(mappedFileData));
    }

    ShellSourceProvider(const String& source, const SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, SourceProviderSourceType sourceType)
        : StringSourceProvider(source, sourceOrigin, WTFMove(sourceURL), startPosition, sourceType)
    {
    }

    static bool cacheEnabled()
    {
        static bool enabled = !!Options::diskCachePath();
        return enabled;
    }

    mutable RefPtr<CachedBytecode> m_cachedBytecode;
};

static inline SourceCode jscSource(const String& source, const SourceOrigin& sourceOrigin, String sourceURL = String(), const TextPosition& startPosition = TextPosition(), SourceProviderSourceType sourceType = SourceProviderSourceType::Program)
{
    return SourceCode(ShellSourceProvider::create(source, sourceOrigin, WTFMove(sourceURL), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt());
}

template<typename Vector>
static inline SourceCode jscSource(const Vector& utf8, const SourceOrigin& sourceOrigin, const String& filename)
{
    // FIXME: This should use an absolute file URL https://bugs.webkit.org/show_bug.cgi?id=193077
    String str = stringFromUTF(utf8);
    return jscSource(str, sourceOrigin, filename);
}

template<typename Vector>
static bool fetchModuleFromLocalFileSystem(const URL& fileURL, Vector& buffer)
{
    String fileName = fileURL.fileSystemPath();
#if OS(WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#maxpath
    // Use long UNC to pass the long path name to the Windows APIs.
    auto pathName = makeString("\\\\?\\", fileName).wideCharacters();
    struct _stat status { };
    if (_wstat(pathName.data(), &status))
        return false;
    if ((status.st_mode & S_IFMT) != S_IFREG)
        return false;

    FILE* f = _wfopen(pathName.data(), L"rb");
#else
    auto pathName = fileName.utf8();
    struct stat status { };
    if (stat(pathName.data(), &status))
        return false;
    if ((status.st_mode & S_IFMT) != S_IFREG)
        return false;

    FILE* f = fopen(pathName.data(), "r");
#endif
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.utf8().data());
        return false;
    }

    bool result = fillBufferWithContentsOfFile(f, buffer);
    if (result)
        convertShebangToJSComment(buffer);
    fclose(f);

    return result;
}

JSInternalPromise* GlobalObject::moduleLoaderFetch(JSGlobalObject* globalObject, JSModuleLoader*, JSValue key, JSValue, JSValue)
{
    VM& vm = globalObject->vm();
    JSInternalPromise* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto reject = [&] (JSValue rejectionReason) {
        catchScope.clearException();
        promise->reject(globalObject, rejectionReason);
        catchScope.clearException();
        return promise;
    };

    String moduleKey = key.toWTFString(globalObject);
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception()->value());

    URL moduleURL({ }, moduleKey);
    ASSERT(moduleURL.isLocalFile());
    // Strip the URI from our key so Errors print canonical system paths.
    moduleKey = moduleURL.fileSystemPath();

    Vector<uint8_t> buffer;
    if (!fetchModuleFromLocalFileSystem(moduleURL, buffer))
        return reject(createError(globalObject, makeString("Could not open file '", moduleKey, "'.")));

#if ENABLE(WEBASSEMBLY)
    // FileSystem does not have mime-type header. The JSC shell recognizes WebAssembly's magic header.
    if (buffer.size() >= 4) {
        if (buffer[0] == '\0' && buffer[1] == 'a' && buffer[2] == 's' && buffer[3] == 'm') {
            auto source = SourceCode(WebAssemblySourceProvider::create(WTFMove(buffer), SourceOrigin { moduleURL }, WTFMove(moduleKey)));
            catchScope.releaseAssertNoException();
            auto sourceCode = JSSourceCode::create(vm, WTFMove(source));
            catchScope.releaseAssertNoException();
            promise->resolve(globalObject, sourceCode);
            catchScope.clearException();
            return promise;
        }
    }
#endif

    auto sourceCode = JSSourceCode::create(vm, jscSource(stringFromUTF(buffer), SourceOrigin { moduleURL }, WTFMove(moduleKey), TextPosition(), SourceProviderSourceType::Module));
    catchScope.releaseAssertNoException();
    promise->resolve(globalObject, sourceCode);
    catchScope.clearException();
    return promise;
}

JSObject* GlobalObject::moduleLoaderCreateImportMetaProperties(JSGlobalObject* globalObject, JSModuleLoader*, JSValue key, JSModuleRecord*, JSValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* metaProperties = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    metaProperties->putDirect(vm, Identifier::fromString(vm, "filename"), key);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return metaProperties;
}

static CString cStringFromViewWithString(JSGlobalObject* globalObject, ThrowScope& scope, StringViewWithUnderlyingString& viewWithString)
{
    Expected<CString, UTF8ConversionError> expectedString = viewWithString.view.tryGetUtf8();
    if (expectedString)
        return expectedString.value();
    switch (expectedString.error()) {
    case UTF8ConversionError::OutOfMemory:
        throwOutOfMemoryError(globalObject, scope);
        break;
    case UTF8ConversionError::IllegalSource:
        scope.throwException(globalObject, createError(globalObject, "Illegal source encountered during UTF8 conversion"));
        break;
    case UTF8ConversionError::SourceExhausted:
        scope.throwException(globalObject, createError(globalObject, "Source exhausted during UTF8 conversion"));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return { };
}

static EncodedJSValue printInternal(JSGlobalObject* globalObject, CallFrame* callFrame, FILE* out)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (asyncTestExpectedPasses) {
        JSValue value = callFrame->argument(0);
        if (value.isString() && WTF::equal(asString(value)->value(globalObject).impl(), "Test262:AsyncTestComplete")) {
            asyncTestPasses++;
            return JSValue::encode(jsUndefined());
        }
    }

    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        if (i)
            if (EOF == fputc(' ', out))
                goto fail;

        auto* jsString = callFrame->uncheckedArgument(i).toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto viewWithString = jsString->viewWithUnderlyingString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto string = cStringFromViewWithString(globalObject, scope, viewWithString);
        RETURN_IF_EXCEPTION(scope, { });
        fwrite(string.data(), sizeof(char), string.length(), out);
        if (ferror(out))
            goto fail;
    }

    fputc('\n', out);
fail:
    fflush(out);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionPrintStdOut(JSGlobalObject* globalObject, CallFrame* callFrame) { return printInternal(globalObject, callFrame, stdout); }
EncodedJSValue JSC_HOST_CALL functionPrintStdErr(JSGlobalObject* globalObject, CallFrame* callFrame) { return printInternal(globalObject, callFrame, stderr); }

EncodedJSValue JSC_HOST_CALL functionDebug(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* jsString = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto viewWithString = jsString->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto string = cStringFromViewWithString(globalObject, scope, viewWithString);
    RETURN_IF_EXCEPTION(scope, { });
    fputs("--> ", stderr);
    fwrite(string.data(), sizeof(char), string.length(), stderr);
    fputc('\n', stderr);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDescribe(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    if (callFrame->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, toString(callFrame->argument(0))));
}

EncodedJSValue JSC_HOST_CALL functionDescribeArray(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    VM& vm = globalObject->vm();
    JSObject* object = jsDynamicCast<JSObject*>(vm, callFrame->argument(0));
    if (!object)
        return JSValue::encode(jsNontrivialString(vm, "<not object>"_s));
    return JSValue::encode(jsNontrivialString(vm, toString("<Butterfly: ", RawPointer(object->butterfly()), "; public length: ", object->getArrayLength(), "; vector length: ", object->getVectorLength(), ">")));
}

EncodedJSValue JSC_HOST_CALL functionSleepSeconds(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (callFrame->argumentCount() >= 1) {
        Seconds seconds = Seconds(callFrame->argument(0).toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        sleep(seconds);
    }
    
    return JSValue::encode(jsUndefined());
}

class FunctionJSCStackFunctor {
public:
    FunctionJSCStackFunctor(StringBuilder& trace)
        : m_trace(trace)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        m_trace.append(makeString("    ", visitor->index(), "   ", visitor->toString(), '\n'));
        return StackVisitor::Continue;
    }

private:
    StringBuilder& m_trace;
};

EncodedJSValue JSC_HOST_CALL functionJSCStack(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    StringBuilder trace;
    trace.appendLiteral("--> Stack trace:\n");

    FunctionJSCStackFunctor functor(trace);
    callFrame->iterate(vm, functor);
    fprintf(stderr, "%s", trace.toString().utf8().data());
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionGCAndSweep(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    vm.heap.collectNow(Sync, CollectionScope::Full);
    return JSValue::encode(jsNumber(vm.heap.sizeAfterLastFullCollection()));
}

EncodedJSValue JSC_HOST_CALL functionFullGC(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    vm.heap.collectSync(CollectionScope::Full);
    return JSValue::encode(jsNumber(vm.heap.sizeAfterLastFullCollection()));
}

EncodedJSValue JSC_HOST_CALL functionEdenGC(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    vm.heap.collectSync(CollectionScope::Eden);
    return JSValue::encode(jsNumber(vm.heap.sizeAfterLastEdenCollection()));
}

EncodedJSValue JSC_HOST_CALL functionHeapSize(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(jsNumber(vm.heap.size()));
}

class JSCMemoryFootprint : public JSDestructibleObject {
    using Base = JSDestructibleObject;
public:
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.destructibleObjectSpace;
    }

    JSCMemoryFootprint(VM& vm, Structure* structure)
        : Base(vm, structure)
    { }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSCMemoryFootprint* create(VM& vm, JSGlobalObject* globalObject)
    {
        Structure* structure = createStructure(vm, globalObject, jsNull());
        JSCMemoryFootprint* footprint = new (NotNull, allocateCell<JSCMemoryFootprint>(vm.heap)) JSCMemoryFootprint(vm, structure);
        footprint->finishCreation(vm);
        return footprint;
    }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);

        auto addProperty = [&] (VM& vm, const char* name, JSValue value) {
            JSCMemoryFootprint::addProperty(vm, name, value);
        };

        MemoryFootprint footprint = MemoryFootprint::now();

        addProperty(vm, "current", jsNumber(footprint.current));
        addProperty(vm, "peak", jsNumber(footprint.peak));
    }

    DECLARE_INFO;

private:
    void addProperty(VM& vm, const char* name, JSValue value)
    {
        Identifier identifier = Identifier::fromString(vm, name);
        putDirect(vm, identifier, value);
    }
};

const ClassInfo JSCMemoryFootprint::s_info = { "MemoryFootprint", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCMemoryFootprint) };

EncodedJSValue JSC_HOST_CALL functionCreateMemoryFootprint(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(JSCMemoryFootprint::create(vm, globalObject));
}

EncodedJSValue JSC_HOST_CALL functionResetMemoryPeak(JSGlobalObject*, CallFrame*)
{
    MemoryFootprint::resetPeak();
    return JSValue::encode(jsUndefined());
}

// This function is not generally very helpful in 64-bit code as the tag and payload
// share a register. But in 32-bit JITed code the tag may not be checked if an
// optimization removes type checking requirements, such as in ===.
EncodedJSValue JSC_HOST_CALL functionAddressOf(JSGlobalObject*, CallFrame* callFrame)
{
    JSValue value = callFrame->argument(0);
    if (!value.isCell())
        return JSValue::encode(jsUndefined());
    // Need to cast to uint64_t so bitwise_cast will play along.
    uint64_t asNumber = reinterpret_cast<uint64_t>(value.asCell());
    EncodedJSValue returnValue = JSValue::encode(jsNumber(bitwise_cast<double>(asNumber)));
    return returnValue;
}

EncodedJSValue JSC_HOST_CALL functionVersion(JSGlobalObject*, CallFrame*)
{
    // We need this function for compatibility with the Mozilla JS tests but for now
    // we don't actually do any version-specific handling
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionRun(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String fileName = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Could not open file."_s)));

    GlobalObject* realm = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>());

    JSArray* array = constructEmptyArray(realm, nullptr);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    for (unsigned i = 1; i < callFrame->argumentCount(); ++i) {
        array->putDirectIndex(realm, i - 1, callFrame->uncheckedArgument(i));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    realm->putDirect(vm, Identifier::fromString(vm, "arguments"), array);

    NakedPtr<Exception> exception;
    StopWatch stopWatch;
    stopWatch.start();
    evaluate(realm, jscSource(script, SourceOrigin { absolutePath(fileName) }, fileName), JSValue(), exception);
    stopWatch.stop();

    if (exception) {
        throwException(realm, scope, exception);
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

EncodedJSValue JSC_HOST_CALL functionRunString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String source = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    GlobalObject* realm = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>());

    JSArray* array = constructEmptyArray(realm, nullptr);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    for (unsigned i = 1; i < callFrame->argumentCount(); ++i) {
        array->putDirectIndex(realm, i - 1, callFrame->uncheckedArgument(i));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    realm->putDirect(vm, Identifier::fromString(vm, "arguments"), array);

    NakedPtr<Exception> exception;
    evaluate(realm, jscSource(source, callFrame->callerSourceOrigin(vm)), JSValue(), exception);

    if (exception) {
        scope.throwException(realm, exception);
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(realm);
}

EncodedJSValue JSC_HOST_CALL functionLoad(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool callerRelative = callFrame->argument(1).getString(globalObject) == "caller relative"_s;
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    String fileName = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    URL path;
    if (callerRelative) {
        path = URL(callFrame->callerSourceOrigin(vm).url(), fileName);
        if (!path.isLocalFile())
            return throwVMException(globalObject, scope, createURIError(globalObject, makeString("caller relative URL path is not a local file: ", path.string())));
    } else
        path = absolutePath(fileName);
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(path.fileSystemPath(), script))
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Could not open file."_s)));

    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(globalObject, jscSource(script, SourceOrigin { path }, fileName), JSValue(), evaluationException);
    if (evaluationException)
        throwException(globalObject, scope, evaluationException);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionLoadString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String sourceCode = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(globalObject, jscSource(sourceCode, callFrame->callerSourceOrigin(vm)), JSValue(), evaluationException);
    if (evaluationException)
        throwException(globalObject, scope, evaluationException);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionReadFile(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String fileName = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    bool isBinary = false;
    if (callFrame->argumentCount() > 1) {
        String type = callFrame->argument(1).toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (type != "binary")
            return throwVMError(globalObject, scope, "Expected 'binary' as second argument.");
        isBinary = true;
    }

    RefPtr<Uint8Array> content = fillBufferWithContentsOfFile(fileName);
    if (!content)
        return throwVMError(globalObject, scope, "Could not open file.");

    if (!isBinary)
        return JSValue::encode(jsString(vm, String::fromUTF8WithLatin1Fallback(content->data(), content->length())));

    Structure* structure = globalObject->typedArrayStructure(TypeUint8);
    JSObject* result = JSUint8Array::create(vm, structure, WTFMove(content));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionCheckSyntax(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String fileName = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Could not open file."_s)));

    StopWatch stopWatch;
    stopWatch.start();

    JSValue syntaxException;
    bool validSyntax = checkSyntax(globalObject, jscSource(script, SourceOrigin { absolutePath(fileName) }, fileName), &syntaxException);
    stopWatch.stop();

    if (!validSyntax)
        throwException(globalObject, scope, syntaxException);
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

#if ENABLE(SAMPLING_FLAGS)
EncodedJSValue JSC_HOST_CALL functionSetSamplingFlags(JSGlobalObject*, CallFrame* callFrame)
{
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        unsigned flag = static_cast<unsigned>(callFrame->uncheckedArgument(i).toNumber(globalObject));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::setFlag(flag);
    }
    return JSValue::encode(jsNull());
}

EncodedJSValue JSC_HOST_CALL functionClearSamplingFlags(JSGlobalObject*, CallFrame* callFrame)
{
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        unsigned flag = static_cast<unsigned>(callFrame->uncheckedArgument(i).toNumber(globalObject));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::clearFlag(flag);
    }
    return JSValue::encode(jsNull());
}
#endif

EncodedJSValue JSC_HOST_CALL functionGetRandomSeed(JSGlobalObject* globalObject, CallFrame*)
{
    return JSValue::encode(jsNumber(globalObject->weakRandom().seed()));
}

EncodedJSValue JSC_HOST_CALL functionSetRandomSeed(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned seed = callFrame->argument(0).toUInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    globalObject->weakRandom().setSeed(seed);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionIsRope(JSGlobalObject*, CallFrame* callFrame)
{
    JSValue argument = callFrame->argument(0);
    if (!argument.isString())
        return JSValue::encode(jsBoolean(false));
    const StringImpl* impl = asString(argument)->tryGetValueImpl();
    return JSValue::encode(jsBoolean(!impl));
}

EncodedJSValue JSC_HOST_CALL functionCallerSourceOrigin(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    SourceOrigin sourceOrigin = callFrame->callerSourceOrigin(vm);
    if (sourceOrigin.url().isNull())
        return JSValue::encode(jsNull());
    return JSValue::encode(jsString(vm, sourceOrigin.string()));
}

EncodedJSValue JSC_HOST_CALL functionReadline(JSGlobalObject* globalObject, CallFrame*)
{
    Vector<char, 256> line;
    int c;
    while ((c = getchar()) != EOF) {
        // FIXME: Should we also break on \r? 
        if (c == '\n')
            break;
        line.append(c);
    }
    line.append('\0');
    return JSValue::encode(jsString(globalObject->vm(), line.data()));
}

EncodedJSValue JSC_HOST_CALL functionPreciseTime(JSGlobalObject*, CallFrame*)
{
    return JSValue::encode(jsNumber(WallTime::now().secondsSinceEpoch().value()));
}

EncodedJSValue JSC_HOST_CALL functionNeverInlineFunction(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return JSValue::encode(setNeverInline(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL functionNoDFG(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return JSValue::encode(setNeverOptimize(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL functionNoFTL(JSGlobalObject*, CallFrame* callFrame)
{
    if (callFrame->argumentCount()) {
        FunctionExecutable* executable = getExecutableForFunction(callFrame->argument(0));
        if (executable)
            executable->setNeverFTLOptimize(true);
    }
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionNoOSRExitFuzzing(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return JSValue::encode(setCannotUseOSRExitFuzzing(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL functionOptimizeNextInvocation(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return JSValue::encode(optimizeNextInvocation(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL functionNumberOfDFGCompiles(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return JSValue::encode(numberOfDFGCompiles(globalObject, callFrame));
}

EncodedJSValue JSC_HOST_CALL functionCallerIsOMGCompiled(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!Options::useBBQTierUpChecks())
        return JSValue::encode(jsBoolean(true));

    CallerFunctor wasmToJSFrame;
    StackVisitor::visit(callFrame, vm, wasmToJSFrame);
    if (!wasmToJSFrame.callerFrame()->isAnyWasmCallee())
        return throwVMError(globalObject, scope, "caller is not a wasm->js import function");

    // We have a wrapper frame that we generate for imports. If we ever can direct call from wasm we would need to change this.
    ASSERT(!wasmToJSFrame.callerFrame()->callee().isWasm());
    CallerFunctor wasmFrame;
    StackVisitor::visit(wasmToJSFrame.callerFrame(), vm, wasmFrame);
    ASSERT(wasmFrame.callerFrame()->callee().isWasm());
#if ENABLE(WEBASSEMBLY)
    auto mode = wasmFrame.callerFrame()->callee().asWasmCallee()->compilationMode();
    return JSValue::encode(jsBoolean(mode == Wasm::CompilationMode::OMGMode || mode == Wasm::CompilationMode::OMGForOSREntryMode));
#endif
    RELEASE_ASSERT_NOT_REACHED();
}

Message::Message(ArrayBufferContents&& contents, int32_t index)
    : m_contents(WTFMove(contents))
    , m_index(index)
{
}

Message::~Message()
{
}

Worker::Worker(Workers& workers)
    : m_workers(workers)
{
    auto locker = holdLock(m_workers.m_lock);
    m_workers.m_workers.append(this);
    
    *currentWorker() = this;
}

Worker::~Worker()
{
    auto locker = holdLock(m_workers.m_lock);
    RELEASE_ASSERT(isOnList());
    remove();
}

void Worker::enqueue(const AbstractLocker&, RefPtr<Message> message)
{
    m_messages.append(message);
}

RefPtr<Message> Worker::dequeue()
{
    auto locker = holdLock(m_workers.m_lock);
    while (m_messages.isEmpty())
        m_workers.m_condition.wait(m_workers.m_lock);
    return m_messages.takeFirst();
}

Worker& Worker::current()
{
    return **currentWorker();
}

ThreadSpecific<Worker*>& Worker::currentWorker()
{
    static ThreadSpecific<Worker*>* result;
    static std::once_flag flag;
    std::call_once(
        flag,
        [] () {
            result = new ThreadSpecific<Worker*>();
        });
    return *result;
}

Workers::Workers()
{
}

Workers::~Workers()
{
    UNREACHABLE_FOR_PLATFORM();
}

template<typename Func>
void Workers::broadcast(const Func& func)
{
    auto locker = holdLock(m_lock);
    for (Worker* worker = m_workers.begin(); worker != m_workers.end(); worker = worker->next()) {
        if (worker != &Worker::current())
            func(locker, *worker);
    }
    m_condition.notifyAll();
}

void Workers::report(const String& string)
{
    auto locker = holdLock(m_lock);
    m_reports.append(string.isolatedCopy());
    m_condition.notifyAll();
}

String Workers::tryGetReport()
{
    auto locker = holdLock(m_lock);
    if (m_reports.isEmpty())
        return String();
    return m_reports.takeFirst();
}

String Workers::getReport()
{
    auto locker = holdLock(m_lock);
    while (m_reports.isEmpty())
        m_condition.wait(m_lock);
    return m_reports.takeFirst();
}

Workers& Workers::singleton()
{
    static Workers* result;
    static std::once_flag flag;
    std::call_once(
        flag,
        [] {
            result = new Workers();
        });
    return *result;
}

EncodedJSValue JSC_HOST_CALL functionDollarCreateRealm(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    GlobalObject* result = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>());
    return JSValue::encode(result->getDirect(vm, Identifier::fromString(vm, "$")));
}

EncodedJSValue JSC_HOST_CALL functionDollarEvalScript(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String sourceCode = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    JSValue global = callFrame->thisValue().get(globalObject, Identifier::fromString(vm, "global"));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    GlobalObject* realm = jsDynamicCast<GlobalObject*>(vm, global);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!realm)
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Expected global to point to a global object"_s)));
    
    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(realm, jscSource(sourceCode, callFrame->callerSourceOrigin(vm)), JSValue(), evaluationException);
    if (evaluationException)
        throwException(globalObject, scope, evaluationException);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentStart(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String sourceCode = callFrame->argument(0).toWTFString(globalObject).isolatedCopy();
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    Lock didStartLock;
    Condition didStartCondition;
    bool didStart = false;

    auto isGigacageMemoryExhausted = [&](Gigacage::Kind kind) {
        if (!Gigacage::isEnabled(kind))
            return false;
        if (Gigacage::footprint(kind) < Gigacage::size(kind) * 0.8)
            return false;
        return true;
    };

    if (isGigacageMemoryExhausted(Gigacage::JSValue) || isGigacageMemoryExhausted(Gigacage::Primitive))
        return JSValue::encode(throwOutOfMemoryError(globalObject, scope, "Gigacage is exhausted"_s));
    
    Thread::create(
        "JSC Agent",
        [sourceCode, &didStartLock, &didStartCondition, &didStart] () {
            CommandLine commandLine(CommandLine::CommandLineForWorkers);
            commandLine.m_interactive = false;
            runJSC(
                commandLine, true,
                [&] (VM&, GlobalObject* globalObject, bool& success) {
                    // Notify the thread that started us that we have registered a worker.
                    {
                        auto locker = holdLock(didStartLock);
                        didStart = true;
                        didStartCondition.notifyOne();
                    }
                    
                    NakedPtr<Exception> evaluationException;
                    JSValue result;
                    result = evaluate(globalObject, jscSource(sourceCode, SourceOrigin(URL({ }, "worker"_s))), JSValue(), evaluationException);
                    if (evaluationException)
                        result = evaluationException->value();
                    checkException(globalObject, true, evaluationException, result, commandLine, success);
                    if (!success)
                        exit(1);
                });
        })->detach();
    
    {
        auto locker = holdLock(didStartLock);
        while (!didStart)
            didStartCondition.wait(didStartLock);
    }
    
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentReceiveBroadcast(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue callback = callFrame->argument(0);
    auto callData = getCallData(vm, callback);
    if (callData.type == CallData::Type::None)
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Expected callback"_s)));
    
    RefPtr<Message> message;
    {
        ReleaseHeapAccessScope releaseAccess(vm.heap);
        message = Worker::current().dequeue();
    }
    
    auto nativeBuffer = ArrayBuffer::create(message->releaseContents());
    ArrayBufferSharingMode sharingMode = nativeBuffer->sharingMode();
    JSArrayBuffer* jsBuffer = JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(sharingMode), WTFMove(nativeBuffer));
    
    MarkedArgumentBuffer args;
    args.append(jsBuffer);
    args.append(jsNumber(message->index()));
    if (UNLIKELY(args.hasOverflowed()))
        return JSValue::encode(throwOutOfMemoryError(globalObject, scope));
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, callback, callData, jsNull(), args)));
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentReport(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String report = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    Workers::singleton().report(report);
    
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentSleep(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (callFrame->argumentCount() >= 1) {
        Seconds seconds = Seconds::fromMilliseconds(callFrame->argument(0).toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        sleep(seconds);
    }
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentBroadcast(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(vm, callFrame->argument(0));
    if (!jsBuffer || !jsBuffer->isShared())
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Expected SharedArrayBuffer"_s)));
    
    int32_t index = callFrame->argument(1).toInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    Workers::singleton().broadcast(
        [&] (const AbstractLocker& locker, Worker& worker) {
            ArrayBuffer* nativeBuffer = jsBuffer->impl();
            ArrayBufferContents contents;
            nativeBuffer->transferTo(vm, contents); // "transferTo" means "share" if the buffer is shared.
            RefPtr<Message> message = adoptRef(new Message(WTFMove(contents), index));
            worker.enqueue(locker, message);
        });
    
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentGetReport(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();

    String string = Workers::singleton().tryGetReport();
    if (!string)
        return JSValue::encode(jsNull());
    
    return JSValue::encode(jsString(vm, string));
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentLeaving(JSGlobalObject*, CallFrame*)
{
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentMonotonicNow(JSGlobalObject*, CallFrame*)
{
    return JSValue::encode(jsNumber(MonotonicTime::now().secondsSinceEpoch().milliseconds()));
}

EncodedJSValue JSC_HOST_CALL functionWaitForReport(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();

    String string;
    {
        ReleaseHeapAccessScope releaseAccess(vm.heap);
        string = Workers::singleton().getReport();
    }
    if (!string)
        return JSValue::encode(jsNull());
    
    return JSValue::encode(jsString(vm, string));
}

EncodedJSValue JSC_HOST_CALL functionHeapCapacity(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    return JSValue::encode(jsNumber(vm.heap.capacity()));
}

EncodedJSValue JSC_HOST_CALL functionFlashHeapAccess(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    double sleepTimeMs = 0;
    if (callFrame->argumentCount() >= 1) {
        sleepTimeMs = callFrame->argument(0).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    vm.heap.releaseAccess();
    if (sleepTimeMs)
        sleep(Seconds::fromMilliseconds(sleepTimeMs));
    vm.heap.acquireAccess();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDisableRichSourceInfo(JSGlobalObject*, CallFrame*)
{
    supportsRichSourceInfo = false;
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionMallocInALoop(JSGlobalObject*, CallFrame*)
{
    Vector<void*> ptrs;
    for (unsigned i = 0; i < 5000; ++i)
        ptrs.append(fastMalloc(1024 * 2));
    for (void* ptr : ptrs)
        fastFree(ptr);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionTotalCompileTime(JSGlobalObject*, CallFrame*)
{
#if ENABLE(JIT)
    return JSValue::encode(jsNumber(JIT::totalCompileTime().milliseconds()));
#else
    return JSValue::encode(jsNumber(0));
#endif
}

template<typename ValueType>
typename std::enable_if<!std::is_fundamental<ValueType>::value>::type addOption(VM&, JSObject*, const Identifier&, ValueType) { }

template<typename ValueType>
typename std::enable_if<std::is_fundamental<ValueType>::value>::type addOption(VM& vm, JSObject* optionsObject, const Identifier& identifier, ValueType value)
{
    optionsObject->putDirect(vm, identifier, JSValue(value));
}

EncodedJSValue JSC_HOST_CALL functionJSCOptions(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSObject* optionsObject = constructEmptyObject(globalObject);
#define READ_OPTION(type_, name_, defaultValue_, availability_, description_) \
    addOption(vm, optionsObject, Identifier::fromString(vm, #name_), Options::name_());
    FOR_EACH_JSC_OPTION(READ_OPTION)
#undef READ_OPTION
    return JSValue::encode(optionsObject);
}

EncodedJSValue JSC_HOST_CALL functionReoptimizationRetryCount(JSGlobalObject*, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    
    CodeBlock* block = getSomeBaselineCodeBlockForFunction(callFrame->argument(0));
    if (!block)
        return JSValue::encode(jsNumber(0));
    
    return JSValue::encode(jsNumber(block->reoptimizationRetryCounter()));
}

EncodedJSValue JSC_HOST_CALL functionTransferArrayBuffer(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (callFrame->argumentCount() < 1)
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Not enough arguments"_s)));
    
    JSArrayBuffer* buffer = jsDynamicCast<JSArrayBuffer*>(vm, callFrame->argument(0));
    if (!buffer)
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Expected an array buffer"_s)));
    
    ArrayBufferContents dummyContents;
    buffer->impl()->transferTo(vm, dummyContents);
    
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionFailNextNewCodeBlock(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    vm.setFailNextNewCodeBlock();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionQuit(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    vm.codeCache()->write(vm);

    jscExit(EXIT_SUCCESS);

#if COMPILER(MSVC)
    // Without this, Visual Studio will complain that this method does not return a value.
    return JSValue::encode(jsUndefined());
#endif
}

EncodedJSValue JSC_HOST_CALL functionFalse(JSGlobalObject*, CallFrame*) { return JSValue::encode(jsBoolean(false)); }

EncodedJSValue JSC_HOST_CALL functionUndefined1(JSGlobalObject*, CallFrame*) { return JSValue::encode(jsUndefined()); }
EncodedJSValue JSC_HOST_CALL functionUndefined2(JSGlobalObject*, CallFrame*) { return JSValue::encode(jsUndefined()); }
EncodedJSValue JSC_HOST_CALL functionIsInt32(JSGlobalObject*, CallFrame* callFrame)
{
    for (size_t i = 0; i < callFrame->argumentCount(); ++i) {
        if (!callFrame->argument(i).isInt32())
            return JSValue::encode(jsBoolean(false));
    }
    return JSValue::encode(jsBoolean(true));
}

EncodedJSValue JSC_HOST_CALL functionIsPureNaN(JSGlobalObject*, CallFrame* callFrame)
{
    for (size_t i = 0; i < callFrame->argumentCount(); ++i) {
        JSValue value = callFrame->argument(i);
        if (!value.isNumber())
            return JSValue::encode(jsBoolean(false));
        double number = value.asNumber();
        if (!std::isnan(number))
            return JSValue::encode(jsBoolean(false));
        if (isImpureNaN(number))
            return JSValue::encode(jsBoolean(false));
    }
    return JSValue::encode(jsBoolean(true));
}

EncodedJSValue JSC_HOST_CALL functionIdentity(JSGlobalObject*, CallFrame* callFrame) { return JSValue::encode(callFrame->argument(0)); }

EncodedJSValue JSC_HOST_CALL functionEffectful42(JSGlobalObject*, CallFrame*)
{
    return JSValue::encode(jsNumber(42));
}

EncodedJSValue JSC_HOST_CALL functionMakeMasquerader(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    return JSValue::encode(InternalFunction::createFunctionThatMasqueradesAsUndefined(vm, globalObject, 0, "IsHTMLDDA"_s, functionCallMasquerader));
}

EncodedJSValue JSC_HOST_CALL functionCallMasquerader(JSGlobalObject*, CallFrame*)
{
    return JSValue::encode(jsNull());
}

EncodedJSValue JSC_HOST_CALL functionHasCustomProperties(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue value = callFrame->argument(0);
    if (value.isObject())
        return JSValue::encode(jsBoolean(asObject(value)->hasCustomProperties(globalObject->vm())));
    return JSValue::encode(jsBoolean(false));
}

EncodedJSValue JSC_HOST_CALL functionDumpTypesForAllVariables(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    vm.dumpTypeProfilerData();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDrainMicrotasks(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    vm.drainMicrotasks();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionSetTimeout(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: This means we can't pass any internal function but I don't think that's common for testing.
    auto callback = jsDynamicCast<JSFunction*>(vm, callFrame->argument(0));
    if (!callback)
        return throwVMTypeError(globalObject, scope, "First argument is not a JS function"_s);

    // FIXME: We don't look at the timeout parameter because we don't have a schedule work later API.
    vm.deferredWorkTimer->addPendingWork(vm, callback, { });
    vm.deferredWorkTimer->scheduleWorkSoon(callback, [callback] {
        JSGlobalObject* globalObject = callback->globalObject();
        VM& vm = globalObject->vm();

        MarkedArgumentBuffer args;
        call(globalObject, callback, jsUndefined(), args, "You shouldn't see this...");
        vm.deferredWorkTimer->cancelPendingWork(callback);
    });
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionReleaseWeakRefs(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    vm.finalizeSynchronousJSExecution();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionFinalizationRegistryLiveCount(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* finalizationRegistry = jsDynamicCast<JSFinalizationRegistry*>(vm, callFrame->argument(0));
    if (!finalizationRegistry)
        return throwVMTypeError(globalObject, scope, "first argument is not a finalizationRegistry"_s);

    auto locker = holdLock(finalizationRegistry->cellLock());
    return JSValue::encode(jsNumber(finalizationRegistry->liveCount(locker)));
}

static EncodedJSValue JSC_HOST_CALL functionFinalizationRegistryDeadCount(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* finalizationRegistry = jsDynamicCast<JSFinalizationRegistry*>(vm, callFrame->argument(0));
    if (!finalizationRegistry)
        return throwVMTypeError(globalObject, scope, "first argument is not a finalizationRegistry"_s);

    auto locker = holdLock(finalizationRegistry->cellLock());
    return JSValue::encode(jsNumber(finalizationRegistry->deadCount(locker)));
}

EncodedJSValue JSC_HOST_CALL functionIs32BitPlatform(JSGlobalObject*, CallFrame*)
{
#if USE(JSVALUE64)
    return JSValue::encode(JSValue(JSC::JSValue::JSFalse));
#else
    return JSValue::encode(JSValue(JSC::JSValue::JSTrue));
#endif
}

EncodedJSValue JSC_HOST_CALL functionCreateGlobalObject(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    return JSValue::encode(GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>()));
}

EncodedJSValue JSC_HOST_CALL functionCreateHeapBigInt(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = callFrame->argument(0);
    JSValue bigInt = toBigInt(globalObject, argument);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
#if USE(BIGINT32)
    if (bigInt.isHeapBigInt())
        return JSValue::encode(bigInt);
    ASSERT(bigInt.isBigInt32());
    int32_t value = bigInt.bigInt32AsInt32();
    RELEASE_AND_RETURN(scope, JSValue::encode(JSBigInt::createFrom(globalObject, value)));
#else
    return JSValue::encode(bigInt);
#endif
}

#if USE(BIGINT32)
EncodedJSValue JSC_HOST_CALL functionCreateBigInt32(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = callFrame->argument(0);
    JSValue bigIntValue = toBigInt(globalObject, argument);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (bigIntValue.isBigInt32())
        return JSValue::encode(bigIntValue);
    ASSERT(bigIntValue.isHeapBigInt());
    JSBigInt* bigInt = jsCast<JSBigInt*>(bigIntValue);
    if (!bigInt->length())
        return JSValue::encode(jsBigInt32(0));
    if (bigInt->length() == 1) {
        JSBigInt::Digit digit = bigInt->digit(0);
        if (bigInt->sign()) {
            if (digit <= static_cast<uint64_t>(-static_cast<int64_t>(INT32_MIN)))
                return JSValue::encode(jsBigInt32(static_cast<int32_t>(-static_cast<int64_t>(digit))));
        } else {
            if (digit <= INT32_MAX)
                return JSValue::encode(jsBigInt32(static_cast<int32_t>(digit)));
        }
    }
    throwTypeError(globalObject, scope, "Out of range of BigInt32"_s);
    return { };
}
#endif

EncodedJSValue JSC_HOST_CALL functionUseBigInt32(JSGlobalObject*, CallFrame*)
{
#if USE(BIGINT32)
    return JSValue::encode(jsBoolean(true));
#else
    return JSValue::encode(jsBoolean(false));
#endif
}

EncodedJSValue JSC_HOST_CALL functionIsBigInt32(JSGlobalObject*, CallFrame* callFrame)
{
#if USE(BIGINT32)
    return JSValue::encode(jsBoolean(callFrame->argument(0).isBigInt32()));
#else
    UNUSED_PARAM(callFrame);
    return JSValue::encode(jsBoolean(false));
#endif
}

EncodedJSValue JSC_HOST_CALL functionIsHeapBigInt(JSGlobalObject*, CallFrame* callFrame)
{
    return JSValue::encode(jsBoolean(callFrame->argument(0).isHeapBigInt()));
}

EncodedJSValue JSC_HOST_CALL functionCheckModuleSyntax(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String source = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    StopWatch stopWatch;
    stopWatch.start();

    ParserError error;
    bool validSyntax = checkModuleSyntax(globalObject, jscSource(source, { }, String(), TextPosition(), SourceProviderSourceType::Module), error);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    stopWatch.stop();

    if (!validSyntax)
        throwException(globalObject, scope, jsNontrivialString(vm, toString("SyntaxError: ", error.message(), ":", error.line())));
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

EncodedJSValue JSC_HOST_CALL functionPlatformSupportsSamplingProfiler(JSGlobalObject*, CallFrame*)
{
#if ENABLE(SAMPLING_PROFILER)
    return JSValue::encode(JSValue(JSC::JSValue::JSTrue));
#else
    return JSValue::encode(JSValue(JSC::JSValue::JSFalse));
#endif
}

EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshot(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    HeapSnapshotBuilder snapshotBuilder(vm.ensureHeapProfiler());
    snapshotBuilder.buildSnapshot();

    String jsonString = snapshotBuilder.json();
    EncodedJSValue result = JSValue::encode(JSONParse(globalObject, jsonString));
    scope.releaseAssertNoException();
    return result;
}

EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshotForGCDebugging(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    String jsonString;
    {
        DeferGCForAWhile deferGC(vm.heap); // Prevent concurrent GC from interfering with the full GC that the snapshot does.

        HeapSnapshotBuilder snapshotBuilder(vm.ensureHeapProfiler(), HeapSnapshotBuilder::SnapshotType::GCDebuggingSnapshot);
        snapshotBuilder.buildSnapshot();

        jsonString = snapshotBuilder.json();
    }
    scope.releaseAssertNoException();
    return JSValue::encode(jsString(vm, jsonString));
}

EncodedJSValue JSC_HOST_CALL functionResetSuperSamplerState(JSGlobalObject*, CallFrame*)
{
    resetSuperSamplerState();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionEnsureArrayStorage(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        if (JSObject* object = jsDynamicCast<JSObject*>(vm, callFrame->argument(i)))
            object->ensureArrayStorage(vm);
    }
    return JSValue::encode(jsUndefined());
}

#if ENABLE(SAMPLING_PROFILER)
EncodedJSValue JSC_HOST_CALL functionStartSamplingProfiler(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    SamplingProfiler& samplingProfiler = vm.ensureSamplingProfiler(WTF::Stopwatch::create());
    samplingProfiler.noticeCurrentThreadAsJSCExecutionThread();
    samplingProfiler.start();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionSamplingProfilerStackTraces(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!vm.samplingProfiler())
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Sampling profiler was never started"_s)));

    String jsonString = vm.samplingProfiler()->stackTracesAsJSON();
    EncodedJSValue result = JSValue::encode(JSONParse(globalObject, jsonString));
    scope.releaseAssertNoException();
    return result;
}
#endif // ENABLE(SAMPLING_PROFILER)

EncodedJSValue JSC_HOST_CALL functionMaxArguments(JSGlobalObject*, CallFrame*)
{
    return JSValue::encode(jsNumber(JSC::maxArguments));
}

EncodedJSValue JSC_HOST_CALL functionAsyncTestStart(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue numberOfAsyncPasses = callFrame->argument(0);
    if (!numberOfAsyncPasses.isUInt32())
        return throwVMError(globalObject, scope, "Expected first argument to be a uint32"_s);

    asyncTestExpectedPasses += numberOfAsyncPasses.asUInt32();
    return encodedJSUndefined();
}

EncodedJSValue JSC_HOST_CALL functionAsyncTestPassed(JSGlobalObject*, CallFrame*)
{
    asyncTestPasses++;
    return encodedJSUndefined();
}

#if ENABLE(WEBASSEMBLY)

static EncodedJSValue JSC_HOST_CALL functionWebAssemblyMemoryMode(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!Wasm::isSupported())
        return throwVMTypeError(globalObject, scope, "WebAssemblyMemoryMode should only be called if the useWebAssembly option is set"_s);

    if (JSObject* object = callFrame->argument(0).getObject()) {
        if (auto* memory = jsDynamicCast<JSWebAssemblyMemory*>(vm, object))
            return JSValue::encode(jsString(vm, makeString(memory->memory().mode())));
        if (auto* instance = jsDynamicCast<JSWebAssemblyInstance*>(vm, object))
            return JSValue::encode(jsString(vm, makeString(instance->memoryMode())));
    }

    return throwVMTypeError(globalObject, scope, "WebAssemblyMemoryMode expects either a WebAssembly.Memory or WebAssembly.Instance"_s);
}

#endif // ENABLE(WEBASSEMBLY)

EncodedJSValue JSC_HOST_CALL functionSetUnhandledRejectionCallback(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    JSObject* object = callFrame->argument(0).getObject();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!object || !object->isCallable(vm))
        return throwVMTypeError(globalObject, scope);

    globalObject->setUnhandledRejectionCallback(vm, object);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionAsDoubleNumber(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    double num = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsDoubleNumber(num));
}

// Use SEH for Release builds only to get rid of the crash report dialog
// (luckily the same tests fail in Release and Debug builds so far). Need to
// be in a separate main function because the jscmain function requires object
// unwinding.

#if COMPILER(MSVC) && !defined(_DEBUG)
#define TRY       __try {
#define EXCEPT(x) } __except (EXCEPTION_EXECUTE_HANDLER) { x; }
#else
#define TRY
#define EXCEPT(x)
#endif

int jscmain(int argc, char** argv);

#if OS(DARWIN) || OS(LINUX)
static size_t memoryLimit;

static void crashIfExceedingMemoryLimit()
{
    if (!memoryLimit)
        return;
    MemoryFootprint footprint = MemoryFootprint::now();
    if (footprint.current > memoryLimit) {
        dataLogLn("Crashing because current footprint: ", footprint.current, " exceeds limit: ", memoryLimit);
        CRASH();
    }
}

static void startMemoryMonitoringThreadIfNeeded()
{
    char* memoryLimitString = getenv("JSCTEST_memoryLimit");
    if (!memoryLimitString)
        return;

    if (sscanf(memoryLimitString, "%zu", &memoryLimit) != 1) {
        dataLogLn("WARNING: malformed JSCTEST_memoryLimit environment variable");
        return;
    }

    if (!memoryLimit)
        return;

    Thread::create("jsc Memory Monitor", [=] {
        while (true) {
            sleep(Seconds::fromMilliseconds(5));
            crashIfExceedingMemoryLimit();
        }
    });
}
#endif // OS(DARWIN) || OS(LINUX)

static double s_desiredTimeout;
static double s_timeoutMultiplier = 1.0;
static Seconds s_timeoutDuration;
static Seconds s_maxAllowedCPUTime;
static VM* s_vm;

static void startTimeoutTimer(Seconds duration)
{
    Thread::create("jsc Timeout Thread", [=] () {
        sleep(duration);
        VMInspector::forEachVM([&] (VM& vm) -> VMInspector::FunctorStatus {
            if (&vm != s_vm)
                return VMInspector::FunctorStatus::Continue;
            vm.notifyNeedShellTimeoutCheck();
            return VMInspector::FunctorStatus::Done;
        });

        if (const char* timeoutString = getenv("JSCTEST_hardTimeout")) {
            double hardTimeoutInDouble = 0;
            if (sscanf(timeoutString, "%lf", &hardTimeoutInDouble) != 1)
                dataLog("WARNING: hardTimeout string is malformed, got ", timeoutString, " but expected a number. Not using a timeout.\n");
            else {
                Seconds hardTimeout { hardTimeoutInDouble };
                sleep(hardTimeout);
                dataLogLn("HARD TIMEOUT after ", hardTimeout);
                exit(EXIT_FAILURE);
            }
        }
    });
}

static void timeoutCheckCallback(VM& vm)
{
    RELEASE_ASSERT(&vm == s_vm);
    auto cpuTime = CPUTime::forCurrentThread();
    if (cpuTime >= s_maxAllowedCPUTime) {
        dataLog("Timed out after ", s_timeoutDuration, " seconds!\n");
        CRASH();
    }
    auto remainingTime = s_maxAllowedCPUTime - cpuTime;
    startTimeoutTimer(remainingTime);
}

static void initializeTimeoutIfNeeded()
{
    if (char* timeoutString = getenv("JSCTEST_timeout")) {
        if (sscanf(timeoutString, "%lf", &s_desiredTimeout) != 1) {
            dataLog("WARNING: timeout string is malformed, got ", timeoutString,
                " but expected a number. Not using a timeout.\n");
        } else
            g_jscConfig.shellTimeoutCheckCallback = timeoutCheckCallback;
    }
}

static void startTimeoutThreadIfNeeded(VM& vm)
{
    if (!g_jscConfig.shellTimeoutCheckCallback)
        return;

    s_vm = &vm;
    s_timeoutDuration = Seconds(s_desiredTimeout * s_timeoutMultiplier);
    s_maxAllowedCPUTime = CPUTime::forCurrentThread() + s_timeoutDuration;
    Seconds timeoutDuration(s_desiredTimeout * s_timeoutMultiplier);
    startTimeoutTimer(timeoutDuration);
}

int main(int argc, char** argv)
{
#if OS(DARWIN) && CPU(ARM_THUMB2)
    // Enabled IEEE754 denormal support.
    fenv_t env;
    fegetenv( &env );
    env.__fpscr &= ~0x01000000u;
    fesetenv( &env );
#endif

#if OS(WINDOWS)
    // Cygwin calls ::SetErrorMode(SEM_FAILCRITICALERRORS), which we will inherit. This is bad for
    // testing/debugging, as it causes the post-mortem debugger not to be invoked. We reset the
    // error mode here to work around Cygwin's behavior. See <http://webkit.org/b/55222>.
    ::SetErrorMode(0);

    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);

#if defined(_DEBUG)
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif

    timeBeginPeriod(1);
#endif

#if PLATFORM(GTK)
    if (!setlocale(LC_ALL, ""))
        WTFLogAlways("Locale not supported by C library.\n\tUsing the fallback 'C' locale.");
#endif

    // Need to initialize WTF before we start any threads. Cannot initialize JSC
    // yet, since that would do somethings that we'd like to defer until after we
    // have a chance to parse options.
    WTF::initialize();

    // We can't use destructors in the following code because it uses Windows
    // Structured Exception Handling
    int res = EXIT_SUCCESS;
    TRY
        res = jscmain(argc, argv);
    EXCEPT(res = EXIT_EXCEPTION)
    finalizeStatsAtEndOfTesting();
    if (getenv("JS_SHELL_WAIT_FOR_INPUT_TO_EXIT")) {
        WTF::fastDisableScavenger();
        fprintf(stdout, "\njs shell waiting for input to exit\n");
        fflush(stdout);
        getc(stdin);
    }

    jscExit(res);
}

static void dumpException(GlobalObject* globalObject, JSValue exception)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

#define CHECK_EXCEPTION() do { \
        if (scope.exception()) { \
            scope.clearException(); \
            return; \
        } \
    } while (false)

    auto exceptionString = exception.toWTFString(globalObject);
    CHECK_EXCEPTION();
    Expected<CString, UTF8ConversionError> expectedCString = exceptionString.tryGetUtf8();
    if (expectedCString)
        printf("Exception: %s\n", expectedCString.value().data());
    else
        printf("Exception: <out of memory while extracting exception string>\n");

    Identifier nameID = Identifier::fromString(vm, "name");
    CHECK_EXCEPTION();
    Identifier fileNameID = Identifier::fromString(vm, "sourceURL");
    CHECK_EXCEPTION();
    Identifier lineNumberID = Identifier::fromString(vm, "line");
    CHECK_EXCEPTION();
    Identifier stackID = Identifier::fromString(vm, "stack");
    CHECK_EXCEPTION();

    JSValue nameValue = exception.get(globalObject, nameID);
    CHECK_EXCEPTION();
    JSValue fileNameValue = exception.get(globalObject, fileNameID);
    CHECK_EXCEPTION();
    JSValue lineNumberValue = exception.get(globalObject, lineNumberID);
    CHECK_EXCEPTION();
    JSValue stackValue = exception.get(globalObject, stackID);
    CHECK_EXCEPTION();
    
    auto nameString = nameValue.toWTFString(globalObject);
    CHECK_EXCEPTION();

    if (nameString == "SyntaxError" && (!fileNameValue.isUndefinedOrNull() || !lineNumberValue.isUndefinedOrNull())) {
        auto fileNameString = fileNameValue.toWTFString(globalObject);
        CHECK_EXCEPTION();
        auto lineNumberString = lineNumberValue.toWTFString(globalObject);
        CHECK_EXCEPTION();
        printf("at %s:%s\n", fileNameString.utf8().data(), lineNumberString.utf8().data());
    }
    
    if (!stackValue.isUndefinedOrNull()) {
        auto stackString = stackValue.toWTFString(globalObject);
        CHECK_EXCEPTION();
        if (stackString.length())
            printf("%s\n", stackString.utf8().data());
    }

#undef CHECK_EXCEPTION
}

static bool checkUncaughtException(VM& vm, GlobalObject* globalObject, JSValue exception, const CommandLine& options)
{
    const String& expectedExceptionName = options.m_uncaughtExceptionName;
    auto scope = DECLARE_CATCH_SCOPE(vm);
    scope.clearException();
    if (!exception) {
        printf("Expected uncaught exception with name '%s' but none was thrown\n", expectedExceptionName.utf8().data());
        return false;
    }

    JSValue exceptionClass = globalObject->get(globalObject, Identifier::fromString(vm, expectedExceptionName));
    if (!exceptionClass.isObject() || scope.exception()) {
        printf("Expected uncaught exception with name '%s' but given exception class is not defined\n", expectedExceptionName.utf8().data());
        return false;
    }

    bool isInstanceOfExpectedException = jsCast<JSObject*>(exceptionClass)->hasInstance(globalObject, exception);
    if (scope.exception()) {
        printf("Expected uncaught exception with name '%s' but given exception class fails performing hasInstance\n", expectedExceptionName.utf8().data());
        return false;
    }
    if (isInstanceOfExpectedException) {
        if (options.m_alwaysDumpUncaughtException)
            dumpException(globalObject, exception);
        return true;
    }

    printf("Expected uncaught exception with name '%s' but exception value is not instance of this exception class\n", expectedExceptionName.utf8().data());
    dumpException(globalObject, exception);
    return false;
}

static void checkException(GlobalObject* globalObject, bool isLastFile, bool hasException, JSValue value, const CommandLine& options, bool& success)
{
    VM& vm = globalObject->vm();

    if (options.m_treatWatchdogExceptionAsSuccess && value.inherits<TerminatedExecutionError>(vm)) {
        ASSERT(hasException);
        return;
    }

    if (!options.m_uncaughtExceptionName || !isLastFile) {
        success = success && !hasException;
        if (options.m_dump && !hasException)
            printf("End: %s\n", value.toWTFString(globalObject).utf8().data());
        if (hasException)
            dumpException(globalObject, value);
    } else
        success = success && checkUncaughtException(vm, globalObject, (hasException) ? value : JSValue(), options);
}

void GlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject* globalObject, Exception* exception)
{
    auto* global = jsCast<GlobalObject*>(globalObject);
    dumpException(global, exception->value());
    bool hideNoReturn = true;
    if (hideNoReturn)
        jscExit(EXIT_EXCEPTION);
}

static void runWithOptions(GlobalObject* globalObject, CommandLine& options, bool& success)
{
    Vector<Script>& scripts = options.m_scripts;
    String fileName;
    Vector<char> scriptBuffer;

    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

#if ENABLE(SAMPLING_FLAGS)
    SamplingFlags::start();
#endif

    for (size_t i = 0; i < scripts.size(); i++) {
        JSInternalPromise* promise = nullptr;
        bool isModule = options.m_module || scripts[i].scriptType == Script::ScriptType::Module;
        if (scripts[i].codeSource == Script::CodeSource::File) {
            fileName = scripts[i].argument;
            if (scripts[i].strictMode == Script::StrictMode::Strict)
                scriptBuffer.append("\"use strict\";\n", strlen("\"use strict\";\n"));

            if (isModule) {
                // If the passed file isn't an absolute path append "./" so the module loader doesn't think this is a bare-name specifier.
                fileName = fileName.startsWith('/') ? fileName : makeString("./", fileName);
                promise = loadAndEvaluateModule(globalObject, fileName, jsUndefined(), jsUndefined());
                scope.releaseAssertNoException();
            } else {
                if (!fetchScriptFromLocalFileSystem(fileName, scriptBuffer)) {
                    success = false; // fail early so we can catch missing files
                    return;
                }
            }
        } else {
            size_t commandLineLength = strlen(scripts[i].argument);
            scriptBuffer.resize(commandLineLength);
            std::copy(scripts[i].argument, scripts[i].argument + commandLineLength, scriptBuffer.begin());
            fileName = "[Command Line]"_s;
        }

        bool isLastFile = i == scripts.size() - 1;
        SourceOrigin sourceOrigin { absolutePath(fileName) };
        if (isModule) {
            if (!promise) {
                // FIXME: This should use an absolute file URL https://bugs.webkit.org/show_bug.cgi?id=193077
                promise = loadAndEvaluateModule(globalObject, jscSource(stringFromUTF(scriptBuffer), sourceOrigin, fileName, TextPosition(), SourceProviderSourceType::Module), jsUndefined());
            }
            scope.clearException();

            JSFunction* fulfillHandler = JSNativeStdFunction::create(vm, globalObject, 1, String(), [&success, &options, isLastFile](JSGlobalObject* globalObject, CallFrame* callFrame) {
                checkException(jsCast<GlobalObject*>(globalObject), isLastFile, false, callFrame->argument(0), options, success);
                return JSValue::encode(jsUndefined());
            });

            JSFunction* rejectHandler = JSNativeStdFunction::create(vm, globalObject, 1, String(), [&success, &options, isLastFile](JSGlobalObject* globalObject, CallFrame* callFrame) {
                checkException(jsCast<GlobalObject*>(globalObject), isLastFile, true, callFrame->argument(0), options, success);
                return JSValue::encode(jsUndefined());
            });

            promise->then(globalObject, fulfillHandler, rejectHandler);
            scope.releaseAssertNoException();
            vm.drainMicrotasks();
        } else {
            NakedPtr<Exception> evaluationException;
            JSValue returnValue = evaluate(globalObject, jscSource(scriptBuffer, sourceOrigin , fileName), JSValue(), evaluationException);
            scope.assertNoException();
            if (evaluationException)
                returnValue = evaluationException->value();
            checkException(globalObject, isLastFile, evaluationException, returnValue, options, success);
        }

        scriptBuffer.clear();
        scope.clearException();
    }

#if ENABLE(REGEXP_TRACING)
    vm.dumpRegExpTrace();
#endif
}

#define RUNNING_FROM_XCODE 0

static void runInteractive(GlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    URL directoryName = currentWorkingDirectory();
    if (!directoryName.isValid())
        return;
    SourceOrigin sourceOrigin(URL(directoryName, "./interpreter"_s));
    
    bool shouldQuit = false;
    while (!shouldQuit) {
#if HAVE(READLINE) && !RUNNING_FROM_XCODE
        ParserError error;
        String source;
        do {
            error = ParserError();
            char* line = readline(source.isEmpty() ? interactivePrompt : "... ");
            shouldQuit = !line;
            if (!line)
                break;
            source = source + String::fromUTF8(line);
            source = source + '\n';
            checkSyntax(vm, jscSource(source, sourceOrigin), error);
            if (!line[0]) {
                free(line);
                break;
            }
            add_history(line);
            free(line);
        } while (error.syntaxErrorType() == ParserError::SyntaxErrorRecoverable);
        
        if (error.isValid()) {
            printf("%s:%d\n", error.message().utf8().data(), error.line());
            continue;
        }
        
        
        NakedPtr<Exception> evaluationException;
        JSValue returnValue = evaluate(globalObject, jscSource(source, sourceOrigin), JSValue(), evaluationException);
#else
        printf("%s", interactivePrompt);
        Vector<char, 256> line;
        int c;
        while ((c = getchar()) != EOF) {
            // FIXME: Should we also break on \r? 
            if (c == '\n')
                break;
            line.append(c);
        }
        if (line.isEmpty())
            break;

        NakedPtr<Exception> evaluationException;
        JSValue returnValue = evaluate(globalObject, jscSource(line, sourceOrigin, sourceOrigin.string()), JSValue(), evaluationException);
#endif
        Expected<CString, UTF8ConversionError> utf8;
        if (evaluationException) {
            fputs("Exception: ", stdout);
            utf8 = evaluationException->value().toWTFString(globalObject).tryGetUtf8();
        } else
            utf8 = returnValue.toWTFString(globalObject).tryGetUtf8();

        CString result;
        if (utf8)
            result = utf8.value();
        else if (utf8.error() == UTF8ConversionError::OutOfMemory)
            result = "OutOfMemory while processing string";
        else
            result = "Error while processing string";
        fwrite(result.data(), sizeof(char), result.length(), stdout);
        putchar('\n');

        scope.clearException();
        vm.drainMicrotasks();
    }
    printf("\n");
}

static NO_RETURN void printUsageStatement(bool help = false)
{
    fprintf(stderr, "Usage: jsc [options] [files] [-- arguments]\n");
    fprintf(stderr, "  -d         Dumps bytecode (debug builds only)\n");
    fprintf(stderr, "  -e         Evaluate argument as script code\n");
    fprintf(stderr, "  -f         Specifies a source file (deprecated)\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  -i         Enables interactive mode (default if no files are specified)\n");
    fprintf(stderr, "  -m         Execute as a module\n");
#if OS(UNIX)
    fprintf(stderr, "  -s         Installs signal handlers that exit on a crash (Unix platforms only, lldb will not work with this option) \n");
#endif
    fprintf(stderr, "  -p <file>  Outputs profiling data to a file\n");
    fprintf(stderr, "  -x         Output exit code before terminating\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  --sample                   Collects and outputs sampling profiler data\n");
    fprintf(stderr, "  --test262-async            Check that some script calls the print function with the string 'Test262:AsyncTestComplete'\n");
    fprintf(stderr, "  --strict-file=<file>       Parse the given file as if it were in strict mode (this option may be passed more than once)\n");
    fprintf(stderr, "  --module-file=<file>       Parse and evaluate the given file as module (this option may be passed more than once)\n");
    fprintf(stderr, "  --exception=<name>         Check the last script exits with an uncaught exception with the specified name\n");
    fprintf(stderr, "  --watchdog-exception-ok    Uncaught watchdog exceptions exit with success\n");
    fprintf(stderr, "  --dumpException            Dump uncaught exception text\n");
    fprintf(stderr, "  --footprint                Dump memory footprint after done executing\n");
    fprintf(stderr, "  --options                  Dumps all JSC VM options and exits\n");
    fprintf(stderr, "  --dumpOptions              Dumps all non-default JSC VM options before continuing\n");
    fprintf(stderr, "  --<jsc VM option>=<value>  Sets the specified JSC VM option\n");
    fprintf(stderr, "  --destroy-vm               Destroy VM before exiting\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Files with a .mjs extension will always be evaluated as modules.\n");
    fprintf(stderr, "\n");

    jscExit(help ? EXIT_SUCCESS : EXIT_FAILURE);
}

static bool isMJSFile(char *filename)
{
    filename = strrchr(filename, '.');

    if (filename)
        return !strcmp(filename, ".mjs");

    return false;
}

void CommandLine::parseArguments(int argc, char** argv)
{
    Options::AllowUnfinalizedAccessScope scope;
    Options::initialize();
    
#if PLATFORM(IOS_FAMILY)
    Options::crashIfCantAllocateJITMemory() = true;
#endif

    if (Options::dumpOptions()) {
        printf("Command line:");
#if PLATFORM(COCOA)
        for (char** envp = *_NSGetEnviron(); *envp; envp++) {
            const char* env = *envp;
            if (!strncmp("JSC_", env, 4))
                printf(" %s", env);
        }
#endif // PLATFORM(COCOA)
        for (int i = 0; i < argc; ++i)
            printf(" %s", argv[i]);
        printf("\n");
    }

    int i = 1;
    JSC::Options::DumpLevel dumpOptionsLevel = JSC::Options::DumpLevel::None;
    bool needToExit = false;

    bool hasBadJSCOptions = false;
    for (; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-f")) {
            if (++i == argc)
                printUsageStatement();
            m_scripts.append(Script(Script::StrictMode::Sloppy, Script::CodeSource::File, Script::ScriptType::Script, argv[i]));
            continue;
        }
        if (!strcmp(arg, "-e")) {
            if (++i == argc)
                printUsageStatement();
            m_scripts.append(Script(Script::StrictMode::Sloppy, Script::CodeSource::CommandLine, Script::ScriptType::Script, argv[i]));
            continue;
        }
        if (!strcmp(arg, "-i")) {
            m_interactive = true;
            continue;
        }
        if (!strcmp(arg, "-d")) {
            m_dump = true;
            continue;
        }
        if (!strcmp(arg, "-p")) {
            if (++i == argc)
                printUsageStatement();
            m_profile = true;
            m_profilerOutput = argv[i];
            continue;
        }
        if (!strcmp(arg, "-m")) {
            m_module = true;
            continue;
        }
        if (!strcmp(arg, "-s")) {
#if OS(UNIX)
            SignalAction (*exit)(Signal, SigInfo&, PlatformRegisters&) = [] (Signal, SigInfo&, PlatformRegisters&) {
                dataLogLn("Signal handler hit. Exiting with status 0");
                _exit(0);
                return SignalAction::ForceDefault;
            };

            addSignalHandler(Signal::IllegalInstruction, SignalHandler(exit));
            addSignalHandler(Signal::AccessFault, SignalHandler(exit));
            addSignalHandler(Signal::FloatingPoint, SignalHandler(exit));
            // once we do this lldb won't work anymore because we will exit on any breakpoints it sets.
            addSignalHandler(Signal::Breakpoint, SignalHandler(exit));

            activateSignalHandlersFor(Signal::IllegalInstruction);
            activateSignalHandlersFor(Signal::AccessFault);
            activateSignalHandlersFor(Signal::FloatingPoint);
            activateSignalHandlersFor(Signal::Breakpoint);

#if !OS(DARWIN)
            addSignalHandler(Signal::Abort, SignalHandler(exit));
            activateSignalHandlersFor(Signal::Abort);
#endif
#endif
            continue;
        }
        if (!strcmp(arg, "-x")) {
            m_exitCode = true;
            continue;
        }
        if (!strcmp(arg, "--")) {
            ++i;
            break;
        }
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            printUsageStatement(true);

        if (!strcmp(arg, "--options")) {
            dumpOptionsLevel = JSC::Options::DumpLevel::Verbose;
            needToExit = true;
            continue;
        }
        if (!strcmp(arg, "--dumpOptions")) {
            dumpOptionsLevel = JSC::Options::DumpLevel::Overridden;
            continue;
        }
        if (!strcmp(arg, "--sample")) {
            JSC::Options::useSamplingProfiler() = true;
            JSC::Options::collectSamplingProfilerDataForJSCShell() = true;
            m_dumpSamplingProfilerData = true;
            continue;
        }
        if (!strcmp(arg, "--destroy-vm")) {
            m_destroyVM = true;
            continue;
        }
        if (!strcmp(arg, "--disableOptionsFreezingForTesting")) {
            Config::disableFreezingForTesting();
            continue;
        }

        static const char* timeoutMultiplierOptStr = "--timeoutMultiplier=";
        static const unsigned timeoutMultiplierOptStrLength = strlen(timeoutMultiplierOptStr);
        if (!strncmp(arg, timeoutMultiplierOptStr, timeoutMultiplierOptStrLength)) {
            const char* valueStr = &arg[timeoutMultiplierOptStrLength];
            if (sscanf(valueStr, "%lf", &s_timeoutMultiplier) != 1)
                dataLog("WARNING: --timeoutMultiplier=", valueStr, " is invalid. Expects a numeric ratio.\n");
            continue;
        }

        if (!strcmp(arg, "--test262-async")) {
            asyncTestExpectedPasses++;
            continue;
        }

        if (!strcmp(arg, "--remote-debug")) {
            m_enableRemoteDebugging = true;
            continue;
        }

        static const unsigned strictFileStrLength = strlen("--strict-file=");
        if (!strncmp(arg, "--strict-file=", strictFileStrLength)) {
            m_scripts.append(Script(Script::StrictMode::Strict, Script::CodeSource::File, Script::ScriptType::Script, argv[i] + strictFileStrLength));
            continue;
        }

        static const unsigned moduleFileStrLength = strlen("--module-file=");
        if (!strncmp(arg, "--module-file=", moduleFileStrLength)) {
            m_scripts.append(Script(Script::StrictMode::Sloppy, Script::CodeSource::File, Script::ScriptType::Module, argv[i] + moduleFileStrLength));
            continue;
        }

        if (!strcmp(arg, "--dumpException")) {
            m_alwaysDumpUncaughtException = true;
            continue;
        }

        if (!strcmp(arg, "--footprint")) {
            m_dumpMemoryFootprint = true;
            continue;
        }

        static const unsigned exceptionStrLength = strlen("--exception=");
        if (!strncmp(arg, "--exception=", exceptionStrLength)) {
            m_uncaughtExceptionName = String(arg + exceptionStrLength);
            continue;
        }

        if (!strcmp(arg, "--watchdog-exception-ok")) {
            m_treatWatchdogExceptionAsSuccess = true;
            continue;
        }

        // See if the -- option is a JSC VM option.
        if (strstr(arg, "--") == arg) {
            if (!JSC::Options::setOption(&arg[2])) {
                hasBadJSCOptions = true;
                dataLog("ERROR: invalid option: ", arg, "\n");
            }
            continue;
        }

        // This arg is not recognized by the VM nor by jsc. Pass it on to the
        // script.
        Script::ScriptType scriptType = isMJSFile(argv[i]) ? Script::ScriptType::Module : Script::ScriptType::Script;
        m_scripts.append(Script(Script::StrictMode::Sloppy, Script::CodeSource::File, scriptType, argv[i]));
    }

    if (hasBadJSCOptions && JSC::Options::validateOptions())
        CRASH();

    if (m_scripts.isEmpty())
        m_interactive = true;

    for (; i < argc; ++i)
        m_arguments.append(argv[i]);

    if (dumpOptionsLevel != JSC::Options::DumpLevel::None) {
        const char* optionsTitle = (dumpOptionsLevel == JSC::Options::DumpLevel::Overridden)
            ? "Modified JSC runtime options:"
            : "All JSC runtime options:";
        JSC::Options::dumpAllOptions(stderr, dumpOptionsLevel, optionsTitle);
    }
    JSC::Options::ensureOptionsAreCoherent();
    if (needToExit)
        jscExit(EXIT_SUCCESS);
}

template<typename Func>
int runJSC(const CommandLine& options, bool isWorker, const Func& func)
{
    Worker worker(Workers::singleton());
    
    VM& vm = VM::create(LargeHeap).leakRef();
#if ENABLE(WEBASSEMBLY)
    Wasm::enableFastMemory();
#endif

    int result;
    bool success = true;
    GlobalObject* globalObject = nullptr;
    {
        JSLockHolder locker(vm);

        startTimeoutThreadIfNeeded(vm);
        if (options.m_profile && !vm.m_perBytecodeProfiler)
            vm.m_perBytecodeProfiler = makeUnique<Profiler::Database>(vm);

        globalObject = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), options.m_arguments);
        globalObject->setRemoteDebuggingEnabled(options.m_enableRemoteDebugging);
        func(vm, globalObject, success);
        vm.drainMicrotasks();
    }
    vm.deferredWorkTimer->runRunLoop();
    {
        JSLockHolder locker(vm);
        if (options.m_interactive && success)
            runInteractive(globalObject);
    }

    result = success && (asyncTestExpectedPasses == asyncTestPasses) ? 0 : 3;

    if (options.m_exitCode) {
        printf("jsc exiting %d", result);
        if (asyncTestExpectedPasses != asyncTestPasses)
            printf(" because expected: %d async test passes but got: %d async test passes", asyncTestExpectedPasses, asyncTestPasses);
        printf("\n");
    }

    if (options.m_profile) {
        JSLockHolder locker(vm);
        if (!vm.m_perBytecodeProfiler->save(options.m_profilerOutput.utf8().data()))
            fprintf(stderr, "could not save profiler output.\n");
    }

#if ENABLE(JIT)
    {
        JSLockHolder locker(vm);
        if (Options::useExceptionFuzz())
            printf("JSC EXCEPTION FUZZ: encountered %u checks.\n", numberOfExceptionFuzzChecks());
        bool fireAtEnabled =
        Options::fireExecutableAllocationFuzzAt() || Options::fireExecutableAllocationFuzzAtOrAfter();
        if (Options::useExecutableAllocationFuzz() && (!fireAtEnabled || Options::verboseExecutableAllocationFuzz()))
            printf("JSC EXECUTABLE ALLOCATION FUZZ: encountered %u checks.\n", numberOfExecutableAllocationFuzzChecks());
        if (Options::useOSRExitFuzz()) {
            printf("JSC OSR EXIT FUZZ: encountered %u static checks.\n", numberOfStaticOSRExitFuzzChecks());
            printf("JSC OSR EXIT FUZZ: encountered %u dynamic checks.\n", numberOfOSRExitFuzzChecks());
        }

        
        auto compileTimeStats = JIT::compileTimeStats();
        Vector<CString> compileTimeKeys;
        for (auto& entry : compileTimeStats)
            compileTimeKeys.append(entry.key);
        std::sort(compileTimeKeys.begin(), compileTimeKeys.end());
        for (const CString& key : compileTimeKeys)
            printf("%40s: %.3lf ms\n", key.data(), compileTimeStats.get(key).milliseconds());

        if (Options::reportTotalPhaseTimes())
            logTotalPhaseTimes();
    }
#endif

    if (Options::gcAtEnd()) {
        // We need to hold the API lock to do a GC.
        JSLockHolder locker(&vm);
        vm.heap.collectNow(Sync, CollectionScope::Full);
    }

    if (options.m_dumpSamplingProfilerData) {
#if ENABLE(SAMPLING_PROFILER)
        JSLockHolder locker(&vm);
        vm.samplingProfiler()->reportTopFunctions();
        vm.samplingProfiler()->reportTopBytecodes();
#else
        dataLog("Sampling profiler is not enabled on this platform\n");
#endif
    }

    vm.codeCache()->write(vm);

    if (options.m_destroyVM || isWorker) {
        JSLockHolder locker(vm);
        // This is needed because we don't want the worker's main
        // thread to die before its compilation threads finish.
        vm.deref();
    }

    return result;
}

int jscmain(int argc, char** argv)
{
    // Need to override and enable restricted options before we start parsing options below.
    Config::enableRestrictedOptions();

    WTF::initializeMainThread();

    // Note that the options parsing can affect VM creation, and thus
    // comes first.
    CommandLine options(argc, argv);

    {
        Options::AllowUnfinalizedAccessScope scope;
        processConfigFile(Options::configFile(), "jsc");
        if (options.m_dump)
            Options::dumpGeneratedBytecodes() = true;
    }

    JSC::initialize();
    initializeTimeoutIfNeeded();

#if OS(DARWIN) || OS(LINUX)
    startMemoryMonitoringThreadIfNeeded();
#endif

    if (Options::useSuperSampler())
        enableSuperSampler();

    bool gigacageDisableRequested = false;
#if GIGACAGE_ENABLED && !COMPILER(MSVC)
    if (char* gigacageEnabled = getenv("GIGACAGE_ENABLED")) {
        if (!strcasecmp(gigacageEnabled, "no") || !strcasecmp(gigacageEnabled, "false") || !strcasecmp(gigacageEnabled, "0"))
            gigacageDisableRequested = true;
    }
#endif
    if (!gigacageDisableRequested)
        Gigacage::forbidDisablingPrimitiveGigacage();

#if PLATFORM(COCOA)
    auto& memoryPressureHandler = MemoryPressureHandler::singleton();
    {
        dispatch_queue_t queue = dispatch_queue_create("jsc shell memory pressure handler", DISPATCH_QUEUE_SERIAL);
        memoryPressureHandler.setDispatchQueue(queue);
        dispatch_release(queue);
    }
    Box<Critical> memoryPressureCriticalState = Box<Critical>::create(Critical::No);
    Box<Synchronous> memoryPressureSynchronousState = Box<Synchronous>::create(Synchronous::No);
    memoryPressureHandler.setLowMemoryHandler([=] (Critical critical, Synchronous synchronous) {
        crashIfExceedingMemoryLimit();

        // We set these racily with respect to reading them from the JS execution thread.
        *memoryPressureCriticalState = critical;
        *memoryPressureSynchronousState = synchronous;
    });
    memoryPressureHandler.setShouldLogMemoryMemoryPressureEvents(false);
    memoryPressureHandler.install();

    auto onEachMicrotaskTick = [&] (VM& vm) {
        if (*memoryPressureCriticalState == Critical::No)
            return;

        *memoryPressureCriticalState = Critical::No;
        bool isSynchronous = *memoryPressureSynchronousState == Synchronous::Yes;

        WTF::releaseFastMallocFreeMemory();
        vm.deleteAllCode(DeleteAllCodeIfNotCollecting);

        if (!vm.heap.isCurrentThreadBusy()) {
            if (isSynchronous) {
                vm.heap.collectNow(Sync, CollectionScope::Full);
                WTF::releaseFastMallocFreeMemory();
            } else
                vm.heap.collectNowFullIfNotDoneRecently(Async);
        }
    };
#endif

    int result = runJSC(
        options, false,
        [&] (VM& vm, GlobalObject* globalObject, bool& success) {
            UNUSED_PARAM(vm);
#if PLATFORM(COCOA)
            vm.setOnEachMicrotaskTick(WTFMove(onEachMicrotaskTick));
#endif
            runWithOptions(globalObject, options, success);
        });

    printSuperSamplerState();

    if (options.m_dumpMemoryFootprint) {
        MemoryFootprint footprint = MemoryFootprint::now();

        printf("Memory Footprint:\n    Current Footprint: %" PRIu64 "\n    Peak Footprint: %" PRIu64 "\n", footprint.current, footprint.peak);
    }

    return result;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif
