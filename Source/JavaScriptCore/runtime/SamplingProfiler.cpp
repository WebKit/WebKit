/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "CodeBlockSet.h"
#include "HeapIterationScope.h"
#include "HeapUtil.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"
#include "LLIntPCRanges.h"
#include "MachineContext.h"
#include "MarkedBlockInlines.h"
#include "MarkedBlockSet.h"
#include "NativeExecutable.h"
#include "VM.h"
#include "VMTrapsInlines.h"
#include "WasmCallee.h"
#include "WasmCalleeRegistry.h"
#include "WasmCapabilities.h"
#include <wtf/FilePrintStream.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/StackTrace.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

static double sNumTotalStackTraces = 0;
static double sNumTotalWalks = 0;
static double sNumFailedWalks = 0;
static const uint32_t sNumWalkReportingFrequency = 50;
static const double sWalkErrorPercentage = .05;
static constexpr bool sReportStatsOnlyWhenTheyreAboveThreshold = false;
static constexpr bool sReportStats = false;

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
    FrameWalker(VM& vm, CallFrame* callFrame, const AbstractLocker& codeBlockSetLocker, const AbstractLocker& machineThreadsLocker)
        : m_vm(vm)
        , m_callFrame(callFrame)
        , m_entryFrame(vm.topEntryFrame)
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
            recordJITFrame(stackTrace);
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

protected:

    SUPPRESS_ASAN
    void recordJITFrame(Vector<UnprocessedStackFrame>& stackTrace)
    {
        CallSiteIndex callSiteIndex;
        CalleeBits unsafeCallee = m_callFrame->unsafeCallee();
        CodeBlock* codeBlock = m_callFrame->unsafeCodeBlock();
        if (unsafeCallee.isWasm())
            codeBlock = nullptr;
        if (codeBlock) {
            ASSERT(isValidCodeBlock(codeBlock));
            callSiteIndex = m_callFrame->unsafeCallSiteIndex();
        }
        stackTrace[m_depth] = UnprocessedStackFrame(codeBlock, unsafeCallee, callSiteIndex);
#if ENABLE(WEBASSEMBLY)
        if (Wasm::isSupported() && unsafeCallee.isWasm()) {
            assertIsHeld(Wasm::CalleeRegistry::singleton().getLock());
            auto* wasmCallee = unsafeCallee.asWasmCallee();
            if (Wasm::CalleeRegistry::singleton().isValidCallee(wasmCallee)) {
                // At this point, Wasm::Callee would be dying (ref count is 0), but its fields are still live.
                // And we can safely copy Wasm::IndexOrName even when any lock is held by suspended threads.
                stackTrace[m_depth].wasmIndexOrName = wasmCallee->indexOrName();
                stackTrace[m_depth].wasmCompilationMode = wasmCallee->compilationMode();
#if ENABLE(JIT)
                stackTrace[m_depth].wasmPCMap = Wasm::CalleeRegistry::singleton().codeOriginMap(wasmCallee);
#endif
            }
        }
#endif
        m_depth++;
    }

    SUPPRESS_ASAN
    void advanceToParentFrame()
    {
        m_callFrame = m_callFrame->unsafeCallerFrame(m_entryFrame);
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

        CodeBlock* codeBlock = m_callFrame->unsafeCodeBlock();
        if (!codeBlock || m_callFrame->unsafeCallee().isWasm())
            return;

        if (!isValidCodeBlock(codeBlock)) {
            m_bailingOut = true;
            if (sReportStats)
                sNumFailedWalks++;
            return;
        }
    }

    bool isValidFramePointer(void* callFrame)
    {
        uint8_t* fpCast = bitwise_cast<uint8_t*>(callFrame);
        for (auto& thread : m_vm.heap.machineThreads().threads(m_machineThreadsLocker)) {
            uint8_t* stackBase = static_cast<uint8_t*>(thread->stack().origin());
            uint8_t* stackLimit = static_cast<uint8_t*>(thread->stack().end());
            RELEASE_ASSERT(stackBase);
            RELEASE_ASSERT(stackLimit);
            RELEASE_ASSERT(stackLimit <= stackBase);
            if (fpCast < stackBase && fpCast >= stackLimit)
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
    CallFrame* m_callFrame;
    EntryFrame* m_entryFrame;
    const AbstractLocker& m_codeBlockSetLocker;
    const AbstractLocker& m_machineThreadsLocker;
    bool m_bailingOut { false };
    size_t m_depth { 0 };
};

class CFrameWalker : public FrameWalker {
public:
    typedef FrameWalker Base;

    CFrameWalker(VM& vm, void* machineFrame, CallFrame* callFrame, const AbstractLocker& codeBlockSetLocker, const AbstractLocker& machineThreadsLocker)
        : Base(vm, callFrame, codeBlockSetLocker, machineThreadsLocker)
        , m_machineFrame(machineFrame)
    {
    }

    size_t walk(Vector<UnprocessedStackFrame>& stackTrace, bool& didRunOutOfSpace)
    {
        if (sReportStats)
            sNumTotalWalks++;
        resetAtMachineFrame();
        size_t maxStackTraceSize = stackTrace.size();
        // The way the C walker decides if a frame it is about to trace is C or JS is by
        // ensuring m_callFrame points to some frame above the machineFrame.
        if (!isAtTop() && !m_bailingOut && m_machineFrame == m_callFrame) {
            recordJITFrame(stackTrace);
            Base::advanceToParentFrame();
            resetAtMachineFrame();
        }

        while (!isAtTop() && !m_bailingOut && m_depth < maxStackTraceSize) {
            if (m_machineFrame >= m_callFrame) {
                // If we get to this state we probably have an invalid trace.
                m_bailingOut = true;
                break;
            }

            if (isCFrame()) {
                RELEASE_ASSERT(!LLInt::isLLIntPC(frame()->callerFrame));
                stackTrace[m_depth] = UnprocessedStackFrame(frame()->returnPC);
                m_depth++;
            } else
                recordJITFrame(stackTrace);
            advanceToParentFrame();
            resetAtMachineFrame();
        }
        didRunOutOfSpace = m_depth >= maxStackTraceSize && !isAtTop();
        reportStats();
        return m_depth;
    }

private:

    bool isCFrame()
    {
        return frame()->callerFrame != m_callFrame;
    }

    void advanceToParentFrame()
    {
        if (!isCFrame())
            Base::advanceToParentFrame();
        m_machineFrame = frame()->callerFrame;
    }

    void resetAtMachineFrame()
    {
        if (!isValidFramePointer(m_machineFrame)) {
            // Guard against pausing the process at weird program points.
            m_bailingOut = true;
            if (sReportStats)
                sNumFailedWalks++;
            return;
        }
        Base::resetAtMachineFrame();
    }

    CallerFrameAndPC* frame()
    {
        return reinterpret_cast<CallerFrameAndPC*>(m_machineFrame);
    }

    void* m_machineFrame;
};

SamplingProfiler::SamplingProfiler(VM& vm, Ref<Stopwatch>&& stopwatch)
    : m_isPaused(false)
    , m_isShutDown(false)
    , m_vm(vm)
    , m_weakRandom()
    , m_stopwatch(WTFMove(stopwatch))
    , m_timingInterval(Seconds::fromMicroseconds(Options::sampleInterval()))
{
    if (sReportStats) {
        sNumTotalWalks = 0;
        sNumFailedWalks = 0;
    }

    m_currentFrames.grow(256);
    vm.heap.objectSpace().enablePreciseAllocationTracking();
}

SamplingProfiler::~SamplingProfiler()
{
}

void SamplingProfiler::createThreadIfNecessary()
{
    ASSERT(m_lock.isLocked());

    if (m_thread)
        return;

    RefPtr<SamplingProfiler> profiler = this;
    m_thread = Thread::create("jsc.sampling-profiler.thread", [profiler] {
        profiler->timerLoop();
    });
}

void SamplingProfiler::timerLoop()
{
    while (true) {
        Seconds stackTraceProcessingTime = 0_s;
        {
            Locker locker { m_lock };
            if (UNLIKELY(m_isShutDown))
                return;

            if (!m_isPaused && m_jscExecutionThread)
                takeSample(stackTraceProcessingTime);

            m_lastTime = m_stopwatch->elapsedTime();
        }

        // Read section 6.2 of this paper for more elaboration of why we add a random
        // fluctuation here. The main idea is to prevent our timer from being in sync
        // with some system process such as a scheduled context switch.
        // http://plv.colorado.edu/papers/mytkowicz-pldi10.pdf
        double randomSignedNumber = (m_weakRandom.get() * 2.0) - 1.0; // A random number between [-1, 1).
        Seconds randomFluctuation = m_timingInterval * 0.2 * randomSignedNumber;
        WTF::sleep(m_timingInterval - std::min(m_timingInterval, stackTraceProcessingTime) + randomFluctuation);
    }
}

void SamplingProfiler::takeSample(Seconds& stackTraceProcessingTime)
{
    ASSERT(m_lock.isLocked());
    if (m_vm.entryScope) {
        Seconds nowTime = m_stopwatch->elapsedTime();

        Locker machineThreadsLocker { m_vm.heap.machineThreads().getLock() };
        Locker codeBlockSetLocker { m_vm.heap.codeBlockSet().getLock() };
        Locker executableAllocatorLocker { ExecutableAllocator::singleton().getLock() };
        std::optional<LockHolder> wasmCalleesLocker;
#if ENABLE(WEBASSEMBLY)
        if (Wasm::isSupported())
            wasmCalleesLocker.emplace(Wasm::CalleeRegistry::singleton().getLock());
#endif

        ThreadSuspendLocker threadSuspendLocker;
        auto didSuspend = m_jscExecutionThread->suspend(threadSuspendLocker);
        if (didSuspend) {
            // While the JSC thread is suspended, we can't do things like malloc because the JSC thread
            // may be holding the malloc lock.
            void* machineFrame;
            CallFrame* callFrame;
            void* machinePC;
            bool topFrameIsLLInt = false;
            RegExp* regExp = nullptr;
            void* llintPC;
            {
                PlatformRegisters registers;
                m_jscExecutionThread->getRegisters(threadSuspendLocker, registers);
                machineFrame = MachineContext::framePointer(registers);
                callFrame = static_cast<CallFrame*>(machineFrame);
                auto instructionPointer = MachineContext::instructionPointer(registers);
                if (instructionPointer)
                    machinePC = instructionPointer->untaggedPtr();
                else
                    machinePC = nullptr;
                llintPC = removeCodePtrTag(MachineContext::llintInstructionPointer(registers));
                assertIsNotTagged(machinePC);
            }

            bool shouldAppendTopFrameAsCCode = false;
            // FIXME: Lets have a way of detecting when we're parsing code.
            // https://bugs.webkit.org/show_bug.cgi?id=152761
            if (ExecutableAllocator::singleton().isValidExecutableMemory(executableAllocatorLocker, machinePC)) {
                regExp = m_vm.m_executingRegExp;
                if (regExp)
                    callFrame = m_vm.topCallFrame; // We need to do this or else we'd fail our backtrace validation b/c this isn't a JS frame.
            } else if (LLInt::isLLIntPC(machinePC)) {
                topFrameIsLLInt = true;
                // We're okay to take a normal stack trace when the PC
                // is in LLInt code.
            } else {
                // RegExp evaluation is leaf. So if RegExp evaluation exists, we can say it is RegExp evaluation is the top user-visible frame.
                regExp = m_vm.m_executingRegExp;
                // We resort to topCallFrame to see if we can get anything
                // useful. We usually get here when we're executing C code.
                callFrame = m_vm.topCallFrame;
                if (Options::collectExtraSamplingProfilerData() && !Options::sampleCCode())
                    shouldAppendTopFrameAsCCode = true;
            }

            size_t walkSize;
            bool wasValidWalk;
            bool didRunOutOfVectorSpace;
            if (Options::sampleCCode()) {
                CFrameWalker walker(m_vm, machineFrame, callFrame, codeBlockSetLocker, machineThreadsLocker);
                walkSize = walker.walk(m_currentFrames, didRunOutOfVectorSpace);
                wasValidWalk = walker.wasValidWalk();
            } else {
                FrameWalker walker(m_vm, callFrame, codeBlockSetLocker, machineThreadsLocker);
                walkSize = walker.walk(m_currentFrames, didRunOutOfVectorSpace);
                wasValidWalk = walker.wasValidWalk();
            }

            m_jscExecutionThread->resume(threadSuspendLocker);

            auto startTime = MonotonicTime::now();
            // We can now use data structures that malloc, and do other interesting things, again.

            // FIXME: It'd be interesting to take data about the program's state when
            // we fail to take a stack trace: https://bugs.webkit.org/show_bug.cgi?id=152758
            if (wasValidWalk && walkSize) {
                if (sReportStats)
                    sNumTotalStackTraces++;
                Vector<UnprocessedStackFrame> stackTrace;
                stackTrace.reserveInitialCapacity(walkSize + !!shouldAppendTopFrameAsCCode);
                if (shouldAppendTopFrameAsCCode)
                    stackTrace.uncheckedAppend(UnprocessedStackFrame { machinePC });
                for (size_t i = 0; i < walkSize; i++) {
                    UnprocessedStackFrame frame = m_currentFrames[i];
                    stackTrace.uncheckedAppend(frame);
                }

                m_unprocessedStackTraces.append(UnprocessedStackTrace { nowTime, machinePC, topFrameIsLLInt, llintPC, regExp, WTFMove(stackTrace) });

                if (didRunOutOfVectorSpace)
                    m_currentFrames.grow(m_currentFrames.size() * 1.25);
            }

            auto endTime = MonotonicTime::now();
            stackTraceProcessingTime = endTime - startTime;
        }
    }
}

static ALWAYS_INLINE BytecodeIndex tryGetBytecodeIndex(unsigned llintPC, CodeBlock* codeBlock)
{
#if ENABLE(DFG_JIT)
    RELEASE_ASSERT(!codeBlock->hasCodeOrigins());
#endif

    unsigned bytecodeOffset = llintPC;
    if (bytecodeOffset < codeBlock->instructionsSize())
        return BytecodeIndex(bytecodeOffset);
    return BytecodeIndex();
}

void SamplingProfiler::processUnverifiedStackTraces()
{
    // This function needs to be called from the JSC execution thread.
    RELEASE_ASSERT(m_lock.isLocked());

    TinyBloomFilter<uintptr_t> filter = m_vm.heap.objectSpace().blocks().filter();

    for (UnprocessedStackTrace& unprocessedStackTrace : m_unprocessedStackTraces) {
        m_stackTraces.append(StackTrace());
        StackTrace& stackTrace = m_stackTraces.last();
        stackTrace.timestamp = unprocessedStackTrace.timestamp;

        auto populateCodeLocation = [] (CodeBlock* codeBlock, JITType jitType, BytecodeIndex bytecodeIndex, StackFrame::CodeLocation& location) {
            if (bytecodeIndex.offset() < codeBlock->instructionsSize()) {
                int divot;
                int startOffset;
                int endOffset;
                codeBlock->expressionRangeForBytecodeIndex(bytecodeIndex, divot, startOffset, endOffset,
                    location.lineNumber, location.columnNumber);
                location.bytecodeIndex = bytecodeIndex;
            }
            if (codeBlock->hasHash())
                location.codeBlockHash = codeBlock->hash();
            location.jitType = jitType;
        };

        auto appendCodeBlock = [&] (CodeBlock* codeBlock, JITType jitType, BytecodeIndex bytecodeIndex) {
            assertIsHeld(m_lock);
            stackTrace.frames.append(StackFrame(codeBlock->ownerExecutable()));
            m_liveCellPointers.add(codeBlock->ownerExecutable());
            populateCodeLocation(codeBlock, jitType, bytecodeIndex, stackTrace.frames.last().semanticLocation);
        };

        auto appendEmptyFrame = [&] {
            stackTrace.frames.append(StackFrame());
        };

#if ENABLE(WEBASSEMBLY)
        auto storeWasmCalleeIntoLastFrame = [&] (UnprocessedStackFrame& unprocessedStackFrame, void* pc) {
            CalleeBits calleeBits = unprocessedStackFrame.unverifiedCallee;
            ASSERT_UNUSED(calleeBits, calleeBits.isWasm());
            StackFrame& stackFrame = stackTrace.frames.last();
            stackFrame.frameType = FrameType::Wasm;
            stackFrame.wasmIndexOrName = unprocessedStackFrame.wasmIndexOrName;
            stackFrame.wasmCompilationMode = unprocessedStackFrame.wasmCompilationMode;
#if ENABLE(JIT)
            if (pc && unprocessedStackFrame.wasmPCMap) {
                if (std::optional<CodeOrigin> codeOrigin = unprocessedStackFrame.wasmPCMap->findPC(pc))
                    stackFrame.wasmOffset = codeOrigin->bytecodeIndex();
            }
#else
            UNUSED_PARAM(pc);
#endif
        };
#endif

        auto storeCalleeIntoLastFrame = [&] (UnprocessedStackFrame& unprocessedStackFrame) {
            assertIsHeld(m_lock);
            // Set the callee if it's a valid GC object.
            CalleeBits calleeBits = unprocessedStackFrame.unverifiedCallee;
            StackFrame& stackFrame = stackTrace.frames.last();
            bool alreadyHasExecutable = !!stackFrame.executable;
#if ENABLE(WEBASSEMBLY)
            if (calleeBits.isWasm()) {
                storeWasmCalleeIntoLastFrame(unprocessedStackFrame, nullptr);
                return;
            }
#endif

            JSValue callee = calleeBits.asCell();
            if (!HeapUtil::isValueGCObject(m_vm.heap, filter, callee)) {
                if (!alreadyHasExecutable)
                    stackFrame.frameType = FrameType::Unknown;
                return;
            }

            JSCell* calleeCell = callee.asCell();
            auto setFallbackFrameType = [&] {
                ASSERT(!alreadyHasExecutable);
                FrameType result = FrameType::Unknown;
                auto callData = JSC::getCallData(calleeCell);
                if (callData.type == CallData::Type::Native)
                    result = FrameType::Host;

                stackFrame.frameType = result;
            };

            auto addCallee = [&] (JSObject* callee) {
                assertIsHeld(m_lock);
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

            RELEASE_ASSERT(HeapUtil::isPointerGCObjectJSCell(m_vm.heap, filter, executable));
            stackFrame.frameType = FrameType::Executable;
            stackFrame.executable = executable;
            m_liveCellPointers.add(executable);
        };

        auto appendCodeOrigin = [&] (CodeBlock* machineCodeBlock, CodeOrigin origin) {
            assertIsHeld(m_lock);
            size_t startIndex = stackTrace.frames.size(); // We want to change stack traces that we're about to append.

            CodeOrigin machineOrigin;
            origin.walkUpInlineStack([&] (const CodeOrigin& codeOrigin) {
                machineOrigin = codeOrigin;
                auto* inlineCallFrame = codeOrigin.inlineCallFrame();
                appendCodeBlock(inlineCallFrame ? inlineCallFrame->baselineCodeBlock.get() : machineCodeBlock, machineCodeBlock->jitType(), codeOrigin.bytecodeIndex());
            });

            if (Options::collectExtraSamplingProfilerData()) {
                RELEASE_ASSERT(machineOrigin.isSet());
                RELEASE_ASSERT(!machineOrigin.inlineCallFrame());

                StackFrame::CodeLocation machineLocation = stackTrace.frames.last().semanticLocation;

                // We want to tell each inlined frame about the machine frame
                // they were inlined into. Currently, we only use this for dumping
                // output on the command line, but we could extend it to the web
                // inspector in the future if we find a need for it there.
                RELEASE_ASSERT(stackTrace.frames.size());
                m_liveCellPointers.add(machineCodeBlock);
                for (size_t i = startIndex; i < stackTrace.frames.size() - 1; i++)
                    stackTrace.frames[i].machineLocation = std::make_pair(machineLocation, machineCodeBlock);
            }
        };

        // Prepend the top-most inlined frame if needed and gather
        // location information about where the top frame is executing.
        size_t startIndex = 0;
        if (unprocessedStackTrace.regExp) {
            // If the stack-trace is annotated with RegExp, the top-frame must be RegExp since RegExp evaluation is leaf function.
            appendEmptyFrame();
            stackTrace.frames.last().regExp = unprocessedStackTrace.regExp;
            stackTrace.frames.last().frameType = FrameType::RegExp;
            stackTrace.frames.last().semanticLocation.isRegExp = true;
            m_liveCellPointers.add(unprocessedStackTrace.regExp);
        } else if (!unprocessedStackTrace.frames.isEmpty() && !!unprocessedStackTrace.frames[0].verifiedCodeBlock) {
            CodeBlock* topCodeBlock = unprocessedStackTrace.frames[0].verifiedCodeBlock;
            if (unprocessedStackTrace.topFrameIsLLInt) {
                // We reuse LLInt CodeBlocks for the baseline JIT, so we need to check for both jit types.
                // This might also be false for various reasons (known and unknown), even though
                // it's super unlikely. One reason that this can be false is when we throw from a DFG frame,
                // and we end up having to unwind past an EntryFrame, we will end up executing
                // inside the LLInt's llint_handle_ucaught_exception. So we just protect against this
                // by ignoring it.
                BytecodeIndex bytecodeIndex = BytecodeIndex(0);
                if (topCodeBlock->jitType() == JITType::InterpreterThunk || topCodeBlock->jitType() == JITType::BaselineJIT) {
                    unsigned bits = static_cast<unsigned>(bitwise_cast<uintptr_t>(unprocessedStackTrace.llintPC));
                    bytecodeIndex = tryGetBytecodeIndex(bits, topCodeBlock);

                    UNUSED_PARAM(bytecodeIndex); // FIXME: do something with this info for the web inspector: https://bugs.webkit.org/show_bug.cgi?id=153455

                    appendCodeBlock(topCodeBlock, topCodeBlock->jitType(), bytecodeIndex);
                    storeCalleeIntoLastFrame(unprocessedStackTrace.frames[0]);
                    startIndex = 1;
                }
            } else {
#if ENABLE(JIT)
                if (std::optional<CodeOrigin> codeOrigin = topCodeBlock->findPC(unprocessedStackTrace.topPC)) {
                    appendCodeOrigin(topCodeBlock, *codeOrigin);
                    storeCalleeIntoLastFrame(unprocessedStackTrace.frames[0]);
                    startIndex = 1;
                }
#endif
                UNUSED_PARAM(appendCodeOrigin);
            }
        }
#if ENABLE(WEBASSEMBLY)
        else if (!unprocessedStackTrace.frames.isEmpty() && unprocessedStackTrace.frames[0].unverifiedCallee.isWasm()) {
            appendEmptyFrame();
            storeWasmCalleeIntoLastFrame(unprocessedStackTrace.frames[0], unprocessedStackTrace.topPC);
            startIndex = 1;
        }
#endif

        for (size_t i = startIndex; i < unprocessedStackTrace.frames.size(); i++) {
            UnprocessedStackFrame& unprocessedStackFrame = unprocessedStackTrace.frames[i];
            if (CodeBlock* codeBlock = unprocessedStackFrame.verifiedCodeBlock) {
                CallSiteIndex callSiteIndex = unprocessedStackFrame.callSiteIndex;

                auto appendCodeBlockNoInlining = [&] {
                    appendCodeBlock(codeBlock, codeBlock->jitType(), tryGetBytecodeIndex(callSiteIndex.bits(), codeBlock));
                };

#if ENABLE(DFG_JIT)
                if (codeBlock->hasCodeOrigins()) {
                    if (codeBlock->canGetCodeOrigin(callSiteIndex))
                        appendCodeOrigin(codeBlock, codeBlock->codeOrigin(callSiteIndex));
                    else
                        appendCodeBlock(codeBlock, codeBlock->jitType(), BytecodeIndex());
                } else
                    appendCodeBlockNoInlining();
#else
                appendCodeBlockNoInlining();
#endif
            } else if (unprocessedStackFrame.cCodePC) {
                appendEmptyFrame();
                stackTrace.frames.last().cCodePC = unprocessedStackFrame.cCodePC;
                stackTrace.frames.last().frameType = FrameType::C;
            } else
                appendEmptyFrame();

            // Note that this is okay to do if we walked the inline stack because
            // the machine frame will be at the top of the processed stack trace.
            if (!unprocessedStackFrame.cCodePC)
                storeCalleeIntoLastFrame(unprocessedStackFrame);
        }
    }

    m_unprocessedStackTraces.clear();
}

template<typename Visitor>
void SamplingProfiler::visit(Visitor& visitor)
{
    RELEASE_ASSERT(m_lock.isLocked());
    for (JSCell* cell : m_liveCellPointers)
        visitor.appendUnbarriered(cell);
}

template void SamplingProfiler::visit(AbstractSlotVisitor&);
template void SamplingProfiler::visit(SlotVisitor&);

void SamplingProfiler::shutdown()
{
    Locker locker { m_lock };
    m_isShutDown = true;
}

void SamplingProfiler::start()
{
    Locker locker { m_lock };
    startWithLock();
}

void SamplingProfiler::startWithLock()
{
    ASSERT(m_lock.isLocked());
    m_isPaused = false;
    createThreadIfNecessary();
}

void SamplingProfiler::pause()
{
    ASSERT(m_lock.isLocked());
    m_isPaused = true;
    reportStats();
}

void SamplingProfiler::noticeCurrentThreadAsJSCExecutionThreadWithLock()
{
    ASSERT(m_lock.isLocked());
    m_jscExecutionThread = &Thread::current();
}

void SamplingProfiler::noticeCurrentThreadAsJSCExecutionThread()
{
    Locker locker { m_lock };
    noticeCurrentThreadAsJSCExecutionThreadWithLock();
}

void SamplingProfiler::noticeJSLockAcquisition()
{
    Locker locker { m_lock };
    noticeCurrentThreadAsJSCExecutionThreadWithLock();
}

void SamplingProfiler::noticeVMEntry()
{
    Locker locker { m_lock };
    ASSERT(m_vm.entryScope);
    noticeCurrentThreadAsJSCExecutionThreadWithLock();
    m_lastTime = m_stopwatch->elapsedTime();
    createThreadIfNecessary();
}

void SamplingProfiler::clearData()
{
    ASSERT(m_lock.isLocked());
    m_stackTraces.clear();
    m_liveCellPointers.clear();
    m_unprocessedStackTraces.clear();
}

String SamplingProfiler::StackFrame::nameFromCallee(VM& vm)
{
    if (!callee) {
        if (regExp)
            return regExp->toSourceString();
        return String();
    }

    DeferTermination deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSGlobalObject* globalObject = callee->globalObject();
    auto getPropertyIfPureOperation = [&] (const Identifier& ident) -> String {
        PropertySlot slot(callee, PropertySlot::InternalMethodType::VMInquiry, &vm);
        PropertyName propertyName(ident);
        bool hasProperty = callee->getPropertySlot(globalObject, propertyName, slot);
        scope.assertNoException();
        if (hasProperty) {
            if (slot.isValue()) {
                JSValue nameValue = slot.getValue(globalObject, propertyName);
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

    switch (frameType) {
    case FrameType::C:
#if HAVE(DLADDR)
        if (frameType == FrameType::C) {
            auto demangled = StackTraceSymbolResolver::demangle(const_cast<void*>(cCodePC));
            if (demangled)
                return String::fromLatin1(demangled->demangledName() ? demangled->demangledName() : demangled->mangledName());
            WTF::dataLog("couldn't get a name");
        }
#endif
        return "(unknown C PC)"_s;

    case FrameType::Unknown:
        return "(unknown)"_s;

    case FrameType::Host:
        return "(host)"_s;

    case FrameType::RegExp:
        return "(regexp)"_s;

    case FrameType::Wasm:
#if ENABLE(WEBASSEMBLY)
        if (wasmIndexOrName)
            return makeString(wasmIndexOrName.value());
#endif
        return "(wasm)"_s;

    case FrameType::Executable:
        if (executable->isHostFunction())
            return static_cast<NativeExecutable*>(executable)->name();

        if (executable->isFunctionExecutable())
            return static_cast<FunctionExecutable*>(executable)->ecmaName().string();
        if (executable->isProgramExecutable() || executable->isEvalExecutable())
            return "(program)"_s;
        if (executable->isModuleProgramExecutable())
            return "(module)"_s;

        RELEASE_ASSERT_NOT_REACHED();
        return String();
    }
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

    switch (frameType) {
    case FrameType::Unknown:
    case FrameType::C:
        return "(unknown)"_s;

    case FrameType::RegExp:
        return "(regexp)"_s;

    case FrameType::Host:
        return "(host)"_s;

    case FrameType::Wasm: {
#if ENABLE(WEBASSEMBLY)
        if (wasmIndexOrName)
            return makeString(wasmIndexOrName.value());
#endif
        return "(wasm)"_s;
    }

    case FrameType::Executable:
        if (executable->isHostFunction())
            return static_cast<NativeExecutable*>(executable)->name();

        if (executable->isFunctionExecutable()) {
            String result = static_cast<FunctionExecutable*>(executable)->ecmaName().string();
            if (result.isEmpty())
                return "(anonymous function)"_s;
            return result;
        }
        if (executable->isEvalExecutable())
            return "(eval)"_s;
        if (executable->isProgramExecutable())
            return "(program)"_s;
        if (executable->isModuleProgramExecutable())
            return "(module)"_s;

        RELEASE_ASSERT_NOT_REACHED();
        return String();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return String();
}

int SamplingProfiler::StackFrame::functionStartLine()
{
    switch (frameType) {
    case FrameType::Unknown:
    case FrameType::Host:
    case FrameType::RegExp:
    case FrameType::C:
    case FrameType::Wasm:
        return -1;

    case FrameType::Executable:
        if (executable->isHostFunction())
            return -1;
        return static_cast<ScriptExecutable*>(executable)->firstLine();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return -1;
}

unsigned SamplingProfiler::StackFrame::functionStartColumn()
{
    switch (frameType) {
    case FrameType::Unknown:
    case FrameType::Host:
    case FrameType::RegExp:
    case FrameType::C:
    case FrameType::Wasm:
        return std::numeric_limits<unsigned>::max();

    case FrameType::Executable:
        if (executable->isHostFunction())
            return std::numeric_limits<unsigned>::max();

        return static_cast<ScriptExecutable*>(executable)->startColumn();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return std::numeric_limits<unsigned>::max();
}

SourceID SamplingProfiler::StackFrame::sourceID()
{
    switch (frameType) {
    case FrameType::Unknown:
    case FrameType::Host:
    case FrameType::RegExp:
    case FrameType::C:
    case FrameType::Wasm:
        return internalSourceID;

    case FrameType::Executable:
        if (executable->isHostFunction())
            return internalSourceID;

        return static_cast<ScriptExecutable*>(executable)->sourceID();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return internalSourceID;
}

String SamplingProfiler::StackFrame::url()
{
    switch (frameType) {
    case FrameType::Unknown:
    case FrameType::Host:
    case FrameType::RegExp:
    case FrameType::C:
    case FrameType::Wasm:
        return emptyString();
    case FrameType::Executable:
        if (executable->isHostFunction())
            return emptyString();

        String url = static_cast<ScriptExecutable*>(executable)->sourceURL();
        if (url.isEmpty())
            return static_cast<ScriptExecutable*>(executable)->source().provider()->sourceURLDirective(); // Fall back to sourceURL directive.
        return url;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return String();
}

Vector<SamplingProfiler::StackTrace> SamplingProfiler::releaseStackTraces()
{
    ASSERT(m_lock.isLocked());
    {
        HeapIterationScope heapIterationScope(m_vm.heap);
        processUnverifiedStackTraces();
    }

    Vector<StackTrace> result(WTFMove(m_stackTraces));
    clearData();
    return result;
}

String SamplingProfiler::stackTracesAsJSON()
{
    DeferGC deferGC(m_vm);
    Locker locker { m_lock };

    {
        HeapIterationScope heapIterationScope(m_vm.heap);
        processUnverifiedStackTraces();
    }

    StringBuilder json;
    json.append('[');

    bool loopedOnce = false;
    auto comma = [&] {
        if (loopedOnce)
            json.append(',');
    };
    for (StackTrace& stackTrace : m_stackTraces) {
        comma();
        json.append('[');
        loopedOnce = false;
        for (StackFrame& stackFrame : stackTrace.frames) {
            comma();
            json.appendQuotedJSONString(stackFrame.displayNameForJSONTests(m_vm));
            loopedOnce = true;
        }
        json.append(']');
        loopedOnce = true;
    }

    json.append(']');

    clearData();

    return json.toString();
}

void SamplingProfiler::registerForReportAtExit()
{
    static Lock registrationLock;
    static HashSet<RefPtr<SamplingProfiler>>* profilesToReport;

    Locker locker { registrationLock };

    if (!profilesToReport) {
        profilesToReport = new HashSet<RefPtr<SamplingProfiler>>();
        atexit([]() {
            for (const auto& profile : *profilesToReport)
                profile->reportDataToOptionFile();
        });
    }

    profilesToReport->add(adoptRef(this));
    m_needsReportAtExit = true;
}

void SamplingProfiler::reportDataToOptionFile()
{
    if (m_needsReportAtExit) {
        m_needsReportAtExit = false;
        JSLockHolder holder(m_vm);
        const char* path = Options::samplingProfilerPath();
        StringPrintStream pathOut;
        pathOut.print(path, "/");
        pathOut.print("JSCSampilingProfile-", reinterpret_cast<uintptr_t>(this), ".txt");
        auto out = FilePrintStream::open(pathOut.toCString().data(), "w");
        reportTopFunctions(*out);
        reportTopBytecodes(*out);
    }
}

void SamplingProfiler::reportTopFunctions()
{
    reportTopFunctions(WTF::dataFile());
}

void SamplingProfiler::reportTopFunctions(PrintStream& out)
{
    Locker locker { m_lock };
    DeferGCForAWhile deferGC(m_vm);

    {
        HeapIterationScope heapIterationScope(m_vm.heap);
        processUnverifiedStackTraces();
    }

    size_t totalSamples = 0;
    HashMap<String, size_t> functionCounts;
    for (StackTrace& stackTrace : m_stackTraces) {
        if (!stackTrace.frames.size())
            continue;

        StackFrame& frame = stackTrace.frames.first();
        String hash = ""_s;
        if (frame.semanticLocation.hasCodeBlockHash()) {
            StringPrintStream stream;
            frame.semanticLocation.codeBlockHash.dump(stream);
            hash = stream.toString();
        } else
            hash = "<nil>"_s;
        SourceID sourceID = frame.sourceID();
        if (Options::samplingProfilerIgnoreExternalSourceID()) {
            if (sourceID != internalSourceID)
                sourceID = aggregatedExternalSourceID;
        }
        auto frameDescription = makeString(frame.displayName(m_vm), '#', hash, ':', sourceID);
        functionCounts.add(frameDescription, 0).iterator->value++;
        totalSamples++;
    }

    auto takeMax = [&] () -> std::pair<String, size_t> {
        String maxFrameDescription;
        size_t maxFrameCount = 0;
        for (const auto& entry : functionCounts) {
            if (entry.value > maxFrameCount) {
                maxFrameCount = entry.value;
                maxFrameDescription = entry.key;
            }
        }
        if (!maxFrameDescription.isEmpty())
            functionCounts.remove(maxFrameDescription);
        return std::make_pair(maxFrameDescription, maxFrameCount);
    };

    if (Options::samplingProfilerTopFunctionsCount()) {
        out.println("\n\nSampling rate: ", m_timingInterval.microseconds(), " microseconds. Total samples: ", totalSamples);
        out.println("Top functions as <numSamples  'functionName#hash:sourceID'>");
        for (size_t i = 0; i < Options::samplingProfilerTopFunctionsCount(); i++) {
            auto pair = takeMax();
            if (pair.first.isEmpty())
                break;
            out.printf("%6zu ", pair.second);
            out.println("   '", pair.first, "'");
        }
    }
}

void SamplingProfiler::reportTopBytecodes()
{
    reportTopBytecodes(WTF::dataFile());
}

void SamplingProfiler::reportTopBytecodes(PrintStream& out)
{
    Locker locker { m_lock };
    DeferGCForAWhile deferGC(m_vm);

    {
        HeapIterationScope heapIterationScope(m_vm.heap);
        processUnverifiedStackTraces();
    }

    size_t totalSamples = 0;
    HashMap<String, size_t> bytecodeCounts;
    HashMap<String, size_t> tierCounts;

    String llint = "LLInt"_s;
    String baseline = "Baseline"_s;
    String dfg = "DFG"_s;
    String ftl = "FTL"_s;
    String builtin = "js builtin"_s;
    String wasm = "Wasm"_s;
    String host = "Host"_s;
    String regexp = "RegExp"_s;
    String cpp = "C/C++"_s;
    String unknownFrame = "Unknown Frame"_s;
    String unknownExecutable = "Unknown Executable"_s;

    auto forEachTier = [&] (auto func) {
        func(llint);
        func(baseline);
        func(dfg);
        func(ftl);
        func(builtin);
        func(wasm);
        func(host);
        func(regexp);
        func(cpp);
        func(unknownFrame);
        func(unknownExecutable);
    };

    for (StackTrace& stackTrace : m_stackTraces) {
        if (!stackTrace.frames.size())
            continue;

        auto descriptionForLocation = [&] (StackFrame::CodeLocation location, std::optional<Wasm::CompilationMode> wasmCompilationMode, BytecodeIndex wasmOffset) -> String {
            if (wasmCompilationMode) {
                StringPrintStream description;
                description.print(":");
                description.print(Wasm::makeString(wasmCompilationMode.value()));
                description.print(":");
                if (wasmOffset) {
                    uintptr_t offset = wasmOffset.offset();
                    description.print(RawHex(offset));
                } else
                    description.print("nil");
                return description.toString();
            }

            String bytecodeIndex;
            String codeBlockHash;
            String jitType;
            if (location.hasBytecodeIndex())
                bytecodeIndex = toString(location.bytecodeIndex);
            else
                bytecodeIndex = "<nil>"_s;

            if (location.hasCodeBlockHash()) {
                StringPrintStream stream;
                location.codeBlockHash.dump(stream);
                codeBlockHash = stream.toString();
            } else
                codeBlockHash = "<nil>"_s;

            if (location.isRegExp)
                jitType = "RegExp"_s;
            else
                jitType = JITCode::typeName(location.jitType);

            return makeString('#', codeBlockHash, ':', jitType, ':', bytecodeIndex);
        };

        StackFrame& frame = stackTrace.frames.first();
        auto frameDescription = makeString(frame.displayName(m_vm), descriptionForLocation(frame.semanticLocation, frame.wasmCompilationMode, frame.wasmOffset));
        if (std::optional<std::pair<StackFrame::CodeLocation, CodeBlock*>> machineLocation = frame.machineLocation) {
            frameDescription = makeString(frameDescription, " <-- ",
                machineLocation->second->inferredName().data(), descriptionForLocation(machineLocation->first, std::nullopt, BytecodeIndex()));
        }
        bytecodeCounts.add(frameDescription, 0).iterator->value++;
        
        {
            String tierName;
            switch (frame.frameType) {
            case SamplingProfiler::FrameType::Executable:
                switch (frame.semanticLocation.jitType) {
                case JITType::HostCallThunk:
                    tierName = host;
                    break;
                case JITType::InterpreterThunk:
                    tierName = llint;
                    break;
                case JITType::BaselineJIT:
                    tierName = baseline;
                    break;
                case JITType::DFGJIT:
                    tierName = dfg;
                    break;
                case JITType::FTLJIT:
                    tierName = ftl;
                    break;
                default:
                    tierName = unknownExecutable;
                    break;
                }

                if (frame.executable) {
                    if (auto* executable = jsDynamicCast<FunctionExecutable*>(frame.executable)) {
                        if (executable->isBuiltinFunction())
                            tierCounts.add(builtin, 0).iterator->value++;
                    }
                }

                break;
            case SamplingProfiler::FrameType::Wasm:
                tierName = wasm;
                break;
            case SamplingProfiler::FrameType::Host:
                tierName = host;
                break;
            case SamplingProfiler::FrameType::RegExp:
                tierName = regexp;
                break;
            case SamplingProfiler::FrameType::C:
                tierName = cpp;
                break;
            case SamplingProfiler::FrameType::Unknown:
                tierName = unknownFrame;
                break;
            }

            tierCounts.add(tierName, 0).iterator->value++;
        }
         
        totalSamples++;
    }

    auto takeMax = [&] () -> std::pair<String, size_t> {
        String maxFrameDescription;
        size_t maxFrameCount = 0;
        for (const auto& entry : bytecodeCounts) {
            if (entry.value > maxFrameCount) {
                maxFrameCount = entry.value;
                maxFrameDescription = entry.key;
            }
        }
        if (!maxFrameDescription.isEmpty())
            bytecodeCounts.remove(maxFrameDescription);
        return std::make_pair(maxFrameDescription, maxFrameCount);
    };

    if (Options::samplingProfilerTopBytecodesCount()) {
        out.println("\n\nSampling rate: ", m_timingInterval.microseconds(), " microseconds. Total samples: ", totalSamples, "\n");

        out.println("Tier breakdown:");
        out.println("-----------------------------------");
        unsigned maxTierNameLength = 0;
        forEachTier([&] (String tier) {
            maxTierNameLength = std::max(maxTierNameLength, tier.length());
        });
        auto printTier = [&] (String tier) {
            size_t count = tierCounts.get(tier);
            if (!count && (tier == unknownFrame || tier == unknownExecutable))
                return;
            out.print(tier, ": ");
            for (unsigned i = 0; i < maxTierNameLength + 2 - tier.length(); ++i)
                out.print(" ");
            out.printf("%6zu ", count);
            out.println(" (", (static_cast<double>(count) / static_cast<double>(totalSamples)) * 100, "%)");
        };
        forEachTier(printTier);
        out.println("\n");

        out.println("Hottest bytecodes as <numSamples   'functionName#hash:JITType:bytecodeIndex'>");
        for (size_t i = 0; i < Options::samplingProfilerTopBytecodesCount(); i++) {
            auto pair = takeMax();
            if (pair.first.isEmpty())
                break;
            out.printf("%6zu ", pair.second);
            out.println("   '", pair.first, "'");
        }
    }
}

Thread* SamplingProfiler::thread() const
{
    return m_thread.get();
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
    case SamplingProfiler::FrameType::Wasm:
        out.print("Wasm");
        break;
    case SamplingProfiler::FrameType::Host:
        out.print("Host");
        break;
    case SamplingProfiler::FrameType::RegExp:
        out.print("RegExp");
        break;
    case SamplingProfiler::FrameType::C:
    case SamplingProfiler::FrameType::Unknown:
        out.print("Unknown");
        break;
    }
}

} // namespace WTF

#endif // ENABLE(SAMPLING_PROFILER)
