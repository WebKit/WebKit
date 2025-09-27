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

#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/Register.h>
#include <JavaScriptCore/VMEntryRecord.h>

#include <wtf/TrailingArray.h>
#include <wtf/Vector.h>

namespace JSC {

// A fragment of of the main execution stack copied to the heap as a unit. A slice may
// include one or more frames. In addition to the actual data copied from the stack, it
// carries a "map" of the copied data that will allow us to install the slice back onto
// the execution stack and kick off the execution of the top (logical top, i.e. most
// recently executed) frame. Instances are created by StackSlicer.
class EvacuatedStackSlice final : public TrailingArray<EvacuatedStackSlice, Register> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(EvacuatedStackSlice);
    friend class LLIntOffsetsExtractor;
public:
    using Base = TrailingArray<EvacuatedStackSlice, Register>;

    static std::unique_ptr<EvacuatedStackSlice> create(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const void* entryPC);


WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    std::span<Register> slots() { return { &first(), size() }; }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    // Offsets of frame records in the trailing array data, in units of Register size.
    // Ordered from most to least recently executed frames (increasing offset values).
    const Vector<unsigned>& frameOffsets() const { return m_frameOffsets; }

    // Copy the stack data captured by this instance to the memory location identified by
    // 'base' and prepare it for execution by relocating all internal pointers. Link the
    // topmost (physically, deepest logically) frame to return to the specified
    // 'previousFrame' and 'returnPC'. Return the FP of the bottommost implanted frame.
    CallFrame* implant(Register* base, CallFrame* previousFrame, void* returnPC);

    // The address to go to to start executing this slice after it's been implanted.
    // SP must be set to the base and FP to the value returned by implant().
    const void* entryPC() const;

    void dump(PrintStream& out) const;

private:
    friend class StackSlicerBase;

    EvacuatedStackSlice(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const void* entryPC);

    void reauthenticateReturnPCs(const Register* originalBottom);

    Vector<unsigned> m_frameOffsets;
    const void* m_entryPC;
};

inline EvacuatedStackSlice::EvacuatedStackSlice(std::span<Register> stackSpan, Vector<unsigned>&& frameOffsets, const void* entryPC)
    : Base(stackSpan.size())
    , m_frameOffsets(WTFMove(frameOffsets))
#if CPU(ARM64E)
    , m_entryPC(ptrauth_sign_unauthenticated(entryPC, ptrauth_key_asib, this))
#else
    , m_entryPC(entryPC)
#endif
{
    memcpySpan(slots(), stackSpan);
}

inline const void* EvacuatedStackSlice::entryPC() const
{
#if CPU(ARM64E)
    return ptrauth_auth_data(m_entryPC, ptrauth_key_asib, this);
#else
    return m_entryPC;
#endif
}

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

    // Return the accumulated slices in the order from top to bottom (least recently to
    // most recently executed). This places the first slice to execute at the end of the
    // vector, and the vector acts as a stack with pop=takeLast() and push=append().
    Vector<std::unique_ptr<EvacuatedStackSlice>> reverseAndTakeSlices();

    // The frame to return into to skip over the evacuated stack slices.
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
    const void* m_futureReturnPC { nullptr }; // with auth stripped
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

// A concrete stack slicer that evacuates the stack as a single slice
// containing all interesting frames.
class SlabSlicer : public StackSlicerBase {
public:
    IterationStatus step(VM&, StackVisitor&);

    bool succeeded() const { return m_state == State::Success; }

private:
    enum class State {
        Initial,
        Scanning,
        ScannedJSToWasm,
        Success,
        Failure
    };

    State m_state { State::Initial };
};

// A concrete stack slicer that evacuates the stack such that each Wasm frame
// gets a slice of its own, except for the topmost and bottommost Wasm frames
// which are combined with the adjacent WasmToJS and JSToWasm frames.
class FragSlicer : public StackSlicerBase {
public:
    IterationStatus step(VM&, StackVisitor&);

    bool succeeded() const { return m_state == State::Success; }

private:
    enum class State {
        Initial,
        ScannedSuspending,
        ScannedWasmToJS,
        ScanningWasm,
        ScannedJSToWasm,
        Success,
        Failure
    };

    State m_state { State::Initial };
};

// A functor given to the standard StackVisitor to drive a concrete stack slicer.
template<ConcreteStackSlicer Slicer>
class StackSlicerFunctor : UnwindFunctorBase {
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
