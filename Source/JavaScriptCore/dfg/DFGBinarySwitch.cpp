/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "DFGBinarySwitch.h"

#if ENABLE(DFG_JIT)

#include "JSCInlines.h"

namespace JSC { namespace DFG {

BinarySwitch::BinarySwitch(GPRReg value, const Vector<int64_t>& cases, Type type)
    : m_value(value)
    , m_index(0)
    , m_caseIndex(UINT_MAX)
    , m_medianBias(0)
    , m_type(type)
{
    if (cases.isEmpty())
        return;
    
    for (unsigned i = 0; i < cases.size(); ++i)
        m_cases.append(Case(cases[i], i));
    std::sort(m_cases.begin(), m_cases.end());
    build(0, m_cases.size());
}

bool BinarySwitch::advance(MacroAssembler& jit)
{
    if (m_cases.isEmpty()) {
        m_fallThrough.append(jit.jump());
        return false;
    }
    
    if (m_index == m_branches.size()) {
        RELEASE_ASSERT(m_jumpStack.isEmpty());
        return false;
    }
    
    for (;;) {
        const BranchCode& code = m_branches[m_index++];
        switch (code.kind) {
        case NotEqualToFallThrough:
            switch (m_type) {
            case Int32:
                m_fallThrough.append(jit.branch32(
                    MacroAssembler::NotEqual, m_value,
                    MacroAssembler::Imm32(static_cast<int32_t>(m_cases[code.index].value))));
                break;
            case IntPtr:
                m_fallThrough.append(jit.branchPtr(
                    MacroAssembler::NotEqual, m_value,
                    MacroAssembler::ImmPtr(bitwise_cast<const void*>(static_cast<intptr_t>(m_cases[code.index].value)))));
                break;
            }
            break;
        case NotEqualToPush:
            switch (m_type) {
            case Int32:
                m_jumpStack.append(jit.branch32(
                    MacroAssembler::NotEqual, m_value,
                    MacroAssembler::Imm32(static_cast<int32_t>(m_cases[code.index].value))));
                break;
            case IntPtr:
                m_jumpStack.append(jit.branchPtr(
                    MacroAssembler::NotEqual, m_value,
                    MacroAssembler::ImmPtr(bitwise_cast<const void*>(static_cast<intptr_t>(m_cases[code.index].value)))));
                break;
            }
            break;
        case LessThanToPush:
            switch (m_type) {
            case Int32:
                m_jumpStack.append(jit.branch32(
                    MacroAssembler::LessThan, m_value,
                    MacroAssembler::Imm32(static_cast<int32_t>(m_cases[code.index].value))));
                break;
            case IntPtr:
                m_jumpStack.append(jit.branchPtr(
                    MacroAssembler::LessThan, m_value,
                    MacroAssembler::ImmPtr(bitwise_cast<const void*>(static_cast<intptr_t>(m_cases[code.index].value)))));
                break;
            }
            break;
        case Pop:
            m_jumpStack.takeLast().link(&jit);
            break;
        case ExecuteCase:
            m_caseIndex = code.index;
            return true;
        }
    }
}

void BinarySwitch::build(unsigned start, unsigned end)
{
    unsigned size = end - start;
    
    switch (size) {
    case 0: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
        
    case 1: {
        if (start
            && m_cases[start - 1].value == m_cases[start].value - 1
            && start + 1 < m_cases.size()
            && m_cases[start + 1].value == m_cases[start].value + 1) {
            m_branches.append(BranchCode(ExecuteCase, start));
            break;
        }
        
        m_branches.append(BranchCode(NotEqualToFallThrough, start));
        m_branches.append(BranchCode(ExecuteCase, start));
        break;
    }
        
    case 2: {
        if (m_cases[start].value + 1 == m_cases[start + 1].value
            && start
            && m_cases[start - 1].value == m_cases[start].value - 1
            && start + 2 < m_cases.size()
            && m_cases[start + 2].value == m_cases[start + 1].value + 1) {
            m_branches.append(BranchCode(NotEqualToPush, start));
            m_branches.append(BranchCode(ExecuteCase, start));
            m_branches.append(BranchCode(Pop));
            m_branches.append(BranchCode(ExecuteCase, start + 1));
            break;
        }
        
        unsigned firstCase = start;
        unsigned secondCase = start + 1;
        if (m_medianBias)
            std::swap(firstCase, secondCase);
        m_medianBias ^= 1;
        
        m_branches.append(BranchCode(NotEqualToPush, firstCase));
        m_branches.append(BranchCode(ExecuteCase, firstCase));
        m_branches.append(BranchCode(Pop));
        m_branches.append(BranchCode(NotEqualToFallThrough, secondCase));
        m_branches.append(BranchCode(ExecuteCase, secondCase));
        break;
    }
        
    default: {
        unsigned medianIndex = (start + end) / 2;
        if (!(size & 1)) {
            // Because end is exclusive, in the even case, this rounds up by
            // default. Hence median bias sometimes flips to subtracing one
            // in order to get round-down behavior.
            medianIndex -= m_medianBias;
            m_medianBias ^= 1;
        }
        
        RELEASE_ASSERT(medianIndex > start);
        RELEASE_ASSERT(medianIndex + 1 < end);
        
        m_branches.append(BranchCode(LessThanToPush, medianIndex));
        m_branches.append(BranchCode(NotEqualToPush, medianIndex));
        m_branches.append(BranchCode(ExecuteCase, medianIndex));
        
        m_branches.append(BranchCode(Pop));
        build(medianIndex + 1, end);
        
        m_branches.append(BranchCode(Pop));
        build(start, medianIndex);
        break;
    } }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

