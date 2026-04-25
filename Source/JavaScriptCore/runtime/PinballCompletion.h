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
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/JSFunction.h>
#include <JavaScriptCore/JSFunctionWithFields.h>

namespace JSC {

// Orchestrates incremental slice-by-slice return for JSPI to pass the result of a
// resolved promise through a series of synchronous code frames, with the value produced
// by that code ultimately used to resolve another promise. "Pinball" because instead of
// returning straight down all captured Wasm frame, we may do so in a series of bumps
// as we execute evacuated slices one after another.

class PinballCompletion final : public JSNonFinalObject {
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
    static PinballCompletion* create(VM&, Vector<std::unique_ptr<EvacuatedStackSlice>>&&, CPURegister* calleeSaves, JSPromise* resultPromise);

    JSPromise* resultPromise() LIFETIME_BOUND { return m_resultPromise.get(); }

    Vector<std::unique_ptr<EvacuatedStackSlice>>& slices() LIFETIME_BOUND { return m_slices; }
    std::unique_ptr<EvacuatedStackSlice> takeTopSlice() { return m_slices.takeLast(); }
    bool hasSlices() const { return !m_slices.isEmpty(); }

    CPURegister* calleeSaves() { return m_calleeSaves; }

    void assimilate(PinballCompletion*);

    DECLARE_INFO;
    DECLARE_VISIT_CHILDREN;

private:
    PinballCompletion(VM&, Structure*, Vector<std::unique_ptr<EvacuatedStackSlice>>&& slices, CPURegister* calleeSaves, JSPromise* resultPromise);
    ~PinballCompletion();

    Vector<std::unique_ptr<EvacuatedStackSlice>> m_slices;
    CPURegister m_calleeSaves[NUMBER_OF_CALLEE_SAVES_REGISTERS];
    WriteBarrier<JSPromise> m_resultPromise;
};

JSFunctionWithFields* createPinballCompletionFulfillHandler(VM&, JSGlobalObject*, PinballCompletion*);
JSFunctionWithFields* createPinballCompletionRejectHandler(VM&, JSGlobalObject*, PinballCompletion*);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
