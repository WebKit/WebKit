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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/Register.h>
#include <JavaScriptCore/VMEntryRecord.h>

#include <wtf/TrailingArray.h>
#include <wtf/Vector.h>

namespace JSC {

// TERMINOLOGY: 'top' and 'bottom' are ambiguous terms when stacks are involved. To agree
// with the VM naming convention, we use these terms in the physical (memory address)
// sense, so the 'bottom' frame is the most recently executed one (top of the call stack).

// A fragment of the main execution stack copied to the heap as a unit. A slice may
// include one or more frames. In addition to the actual copied stack data, it carries
// metadata that will allow us to implant the slice back onto the execution stack and kick
// off the execution of the bottom frame. Instances are created by StackSlicer.
class EvacuatedStackSlice final : public TrailingArray<EvacuatedStackSlice, Register> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(EvacuatedStackSlice);
    friend class LLIntOffsetsExtractor;
public:
    using Base = TrailingArray<EvacuatedStackSlice, Register>;

    static std::unique_ptr<EvacuatedStackSlice> create(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const CallFrame* frametoReturnFromForEntry);

    std::span<Register> slots();

    // Offsets of frame records in the trailing array data, in units of Register size.
    // Ordered from lowest to highest (shallowest to deepest frames).
    const Vector<unsigned>& frameOffsets() const { return m_frameOffsets; }

    // Copy the stack data captured by this instance to the memory location identified by
    // 'base' and prepare it for execution by relocating all internal pointers. Link the
    // top frame to return to the specified 'previousFrame' (running _exit_implanted_slice
    // function implemented in InPlaceInterpreter.asm).
    CallFrame* implant(Register* base, CallFrame* previousFrame);

    // The PC to return to to enter the top (logically) frame of the slice.
    // The value as it was on the original stack--if PAC is in use, signed by 'pacibsp'.
    const void* entryPC() const { return m_entryPC; };

    // The address of the original frame that contained entryPC. Saved for authenticating entryPC.
    // 'void*' because the frame does not exist anymore--do not dereference!
    const void* entryPCFrame() const { return m_entryPCFrame; }

    void dump(PrintStream& out) const;

private:
    friend class StackSlicerBase;

    EvacuatedStackSlice(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const CallFrame* frameToReturnFromForEntry);

    const Register* m_originalBase;
    Vector<unsigned> m_frameOffsets;
    const void* m_entryPC;
    const void* m_entryPCFrame;
};

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

inline std::span<Register> EvacuatedStackSlice::slots()
{
    return { &first(), size() };
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

// Authenticate 'returnPC' assuming is was stored in a frame pointed at by 'originalFP', and
// re-sign it so it can be used in a frame pointed at by 'newFP'.
void* relocateReturnPC(void* returnPC, const CallerFrameAndPC* originalFP, const CallerFrameAndPC* newFP);

// An abstract class with a set of utilities for implementing a concrete stack slicer.
// A stack slicer is a class driven by a StackVisitor via a StackSlicerFunctor. It
// walks the stack from a Suspending frame to a Promising or PinballHandler frame and
// moves the frames to the heap as a series of EvacuatedStackSlices. A concrete slicer
// class determines the policy of how exactly the frames on the stack are grouped into
// slices.
class StackSlicerBase {
public:
    const String& errorMessage() const { return m_errorMessage; }
    const Vector<std::unique_ptr<EvacuatedStackSlice>>& slices() const { return m_slices; }

    // Return the accumulated slices in the order from top to bottom. This places the
    // first slice to execute at the end of the vector, and the vector acts as a stack
    // with pop=takeLast() and push=append().
    Vector<std::unique_ptr<EvacuatedStackSlice>> reverseAndTakeSlices();

    // After the slices are evacuated, we skip over them by returnin into this frame.
    CallFrame* teleportFrame() const { return m_teleportFrame; }

protected:
    // Create a slice for the stack area identified by the future bottom and top pointers.
    // Include extra 'headroomSlotCount' registers above the actual frame top pointer.
    // The amount of the headroom is dictated by the frame callee.
    std::unique_ptr<EvacuatedStackSlice> evacuatePendingSlice(unsigned headroomSlotCount);
    void commitPendingSliceWithAdditionalFrame(CallFrame*);
    void commitPendingSlice();

    String m_errorMessage { "?"_s };
    Vector<std::unique_ptr<EvacuatedStackSlice>> m_slices;
    const CallFrame* m_lastVisitedFrame { nullptr };
    const Register* m_futureSliceBottom { nullptr };
    const Register* m_futureSliceTop { nullptr };
    const CallFrame* m_futureReturnFromFrame { nullptr };
    Vector<CallFrame*> m_pendingFrameRecords;
    CallFrame* m_teleportFrame;
};

template<typename T>
concept ConcreteStackSlicer =
    std::derived_from<T, StackSlicerBase>
    && requires(T& t, VM& vm, StackVisitor& sv) {
        // IterationStatus step(VM&, StackVisitor&);
        { t.step(vm, sv) } -> std::same_as<IterationStatus>;
    };

// A concrete stack slicer that evacuates the stack as a single slice that contains all
// interesting frames.
class SlabSlicer final : public StackSlicerBase {
public:
    IterationStatus step(VM&, StackVisitor&);

    bool succeeded() const { return m_state == State::Success; }
    bool didOverrun() const { return m_state == State::Overrun; }

private:
    enum class State {
        Initial,
        Scanning,
        ScannedJSToWasm,
        // The following are the three final states
        Success,
        Overrun, // traversed all Wasm frames but did not reach limitFrame
        Failure
    };

    State m_state { State::Initial };
};

// A concrete stack slicer that evacuates the stack such that each Wasm frame
// gets a slice of its own, except for the topmost and bottommost Wasm frames
// which are combined with the adjacent WasmToJS and JSToWasm frames.
class FragSlicer final : public StackSlicerBase {
public:
    IterationStatus step(VM&, StackVisitor&);

    bool succeeded() const { return m_state == State::Success; }
    bool didOverrun() const { return m_state == State::Overrun; }

private:
    enum class State {
        Initial,
        ScannedSuspending,
        ScannedWasmToJS,
        ScanningWasm,
        ScannedJSToWasm,
        // The following are the three final states
        Success,
        Overrun,
        Failure
    };

    State m_state { State::Initial };
};

// A functor given to the standard StackVisitor to drive a concrete stack slicer.
template<ConcreteStackSlicer Slicer>
class StackSlicerFunctor final : UnwindFunctorBase {
public:
    StackSlicerFunctor(VM& vm, Slicer& scanner)
        : UnwindFunctorBase(vm)
        , m_scanner(scanner)
    { }

    IterationStatus operator() (StackVisitor&) const;

private:
    Slicer& m_scanner;
};


template <ConcreteStackSlicer Slicer>
IterationStatus StackSlicerFunctor<Slicer>::operator() (StackVisitor& visitor) const
{
    visitor.unwindToMachineCodeBlockFrame();

    IterationStatus result = m_scanner.step(m_vm, visitor);

    if (result == IterationStatus::Continue) {
        auto* currentFrame = visitor->callFrame();
        JSGlobalObject* lexicalGlobalObject = currentFrame->lexicalGlobalObject(m_vm);
        notifyDebuggerOfUnwinding(lexicalGlobalObject, currentFrame);
        copyCalleeSavesToEntryFrameCalleeSavesBuffer(visitor);
    }
    return result;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
