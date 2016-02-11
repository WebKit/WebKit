/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SamplingProfiler.h"

#if ENABLE(SAMPLING_PROFILER)

#include "CallFrame.h"
#include "CodeBlock.h"
#include "Debugger.h"
#include "Executable.h"
#include "HeapInlines.h"
#include "HeapIterationScope.h"
#include "InlineCallFrame.h"
#include "Interpreter.h"
#include "JSCJSValueInlines.h"
#include "JSFunction.h"
#include "LLIntPCRanges.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include "PCToCodeOriginMap.h"
#include "SlotVisitor.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"
#include "VM.h"
#include "VMEntryScope.h"

namespace JSC {

static double sNumTotalStackTraces = 0;
static double sNumTotalWalks = 0;
static double sNumFailedWalks = 0;
static const uint32_t sNumWalkReportingFrequency = 50;
static const double sWalkErrorPercentage = .05;
static const bool sReportStatsOnlyWhenTheyreAboveThreshold = false;
static const bool sReportStats = false;

using FrameType = SamplingProfiler::FrameType;
using UnprocessedStackFrame = SamplingProfiler::UnprocessedStackFrame;

ALWAYS_INLINE static void reportStats()
{
    if (sReportStats && sNumTotalWalks && static_cast<uint64_t>(sNumTotalWalks) % sNumWalkReportingFrequency == 0) {
        if (!sReportStatsOnlyWhenTheyreAboveThreshold || (sNumFailedWalks / sNumTotalWalks > sWalkErrorPercentage)) {
            dataLogF("Num total walks: %llu. Failed walks percent: %lf\n",
                static_cast<unsigned long long>(sNumTotalWalks), sNumFailedWalks / sNumTotalWalks);
        }
    }
}

class FrameWalker {
public:
    FrameWalker(ExecState* callFrame, VM& vm, const LockHolder& codeBlockSetLocker, const LockHolder& machineThreadsLocker)
        : m_vm(vm)
        , m_callFrame(callFrame)
        , m_vmEntryFrame(vm.topVMEntryFrame)
        , m_codeBlockSetLocker(codeBlockSetLocker)
        , m_machineThreadsLocker(machineThreadsLocker)
    {
    }

    SUPPRESS_ASAN
    size_t walk(Vector<UnprocessedStackFrame>& stackTrace, bool& didRunOutOfSpace)
    {
        if (sReportStats)
            sNumTotalWalks++;
        resetAtMachineFrame();
        size_t maxStackTraceSize = stackTrace.size();
        while (!isAtTop() && !m_bailingOut && m_depth < maxStackTraceSize) {
            CallSiteIndex callSiteIndex;
            JSValue unsafeCallee = m_callFrame->unsafeCallee();
            CodeBlock* codeBlock = m_callFrame->codeBlock();
            if (codeBlock) {
                ASSERT(isValidCodeBlock(codeBlock));
                callSiteIndex = m_callFrame->callSiteIndex();
            }
            stackTrace[m_depth] = UnprocessedStackFrame(codeBlock, JSValue::encode(unsafeCallee), callSiteIndex);
            m_depth++;
            advanceToParentFrame();
            resetAtMachineFrame();
        }
        didRunOutOfSpace = m_depth >= maxStackTraceSize && !isAtTop();
        reportStats();
        return m_depth;
    }

    bool wasValidWalk() const
    {
        return !m_bailingOut;
    }

private:

    SUPPRESS_ASAN
    void advanceToParentFrame()
    {
        m_callFrame = m_callFrame->callerFrame(m_vmEntryFrame);
    }

    bool isAtTop() const
    {
        return !m_callFrame;
    }

    SUPPRESS_ASAN
    void resetAtMachineFrame()
    {
        if (isAtTop())
            return;

        if (!isValidFramePointer(m_callFrame)) {
            // Guard against pausing the process at weird program points.
            m_bailingOut = true;
            if (sReportStats)
                sNumFailedWalks++;
            return;
        }

        CodeBlock* codeBlock = m_callFrame->codeBlock();
        if (!codeBlock)
            return;

        if (!isValidCodeBlock(codeBlock)) {
            m_bailingOut = true;
            if (sReportStats)
                sNumFailedWalks++;
            return;
        }
    }

    bool isValidFramePointer(ExecState* exec)
    {
        uint8_t* fpCast = bitwise_cast<uint8_t*>(exec);
        for (MachineThreads::Thread* thread = m_vm.heap.machineThreads().threadsListHead(m_machineThreadsLocker); thread; thread = thread->next) {
            uint8_t* stackBase = static_cast<uint8_t*>(thread->stackBase);
            uint8_t* stackLimit = static_cast<uint8_t*>(thread->stackEnd);
            RELEASE_ASSERT(stackBase);
            RELEASE_ASSERT(stackLimit);
            if (fpCast <= stackBase && fpCast >= stackLimit)
                return true;
        }
        return false;
    }

    bool isValidCodeBlock(CodeBlock* codeBlock)
    {
        if (!codeBlock)
            return false;
        bool result = m_vm.heap.codeBlockSet().contains(m_codeBlockSetLocker, codeBlock);
        return result;
    }

    VM& m_vm;
    ExecState* m_callFrame;
    VMEntryFrame* m_vmEntryFrame;
    const LockHolder& m_codeBlockSetLocker;
    const LockHolder& m_machineThreadsLocker;
    bool m_bailingOut { false };
    size_t m_depth { 0 };
};

SamplingProfiler::SamplingProfiler(VM& vm, RefPtr<Stopwatch>&& stopwatch)
    : m_vm(vm)
    , m_stopwatch(WTFMove(stopwatch))
    , m_timingInterval(std::chrono::microseconds(1000))
    , m_totalTime(0)
    , m_timerQueue(WorkQueue::create("jsc.sampling-profiler.queue", WorkQueue::Type::Serial, WorkQueue::QOS::UserInteractive))
    , m_jscExecutionThread(nullptr)
    , m_isActive(false)
    , m_isPaused(false)
    , m_hasDispatchedFunction(false)
{
    if (sReportStats) {
        sNumTotalWalks = 0;
        sNumFailedWalks = 0;
    }

    m_currentFrames.grow(256);

    m_handler = [this] () {
        LockHolder samplingProfilerLocker(m_lock);
        if (!m_isActive || !m_jscExecutionThread || m_isPaused) {
            m_hasDispatchedFunction = false;
            deref();
            return;
        }

        if (m_vm.entryScope) {
            double nowTime = m_stopwatch->elapsedTime();

            LockHolder machineThreadsLocker(m_vm.heap.machineThreads().getLock());
            LockHolder codeBlockSetLocker(m_vm.heap.codeBlockSet().getLock());
            LockHolder executableAllocatorLocker(m_vm.executableAllocator.getLock());

            bool didSuspend = m_jscExecutionThread->suspend();
            if (didSuspend) {
                // While the JSC thread is suspended, we can't do things like malloc because the JSC thread
                // may be holding the malloc lock.
                ExecState* callFrame;
                void* machinePC;
                bool topFrameIsLLInt = false;
                void* llintPC;
                {
                    MachineThreads::Thread::Registers registers;
                    m_jscExecutionThread->getRegisters(registers);
                    callFrame = static_cast<ExecState*>(registers.framePointer());
                    machinePC = registers.instructionPointer();
                    llintPC = registers.llintPC();
                    m_jscExecutionThread->freeRegisters(registers);
                }
                // FIXME: Lets have a way of detecting when we're parsing code.
                // https://bugs.webkit.org/show_bug.cgi?id=152761
                if (m_vm.executableAllocator.isValidExecutableMemory(executableAllocatorLocker, machinePC)) {
                    if (m_vm.isExecutingInRegExpJIT) {
                        // FIXME: We're executing a regexp. Lets gather more intersting data.
                        // https://bugs.webkit.org/show_bug.cgi?id=152729
                        callFrame = m_vm.topCallFrame; // We need to do this or else we'd fail our backtrace validation b/c this isn't a JS frame.
                    }
                } else if (LLInt::isLLIntPC(machinePC)) {
                    topFrameIsLLInt = true;
                    // We're okay to take a normal stack trace when the PC
                    // is in LLInt code.
                } else {
                    // We resort to topCallFrame to see if we can get anything
                    // useful. We usually get here when we're executing C code.
                    callFrame = m_vm.topCallFrame;
                }

                size_t walkSize;
                bool wasValidWalk;
                bool didRunOutOfVectorSpace;
                {
                    FrameWalker walker(callFrame, m_vm, codeBlockSetLocker, machineThreadsLocker);
                    walkSize = walker.walk(m_currentFrames, didRunOutOfVectorSpace);
                    wasValidWalk = walker.wasValidWalk();
                }

                m_jscExecutionThread->resume();

                // We can now use data structures that malloc, and do other interesting things, again.

                // FIXME: It'd be interesting to take data about the program's state when
                // we fail to take a stack trace: https://bugs.webkit.org/show_bug.cgi?id=152758
                if (wasValidWalk && walkSize) {
                    if (sReportStats)
                        sNumTotalStackTraces++;
                    Vector<UnprocessedStackFrame> stackTrace;
                    stackTrace.reserveInitialCapacity(walkSize);
                    for (size_t i = 0; i < walkSize; i++) {
                        UnprocessedStackFrame frame = m_currentFrames[i];
                        stackTrace.uncheckedAppend(frame);
                    }

                    m_unprocessedStackTraces.append(UnprocessedStackTrace { nowTime, machinePC, topFrameIsLLInt, llintPC, WTFMove(stackTrace) });

                    if (didRunOutOfVectorSpace)
                        m_currentFrames.grow(m_currentFrames.size() * 1.25);

                    m_totalTime += nowTime - m_lastTime;
                }
            }
        }

        m_lastTime = m_stopwatch->elapsedTime();

        dispatchFunction(samplingProfilerLocker);
    };
}

SamplingProfiler::~SamplingProfiler()
{
}

static ALWAYS_INLINE unsigned tryGetBytecodeIndex(unsigned llintPC, CodeBlock* codeBlock, bool& isValid)
{
    RELEASE_ASSERT(!codeBlock->hasCodeOrigins());

#if USE(JSVALUE64)
    unsigned bytecodeIndex = llintPC;
    if (bytecodeIndex < codeBlock->instructionCount()) {
        isValid = true;
        return bytecodeIndex;
    }
    isValid = false;
    return 0;
#else
    Instruction* instruction = bitwise_cast<Instruction*>(llintPC);
    if (instruction >= codeBlock->instructions().begin() && instruction < codeBlock->instructions().begin() + codeBlock->instructionCount()) {
        isValid = true;
        unsigned bytecodeIndex = instruction - codeBlock->instructions().begin();
        return bytecodeIndex;
    }
    isValid = false;
    return 0;
#endif
}

void SamplingProfiler::processUnverifiedStackTraces()
{
    // This function needs to be called from the JSC execution thread.
    RELEASE_ASSERT(m_lock.isLocked());

    TinyBloomFilter filter = m_vm.heap.objectSpace().blocks().filter();
    MarkedBlockSet& markedBlockSet = m_vm.heap.objectSpace().blocks();

    for (UnprocessedStackTrace& unprocessedStackTrace : m_unprocessedStackTraces) {
        m_stackTraces.append(StackTrace());
        StackTrace& stackTrace = m_stackTraces.last();
        stackTrace.timestamp = unprocessedStackTrace.timestamp;

        auto appendCodeBlock = [&] (CodeBlock* codeBlock, unsigned bytecodeIndex) {
            stackTrace.frames.append(StackFrame(codeBlock->ownerExecutable()));
            m_liveCellPointers.add(codeBlock->ownerExecutable());

            if (bytecodeIndex < codeBlock->instructionCount()) {
                int divot;
                int startOffset;
                int endOffset;
                codeBlock->expressionRangeForBytecodeOffset(bytecodeIndex, divot, startOffset, endOffset,
                    stackTrace.frames.last().lineNumber, stackTrace.frames.last().columnNumber);
            }
        };

        auto appendEmptyFrame = [&] {
            stackTrace.frames.append(StackFrame());
        };

        auto storeCalleeIntoTopFrame = [&] (EncodedJSValue encodedCallee) {
            // Set the callee if it's a valid GC object.
            JSValue callee = JSValue::decode(encodedCallee);
            StackFrame& stackFrame = stackTrace.frames.last();
            bool alreadyHasExecutable = !!stackFrame.executable;
            if (!Heap::isValueGCObject(filter, markedBlockSet, callee)) {
                if (!alreadyHasExecutable)
                    stackFrame.frameType = FrameType::Unknown;
                return;
            }

            JSCell* calleeCell = callee.asCell();
            auto setFallbackFrameType = [&] {
                ASSERT(!alreadyHasExecutable);
                FrameType result = FrameType::Unknown;
                CallData callData;
                CallType callType;
                callType = getCallData(calleeCell, callData);
                if (callType == CallTypeHost)
                    result = FrameType::Host;

                stackFrame.frameType = result;
            };

            auto addCallee = [&] (JSObject* callee) {
                stackFrame.callee = callee;
                m_liveCellPointers.add(callee);
            };

            if (calleeCell->type() != JSFunctionType) {
                if (JSObject* object = jsDynamicCast<JSObject*>(calleeCell))
                    addCallee(object);

                if (!alreadyHasExecutable)
                    setFallbackFrameType();

                return;
            }

            addCallee(jsCast<JSFunction*>(calleeCell));

            if (alreadyHasExecutable)
                return;

            ExecutableBase* executable = jsCast<JSFunction*>(calleeCell)->executable();
            if (!executable) {
                setFallbackFrameType();
                return;
            }

            RELEASE_ASSERT(Heap::isPointerGCObject(filter, markedBlockSet, executable));
            stackFrame.frameType = FrameType::Executable;
            stackFrame.executable = executable;
            m_liveCellPointers.add(executable);
        };


        // Prepend the top-most inlined frame if needed and gather
        // location information about where the top frame is executing.
        size_t startIndex = 0;
        if (unprocessedStackTrace.frames.size() && !!unprocessedStackTrace.frames[0].verifiedCodeBlock) {
            CodeBlock* topCodeBlock = unprocessedStackTrace.frames[0].verifiedCodeBlock;
            if (unprocessedStackTrace.topFrameIsLLInt) {
                // We reuse LLInt CodeBlocks for the baseline JIT, so we need to check for both jit types.
                // This might also be false for various reasons (known and unknown), even though
                // it's super unlikely. One reason that this can be false is when we throw from a DFG frame,
                // and we end up having to unwind past a VMEntryFrame, we will end up executing
                // inside the LLInt's handleUncaughtException. So we just protect against this
                // by ignoring it.
                unsigned bytecodeIndex = 0;
                if (topCodeBlock->jitType() == JITCode::InterpreterThunk || topCodeBlock->jitType() == JITCode::BaselineJIT) {
                    bool isValidPC;
                    unsigned bits;
#if USE(JSVALUE64)
                    bits = static_cast<unsigned>(bitwise_cast<uintptr_t>(unprocessedStackTrace.llintPC));
#else
                    bits = bitwise_cast<unsigned>(unprocessedStackTrace.llintPC);
#endif
                    bytecodeIndex = tryGetBytecodeIndex(bits, topCodeBlock, isValidPC);

                    UNUSED_PARAM(isValidPC); // FIXME: do something with this info for the web inspector: https://bugs.webkit.org/show_bug.cgi?id=153455

                    appendCodeBlock(topCodeBlock, bytecodeIndex);
                    storeCalleeIntoTopFrame(unprocessedStackTrace.frames[0].unverifiedCallee);
                    startIndex = 1;
                }
            } else if (Optional<CodeOrigin> codeOrigin = topCodeBlock->findPC(unprocessedStackTrace.topPC)) {
                codeOrigin->walkUpInlineStack([&] (const CodeOrigin& codeOrigin) {
                    appendCodeBlock(codeOrigin.inlineCallFrame ? codeOrigin.inlineCallFrame->baselineCodeBlock.get() : topCodeBlock, codeOrigin.bytecodeIndex);
                });
                storeCalleeIntoTopFrame(unprocessedStackTrace.frames[0].unverifiedCallee);
                startIndex = 1;
            }
        }

        for (size_t i = startIndex; i < unprocessedStackTrace.frames.size(); i++) {
            UnprocessedStackFrame& unprocessedStackFrame = unprocessedStackTrace.frames[i];
            if (CodeBlock* codeBlock = unprocessedStackFrame.verifiedCodeBlock) {
                CallSiteIndex callSiteIndex = unprocessedStackFrame.callSiteIndex;

                auto appendCodeBlockNoInlining = [&] {
                    bool isValidPC;
                    appendCodeBlock(codeBlock, tryGetBytecodeIndex(callSiteIndex.bits(), codeBlock, isValidPC));
                };

#if ENABLE(DFG_JIT)
                if (codeBlock->hasCodeOrigins()) {
                    if (codeBlock->canGetCodeOrigin(callSiteIndex)) {
                        codeBlock->codeOrigin(callSiteIndex).walkUpInlineStack([&] (const CodeOrigin& codeOrigin) {
                            appendCodeBlock(codeOrigin.inlineCallFrame ? codeOrigin.inlineCallFrame->baselineCodeBlock.get() : codeBlock, codeOrigin.bytecodeIndex);
                        });
                    } else
                        appendCodeBlock(codeBlock, std::numeric_limits<unsigned>::max());
                } else
                    appendCodeBlockNoInlining();
#else
                appendCodeBlockNoInlining();
#endif
            } else
                appendEmptyFrame();

            // Note that this is okay to do if we walked the inline stack because
            // the machine frame will be at the top of the processed stack trace.
            storeCalleeIntoTopFrame(unprocessedStackFrame.unverifiedCallee);
        }
    }

    m_unprocessedStackTraces.clear();
}

void SamplingProfiler::visit(SlotVisitor& slotVisitor)
{
    RELEASE_ASSERT(m_lock.isLocked());
    for (JSCell* cell : m_liveCellPointers)
        slotVisitor.appendUnbarrieredReadOnlyPointer(cell);
}

void SamplingProfiler::shutdown()
{
    stop();
}

void SamplingProfiler::start()
{
    LockHolder locker(m_lock);
    start(locker);
}

void SamplingProfiler::start(const LockHolder& locker)
{
    ASSERT(m_lock.isLocked());
    m_isActive = true;
    dispatchIfNecessary(locker);
}

void SamplingProfiler::stop()
{
    LockHolder locker(m_lock);
    stop(locker);
}

void SamplingProfiler::stop(const LockHolder&)
{
    ASSERT(m_lock.isLocked());
    m_isActive = false;
    reportStats();
}

void SamplingProfiler::pause()
{
    LockHolder locker(m_lock);
    m_isPaused = true;
    reportStats();
}

void SamplingProfiler::noticeCurrentThreadAsJSCExecutionThread(const LockHolder&)
{
    ASSERT(m_lock.isLocked());
    m_jscExecutionThread = m_vm.heap.machineThreads().machineThreadForCurrentThread();
}

void SamplingProfiler::noticeCurrentThreadAsJSCExecutionThread()
{
    LockHolder locker(m_lock);
    noticeCurrentThreadAsJSCExecutionThread(locker);
}

void SamplingProfiler::dispatchIfNecessary(const LockHolder& locker)
{
    if (m_isActive && !m_hasDispatchedFunction && m_jscExecutionThread && m_vm.entryScope) {
        ref(); // Matching deref() is inside m_handler when m_handler stops recursing.
        dispatchFunction(locker);
    }
}

void SamplingProfiler::dispatchFunction(const LockHolder&)
{
    m_hasDispatchedFunction = true;
    m_isPaused = false;
    m_lastTime = m_stopwatch->elapsedTime();
    m_timerQueue->dispatchAfter(m_timingInterval, m_handler);
}

void SamplingProfiler::noticeJSLockAcquisition()
{
    LockHolder locker(m_lock);
    noticeCurrentThreadAsJSCExecutionThread(locker);
}

void SamplingProfiler::noticeVMEntry()
{
    LockHolder locker(m_lock);
    ASSERT(m_vm.entryScope);
    noticeCurrentThreadAsJSCExecutionThread(locker);
    m_lastTime = m_stopwatch->elapsedTime();
    dispatchIfNecessary(locker);
}

void SamplingProfiler::clearData(const LockHolder&)
{
    ASSERT(m_lock.isLocked());
    m_stackTraces.clear();
    m_liveCellPointers.clear();
    m_unprocessedStackTraces.clear();
}

String SamplingProfiler::StackFrame::nameFromCallee(VM& vm)
{
    if (!callee)
        return String();

    ExecState* exec = callee->globalObject()->globalExec();
    auto getPropertyIfPureOperation = [&] (const Identifier& ident) -> String {
        PropertySlot slot(callee);
        PropertyName propertyName(ident);
        if (callee->getPropertySlot(exec, propertyName, slot)) {
            if (slot.isValue()) {
                JSValue nameValue = slot.getValue(exec, propertyName);
                if (isJSString(nameValue))
                    return asString(nameValue)->tryGetValue();
            }
        }
        return String();
    };

    String name = getPropertyIfPureOperation(vm.propertyNames->displayName);
    if (!name.isEmpty())
        return name;

    return getPropertyIfPureOperation(vm.propertyNames->name);
}

String SamplingProfiler::StackFrame::displayName(VM& vm)
{
    {
        String name = nameFromCallee(vm);
        if (!name.isEmpty())
            return name;
    }

    if (frameType == FrameType::Unknown)
        return ASCIILiteral("(unknown)");
    if (frameType == FrameType::Host)
        return ASCIILiteral("(host)");

    if (executable->isHostFunction())
        return static_cast<NativeExecutable*>(executable)->name();

    if (executable->isFunctionExecutable())
        return static_cast<FunctionExecutable*>(executable)->inferredName().string();
    if (executable->isProgramExecutable() || executable->isEvalExecutable())
        return ASCIILiteral("(program)");
    if (executable->isModuleProgramExecutable())
        return ASCIILiteral("(module)");

    RELEASE_ASSERT_NOT_REACHED();
    return String();
}

String SamplingProfiler::StackFrame::displayNameForJSONTests(VM& vm)
{
    {
        String name = nameFromCallee(vm);
        if (!name.isEmpty())
            return name;
    }

    if (frameType == FrameType::Unknown)
        return ASCIILiteral("(unknown)");
    if (frameType == FrameType::Host)
        return ASCIILiteral("(host)");

    if (executable->isHostFunction())
        return static_cast<NativeExecutable*>(executable)->name();

    if (executable->isFunctionExecutable()) {
        String result = static_cast<FunctionExecutable*>(executable)->inferredName().string();
        if (result.isEmpty())
            return ASCIILiteral("(anonymous function)");
        return result;
    }
    if (executable->isEvalExecutable())
        return ASCIILiteral("(eval)");
    if (executable->isProgramExecutable())
        return ASCIILiteral("(program)");
    if (executable->isModuleProgramExecutable())
        return ASCIILiteral("(module)");

    RELEASE_ASSERT_NOT_REACHED();
    return String();
}

int SamplingProfiler::StackFrame::functionStartLine()
{
    if (frameType == FrameType::Unknown || frameType == FrameType::Host)
        return -1;

    if (executable->isHostFunction())
        return -1;
    return static_cast<ScriptExecutable*>(executable)->firstLine();
}

unsigned SamplingProfiler::StackFrame::functionStartColumn()
{
    if (frameType == FrameType::Unknown || frameType == FrameType::Host)
        return -1;

    if (executable->isHostFunction())
        return -1;

    return static_cast<ScriptExecutable*>(executable)->startColumn();
}

intptr_t SamplingProfiler::StackFrame::sourceID()
{
    if (frameType == FrameType::Unknown || frameType == FrameType::Host)
        return -1;

    if (executable->isHostFunction())
        return -1;

    return static_cast<ScriptExecutable*>(executable)->sourceID();
}

String SamplingProfiler::StackFrame::url()
{
    if (frameType == FrameType::Unknown || frameType == FrameType::Host)
        return emptyString();

    if (executable->isHostFunction())
        return emptyString();

    String url = static_cast<ScriptExecutable*>(executable)->sourceURL();
    if (url.isEmpty())
        return static_cast<ScriptExecutable*>(executable)->source().provider()->sourceURL(); // Fall back to sourceURL directive.
    return url;
}

Vector<SamplingProfiler::StackTrace> SamplingProfiler::releaseStackTraces(const LockHolder& locker)
{
    ASSERT(m_lock.isLocked());
    {
        HeapIterationScope heapIterationScope(m_vm.heap);
        processUnverifiedStackTraces();
    }

    Vector<StackTrace> result(WTFMove(m_stackTraces));
    clearData(locker);
    return result;
}

String SamplingProfiler::stackTracesAsJSON()
{
    LockHolder locker(m_lock);

    {
        HeapIterationScope heapIterationScope(m_vm.heap);
        processUnverifiedStackTraces();
    }

    StringBuilder json;
    json.appendLiteral("[");

    bool loopedOnce = false;
    auto comma = [&] {
        if (loopedOnce)
            json.appendLiteral(",");
    };
    for (StackTrace& stackTrace : m_stackTraces) {
        comma();
        json.appendLiteral("[");
        loopedOnce = false;
        for (StackFrame& stackFrame : stackTrace.frames) {
            comma();
            json.appendLiteral("\"");
            json.append(stackFrame.displayNameForJSONTests(m_vm));
            json.appendLiteral("\"");
            loopedOnce = true;
        }
        json.appendLiteral("]");
        loopedOnce = true;
    }

    json.appendLiteral("]");

    clearData(locker);

    return json.toString();
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, SamplingProfiler::FrameType frameType)
{
    switch (frameType) {
    case SamplingProfiler::FrameType::Executable:
        out.print("Executable");
        break;
    case SamplingProfiler::FrameType::Host:
        out.print("Host");
        break;
    case SamplingProfiler::FrameType::Unknown:
        out.print("Unknown");
        break;
    }
}

} // namespace WTF

#endif // ENABLE(SAMPLING_PROFILER)
