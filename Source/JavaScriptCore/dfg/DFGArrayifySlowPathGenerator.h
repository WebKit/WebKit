/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGArrayMode.h"
#include "DFGOSRExitJumpPlaceholder.h"
#include "DFGOperations.h"
#include "DFGSlowPathGenerator.h"
#include "DFGSpeculativeJIT.h"
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

class ArrayifySlowPathGenerator final : public JumpingSlowPathGenerator<MacroAssembler::JumpList> {
    WTF_MAKE_TZONE_ALLOCATED(ArrayifySlowPathGenerator);
public:
    ArrayifySlowPathGenerator(
        const MacroAssembler::JumpList& from, SpeculativeJIT* jit, Node* node, GPRReg baseGPR,
        GPRReg propertyGPR, GPRReg tempGPR, GPRReg structureGPR)
        : JumpingSlowPathGenerator<MacroAssembler::JumpList>(from, jit)
        , m_op(node->op())
        , m_structure(node->op() == ArrayifyToStructure ? node->structure() : RegisteredStructure())
        , m_arrayMode(node->arrayMode())
        , m_baseGPR(baseGPR)
        , m_propertyGPR(propertyGPR)
        , m_tempGPR(tempGPR)
        , m_structureGPR(structureGPR)
    {
        ASSERT(m_op == Arrayify || m_op == ArrayifyToStructure);
        
        jit->silentSpillAllRegistersImpl(false, m_plans, InvalidGPRReg);
        
        if (m_propertyGPR != InvalidGPRReg) {
            switch (m_arrayMode.type()) {
            case Array::Int32:
            case Array::Double:
            case Array::Contiguous:
                m_badPropertyJump = jit->speculationCheck(Uncountable, JSValueRegs(), nullptr);
                break;
            default:
                break;
            }
        }
        m_badIndexingTypeJump = jit->speculationCheck(BadIndexingType, JSValueSource::unboxedCell(m_baseGPR), nullptr);
    }
    
private:
    void generateInternal(SpeculativeJIT* jit) final
    {
        linkFrom(jit);
        
        ASSERT(m_op == Arrayify || m_op == ArrayifyToStructure);
        
        if (m_propertyGPR != InvalidGPRReg) {
            switch (m_arrayMode.type()) {
            case Array::Int32:
            case Array::Double:
            case Array::Contiguous:
                m_badPropertyJump.fill(jit, jit->branch32(
                    MacroAssembler::AboveOrEqual, m_propertyGPR,
                    MacroAssembler::TrustedImm32(MIN_SPARSE_ARRAY_INDEX)));
                break;
            default:
                break;
            }
        }
        
        VM& vm = jit->vm();
        switch (m_arrayMode.type()) {
        case Array::Int32:
            jit->callOperationWithSilentSpill(m_plans.span(), operationEnsureInt32, m_tempGPR, SpeculativeJIT::TrustedImmPtr(&vm), m_baseGPR);
            break;
        case Array::Double:
            jit->callOperationWithSilentSpill(m_plans.span(), operationEnsureDouble, m_tempGPR, SpeculativeJIT::TrustedImmPtr(&vm), m_baseGPR);
            break;
        case Array::Contiguous:
            jit->callOperationWithSilentSpill(m_plans.span(), operationEnsureContiguous, m_tempGPR, SpeculativeJIT::TrustedImmPtr(&vm), m_baseGPR);
            break;
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            jit->callOperationWithSilentSpill(m_plans.span(), operationEnsureArrayStorage, m_tempGPR, SpeculativeJIT::TrustedImmPtr(&vm), m_baseGPR);
            break;
        default:
            CRASH();
            break;
        }
        
        if (m_op == ArrayifyToStructure) {
            ASSERT(m_structure.get());
            m_badIndexingTypeJump.fill(
                jit, jit->branchWeakStructure(MacroAssembler::NotEqual, MacroAssembler::Address(m_baseGPR, JSCell::structureIDOffset()), m_structure));
        } else {
            // Finally, check that we have the kind of array storage that we wanted to get.
            // Note that this is a backwards speculation check, which will result in the 
            // bytecode operation corresponding to this arrayification being reexecuted.
            // That's fine, since arrayification is not user-visible.
            jit->load8(
                MacroAssembler::Address(m_baseGPR, JSCell::indexingTypeAndMiscOffset()),
                m_structureGPR);
            m_badIndexingTypeJump.fill(
                jit, jit->jumpSlowForUnwantedArrayMode(m_structureGPR, m_arrayMode));
        }
        
        jumpTo(jit);
    }

    NodeType m_op;
    RegisteredStructure m_structure;
    ArrayMode m_arrayMode;
    GPRReg m_baseGPR;
    GPRReg m_propertyGPR;
    GPRReg m_tempGPR;
    GPRReg m_structureGPR;
    OSRExitJumpPlaceholder m_badPropertyJump;
    OSRExitJumpPlaceholder m_badIndexingTypeJump;
    Vector<SilentRegisterSavePlan, 2> m_plans;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
