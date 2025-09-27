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

#include "JSWebAssemblyPromisingFunction.h"
#include "NativeCallee.h"
#include "WasmCallee.h"

namespace JSC {

std::unique_ptr<EvacuatedStackSlice> EvacuatedStackSlice::create(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const void* entryPC)
{
    ASSERT(stackSpan.size());
    return std::unique_ptr<EvacuatedStackSlice> {
        new (NotNull, fastMalloc(Base::allocationSize(stackSpan.size()))) EvacuatedStackSlice(stackSpan, WTFMove(frameOffsets), entryPC)
    };
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

CallFrame* EvacuatedStackSlice::implant(Register* base, CallFrame* lastFrame, void* returnPC)
{
    memcpySpan(std::span<Register>(base, size()), slots());
    for (unsigned i = m_frameOffsets.size(); i > 0; --i) {
        bool isTopFrame = i == m_frameOffsets.size();
        unsigned offset = m_frameOffsets[i - 1];
        CallerFrameAndPC* frameRecord = reinterpret_cast<CallerFrameAndPC*>(base + offset);
        // Fix the caller frame pointer
        frameRecord->callerFrame = lastFrame;
        // Resign the return address for the new position
        void* resignedAddr = nullptr;
#if CPU(ARM64E)
        if (isTopFrame) {
            // The returnPC passed in as an argument is a label address signed by Clang using asia and a magic value of 0x590c as the discriminator.
            auto unsignedAddr = ptrauth_strip(returnPC, 0);
            auto discriminator = frameRecord + 1;
            resignedAddr = ptrauth_sign_unauthenticated(unsignedAddr, ptrauth_key_asib, discriminator);
        } else {
            auto discriminatorFrom = &first() + offset;
            auto discriminatorTo = frameRecord + 1;
            resignedAddr = ptrauth_auth_and_resign(frameRecord->returnPC, ptrauth_key_asib, discriminatorFrom, ptrauth_key_asib, discriminatorTo);
        }
#else
        resignedAddr = returnPC;
        UNUSED_VARIABLE(isTopFrame);
#endif
        frameRecord->returnPC = resignedAddr;
        lastFrame = static_cast<CallFrame*>(static_cast<void*>(frameRecord));
    }
    return lastFrame;
}

// Called during slice creation to resign all return PCs in the saved data. The signing
// discriminator is the address of the frame record itself. This is not how it works in
// a live frame, but the saved data is not used as a live frame.
void EvacuatedStackSlice::reauthenticateReturnPCs(const Register* originalBottom)
{
#if CPU(ARM64E)
    Register* copiedBottom = &first();
    for (unsigned offset : m_frameOffsets) {
        CallerFrameAndPC* frameRecord = reinterpret_cast<CallerFrameAndPC*>(copiedBottom + offset);
        const Register* signatureSP = originalBottom + offset + 2;
        frameRecord->returnPC = ptrauth_auth_and_resign(frameRecord->returnPC, ptrauth_key_asib, signatureSP, ptrauth_key_asib, frameRecord);
    }
#else
    UNUSED_PARAM(originalBottom);
#endif
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

static const Register* topOfFrame(const CallFrame* callFrame)
{
    // We include a few extra slots above the frame record via the
    // headroomSlotCount parameter of StackSlicer::evacuatePendingSlice, but we still count
    // the frame record as the real top of Wasm frames and the bottom of the next frame.
    CalleeBits calleeBits = callFrame->callee();
    if (calleeBits.isNativeCallee()) {
        auto* nativeCallee = calleeBits.asNativeCallee();
        ASSERT(nativeCallee->category() == NativeCallee::Category::Wasm);
        auto* wasmCallee = static_cast<Wasm::Callee*>(nativeCallee);

        switch (wasmCallee->compilationMode()) {
        case Wasm::CompilationMode::WasmToJSMode:
        case Wasm::CompilationMode::IPIntMode:
        case Wasm::CompilationMode::BBQMode:
        case Wasm::CompilationMode::OMGMode:
            return callFrame->registers() + 2;
        case Wasm::CompilationMode::JSToWasmMode:
            return callFrame->registers() + static_cast<size_t>(CallFrameSlot::firstArgument) + callFrame->argumentCount();
        default:
            RELEASE_ASSERT_NOT_REACHED(); // case not accounted for
        }
    } else {
        // Assuming a JSFunction callee with a well-formed frame.
        return callFrame->registers() + static_cast<size_t>(CallFrameSlot::firstArgument) + callFrame->argumentCount();
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
    auto* wasmCallee = static_cast<Wasm::Callee*>(nativeCallee);
    return wasmCallee->compilationMode();
}

// We save this many extra slots above the actual frame record (the fp/lr pair) of a Wasm
// frame because IPInt stores register values there before a call. This value used to be
// conditional based on the frame type but it appears tiering up breaks without a
// consistent headroom.
constexpr unsigned StandardHeadroom = 8;

void StackSlicerBase::commitPendingSliceWithAdditionalFrame(CallFrame* callFrame)
{
    m_futureSliceTop = topOfFrame(callFrame);
    m_pendingFrameRecords.append(callFrame);
    m_lastVisitedFrame = callFrame;
    commitPendingSlice();
}

void StackSlicerBase::commitPendingSlice()
{
    auto slice = evacuatePendingSlice(StandardHeadroom);
    m_slices.append(WTFMove(slice));
    m_futureSliceBottom = nullptr;
    m_futureSliceTop = nullptr;
    m_futureReturnPC = nullptr;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

std::unique_ptr<EvacuatedStackSlice> StackSlicerBase::evacuatePendingSlice(unsigned headroomSlotCount)
{
    ASSERT(m_futureSliceBottom);
    ASSERT(m_futureSliceTop);
    ASSERT(m_futureReturnPC);
    ASSERT(!m_pendingFrameRecords.isEmpty());

    Vector<unsigned> frameOffsets;
    for (auto* callFrame : m_pendingFrameRecords) {
        unsigned frameOffset = callFrame->registers() - m_futureSliceBottom;
        frameOffsets.append(frameOffset);
    }

    std::span<Register> span(const_cast<Register*>(m_futureSliceBottom), m_futureSliceTop - m_futureSliceBottom + headroomSlotCount);
    auto result = EvacuatedStackSlice::create(span, WTFMove(frameOffsets), m_futureReturnPC);
    result->reauthenticateReturnPCs(m_futureSliceBottom);
    m_pendingFrameRecords.clear();
    m_futureReturnPC = nullptr;
    return result;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

Vector<std::unique_ptr<EvacuatedStackSlice>> StackSlicerBase::reverseAndTakeSlices()
{
    m_slices.reverse();
    return WTFMove(m_slices);
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
        JSToWasm
        Wasm +
        WasmToJS
        Suspending

    Completing stack configuration. The JSToWasm frame is shown in brackets to indicate
    that it may or may not be present, depending on whether the slice is from the
    (logical) bottom of the original stack or not. WasmToJS frame is always present
    because slicing is always initiated by a Suspending function, reached by Wasm via
    a WasmToJSFrame.

        PinballHandlerFulfillFunction
    ->  Sentinel
        [JSToWasm]
        Wasm +
        WasmToJS
        Suspending

    SlabSlicer simply walks the stack until it reaches the limit frame, noting frame
    positions. It then saves as a single slice all frames from WasmToJS and up to but not
    including the limit frame.

    FragSlicer combines a JSToWasm and WasmToJS frame with an adjacent Wasm frame into
    one slice. If there is only one Wasm frame, it and the adjacent WasmToJS and JSToWasm
    frames are combined into a single slice.

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
#if CPU(ARM64E)
            m_futureReturnPC = ptrauth_strip(callFrame->rawReturnPC(), 0);
#else
            m_futureReturnPC = callFrame->rawReturnPC();
#endif
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
            case Wasm::CompilationMode::OMGMode: {
                m_pendingFrameRecords.append(callFrame);
                break;
            }
            case Wasm::CompilationMode::JSToWasmMode: {
                m_pendingFrameRecords.append(callFrame);
                m_state = State::ScannedJSToWasm;
                break;
            }
            default: {
                m_errorMessage = "encountered an unrecognized type of Wasm frame"_s;
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
        m_errorMessage = "unexpected frames after seeing a JSToWasmFrame"_s;
        m_state = State::Failure;
        break;
    }
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }

    m_lastVisitedFrame = callFrame;
    if (m_state == State::Failure)
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
#if CPU(ARM64E)
            m_futureReturnPC = ptrauth_strip(callFrame->rawReturnPC(), 0);
#else
            m_futureReturnPC = callFrame->rawReturnPC();
#endif
            m_state = State::ScannedSuspending;
        } else {
            m_errorMessage = "expected suspending frame not found"_s;
            m_state = State::Failure;
        }
        break;
    }
    case State::ScannedSuspending: {
        if (compilationMode == Wasm::CompilationMode::WasmToJSMode) {
            ASSERT(m_futureReturnPC);
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
            ASSERT(m_futureSliceBottom && m_futureReturnPC);
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
            case Wasm::CompilationMode::OMGMode: {
                // commit the pending slice and start a new pending from the bottom of this frame
                void* nextReturnPC = m_pendingFrameRecords.last()->rawReturnPC();
                auto* nextBottom = m_futureSliceTop;
                commitPendingSlice();
                m_futureSliceBottom = nextBottom;
                m_futureSliceTop = topOfFrame(callFrame);
#if CPU(ARM64E)
                m_futureReturnPC = ptrauth_strip(nextReturnPC, 0);
#else
                m_futureReturnPC = nextReturnPC;
#endif
                m_pendingFrameRecords.append(callFrame);
                break;
            }
            case Wasm::CompilationMode::JSToWasmMode: {
                commitPendingSliceWithAdditionalFrame(callFrame);
                m_state = State::ScannedJSToWasm;
                break;
            }
            default: {
                m_errorMessage = "encountered an unrecognized type of Wasm frame"_s;
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
        m_errorMessage = "unexpected frames after seeing a JSToWasmFrame"_s;
        m_state = State::Failure;
        break;
    }
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }

    m_lastVisitedFrame = callFrame;
    if (m_state == State::Failure)
        return IterationStatus::Done;
    return IterationStatus::Continue;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
