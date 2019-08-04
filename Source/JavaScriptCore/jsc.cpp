/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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
#include "ArrayPrototype.h"
#include "BuiltinNames.h"
#include "ButterflyInlines.h"
#include "CatchScope.h"
#include "CodeBlock.h"
#include "CodeCache.h"
#include "Completion.h"
#include "ConfigFile.h"
#include "Disassembler.h"
#include "Exception.h"
#include "ExceptionHelpers.h"
#include "HeapProfiler.h"
#include "HeapSnapshotBuilder.h"
#include "InitializeThreading.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSArray.h"
#include "JSArrayBuffer.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseDeferred.h"
#include "JSLock.h"
#include "JSModuleLoader.h"
#include "JSNativeStdFunction.h"
#include "JSONObject.h"
#include "JSSourceCode.h"
#include "JSString.h"
#include "JSTypedArrays.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyMemory.h"
#include "LLIntThunks.h"
#include "ObjectConstructor.h"
#include "ParserError.h"
#include "ProfilerDatabase.h"
#include "PromiseDeferredTimer.h"
#include "ProtoCallFrame.h"
#include "ReleaseHeapAccessScope.h"
#include "SamplingProfiler.h"
#include "StackVisitor.h"
#include "StructureInlines.h"
#include "StructureRareDataInlines.h"
#include "SuperSampler.h"
#include "TestRunnerUtils.h"
#include "TypedArrayInlines.h"
#include "WasmContext.h"
#include "WasmFaultSignalHandler.h"
#include "WasmMemory.h"
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <wtf/Box.h>
#include <wtf/CommaPrinter.h>
#include <wtf/MainThread.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if OS(WINDOWS)
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <wtf/text/win/WCharStringExtras.h>
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

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SIGNAL_H)
#include <signal.h>
#endif

#if COMPILER(MSVC)
#include <crtdbg.h>
#include <mmsystem.h>
#include <windows.h>
#endif

#if PLATFORM(IOS_FAMILY) && CPU(ARM_THUMB2)
#include <fenv.h>
#include <arm/arch.h>
#endif

#if __has_include(<WebKitAdditions/MemoryFootprint.h>)
#include <WebKitAdditions/MemoryFootprint.h>
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

class Masquerader : public JSNonFinalObject {
public:
    Masquerader(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | JSC::MasqueradesAsUndefined;

    static Masquerader* create(VM& vm, JSGlobalObject* globalObject)
    {
        globalObject->masqueradesAsUndefinedWatchpoint()->fireAll(vm, "Masquerading object allocated");
        Structure* structure = createStructure(vm, globalObject, jsNull());
        Masquerader* result = new (NotNull, allocateCell<Masquerader>(vm.heap, sizeof(Masquerader))) Masquerader(vm, structure);
        result->finishCreation(vm);
        return result;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;
};

const ClassInfo Masquerader::s_info = { "Masquerader", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(Masquerader) };
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
static void checkException(ExecState*, GlobalObject*, bool isLastFile, bool hasException, JSValue, CommandLine&, bool& success);

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


static EncodedJSValue JSC_HOST_CALL functionCreateGlobalObject(ExecState*);

static EncodedJSValue JSC_HOST_CALL functionPrintStdOut(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionPrintStdErr(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDebug(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDescribe(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDescribeArray(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionSleepSeconds(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionJSCStack(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGCAndSweep(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFullGC(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionEdenGC(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionForceGCSlowPaths(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionHeapSize(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateMemoryFootprint(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionResetMemoryPeak(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionAddressOf(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionVersion(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionRun(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionRunString(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionLoad(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionLoadString(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionReadFile(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCheckSyntax(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionReadline(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionPreciseTime(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNeverInlineFunction(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNoDFG(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNoFTL(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNoOSRExitFuzzing(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionOptimizeNextInvocation(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNumberOfDFGCompiles(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionJSCOptions(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionReoptimizationRetryCount(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionTransferArrayBuffer(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFailNextNewCodeBlock(ExecState*);
static NO_RETURN_WITH_VALUE EncodedJSValue JSC_HOST_CALL functionQuit(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFalse(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionUndefined1(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionUndefined2(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIsInt32(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIsPureNaN(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionEffectful42(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIdentity(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionMakeMasquerader(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionHasCustomProperties(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDumpTypesForAllVariables(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDrainMicrotasks(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIs32BitPlatform(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCheckModuleSyntax(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionPlatformSupportsSamplingProfiler(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshot(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshotForGCDebugging(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionResetSuperSamplerState(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionEnsureArrayStorage(ExecState*);
#if ENABLE(SAMPLING_PROFILER)
static EncodedJSValue JSC_HOST_CALL functionStartSamplingProfiler(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionSamplingProfilerStackTraces(ExecState*);
#endif

static EncodedJSValue JSC_HOST_CALL functionMaxArguments(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionAsyncTestStart(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionAsyncTestPassed(ExecState*);

#if ENABLE(WEBASSEMBLY)
static EncodedJSValue JSC_HOST_CALL functionWebAssemblyMemoryMode(ExecState*);
#endif

#if ENABLE(SAMPLING_FLAGS)
static EncodedJSValue JSC_HOST_CALL functionSetSamplingFlags(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionClearSamplingFlags(ExecState*);
#endif

static EncodedJSValue JSC_HOST_CALL functionGetRandomSeed(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionSetRandomSeed(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIsRope(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCallerSourceOrigin(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarCreateRealm(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarDetachArrayBuffer(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarEvalScript(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentStart(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentReceiveBroadcast(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentReport(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentSleep(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentBroadcast(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentGetReport(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentLeaving(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDollarAgentMonotonicNow(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionWaitForReport(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionHeapCapacity(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFlashHeapAccess(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDisableRichSourceInfo(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionMallocInALoop(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionTotalCompileTime(ExecState*);

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

    bool m_interactive { false };
    bool m_dump { false };
    bool m_module { false };
    bool m_exitCode { false };
    Vector<Script> m_scripts;
    Vector<String> m_arguments;
    bool m_profile { false };
    String m_profilerOutput;
    String m_uncaughtExceptionName;
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

class GlobalObject : public JSGlobalObject {
private:
    GlobalObject(VM&, Structure*);

public:
    typedef JSGlobalObject Base;

    static GlobalObject* create(VM& vm, Structure* structure, const Vector<String>& arguments)
    {
        GlobalObject* object = new (NotNull, allocateCell<GlobalObject>(vm.heap)) GlobalObject(vm, structure);
        object->finishCreation(vm, arguments);
        return object;
    }

    static const bool needsDestruction = false;

    DECLARE_INFO;
    static const GlobalObjectMethodTable s_globalObjectMethodTable;

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        return Structure::create(vm, 0, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    }

    static RuntimeFlags javaScriptRuntimeFlags(const JSGlobalObject*) { return RuntimeFlags::createAllEnabled(); }

protected:
    void finishCreation(VM& vm, const Vector<String>& arguments)
    {
        Base::finishCreation(vm);

        addFunction(vm, "debug", functionDebug, 1);
        addFunction(vm, "describe", functionDescribe, 1);
        addFunction(vm, "describeArray", functionDescribeArray, 1);
        addFunction(vm, "print", functionPrintStdOut, 1);
        addFunction(vm, "printErr", functionPrintStdErr, 1);
        addFunction(vm, "quit", functionQuit, 0);
        addFunction(vm, "gc", functionGCAndSweep, 0);
        addFunction(vm, "fullGC", functionFullGC, 0);
        addFunction(vm, "edenGC", functionEdenGC, 0);
        addFunction(vm, "forceGCSlowPaths", functionForceGCSlowPaths, 0);
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
        addFunction(vm, "jscOptions", functionJSCOptions, 0);
        addFunction(vm, "optimizeNextInvocation", functionOptimizeNextInvocation, 1);
        addFunction(vm, "reoptimizationRetryCount", functionReoptimizationRetryCount, 1);
        addFunction(vm, "transferArrayBuffer", functionTransferArrayBuffer, 1);
        addFunction(vm, "failNextNewCodeBlock", functionFailNextNewCodeBlock, 1);
#if ENABLE(SAMPLING_FLAGS)
        addFunction(vm, "setSamplingFlags", functionSetSamplingFlags, 1);
        addFunction(vm, "clearSamplingFlags", functionClearSamplingFlags, 1);
#endif

        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "OSRExit"), 0, functionUndefined1, OSRExitIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "isFinalTier"), 0, functionFalse, IsFinalTierIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "predictInt32"), 0, functionUndefined2, SetInt32HeapPredictionIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "isInt32"), 0, functionIsInt32, CheckInt32Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "isPureNaN"), 0, functionIsPureNaN, CheckInt32Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "fiatInt52"), 0, functionIdentity, FiatInt52Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        
        addFunction(vm, "effectful42", functionEffectful42, 0);
        addFunction(vm, "makeMasquerader", functionMakeMasquerader, 0);
        addFunction(vm, "hasCustomProperties", functionHasCustomProperties, 0);

        addFunction(vm, "createGlobalObject", functionCreateGlobalObject, 0);

        addFunction(vm, "dumpTypesForAllVariables", functionDumpTypesForAllVariables , 0);

        addFunction(vm, "drainMicrotasks", functionDrainMicrotasks, 0);

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
            JSArray* array = constructEmptyArray(globalExec(), 0);
            for (size_t i = 0; i < arguments.size(); ++i)
                array->putDirectIndex(globalExec(), i, jsString(globalExec(), arguments[i]));
            putDirect(vm, Identifier::fromString(globalExec(), "arguments"), array);
        }

        putDirect(vm, Identifier::fromString(globalExec(), "console"), jsUndefined());
        
        Structure* plainObjectStructure = JSFinalObject::createStructure(vm, this, objectPrototype(), 0);
        
        JSObject* dollar = JSFinalObject::create(vm, plainObjectStructure);
        putDirect(vm, Identifier::fromString(globalExec(), "$"), dollar);
        putDirect(vm, Identifier::fromString(globalExec(), "$262"), dollar);
        
        addFunction(vm, dollar, "createRealm", functionDollarCreateRealm, 0);
        addFunction(vm, dollar, "detachArrayBuffer", functionDollarDetachArrayBuffer, 1);
        addFunction(vm, dollar, "evalScript", functionDollarEvalScript, 1);
        
        dollar->putDirect(vm, Identifier::fromString(globalExec(), "global"), this);
        
        JSObject* agent = JSFinalObject::create(vm, plainObjectStructure);
        dollar->putDirect(vm, Identifier::fromString(globalExec(), "agent"), agent);
        
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
    }
    
    void addFunction(VM& vm, JSObject* object, const char* name, NativeFunction function, unsigned arguments)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        object->putDirect(vm, identifier, JSFunction::create(vm, this, arguments, identifier.string(), function));
    }

    void addFunction(VM& vm, const char* name, NativeFunction function, unsigned arguments)
    {
        addFunction(vm, this, name, function, arguments);
    }
    
    static JSInternalPromise* moduleLoaderImportModule(JSGlobalObject*, ExecState*, JSModuleLoader*, JSString*, JSValue, const SourceOrigin&);
    static Identifier moduleLoaderResolve(JSGlobalObject*, ExecState*, JSModuleLoader*, JSValue, JSValue, JSValue);
    static JSInternalPromise* moduleLoaderFetch(JSGlobalObject*, ExecState*, JSModuleLoader*, JSValue, JSValue, JSValue);
    static JSObject* moduleLoaderCreateImportMetaProperties(JSGlobalObject*, ExecState*, JSModuleLoader*, JSValue, JSModuleRecord*, JSValue);
};

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

struct DirectoryName {
    // In unix, it is "/". In Windows, it becomes a drive letter like "C:\"
    String rootName;

    // If the directory name is "/home/WebKit", this becomes "home/WebKit". If the directory name is "/", this becomes "".
    String queryName;
};

struct ModuleName {
    ModuleName(const String& moduleName);

    bool startsWithRoot() const
    {
        return !queries.isEmpty() && queries[0].isEmpty();
    }

    Vector<String> queries;
};

ModuleName::ModuleName(const String& moduleName)
{
    // A module name given from code is represented as the UNIX style path. Like, `./A/B.js`.
    queries = moduleName.splitAllowingEmptyEntries('/');
}

static Optional<DirectoryName> extractDirectoryName(const String& absolutePathToFile)
{
    size_t firstSeparatorPosition = absolutePathToFile.find(pathSeparator());
    if (firstSeparatorPosition == notFound)
        return WTF::nullopt;
    DirectoryName directoryName;
    directoryName.rootName = absolutePathToFile.substring(0, firstSeparatorPosition + 1); // Include the separator.
    size_t lastSeparatorPosition = absolutePathToFile.reverseFind(pathSeparator());
    ASSERT_WITH_MESSAGE(lastSeparatorPosition != notFound, "If the separator is not found, this function already returns when performing the forward search.");
    if (firstSeparatorPosition == lastSeparatorPosition)
        directoryName.queryName = StringImpl::empty();
    else {
        size_t queryStartPosition = firstSeparatorPosition + 1;
        size_t queryLength = lastSeparatorPosition - queryStartPosition; // Not include the last separator.
        directoryName.queryName = absolutePathToFile.substring(queryStartPosition, queryLength);
    }
    return directoryName;
}

static Optional<DirectoryName> currentWorkingDirectory()
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
        return WTF::nullopt;
    // In Windows, wchar_t is the UTF-16LE.
    // https://msdn.microsoft.com/en-us/library/dd374081.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff381407.aspx
    Vector<wchar_t> buffer(bufferLength);
    DWORD lengthNotIncludingNull = ::GetCurrentDirectoryW(bufferLength, buffer.data());
    String directoryString = wcharToString(buffer.data(), lengthNotIncludingNull);
    // We don't support network path like \\host\share\<path name>.
    if (directoryString.startsWith("\\\\"))
        return WTF::nullopt;
#else
    Vector<char> buffer(PATH_MAX);
    if (!getcwd(buffer.data(), PATH_MAX))
        return WTF::nullopt;
    String directoryString = String::fromUTF8(buffer.data());
#endif
    if (directoryString.isEmpty())
        return WTF::nullopt;

    if (directoryString[directoryString.length() - 1] == pathSeparator())
        return extractDirectoryName(directoryString);
    // Append the seperator to represents the file name. extractDirectoryName only accepts the absolute file name.
    return extractDirectoryName(makeString(directoryString, pathSeparator()));
}

static String resolvePath(const DirectoryName& directoryName, const ModuleName& moduleName)
{
    Vector<String> directoryPieces = directoryName.queryName.split(pathSeparator());

    // Only first '/' is recognized as the path from the root.
    if (moduleName.startsWithRoot())
        directoryPieces.clear();

    for (const auto& query : moduleName.queries) {
        if (query == String(".."_s)) {
            if (!directoryPieces.isEmpty())
                directoryPieces.removeLast();
        } else if (!query.isEmpty() && query != String("."_s))
            directoryPieces.append(query);
    }

    StringBuilder builder;
    builder.append(directoryName.rootName);
    for (size_t i = 0; i < directoryPieces.size(); ++i) {
        builder.append(directoryPieces[i]);
        if (i + 1 != directoryPieces.size())
            builder.append(pathSeparator());
    }
    return builder.toString();
}

static String absolutePath(const String& fileName)
{
    auto directoryName = currentWorkingDirectory();
    if (!directoryName)
        return fileName;
    return resolvePath(directoryName.value(), ModuleName(fileName.impl()));
}

JSInternalPromise* GlobalObject::moduleLoaderImportModule(JSGlobalObject* globalObject, ExecState* exec, JSModuleLoader*, JSString* moduleNameValue, JSValue parameters, const SourceOrigin& sourceOrigin)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto* deferred = JSInternalPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto reject = [&] (JSValue rejectionReason) {
        catchScope.clearException();
        auto result = deferred->reject(exec, rejectionReason);
        catchScope.clearException();
        return result;
    };

    if (sourceOrigin.isNull())
        return reject(createError(exec, "Could not resolve the module specifier."_s));

    const auto& referrer = sourceOrigin.string();
    const auto& moduleName = moduleNameValue->value(exec);
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception());

    auto directoryName = extractDirectoryName(referrer.impl());
    if (!directoryName)
        return reject(createError(exec, makeString("Could not resolve the referrer name '", String(referrer.impl()), "'.")));

    auto result = JSC::importModule(exec, Identifier::fromString(&vm, resolvePath(directoryName.value(), ModuleName(moduleName))), parameters, jsUndefined());
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception());
    return result;
}

Identifier GlobalObject::moduleLoaderResolve(JSGlobalObject* globalObject, ExecState* exec, JSModuleLoader*, JSValue keyValue, JSValue referrerValue, JSValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    scope.releaseAssertNoException();
    const Identifier key = keyValue.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, { });

    if (key.isSymbol())
        return key;

    if (referrerValue.isUndefined()) {
        auto directoryName = currentWorkingDirectory();
        if (!directoryName) {
            throwException(exec, scope, createError(exec, "Could not resolve the current working directory."_s));
            return { };
        }
        return Identifier::fromString(&vm, resolvePath(directoryName.value(), ModuleName(key.impl())));
    }

    const Identifier referrer = referrerValue.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, { });

    if (referrer.isSymbol()) {
        auto directoryName = currentWorkingDirectory();
        if (!directoryName) {
            throwException(exec, scope, createError(exec, "Could not resolve the current working directory."_s));
            return { };
        }
        return Identifier::fromString(&vm, resolvePath(directoryName.value(), ModuleName(key.impl())));
    }

    // If the referrer exists, we assume that the referrer is the correct absolute path.
    auto directoryName = extractDirectoryName(referrer.impl());
    if (!directoryName) {
        throwException(exec, scope, createError(exec, makeString("Could not resolve the referrer name '", String(referrer.impl()), "'.")));
        return { };
    }
    return Identifier::fromString(&vm, resolvePath(directoryName.value(), ModuleName(key.impl())));
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

class ShellSourceProvider : public StringSourceProvider {
public:
    static Ref<ShellSourceProvider> create(const String& source, const SourceOrigin& sourceOrigin, URL&& url, const TextPosition& startPosition, SourceProviderSourceType sourceType)
    {
        return adoptRef(*new ShellSourceProvider(source, sourceOrigin, WTFMove(url), startPosition, sourceType));
    }

    ~ShellSourceProvider()
    {
#if OS(DARWIN)
        if (m_cachedBytecode.size())
            munmap(const_cast<void*>(m_cachedBytecode.data()), m_cachedBytecode.size());
#endif
    }

    const CachedBytecode* cachedBytecode() const override
    {
        return &m_cachedBytecode;
    }

    void cacheBytecode(const BytecodeCacheGenerator& generator) const override
    {
#if OS(DARWIN)
        String filename = cachePath();
        if (filename.isNull())
            return;
        int fd = open(filename.utf8().data(), O_CREAT | O_WRONLY | O_TRUNC | O_EXLOCK | O_NONBLOCK, 0666);
        if (fd == -1)
            return;
        CachedBytecode cachedBytecode = generator();
        write(fd, cachedBytecode.data(), cachedBytecode.size());
        close(fd);
#else
        UNUSED_PARAM(generator);
#endif
    }

private:
    String cachePath() const
    {
        const char* cachePath = Options::diskCachePath();
        if (!cachePath)
            return static_cast<const char*>(nullptr);
        String filename = sourceOrigin().string();
        filename.replace('/', '_');
        return makeString(cachePath, '/', source().toString().hash(), '-', filename, ".bytecode-cache");
    }

    void loadBytecode()
    {
#if OS(DARWIN)
        String filename = cachePath();
        if (filename.isNull())
            return;

        int fd = open(filename.utf8().data(), O_RDONLY | O_SHLOCK | O_NONBLOCK);
        if (fd == -1)
            return;

        auto closeFD = makeScopeExit([&] {
            close(fd);
        });

        struct stat sb;
        int res = fstat(fd, &sb);
        size_t size = static_cast<size_t>(sb.st_size);
        if (res || !size)
            return;

        void* buffer = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (buffer == MAP_FAILED)
            return;
        m_cachedBytecode = CachedBytecode { buffer, size };
#endif
    }

    ShellSourceProvider(const String& source, const SourceOrigin& sourceOrigin, URL&& url, const TextPosition& startPosition, SourceProviderSourceType sourceType)
        : StringSourceProvider(source, sourceOrigin, WTFMove(url), startPosition, sourceType)
    {
        loadBytecode();
    }

    CachedBytecode m_cachedBytecode;
};

static inline SourceCode jscSource(const String& source, const SourceOrigin& sourceOrigin, URL&& url = URL(), const TextPosition& startPosition = TextPosition(), SourceProviderSourceType sourceType = SourceProviderSourceType::Program)
{
    return SourceCode(ShellSourceProvider::create(source, sourceOrigin, WTFMove(url), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt());
}

template<typename Vector>
static inline SourceCode jscSource(const Vector& utf8, const SourceOrigin& sourceOrigin, const String& filename)
{
    // FIXME: This should use an absolute file URL https://bugs.webkit.org/show_bug.cgi?id=193077
    String str = stringFromUTF(utf8);
    return jscSource(str, sourceOrigin, URL({ }, filename));
}

template<typename Vector>
static bool fetchModuleFromLocalFileSystem(const String& fileName, Vector& buffer)
{
    // We assume that fileName is always an absolute path.
#if OS(WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#maxpath
    // Use long UNC to pass the long path name to the Windows APIs.
    String longUNCPathName = WTF::makeString("\\\\?\\", fileName);
    auto pathName = stringToNullTerminatedWChar(longUNCPathName);
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

JSInternalPromise* GlobalObject::moduleLoaderFetch(JSGlobalObject* globalObject, ExecState* exec, JSModuleLoader*, JSValue key, JSValue, JSValue)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto reject = [&] (JSValue rejectionReason) {
        catchScope.clearException();
        auto result = deferred->reject(exec, rejectionReason);
        catchScope.clearException();
        return result;
    };

    String moduleKey = key.toWTFString(exec);
    if (UNLIKELY(catchScope.exception()))
        return reject(catchScope.exception());

    // Here, now we consider moduleKey as the fileName.
    Vector<uint8_t> buffer;
    if (!fetchModuleFromLocalFileSystem(moduleKey, buffer))
        return reject(createError(exec, makeString("Could not open file '", moduleKey, "'.")));


    URL moduleURL = URL({ }, moduleKey);
#if ENABLE(WEBASSEMBLY)
    // FileSystem does not have mime-type header. The JSC shell recognizes WebAssembly's magic header.
    if (buffer.size() >= 4) {
        if (buffer[0] == '\0' && buffer[1] == 'a' && buffer[2] == 's' && buffer[3] == 'm') {
            auto source = SourceCode(WebAssemblySourceProvider::create(WTFMove(buffer), SourceOrigin { moduleKey }, WTFMove(moduleURL)));
            catchScope.releaseAssertNoException();
            auto sourceCode = JSSourceCode::create(vm, WTFMove(source));
            catchScope.releaseAssertNoException();
            auto result = deferred->resolve(exec, sourceCode);
            catchScope.clearException();
            return result;
        }
    }
#endif

    auto sourceCode = JSSourceCode::create(vm, jscSource(stringFromUTF(buffer), SourceOrigin { moduleKey }, WTFMove(moduleURL), TextPosition(), SourceProviderSourceType::Module));
    catchScope.releaseAssertNoException();
    auto result = deferred->resolve(exec, sourceCode);
    catchScope.clearException();
    return result;
}

JSObject* GlobalObject::moduleLoaderCreateImportMetaProperties(JSGlobalObject* globalObject, ExecState* exec, JSModuleLoader*, JSValue key, JSModuleRecord*, JSValue)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* metaProperties = constructEmptyObject(exec, globalObject->nullPrototypeObjectStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    metaProperties->putDirect(vm, Identifier::fromString(&vm, "filename"), key);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return metaProperties;
}

static CString cStringFromViewWithString(ExecState* exec, ThrowScope& scope, StringViewWithUnderlyingString& viewWithString)
{
    Expected<CString, UTF8ConversionError> expectedString = viewWithString.view.tryGetUtf8();
    if (expectedString)
        return expectedString.value();
    switch (expectedString.error()) {
    case UTF8ConversionError::OutOfMemory:
        throwOutOfMemoryError(exec, scope);
        break;
    case UTF8ConversionError::IllegalSource:
        scope.throwException(exec, createError(exec, "Illegal source encountered during UTF8 conversion"));
        break;
    case UTF8ConversionError::SourceExhausted:
        scope.throwException(exec, createError(exec, "Source exhausted during UTF8 conversion"));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return { };
}

static EncodedJSValue printInternal(ExecState* exec, FILE* out)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (asyncTestExpectedPasses) {
        JSValue value = exec->argument(0);
        if (value.isString() && WTF::equal(asString(value)->value(exec).impl(), "Test262:AsyncTestComplete")) {
            asyncTestPasses++;
            return JSValue::encode(jsUndefined());
        }
    }

    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        if (i)
            if (EOF == fputc(' ', out))
                goto fail;

        auto viewWithString = exec->uncheckedArgument(i).toString(exec)->viewWithUnderlyingString(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        auto string = cStringFromViewWithString(exec, scope, viewWithString);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (fprintf(out, "%s", string.data()) < 0)
            goto fail;
    }

    fputc('\n', out);
fail:
    fflush(out);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionPrintStdOut(ExecState* exec) { return printInternal(exec, stdout); }
EncodedJSValue JSC_HOST_CALL functionPrintStdErr(ExecState* exec) { return printInternal(exec, stderr); }

EncodedJSValue JSC_HOST_CALL functionDebug(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto viewWithString = exec->argument(0).toString(exec)->viewWithUnderlyingString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto string = cStringFromViewWithString(exec, scope, viewWithString);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    fprintf(stderr, "--> %s\n", string.data());
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDescribe(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(exec, toString(exec->argument(0))));
}

EncodedJSValue JSC_HOST_CALL functionDescribeArray(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    VM& vm = exec->vm();
    JSObject* object = jsDynamicCast<JSObject*>(vm, exec->argument(0));
    if (!object)
        return JSValue::encode(jsNontrivialString(exec, "<not object>"_s));
    return JSValue::encode(jsNontrivialString(exec, toString("<Butterfly: ", RawPointer(object->butterfly()), "; public length: ", object->getArrayLength(), "; vector length: ", object->getVectorLength(), ">")));
}

EncodedJSValue JSC_HOST_CALL functionSleepSeconds(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (exec->argumentCount() >= 1) {
        Seconds seconds = Seconds(exec->argument(0).toNumber(exec));
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

EncodedJSValue JSC_HOST_CALL functionJSCStack(ExecState* exec)
{
    StringBuilder trace;
    trace.appendLiteral("--> Stack trace:\n");

    FunctionJSCStackFunctor functor(trace);
    exec->iterate(functor);
    fprintf(stderr, "%s", trace.toString().utf8().data());
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionGCAndSweep(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    vm.heap.collectNow(Sync, CollectionScope::Full);
    return JSValue::encode(jsNumber(vm.heap.sizeAfterLastFullCollection()));
}

EncodedJSValue JSC_HOST_CALL functionFullGC(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    vm.heap.collectSync(CollectionScope::Full);
    return JSValue::encode(jsNumber(vm.heap.sizeAfterLastFullCollection()));
}

EncodedJSValue JSC_HOST_CALL functionEdenGC(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    vm.heap.collectSync(CollectionScope::Eden);
    return JSValue::encode(jsNumber(vm.heap.sizeAfterLastEdenCollection()));
}

EncodedJSValue JSC_HOST_CALL functionForceGCSlowPaths(ExecState*)
{
    // It's best for this to be the first thing called in the 
    // JS program so the option is set to true before we JIT.
    Options::forceGCSlowPaths() = true;
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionHeapSize(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(jsNumber(vm.heap.size()));
}

class JSCMemoryFootprint : public JSDestructibleObject {
    using Base = JSDestructibleObject;
public:
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
        JSCMemoryFootprint* footprint = new (NotNull, allocateCell<JSCMemoryFootprint>(vm.heap, sizeof(JSCMemoryFootprint))) JSCMemoryFootprint(vm, structure);
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
        Identifier identifier = Identifier::fromString(&vm, name);
        putDirect(vm, identifier, value);
    }
};

const ClassInfo JSCMemoryFootprint::s_info = { "MemoryFootprint", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCMemoryFootprint) };

EncodedJSValue JSC_HOST_CALL functionCreateMemoryFootprint(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(JSCMemoryFootprint::create(vm, exec->lexicalGlobalObject()));
}

EncodedJSValue JSC_HOST_CALL functionResetMemoryPeak(ExecState*)
{
    MemoryFootprint::resetPeak();
    return JSValue::encode(jsUndefined());
}

// This function is not generally very helpful in 64-bit code as the tag and payload
// share a register. But in 32-bit JITed code the tag may not be checked if an
// optimization removes type checking requirements, such as in ===.
EncodedJSValue JSC_HOST_CALL functionAddressOf(ExecState* exec)
{
    JSValue value = exec->argument(0);
    if (!value.isCell())
        return JSValue::encode(jsUndefined());
    // Need to cast to uint64_t so bitwise_cast will play along.
    uint64_t asNumber = reinterpret_cast<uint64_t>(value.asCell());
    EncodedJSValue returnValue = JSValue::encode(jsNumber(bitwise_cast<double>(asNumber)));
    return returnValue;
}

EncodedJSValue JSC_HOST_CALL functionVersion(ExecState*)
{
    // We need this function for compatibility with the Mozilla JS tests but for now
    // we don't actually do any version-specific handling
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionRun(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String fileName = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(throwException(exec, scope, createError(exec, "Could not open file."_s)));

    GlobalObject* globalObject = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>());

    JSArray* array = constructEmptyArray(globalObject->globalExec(), 0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    for (unsigned i = 1; i < exec->argumentCount(); ++i) {
        array->putDirectIndex(globalObject->globalExec(), i - 1, exec->uncheckedArgument(i));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    globalObject->putDirect(
        vm, Identifier::fromString(globalObject->globalExec(), "arguments"), array);

    NakedPtr<Exception> exception;
    StopWatch stopWatch;
    stopWatch.start();
    evaluate(globalObject->globalExec(), jscSource(script, SourceOrigin { absolutePath(fileName) }, fileName), JSValue(), exception);
    stopWatch.stop();

    if (exception) {
        throwException(globalObject->globalExec(), scope, exception);
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

EncodedJSValue JSC_HOST_CALL functionRunString(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String source = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    GlobalObject* globalObject = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>());

    JSArray* array = constructEmptyArray(globalObject->globalExec(), 0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    for (unsigned i = 1; i < exec->argumentCount(); ++i) {
        array->putDirectIndex(globalObject->globalExec(), i - 1, exec->uncheckedArgument(i));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    globalObject->putDirect(
        vm, Identifier::fromString(globalObject->globalExec(), "arguments"), array);

    NakedPtr<Exception> exception;
    evaluate(globalObject->globalExec(), makeSource(source, exec->callerSourceOrigin()), JSValue(), exception);

    if (exception) {
        scope.throwException(globalObject->globalExec(), exception);
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(globalObject);
}

EncodedJSValue JSC_HOST_CALL functionLoad(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String fileName = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(throwException(exec, scope, createError(exec, "Could not open file."_s)));

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    
    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(globalObject->globalExec(), jscSource(script, SourceOrigin { absolutePath(fileName) }, fileName), JSValue(), evaluationException);
    if (evaluationException)
        throwException(exec, scope, evaluationException);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionLoadString(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String sourceCode = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(globalObject->globalExec(), makeSource(sourceCode, exec->callerSourceOrigin()), JSValue(), evaluationException);
    if (evaluationException)
        throwException(exec, scope, evaluationException);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionReadFile(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String fileName = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    bool isBinary = false;
    if (exec->argumentCount() > 1) {
        String type = exec->argument(1).toWTFString(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (type != "binary")
            return throwVMError(exec, scope, "Expected 'binary' as second argument.");
        isBinary = true;
    }

    RefPtr<Uint8Array> content = fillBufferWithContentsOfFile(fileName);
    if (!content)
        return throwVMError(exec, scope, "Could not open file.");

    if (!isBinary)
        return JSValue::encode(jsString(exec, String::fromUTF8WithLatin1Fallback(content->data(), content->length())));

    Structure* structure = exec->lexicalGlobalObject()->typedArrayStructure(TypeUint8);
    JSObject* result = JSUint8Array::create(vm, structure, WTFMove(content));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionCheckSyntax(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String fileName = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(throwException(exec, scope, createError(exec, "Could not open file."_s)));

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    StopWatch stopWatch;
    stopWatch.start();

    JSValue syntaxException;
    bool validSyntax = checkSyntax(globalObject->globalExec(), jscSource(script, SourceOrigin { absolutePath(fileName) }, fileName), &syntaxException);
    stopWatch.stop();

    if (!validSyntax)
        throwException(exec, scope, syntaxException);
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

#if ENABLE(SAMPLING_FLAGS)
EncodedJSValue JSC_HOST_CALL functionSetSamplingFlags(ExecState* exec)
{
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        unsigned flag = static_cast<unsigned>(exec->uncheckedArgument(i).toNumber(exec));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::setFlag(flag);
    }
    return JSValue::encode(jsNull());
}

EncodedJSValue JSC_HOST_CALL functionClearSamplingFlags(ExecState* exec)
{
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        unsigned flag = static_cast<unsigned>(exec->uncheckedArgument(i).toNumber(exec));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::clearFlag(flag);
    }
    return JSValue::encode(jsNull());
}
#endif

EncodedJSValue JSC_HOST_CALL functionGetRandomSeed(ExecState* exec)
{
    return JSValue::encode(jsNumber(exec->lexicalGlobalObject()->weakRandom().seed()));
}

EncodedJSValue JSC_HOST_CALL functionSetRandomSeed(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned seed = exec->argument(0).toUInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    exec->lexicalGlobalObject()->weakRandom().setSeed(seed);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionIsRope(ExecState* exec)
{
    JSValue argument = exec->argument(0);
    if (!argument.isString())
        return JSValue::encode(jsBoolean(false));
    const StringImpl* impl = asString(argument)->tryGetValueImpl();
    return JSValue::encode(jsBoolean(!impl));
}

EncodedJSValue JSC_HOST_CALL functionCallerSourceOrigin(ExecState* state)
{
    SourceOrigin sourceOrigin = state->callerSourceOrigin();
    if (sourceOrigin.isNull())
        return JSValue::encode(jsNull());
    return JSValue::encode(jsString(state, sourceOrigin.string()));
}

EncodedJSValue JSC_HOST_CALL functionReadline(ExecState* exec)
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
    return JSValue::encode(jsString(exec, line.data()));
}

EncodedJSValue JSC_HOST_CALL functionPreciseTime(ExecState*)
{
    return JSValue::encode(jsNumber(WallTime::now().secondsSinceEpoch().value()));
}

EncodedJSValue JSC_HOST_CALL functionNeverInlineFunction(ExecState* exec)
{
    return JSValue::encode(setNeverInline(exec));
}

EncodedJSValue JSC_HOST_CALL functionNoDFG(ExecState* exec)
{
    return JSValue::encode(setNeverOptimize(exec));
}

EncodedJSValue JSC_HOST_CALL functionNoFTL(ExecState* exec)
{
    if (exec->argumentCount()) {
        FunctionExecutable* executable = getExecutableForFunction(exec->argument(0));
        if (executable)
            executable->setNeverFTLOptimize(true);
    }
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionNoOSRExitFuzzing(ExecState* exec)
{
    return JSValue::encode(setCannotUseOSRExitFuzzing(exec));
}

EncodedJSValue JSC_HOST_CALL functionOptimizeNextInvocation(ExecState* exec)
{
    return JSValue::encode(optimizeNextInvocation(exec));
}

EncodedJSValue JSC_HOST_CALL functionNumberOfDFGCompiles(ExecState* exec)
{
    return JSValue::encode(numberOfDFGCompiles(exec));
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

EncodedJSValue JSC_HOST_CALL functionDollarCreateRealm(ExecState* exec)
{
    VM& vm = exec->vm();
    GlobalObject* result = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>());
    return JSValue::encode(result->getDirect(vm, Identifier::fromString(exec, "$")));
}

EncodedJSValue JSC_HOST_CALL functionDollarDetachArrayBuffer(ExecState* exec)
{
    return functionTransferArrayBuffer(exec);
}

EncodedJSValue JSC_HOST_CALL functionDollarEvalScript(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String sourceCode = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    GlobalObject* globalObject = jsDynamicCast<GlobalObject*>(vm,
        exec->thisValue().get(exec, Identifier::fromString(exec, "global")));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!globalObject)
        return JSValue::encode(throwException(exec, scope, createError(exec, "Expected global to point to a global object"_s)));
    
    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(globalObject->globalExec(), jscSource(sourceCode, exec->callerSourceOrigin()), JSValue(), evaluationException);
    if (evaluationException)
        throwException(exec, scope, evaluationException);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentStart(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String sourceCode = exec->argument(0).toWTFString(exec).isolatedCopy();
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    Lock didStartLock;
    Condition didStartCondition;
    bool didStart = false;
    
    Thread::create(
        "JSC Agent",
        [sourceCode, &didStartLock, &didStartCondition, &didStart] () {
            CommandLine commandLine(0, nullptr);
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
                    result = evaluate(globalObject->globalExec(), makeSource(sourceCode, SourceOrigin("worker"_s)), JSValue(), evaluationException);
                    if (evaluationException)
                        result = evaluationException->value();
                    checkException(globalObject->globalExec(), globalObject, true, evaluationException, result, commandLine, success);
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

EncodedJSValue JSC_HOST_CALL functionDollarAgentReceiveBroadcast(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue callback = exec->argument(0);
    CallData callData;
    CallType callType = getCallData(vm, callback, callData);
    if (callType == CallType::None)
        return JSValue::encode(throwException(exec, scope, createError(exec, "Expected callback"_s)));
    
    RefPtr<Message> message;
    {
        ReleaseHeapAccessScope releaseAccess(vm.heap);
        message = Worker::current().dequeue();
    }
    
    auto nativeBuffer = ArrayBuffer::create(message->releaseContents());
    ArrayBufferSharingMode sharingMode = nativeBuffer->sharingMode();
    JSArrayBuffer* jsBuffer = JSArrayBuffer::create(vm, exec->lexicalGlobalObject()->arrayBufferStructure(sharingMode), WTFMove(nativeBuffer));
    
    MarkedArgumentBuffer args;
    args.append(jsBuffer);
    args.append(jsNumber(message->index()));
    if (UNLIKELY(args.hasOverflowed()))
        return JSValue::encode(throwOutOfMemoryError(exec, scope));
    RELEASE_AND_RETURN(scope, JSValue::encode(call(exec, callback, callType, callData, jsNull(), args)));
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentReport(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String report = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    Workers::singleton().report(report);
    
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentSleep(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (exec->argumentCount() >= 1) {
        Seconds seconds = Seconds::fromMilliseconds(exec->argument(0).toNumber(exec));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        sleep(seconds);
    }
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentBroadcast(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(vm, exec->argument(0));
    if (!jsBuffer || !jsBuffer->isShared())
        return JSValue::encode(throwException(exec, scope, createError(exec, "Expected SharedArrayBuffer"_s)));
    
    int32_t index = exec->argument(1).toInt32(exec);
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

EncodedJSValue JSC_HOST_CALL functionDollarAgentGetReport(ExecState* exec)
{
    VM& vm = exec->vm();

    String string = Workers::singleton().tryGetReport();
    if (!string)
        return JSValue::encode(jsNull());
    
    return JSValue::encode(jsString(&vm, string));
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentLeaving(ExecState*)
{
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDollarAgentMonotonicNow(ExecState*)
{
    return JSValue::encode(jsNumber(MonotonicTime::now().secondsSinceEpoch().milliseconds()));
}

EncodedJSValue JSC_HOST_CALL functionWaitForReport(ExecState* exec)
{
    VM& vm = exec->vm();

    String string;
    {
        ReleaseHeapAccessScope releaseAccess(vm.heap);
        string = Workers::singleton().getReport();
    }
    if (!string)
        return JSValue::encode(jsNull());
    
    return JSValue::encode(jsString(&vm, string));
}

EncodedJSValue JSC_HOST_CALL functionHeapCapacity(ExecState* exec)
{
    VM& vm = exec->vm();
    return JSValue::encode(jsNumber(vm.heap.capacity()));
}

EncodedJSValue JSC_HOST_CALL functionFlashHeapAccess(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    double sleepTimeMs = 0;
    if (exec->argumentCount() >= 1) {
        sleepTimeMs = exec->argument(0).toNumber(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    vm.heap.releaseAccess();
    if (sleepTimeMs)
        sleep(Seconds::fromMilliseconds(sleepTimeMs));
    vm.heap.acquireAccess();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDisableRichSourceInfo(ExecState*)
{
    supportsRichSourceInfo = false;
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionMallocInALoop(ExecState*)
{
    Vector<void*> ptrs;
    for (unsigned i = 0; i < 5000; ++i)
        ptrs.append(fastMalloc(1024 * 2));
    for (void* ptr : ptrs)
        fastFree(ptr);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionTotalCompileTime(ExecState*)
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

EncodedJSValue JSC_HOST_CALL functionJSCOptions(ExecState* exec)
{
    VM& vm = exec->vm();
    JSObject* optionsObject = constructEmptyObject(exec);
#define FOR_EACH_OPTION(type_, name_, defaultValue_, availability_, description_) \
    addOption(vm, optionsObject, Identifier::fromString(exec, #name_), Options::name_());
    JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION
    return JSValue::encode(optionsObject);
}

EncodedJSValue JSC_HOST_CALL functionReoptimizationRetryCount(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    
    CodeBlock* block = getSomeBaselineCodeBlockForFunction(exec->argument(0));
    if (!block)
        return JSValue::encode(jsNumber(0));
    
    return JSValue::encode(jsNumber(block->reoptimizationRetryCounter()));
}

EncodedJSValue JSC_HOST_CALL functionTransferArrayBuffer(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (exec->argumentCount() < 1)
        return JSValue::encode(throwException(exec, scope, createError(exec, "Not enough arguments"_s)));
    
    JSArrayBuffer* buffer = jsDynamicCast<JSArrayBuffer*>(vm, exec->argument(0));
    if (!buffer)
        return JSValue::encode(throwException(exec, scope, createError(exec, "Expected an array buffer"_s)));
    
    ArrayBufferContents dummyContents;
    buffer->impl()->transferTo(vm, dummyContents);
    
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionFailNextNewCodeBlock(ExecState* exec)
{
    VM& vm = exec->vm();
    vm.setFailNextNewCodeBlock();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionQuit(ExecState* exec)
{
    VM& vm = exec->vm();
    vm.codeCache()->write(vm);

    jscExit(EXIT_SUCCESS);

#if COMPILER(MSVC)
    // Without this, Visual Studio will complain that this method does not return a value.
    return JSValue::encode(jsUndefined());
#endif
}

EncodedJSValue JSC_HOST_CALL functionFalse(ExecState*) { return JSValue::encode(jsBoolean(false)); }

EncodedJSValue JSC_HOST_CALL functionUndefined1(ExecState*) { return JSValue::encode(jsUndefined()); }
EncodedJSValue JSC_HOST_CALL functionUndefined2(ExecState*) { return JSValue::encode(jsUndefined()); }
EncodedJSValue JSC_HOST_CALL functionIsInt32(ExecState* exec)
{
    for (size_t i = 0; i < exec->argumentCount(); ++i) {
        if (!exec->argument(i).isInt32())
            return JSValue::encode(jsBoolean(false));
    }
    return JSValue::encode(jsBoolean(true));
}

EncodedJSValue JSC_HOST_CALL functionIsPureNaN(ExecState* exec)
{
    for (size_t i = 0; i < exec->argumentCount(); ++i) {
        JSValue value = exec->argument(i);
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

EncodedJSValue JSC_HOST_CALL functionIdentity(ExecState* exec) { return JSValue::encode(exec->argument(0)); }

EncodedJSValue JSC_HOST_CALL functionEffectful42(ExecState*)
{
    return JSValue::encode(jsNumber(42));
}

EncodedJSValue JSC_HOST_CALL functionMakeMasquerader(ExecState* exec)
{
    VM& vm = exec->vm();
    return JSValue::encode(Masquerader::create(vm, exec->lexicalGlobalObject()));
}

EncodedJSValue JSC_HOST_CALL functionHasCustomProperties(ExecState* exec)
{
    JSValue value = exec->argument(0);
    if (value.isObject())
        return JSValue::encode(jsBoolean(asObject(value)->hasCustomProperties(exec->vm())));
    return JSValue::encode(jsBoolean(false));
}

EncodedJSValue JSC_HOST_CALL functionDumpTypesForAllVariables(ExecState* exec)
{
    VM& vm = exec->vm();
    vm.dumpTypeProfilerData();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDrainMicrotasks(ExecState* exec)
{
    VM& vm = exec->vm();
    vm.drainMicrotasks();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionIs32BitPlatform(ExecState*)
{
#if USE(JSVALUE64)
    return JSValue::encode(JSValue(JSC::JSValue::JSFalse));
#else
    return JSValue::encode(JSValue(JSC::JSValue::JSTrue));
#endif
}

EncodedJSValue JSC_HOST_CALL functionCreateGlobalObject(ExecState* exec)
{
    VM& vm = exec->vm();
    return JSValue::encode(GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>()));
}

EncodedJSValue JSC_HOST_CALL functionCheckModuleSyntax(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String source = exec->argument(0).toWTFString(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    StopWatch stopWatch;
    stopWatch.start();

    ParserError error;
    bool validSyntax = checkModuleSyntax(exec, makeSource(source, { }, URL(), TextPosition(), SourceProviderSourceType::Module), error);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    stopWatch.stop();

    if (!validSyntax)
        throwException(exec, scope, jsNontrivialString(exec, toString("SyntaxError: ", error.message(), ":", error.line())));
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

EncodedJSValue JSC_HOST_CALL functionPlatformSupportsSamplingProfiler(ExecState*)
{
#if ENABLE(SAMPLING_PROFILER)
    return JSValue::encode(JSValue(JSC::JSValue::JSTrue));
#else
    return JSValue::encode(JSValue(JSC::JSValue::JSFalse));
#endif
}

EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshot(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    HeapSnapshotBuilder snapshotBuilder(vm.ensureHeapProfiler());
    snapshotBuilder.buildSnapshot();

    String jsonString = snapshotBuilder.json();
    EncodedJSValue result = JSValue::encode(JSONParse(exec, jsonString));
    scope.releaseAssertNoException();
    return result;
}

EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshotForGCDebugging(ExecState* exec)
{
    VM& vm = exec->vm();
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
    return JSValue::encode(jsString(&vm, jsonString));
}

EncodedJSValue JSC_HOST_CALL functionResetSuperSamplerState(ExecState*)
{
    resetSuperSamplerState();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionEnsureArrayStorage(ExecState* exec)
{
    VM& vm = exec->vm();
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        if (JSObject* object = jsDynamicCast<JSObject*>(vm, exec->argument(i)))
            object->ensureArrayStorage(vm);
    }
    return JSValue::encode(jsUndefined());
}

#if ENABLE(SAMPLING_PROFILER)
EncodedJSValue JSC_HOST_CALL functionStartSamplingProfiler(ExecState* exec)
{
    VM& vm = exec->vm();
    SamplingProfiler& samplingProfiler = vm.ensureSamplingProfiler(WTF::Stopwatch::create());
    samplingProfiler.noticeCurrentThreadAsJSCExecutionThread();
    samplingProfiler.start();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionSamplingProfilerStackTraces(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!vm.samplingProfiler())
        return JSValue::encode(throwException(exec, scope, createError(exec, "Sampling profiler was never started"_s)));

    String jsonString = vm.samplingProfiler()->stackTracesAsJSON();
    EncodedJSValue result = JSValue::encode(JSONParse(exec, jsonString));
    scope.releaseAssertNoException();
    return result;
}
#endif // ENABLE(SAMPLING_PROFILER)

EncodedJSValue JSC_HOST_CALL functionMaxArguments(ExecState*)
{
    return JSValue::encode(jsNumber(JSC::maxArguments));
}

EncodedJSValue JSC_HOST_CALL functionAsyncTestStart(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue numberOfAsyncPasses = exec->argument(0);
    if (!numberOfAsyncPasses.isUInt32())
        return throwVMError(exec, scope, "Expected first argument to a uint32"_s);

    asyncTestExpectedPasses += numberOfAsyncPasses.asUInt32();
    return encodedJSUndefined();
}

EncodedJSValue JSC_HOST_CALL functionAsyncTestPassed(ExecState*)
{
    asyncTestPasses++;
    return encodedJSUndefined();
}

#if ENABLE(WEBASSEMBLY)

static EncodedJSValue JSC_HOST_CALL functionWebAssemblyMemoryMode(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!Options::useWebAssembly())
        return throwVMTypeError(exec, scope, "WebAssemblyMemoryMode should only be called if the useWebAssembly option is set"_s);

    if (JSObject* object = exec->argument(0).getObject()) {
        if (auto* memory = jsDynamicCast<JSWebAssemblyMemory*>(vm, object))
            return JSValue::encode(jsString(&vm, makeString(memory->memory().mode())));
        if (auto* instance = jsDynamicCast<JSWebAssemblyInstance*>(vm, object))
            return JSValue::encode(jsString(&vm, makeString(instance->memoryMode())));
    }

    return throwVMTypeError(exec, scope, "WebAssemblyMemoryMode expects either a WebAssembly.Memory or WebAssembly.Instance"_s);
}

#endif // ENABLE(WEBASSEMBLY)

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

static double s_desiredTimeout;
static double s_timeoutMultiplier = 1.0;

static void startTimeoutThreadIfNeeded()
{
    if (char* timeoutString = getenv("JSCTEST_timeout")) {
        if (sscanf(timeoutString, "%lf", &s_desiredTimeout) != 1) {
            dataLog("WARNING: timeout string is malformed, got ", timeoutString,
                " but expected a number. Not using a timeout.\n");
        } else {
            Thread::create("jsc Timeout Thread", [] () {
                Seconds timeoutDuration(s_desiredTimeout * s_timeoutMultiplier);
                sleep(timeoutDuration);
                dataLog("Timed out after ", timeoutDuration, " seconds!\n");
                CRASH();
            });
        }
    }
}

int main(int argc, char** argv)
{
#if PLATFORM(IOS_FAMILY) && CPU(ARM_THUMB2)
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

    // Need to initialize WTF threading before we start any threads. Cannot initialize JSC
    // threading yet, since that would do somethings that we'd like to defer until after we
    // have a chance to parse options.
    WTF::initializeThreading();

#if PLATFORM(IOS_FAMILY)
    Options::crashIfCantAllocateJITMemory() = true;
#endif

    // We can't use destructors in the following code because it uses Windows
    // Structured Exception Handling
    int res = 0;
    TRY
        res = jscmain(argc, argv);
    EXCEPT(res = 3)
    finalizeStatsAtEndOfTesting();

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

    auto exceptionString = exception.toWTFString(globalObject->globalExec());
    Expected<CString, UTF8ConversionError> expectedCString = exceptionString.tryGetUtf8();
    if (expectedCString)
        printf("Exception: %s\n", expectedCString.value().data());
    else
        printf("Exception: <out of memory while extracting exception string>\n");

    Identifier nameID = Identifier::fromString(globalObject->globalExec(), "name");
    CHECK_EXCEPTION();
    Identifier fileNameID = Identifier::fromString(globalObject->globalExec(), "sourceURL");
    CHECK_EXCEPTION();
    Identifier lineNumberID = Identifier::fromString(globalObject->globalExec(), "line");
    CHECK_EXCEPTION();
    Identifier stackID = Identifier::fromString(globalObject->globalExec(), "stack");
    CHECK_EXCEPTION();

    JSValue nameValue = exception.get(globalObject->globalExec(), nameID);
    CHECK_EXCEPTION();
    JSValue fileNameValue = exception.get(globalObject->globalExec(), fileNameID);
    CHECK_EXCEPTION();
    JSValue lineNumberValue = exception.get(globalObject->globalExec(), lineNumberID);
    CHECK_EXCEPTION();
    JSValue stackValue = exception.get(globalObject->globalExec(), stackID);
    CHECK_EXCEPTION();
    
    if (nameValue.toWTFString(globalObject->globalExec()) == "SyntaxError"
        && (!fileNameValue.isUndefinedOrNull() || !lineNumberValue.isUndefinedOrNull())) {
        printf(
            "at %s:%s\n",
            fileNameValue.toWTFString(globalObject->globalExec()).utf8().data(),
            lineNumberValue.toWTFString(globalObject->globalExec()).utf8().data());
    }
    
    if (!stackValue.isUndefinedOrNull()) {
        auto stackString = stackValue.toWTFString(globalObject->globalExec());
        if (stackString.length())
            printf("%s\n", stackString.utf8().data());
    }

#undef CHECK_EXCEPTION
}

static bool checkUncaughtException(VM& vm, GlobalObject* globalObject, JSValue exception, CommandLine& options)
{
    const String& expectedExceptionName = options.m_uncaughtExceptionName;
    auto scope = DECLARE_CATCH_SCOPE(vm);
    scope.clearException();
    if (!exception) {
        printf("Expected uncaught exception with name '%s' but none was thrown\n", expectedExceptionName.utf8().data());
        return false;
    }

    ExecState* exec = globalObject->globalExec();
    JSValue exceptionClass = globalObject->get(exec, Identifier::fromString(exec, expectedExceptionName));
    if (!exceptionClass.isObject() || scope.exception()) {
        printf("Expected uncaught exception with name '%s' but given exception class is not defined\n", expectedExceptionName.utf8().data());
        return false;
    }

    bool isInstanceOfExpectedException = jsCast<JSObject*>(exceptionClass)->hasInstance(exec, exception);
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

static void checkException(ExecState* exec, GlobalObject* globalObject, bool isLastFile, bool hasException, JSValue value, CommandLine& options, bool& success)
{
    VM& vm = globalObject->vm();

    if (options.m_treatWatchdogExceptionAsSuccess && value.inherits<TerminatedExecutionError>(vm)) {
        ASSERT(hasException);
        return;
    }

    if (!options.m_uncaughtExceptionName || !isLastFile) {
        success = success && !hasException;
        if (options.m_dump && !hasException)
            printf("End: %s\n", value.toWTFString(exec).utf8().data());
        if (hasException)
            dumpException(globalObject, value);
    } else
        success = success && checkUncaughtException(vm, globalObject, (hasException) ? value : JSValue(), options);
}

static void runWithOptions(GlobalObject* globalObject, CommandLine& options, bool& success)
{
    Vector<Script>& scripts = options.m_scripts;
    String fileName;
    Vector<char> scriptBuffer;

    if (options.m_dump)
        JSC::Options::dumpGeneratedBytecodes() = true;

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
                promise = loadAndEvaluateModule(globalObject->globalExec(), fileName, jsUndefined(), jsUndefined());
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
        if (isModule) {
            if (!promise) {
                // FIXME: This should use an absolute file URL https://bugs.webkit.org/show_bug.cgi?id=193077
                promise = loadAndEvaluateModule(globalObject->globalExec(), jscSource(stringFromUTF(scriptBuffer), SourceOrigin { absolutePath(fileName) }, URL({ }, fileName), TextPosition(), SourceProviderSourceType::Module), jsUndefined());
            }
            scope.clearException();

            JSFunction* fulfillHandler = JSNativeStdFunction::create(vm, globalObject, 1, String(), [&success, &options, isLastFile](ExecState* exec) {
                checkException(exec, jsCast<GlobalObject*>(exec->lexicalGlobalObject()), isLastFile, false, exec->argument(0), options, success);
                return JSValue::encode(jsUndefined());
            });

            JSFunction* rejectHandler = JSNativeStdFunction::create(vm, globalObject, 1, String(), [&success, &options, isLastFile](ExecState* exec) {
                checkException(exec, jsCast<GlobalObject*>(exec->lexicalGlobalObject()), isLastFile, true, exec->argument(0), options, success);
                return JSValue::encode(jsUndefined());
            });

            promise->then(globalObject->globalExec(), fulfillHandler, rejectHandler);
            scope.releaseAssertNoException();
            vm.drainMicrotasks();
        } else {
            NakedPtr<Exception> evaluationException;
            JSValue returnValue = evaluate(globalObject->globalExec(), jscSource(scriptBuffer, SourceOrigin { absolutePath(fileName) }, fileName), JSValue(), evaluationException);
            scope.assertNoException();
            if (evaluationException)
                returnValue = evaluationException->value();
            checkException(globalObject->globalExec(), globalObject, isLastFile, evaluationException, returnValue, options, success);
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

    Optional<DirectoryName> directoryName = currentWorkingDirectory();
    if (!directoryName)
        return;
    SourceOrigin sourceOrigin(resolvePath(directoryName.value(), ModuleName("interpreter")));
    
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
            checkSyntax(vm, makeSource(source, sourceOrigin), error);
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
        JSValue returnValue = evaluate(globalObject->globalExec(), makeSource(source, sourceOrigin), JSValue(), evaluationException);
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
        JSValue returnValue = evaluate(globalObject->globalExec(), jscSource(line, sourceOrigin, sourceOrigin.string()), JSValue(), evaluationException);
#endif
        if (evaluationException)
            printf("Exception: %s\n", evaluationException->value().toWTFString(globalObject->globalExec()).utf8().data());
        else
            printf("%s\n", returnValue.toWTFString(globalObject->globalExec()).utf8().data());

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
#if HAVE(SIGNAL_H)
    fprintf(stderr, "  -s         Installs signal handlers that exit on a crash (Unix platforms only)\n");
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
    Options::initialize();
    
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
#if HAVE(SIGNAL_H)
            signal(SIGILL, _exit);
            signal(SIGFPE, _exit);
            signal(SIGBUS, _exit);
            signal(SIGSEGV, _exit);
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
    int result;
    bool success = true;
    GlobalObject* globalObject = nullptr;
    {
        JSLockHolder locker(vm);

        if (options.m_profile && !vm.m_perBytecodeProfiler)
            vm.m_perBytecodeProfiler = std::make_unique<Profiler::Database>(vm);

        globalObject = GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), options.m_arguments);
        globalObject->setRemoteDebuggingEnabled(options.m_enableRemoteDebugging);
        func(vm, globalObject, success);
        vm.drainMicrotasks();
    }
    vm.promiseDeferredTimer->runRunLoop();
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

    if (isWorker) {
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
    Options::enableRestrictedOptions(true);

    // Note that the options parsing can affect VM creation, and thus
    // comes first.
    CommandLine options(argc, argv);

    processConfigFile(Options::configFile(), "jsc");

    // Initialize JSC before getting VM.
    WTF::initializeMainThread();
    JSC::initializeThreading();
    startTimeoutThreadIfNeeded();
#if ENABLE(WEBASSEMBLY)
    JSC::Wasm::enableFastMemory();
#endif
    Gigacage::disableDisablingPrimitiveGigacageIfShouldBeEnabled();

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
