/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "EvacuatedStack.h"

#if ENABLE(WEBASSEMBLY)

#include "CallFrame.h"
#include "JSCConfig.h"
#include "NativeCallee.h"
#include "WasmCallee.h"

#include <wtf/PtrTag.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/MakeString.h>

extern "C" void* SYSV_ABI relocateJITReturnPC(const void* codePtr, const void* oldSignatureSP, const void* newSignatureSP);
extern "C" void* SYSV_ABI getSentinelFrameReturnPC(const void* signatureSP);

namespace JSC {

std::unique_ptr<EvacuatedStackSlice> EvacuatedStackSlice::create(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const CallFrame* frameToReturnFromForEntry)
{
    ASSERT(stackSpan.size());
    ASSERT(isMultipleOf(stackAlignmentRegisters(), stackSpan.size()));
    auto slice = std::unique_ptr<EvacuatedStackSlice> {
        new (NotNull, fastMalloc(Base::allocationSize(stackSpan.size()))) EvacuatedStackSlice(stackSpan, WTF::move(frameOffsets), frameToReturnFromForEntry)
    };
    return slice;
}

EvacuatedStackSlice::EvacuatedStackSlice(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const CallFrame* frameToReturnFromForEntry)
    : Base(stackSpan.size())
    , m_originalBase(stackSpan.data())
    , m_frameOffsets(WTF::move(frameOffsets))
    , m_entryPC(frameToReturnFromForEntry->rawReturnPC())
    , m_entryPCFrame(frameToReturnFromForEntry)
{
    memcpySpan(slots(), stackSpan);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void* relocateReturnPC(void* returnPC, const CallerFrameAndPC* originalFP, const CallerFrameAndPC* newFP)
{
#if CPU(ARM64E)
    auto* originalSignatureSP = reinterpret_cast<const void*>(originalFP + 1);
    auto* newSignatureSP = reinterpret_cast<const void*>(newFP + 1);
    if (Options::useJITCage() && isJITPC(removeCodePtrTag(returnPC)))
        return relocateJITReturnPC(returnPC, originalSignatureSP, newSignatureSP);
    return ptrauth_auth_and_resign(returnPC, ptrauth_key_asib, originalSignatureSP, ptrauth_key_asib, newSignatureSP);
#else
    UNUSED_PARAM(originalFP);
    UNUSED_PARAM(newFP);
    return returnPC;
#endif
}

CallFrame* EvacuatedStackSlice::implant(Register* base, CallFrame* lastFrame)
{
    ASSERT(isStackAligned(lastFrame));

    // Copy the captured stack data onto the new stack.
    memcpySpan(std::span<Register>(base, size()), slots());

    // Walk all frames on the new stack and fix return PC signatures.
    // First frame visited is the deepest, i.e. the one that will return to 'returnPC'.
    bool isReturnToSentinelFrame = true;
    for (unsigned i = m_frameOffsets.size(); i-- > 0; ) {
        unsigned offset = m_frameOffsets[i];

        SUPPRESS_MEMORY_UNSAFE_CAST // cast validity is guaranteed by stack construction
        auto* frameRecord = reinterpret_cast<CallerFrameAndPC*>(base + offset);
        SUPPRESS_MEMORY_UNSAFE_CAST // ditto
        auto* originalFrameRecordAddr = reinterpret_cast<const CallerFrameAndPC*>(m_originalBase + offset);

        // Link this frame to the one above and re-sign the returnPC for the new location
        frameRecord->callerFrame = lastFrame;
        frameRecord->returnPC = isReturnToSentinelFrame
            ? getSentinelFrameReturnPC(frameRecord + 1)
            : relocateReturnPC(frameRecord->returnPC, originalFrameRecordAddr, frameRecord);

        lastFrame = static_cast<CallFrame*>(static_cast<void*>(frameRecord));
        ASSERT(isStackAligned(lastFrame));
        isReturnToSentinelFrame = false;
    }

    return lastFrame;
}

void EvacuatedStackSlice::dump(PrintStream& out) const
{
    out.print("EvacuatedStackSlice{ size: ", size());
    out.print(" frame offsets: [");
    CommaPrinter comma;
    for (auto offset : m_frameOffsets)
        out.print(comma, offset);
    out.print("]");
    out.print(", entryPC=", RawPointer(m_entryPC), " }");
}

static const Register* alignStackPointer(const Register* ptr)
{
    constexpr size_t stackAlignmentMask = stackAlignmentBytes() - 1;
    uintptr_t alignedAddress = (reinterpret_cast<uintptr_t>(ptr) + stackAlignmentMask) & ~stackAlignmentMask;
    return reinterpret_cast<const Register*>(alignedAddress);
}

static const Register* topOfFrame(const CallFrame* callFrame)
{
    // We include a few extra slots above the frame record via the
    // headroomSlotCount parameter of StackSlicer::evacuatePendingSlice, but we still count
    // the frame record as the real top of a Wasm frame and the bottom of the next frame.
    CalleeBits calleeBits = callFrame->callee();
    if (calleeBits.isNativeCallee()) {
        auto* nativeCallee = calleeBits.asNativeCallee();
        ASSERT(nativeCallee->category() == NativeCallee::Category::Wasm);
        auto* wasmCallee = uncheckedDowncast<Wasm::Callee>(nativeCallee);

        switch (wasmCallee->compilationMode()) {
        case Wasm::CompilationMode::WasmToJSMode:
        case Wasm::CompilationMode::IPIntMode:
        case Wasm::CompilationMode::BBQMode:
        case Wasm::CompilationMode::OMGMode:
            return callFrame->registers() + 2;
        case Wasm::CompilationMode::JSToWasmMode:
            return alignStackPointer(callFrame->registers() + static_cast<size_t>(CallFrameSlot::firstArgument) + callFrame->argumentCount());
        default:
            RELEASE_ASSERT_NOT_REACHED(); // case not accounted for
        }
    } else {
        // A JSFunction
        return alignStackPointer(callFrame->registers() + static_cast<size_t>(CallFrameSlot::firstArgument) + callFrame->argumentCount());
    }
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

static std::optional<Wasm::CompilationMode> compilationModeOfCallee(CalleeBits calleeBits)
{
    if (!calleeBits.isNativeCallee())
        return std::nullopt;
    auto* nativeCallee = calleeBits.asNativeCallee();
    if (nativeCallee->category() != NativeCallee::Category::Wasm)
        return std::nullopt;
    auto* wasmCallee = uncheckedDowncast<Wasm::Callee>(nativeCallee);
    return wasmCallee->compilationMode();
}

// We save this many extra slots above the actual frame record (the fp/lr pair) of a Wasm
// frame because IPInt stores register values there before a call. Some frame types do not
// actually use this many slots, but it appears tiering up breaks without a consistent headroom.
constexpr unsigned standardHeadroom = 8;

void StackSlicerBase::commitPendingSliceWithAdditionalFrame(CallFrame* callFrame)
{
    m_futureSliceTop = topOfFrame(callFrame);
    m_pendingFrameRecords.append(callFrame);
    m_lastVisitedFrame = callFrame;
    commitPendingSlice();
}

void StackSlicerBase::commitPendingSlice()
{
    auto slice = evacuatePendingSlice(standardHeadroom);
    m_slices.append(WTF::move(slice));
    m_futureSliceBottom = nullptr;
    m_futureSliceTop = nullptr;
    m_futureReturnFromFrame = nullptr;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

std::unique_ptr<EvacuatedStackSlice> StackSlicerBase::evacuatePendingSlice(unsigned headroomSlotCount)
{
    ASSERT(m_futureSliceBottom && isStackAligned(m_futureSliceBottom));
    ASSERT(m_futureSliceTop && isStackAligned(m_futureSliceTop));
    ASSERT(m_futureReturnFromFrame);
    ASSERT(!m_pendingFrameRecords.isEmpty());

    Vector<unsigned> frameOffsets;
    for (auto* callFrame : m_pendingFrameRecords) {
        unsigned frameOffset = callFrame->registers() - m_futureSliceBottom;
        frameOffsets.append(frameOffset);
    }

    std::span<Register> span(const_cast<Register*>(m_futureSliceBottom), m_futureSliceTop - m_futureSliceBottom + headroomSlotCount);
    auto result = EvacuatedStackSlice::create(span, WTF::move(frameOffsets), m_futureReturnFromFrame);

    m_pendingFrameRecords.clear();
    m_futureReturnFromFrame = nullptr;
    return result;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

Vector<std::unique_ptr<EvacuatedStackSlice>> StackSlicerBase::reverseAndTakeSlices()
{
    m_slices.reverse();
    return WTF::move(m_slices);
}

/*
    Slicing Strategies Overview

    Before slicing (always initiated by a Suspending function), the stack is in one of the
    following configurations, as indicated by the value of JSPIContext::purpose of
    vmTopJSPIContext (Promising vs Completing). There is always one or more Wasm frames
    (IPInt, BBQ, or OMG) between JSToWasm and WasmToJS frames. The position of
    JSPIContext::limitFrame is indicated by an arrow. Higher addresses/older calls are on
    top.

    Promising stack configuration:

    ->  Promising
        VM entry frame  <- VM.topEntryFrame
        JSToWasm
        Wasm +
        WasmToJS
        Suspending

    Completing stack configuration:

    The JSToWasm frame is shown in brackets to indicate that it may or may not be present,
    depending on whether the slice is from the (logical) bottom of the original stack or
    not. WasmToJS frame is always present because slicing is always initiated by a
    Suspending function, reached by Wasm via a WasmToJSFrame.

    ->  PinballHandlerFulfillFunction
        Sentinel                      <- VM.topEntryFrame
        [JSToWasm]
        Wasm +
        WasmToJS
        Suspending

    SlabSlicer walks the stack until it reaches the limit frame, noting frame positions.
    Sentinel frame, being a top VM entry frame, is skipped by StackVisitor. SlabSlicer
    saves as a single slice all frames from WasmToJS and up to but not including the
    sentinel.

    FragSlicer generally saves each frame as a slice of its own. As an exception, it
    combines a JSToWasm and WasmToJS frame with an adjacent Wasm frame into one slice. If
    there is only one Wasm frame, that frame and the adjacent WasmToJS and JSToWasm frames
    are combined into a single slice.

    Stack walk begins at a Suspending frame, and FragSlicer goes through the following
    sequence of states:

        Initial - expecting a Suspending frame
        ScannedSuspending - expecting a WasmToJS frame
        ScannedWasmToJS - expecting a Wasm frame
        ScanningWasm - scanned a Wasm frame, expecting one of: Wasm, JSToWasm, limitFrame (Promising or Sentinel)

    The first three states are traversed sequentially. Once the ScanningWasm state is
    reached, the slicer may remain in it for a while as more Wasm frames are visited. If a
    JSToWasmFrame is encountered in this state, the slicer switches to the ScannedJSToWasm
    state. Once in ScannedJSToWasm state, the next visited frame must be the limit frame.
    Limit frame may also be encountered while in ScanningWasm state without an intervening
    JSToWasm state, but that is only valid when JSPIContext::purpose is Completing.
    With Promising purpose, a limit frame must always be preceded by a JSToWasm frame.

    Once limitFrame is reached, the walk is complete.
*/

IterationStatus SlabSlicer::step(VM& vm, StackVisitor& visitor)
{
    CallFrame* callFrame = visitor->callFrame();
    auto compilationMode = compilationModeOfCallee(visitor->callee());

    if (callFrame == m_lastVisitedFrame) {
        // Inlining causes apparently the same frame to be visited multiple times.
        // These additional visits do not affect the slicing decisions.
        return IterationStatus::Continue;
    }

    JSPIContext* context = vm.topJSPIContext;
    bool inPromisingContext = context->purpose == JSPIContext::Purpose::Promising;

    if (callFrame == context->limitFrame) {
        if (m_state == State::ScannedJSToWasm || (m_state == State::Scanning && !inPromisingContext)) {
            m_futureSliceTop = topOfFrame(m_lastVisitedFrame);
            commitPendingSlice();
            m_state = State::Success;
        } else {
            m_errorMessage = "JSPI stack scan reached the limit frame unexpectedly"_s;
            m_state = State::Failure;
        }
        m_teleportFrame = const_cast<CallFrame*>(m_lastVisitedFrame);
        return IterationStatus::Done;
    }

    switch (m_state) {
    case State::Initial: {
        if (!compilationMode) {
            m_futureSliceBottom = topOfFrame(callFrame);
            m_futureReturnFromFrame = callFrame;
            m_state = State::Scanning;
        } else {
            m_errorMessage = "expected suspending frame not found"_s;
            m_state = State::Failure;
        }
        break;
    }
    case State::Scanning: {
        if (compilationMode.has_value()) {
            switch (*compilationMode) {
            case Wasm::CompilationMode::WasmToJSMode:
            case Wasm::CompilationMode::IPIntMode:
            case Wasm::CompilationMode::BBQMode:
            case Wasm::CompilationMode::OMGMode:
            case Wasm::CompilationMode::OMGForOSREntryMode: {
                m_pendingFrameRecords.append(callFrame);
                break;
            }
            case Wasm::CompilationMode::JSToWasmICMode:
            case Wasm::CompilationMode::JSToWasmMode: {
                m_pendingFrameRecords.append(callFrame);
                m_state = State::ScannedJSToWasm;
                break;
            }
            default: {
                StringPrintStream modeDescription;
                modeDescription.print(*compilationMode);
                m_errorMessage = makeString("encountered an unrecognized type of Wasm frame: "_s, modeDescription.toString());
                m_state = State::Failure;
            }
            }
        } else { // no compilationMode - a JS frame
            m_errorMessage = "encountered an unexpected non-Wasm frame"_s;
            m_state = State::Failure;
        }
        break;
    }
    case State::ScannedJSToWasm: {
        // Once we are in ScannedJSToWasm, we expect to see limitFrame next and get out at the top of step().
        // Getting here means there are JS frames between the suspension point and the limitFrame. In other
        // words, it means execution left Wasm before returning and leaving again to get suspended, which is
        // a SuspensionError per the spec.
        m_errorMessage = "unexpected frame after reaching a JSToWasmFrame"_s;
        m_state = State::Overrun;
        break;
    }
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }

    m_lastVisitedFrame = callFrame;
    if (m_state == State::Failure || m_state == State::Overrun)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

IterationStatus FragSlicer::step(VM& vm, StackVisitor& visitor)
{
    CallFrame* callFrame = visitor->callFrame();
    auto compilationMode = compilationModeOfCallee(visitor->callee());

    if (callFrame == m_lastVisitedFrame) {
        // Inlining causes apparently the same frame to be visited multiple times.
        // These additional visits do not affect the slicing decisions.
        return IterationStatus::Continue;
    }

    JSPIContext* context = vm.topJSPIContext;
    bool inPromisingContext = context->purpose == JSPIContext::Purpose::Promising;

    if (callFrame == context->limitFrame) {
        if (m_state == State::ScannedJSToWasm) {
            m_state = State::Success;
        } else if (m_state == State::ScanningWasm && !inPromisingContext) {
            commitPendingSlice();
            m_state = State::Success;
        } else {
            m_errorMessage = "JSPI stack scan reached the limit frame unexpectedly"_s;
            m_state = State::Failure;
        }
        m_teleportFrame = const_cast<CallFrame*>(m_lastVisitedFrame);
        return IterationStatus::Done;
    }

    switch (m_state) {
    case State::Initial: {
        if (!compilationMode) {
            m_futureSliceBottom = topOfFrame(callFrame);
            m_futureReturnFromFrame = callFrame;
            m_state = State::ScannedSuspending;
        } else {
            m_errorMessage = "expected suspending frame not found"_s;
            m_state = State::Failure;
        }
        break;
    }
    case State::ScannedSuspending: {
        if (compilationMode == Wasm::CompilationMode::WasmToJSMode) {
            ASSERT(m_futureReturnFromFrame);
            m_pendingFrameRecords.append(callFrame);
            m_state = State::ScannedWasmToJS;
        } else {
            m_errorMessage = "suspending frame not followed by a WasmToJS frame as expected"_s;
            m_state = State::Failure;
        }
        break;
    }
    case State::ScannedWasmToJS: {
        if (compilationMode == Wasm::CompilationMode::IPIntMode
            || compilationMode == Wasm::CompilationMode::BBQMode
            || compilationMode == Wasm::CompilationMode::OMGMode
        ) {
            ASSERT(m_futureSliceBottom && m_futureReturnFromFrame);
            m_futureSliceTop = topOfFrame(callFrame);
            m_pendingFrameRecords.append(callFrame);
            m_state = State::ScanningWasm;
        } else {
            m_errorMessage = "a WasmToJSFrame not followed by a recognized Wasm frame"_s;
            m_state = State::Failure;
        }
        break;
    }
    case State::ScanningWasm: {
        if (compilationMode.has_value()) {
            switch (*compilationMode) {
            case Wasm::CompilationMode::IPIntMode:
            case Wasm::CompilationMode::BBQMode:
            case Wasm::CompilationMode::OMGMode:
            case Wasm::CompilationMode::OMGForOSREntryMode: {
                // Commit the pending slice and start a new pending from the bottom of this frame.
                const CallFrame* savedReturnFromFrame = m_pendingFrameRecords.last();
                auto* savedBottom = m_futureSliceTop;
                commitPendingSlice(); // clobbers some members, if needed they should be saved as above
                m_futureSliceBottom = savedBottom;
                m_futureSliceTop = topOfFrame(callFrame);
                m_futureReturnFromFrame = savedReturnFromFrame;
                m_pendingFrameRecords.append(callFrame);
                break;
            }
            case Wasm::CompilationMode::JSToWasmICMode:
            case Wasm::CompilationMode::JSToWasmMode: {
                commitPendingSliceWithAdditionalFrame(callFrame);
                m_state = State::ScannedJSToWasm;
                break;
            }
            default: {
                StringPrintStream modeDescription;
                modeDescription.print(*compilationMode);
                m_errorMessage = makeString("encountered an unrecognized type of Wasm frame: "_s, modeDescription.toString());
                m_state = State::Failure;
            }
            }
        } else { // no compilationMode - a JS frame
            m_errorMessage = "encountered an unexpected non-Wasm frame"_s;
            m_state = State::Failure;
        }
        break;
    }
    case State::ScannedJSToWasm: {
        m_errorMessage = "unexpected frame after reaching a JSToWasmFrame"_s;
        m_state = State::Overrun;
        break;
    }
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }

    m_lastVisitedFrame = callFrame;
    if (m_state == State::Failure || m_state == State::Overrun)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
