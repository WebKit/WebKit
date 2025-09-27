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

#include <JavaScriptCore/EvacuatedStack.h>
#include <JavaScriptCore/JSFunction.h>

namespace JSC {

// Orchestrates incremental slice-by-slice return for JSPI to pass the result of a
// resolved promise through a series of synchronous code frames, with the value produced
// by that code ultimately used to resolve another promise. "Pinball" because instead of
// returning straight down the system stack, we may do so in a series of bumps.
class PinballCompletion : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.pinballCompletionSpace<mode>();
    }

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue proto);
    static PinballCompletion* create(VM&, Vector<std::unique_ptr<EvacuatedStackSlice>>&&, CPURegister* calleeSaves);

    JSPromise* resultPromise() { return m_resultPromise.get(); }
    void setResultPromise(VM&, JSPromise*);

    Vector<std::unique_ptr<EvacuatedStackSlice>>& slices() { return m_slices; }
    std::unique_ptr<EvacuatedStackSlice> takeTopSlice() { return m_slices.takeLast(); }
    bool hasSlices() const { return !m_slices.isEmpty(); }

    CPURegister* calleeSaves() { return m_calleeSaves; }

    void assimilate(VM&, PinballCompletion*);

    DECLARE_INFO;
    DECLARE_VISIT_CHILDREN;

private:
    PinballCompletion(VM&, Structure*, Vector<std::unique_ptr<EvacuatedStackSlice>>&& slices, CPURegister* calleeSaves);
    Vector<std::unique_ptr<EvacuatedStackSlice>> m_slices;
    CPURegister m_calleeSaves[NUMBER_OF_CALLEE_SAVES_REGISTERS];
    WriteBarrier<JSPromise> m_resultPromise;
};

// A JS function used as promise fulfill or reject handler to hook it up to the pinball
// return machinery. Which handler it is depends on the factory method used to create the
// function.
class PinballHandler final : public JSFunction {
public:
    using Base = JSFunction;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.pinballHandlerSpace<mode>();
    }

    DECLARE_INFO;
    DECLARE_VISIT_CHILDREN;

    static PinballHandler* createFulfiller(VM&, JSGlobalObject*, PinballCompletion*);
    static PinballHandler* createRejecter(VM&, JSGlobalObject*, PinballCompletion*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    PinballCompletion* pinballCompletion() { return m_pinballCompletion.get(); }

private:
    static PinballHandler* create(VM&, JSGlobalObject*, NativeFunction, PinballCompletion*);

    PinballHandler(VM& vm, NativeExecutable* native, JSGlobalObject* globalObject, Structure* structure, PinballCompletion* pinball)
        : Base(vm, native, globalObject, structure)
        , m_pinballCompletion(pinball, WriteBarrierEarlyInit)
    { }

    void finishCreation(VM&, NativeExecutable*, const String&);

    WriteBarrier<PinballCompletion> m_pinballCompletion;
};

// Effectively the local variables of the function pinballCompletionFulfillmentHandler implemented in offlineasm.
struct PinballFulfillContext {
#if ASSERT_ENABLED
    size_t magic;
#endif
    JSGlobalObject* globalObject;
    PinballHandler* handler;
    JSPIContext jspiContext;
    EvacuatedStackSlice* slice;
    size_t sliceByteSize;
    CPURegister* evacuatedCalleeSaves; // callee saves from the handler's pinballCompletion, to pass into Wasm
    CPURegister cppCalleeSaves[NUMBER_OF_CALLEE_SAVES_REGISTERS]; // callee saves state on entering the handler
    CPURegister arguments[GPRInfo::numberOfArgumentRegisters + FPRInfo::numberOfArgumentRegisters];
};

// TODO: once we have full unwind handling this might be close enough to the fulfill context to be merged
struct PinballUnwindContext {
#if ASSERT_ENABLED
    size_t magic;
#endif
    VM* vm;
    JSCallee* zombieFrameCallee;
    EvacuatedStackSlice* slice;
    size_t sliceByteSize;
    JSWebAssemblyException* exception;
    unsigned exceptionIndex;
    CPURegister* evacuatedCalleeSaves; // callee saves from the handler's pinballCompletion, to pass into Wasm
    CPURegister cppCalleeSaves[NUMBER_OF_CALLEE_SAVES_REGISTERS]; // callee saves state on entering the handler
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
