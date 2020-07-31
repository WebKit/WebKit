/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER) && CPU(ARM64E)

// We need to include this before MacroAssemblerARM64.h because MacroAssemblerARM64
// will be defined in terms of ARM64EAssembler for ARM64E.
#include "ARM64EAssembler.h"
#include "JSCPtrTag.h"
#include "MacroAssemblerARM64.h"

namespace JSC {

using Assembler = TARGET_ASSEMBLER;

class MacroAssemblerARM64E : public MacroAssemblerARM64 {
public:
    static constexpr unsigned numberOfPACBits = WTF::maximumNumberOfPointerAuthenticationBits;
    static constexpr uintptr_t nonPACBitsMask = static_cast<uintptr_t>(-1) >> numberOfPACBits;

    ALWAYS_INLINE void tagReturnAddress()
    {
        tagPtr(ARM64Registers::sp, ARM64Registers::lr);
    }

    ALWAYS_INLINE void untagReturnAddress()
    {
        untagPtr(ARM64Registers::sp, ARM64Registers::lr);
    }

    ALWAYS_INLINE void tagPtr(PtrTag tag, RegisterID target)
    {
        auto tagGPR = getCachedDataTempRegisterIDAndInvalidate();
        move(TrustedImm64(tag), tagGPR);
        m_assembler.pacib(target, tagGPR);
    }

    ALWAYS_INLINE void tagPtr(RegisterID tag, RegisterID target)
    {
        if (target == ARM64Registers::lr && tag == ARM64Registers::sp) {
            m_assembler.pacibsp();
            return;
        }
        m_assembler.pacib(target, tag);
    }

    ALWAYS_INLINE void untagPtr(PtrTag tag, RegisterID target)
    {
        auto tagGPR = getCachedDataTempRegisterIDAndInvalidate();
        move(TrustedImm64(tag), tagGPR);
        m_assembler.autib(target, tagGPR);
    }

    ALWAYS_INLINE void untagPtr(RegisterID tag, RegisterID target)
    {
        m_assembler.autib(target, tag);
    }

    ALWAYS_INLINE void removePtrTag(RegisterID target)
    {
        m_assembler.xpaci(target);
    }

    ALWAYS_INLINE void tagArrayPtr(RegisterID length, RegisterID target)
    {
        m_assembler.pacdb(target, length);
    }

    ALWAYS_INLINE void untagArrayPtr(RegisterID length, RegisterID target, bool validateAuth, RegisterID scratch)
    {
        if (validateAuth) {
            ASSERT(scratch != InvalidGPRReg);
            move(target, scratch);
        }

        m_assembler.autdb(target, length);

        if (validateAuth) {
            ASSERT(target != ARM64Registers::sp);
            ASSERT(scratch != ARM64Registers::sp);
            removeArrayPtrTag(scratch);
            auto isValidPtr = branch64(Equal, scratch, target);
            breakpoint(0xabcd);
            isValidPtr.link(this);
        }
    }

    ALWAYS_INLINE void untagArrayPtr(Address length, RegisterID target, bool validateAuth)
    {
        auto lengthGPR = getCachedDataTempRegisterIDAndInvalidate();
        load32(length, lengthGPR);
        auto scratch = InvalidGPRReg; 
        if (validateAuth) {
            scratch = getCachedMemoryTempRegisterIDAndInvalidate();
            move(target, scratch);
        }

        m_assembler.autdb(target, lengthGPR);

        if (validateAuth) {
            ASSERT(target != ARM64Registers::sp);
            removeArrayPtrTag(scratch);
            auto isValidPtr = branch64(Equal, scratch, target);
            breakpoint(0xabcd);
            isValidPtr.link(this);
        }
    }

    ALWAYS_INLINE void removeArrayPtrTag(RegisterID target)
    {
        // If we couldn't fit this into a single instruction, we'd be better
        // off emitting two shifts to mask off the top bits.
        ASSERT(LogicalImmediate::create64(nonPACBitsMask).isValid());
        and64(TrustedImmPtr(nonPACBitsMask), target);
    }

    static constexpr RegisterID InvalidGPR  = static_cast<RegisterID>(-1);

    enum class CallSignatureType {
        CFunctionCall,
        OtherCall
    };

    template<CallSignatureType type>
    ALWAYS_INLINE Call callTrustedPtr(RegisterID tagGPR = InvalidGPR)
    {
        ASSERT(tagGPR != dataTempRegister);
        AssemblerLabel pointerLabel = m_assembler.label();
        moveWithFixedWidth(TrustedImmPtr(nullptr), getCachedDataTempRegisterIDAndInvalidate());
        invalidateAllTempRegisters();
        if (type == CallSignatureType::CFunctionCall)
            m_assembler.blraaz(dataTempRegister);
        else
            m_assembler.blrab(dataTempRegister, tagGPR);
        AssemblerLabel callLabel = m_assembler.label();
        ASSERT_UNUSED(pointerLabel, ARM64Assembler::getDifferenceBetweenLabels(callLabel, pointerLabel) == REPATCH_OFFSET_CALL_TO_POINTER);
        return Call(callLabel, Call::Linkable);
    }

    ALWAYS_INLINE Call call(PtrTag tag)
    {
        if (tag == NoPtrTag)
            return MacroAssemblerARM64::call(tag);
        if (tag == CFunctionPtrTag)
            return callTrustedPtr<CallSignatureType::CFunctionCall>();
        move(TrustedImm64(tag), ARM64Registers::lr);
        return callTrustedPtr<CallSignatureType::OtherCall>(ARM64Registers::lr);
    }

    ALWAYS_INLINE Call call(RegisterID tagGPR)
    {
        return callTrustedPtr<CallSignatureType::OtherCall>(tagGPR);
    }

    template<CallSignatureType type>
    ALWAYS_INLINE Call callRegister(RegisterID targetGPR, RegisterID tagGPR = InvalidGPR)
    {
        ASSERT(tagGPR != targetGPR);
        invalidateAllTempRegisters();
        if (type == CallSignatureType::CFunctionCall)
            m_assembler.blraaz(targetGPR);
        else
            m_assembler.blrab(targetGPR, tagGPR);
        return Call(m_assembler.label(), Call::None);
    }

    ALWAYS_INLINE Call call(RegisterID targetGPR, PtrTag tag)
    {
        if (tag == NoPtrTag)
            return MacroAssemblerARM64::call(targetGPR, tag);
        if (tag == CFunctionPtrTag)
            return callRegister<CallSignatureType::CFunctionCall>(targetGPR);
        move(TrustedImm64(tag), ARM64Registers::lr);
        return callRegister<CallSignatureType::OtherCall>(targetGPR, ARM64Registers::lr);
    }

    ALWAYS_INLINE Call call(RegisterID targetGPR, RegisterID tagGPR)
    {
        return callRegister<CallSignatureType::OtherCall>(targetGPR, tagGPR);
    }

    ALWAYS_INLINE Call call(Address address, PtrTag tag)
    {
        if (tag == NoPtrTag)
            return MacroAssemblerARM64::call(address, tag);

        load64(address, getCachedDataTempRegisterIDAndInvalidate());
        return call(dataTempRegister, tag);
    }

    ALWAYS_INLINE Call call(Address address, RegisterID tag)
    {
        ASSERT(tag != dataTempRegister);
        load64(address, getCachedDataTempRegisterIDAndInvalidate());
        return call(dataTempRegister, tag);
    }

    ALWAYS_INLINE void callOperation(const FunctionPtr<OperationPtrTag> operation)
    {
        auto tmp = getCachedDataTempRegisterIDAndInvalidate();
        move(TrustedImmPtr(operation.executableAddress()), tmp);
        call(tmp, OperationPtrTag);
    }

    ALWAYS_INLINE Jump jump() { return MacroAssemblerARM64::jump(); }

    void farJump(RegisterID target, PtrTag tag)
    {
        if (tag == NoPtrTag)
            return MacroAssemblerARM64::farJump(target, tag);

        ASSERT(tag != CFunctionPtrTag);
        RegisterID diversityGPR = getCachedDataTempRegisterIDAndInvalidate();
        move(TrustedImm64(tag), diversityGPR);
        farJump(target, diversityGPR);
    }

    void farJump(RegisterID target, RegisterID tag)
    {
        ASSERT(tag != target);
        m_assembler.brab(target, tag);
    }

    void farJump(Address address, PtrTag tag)
    {
        if (tag == NoPtrTag)
            return MacroAssemblerARM64::farJump(address, tag);

        ASSERT(tag != CFunctionPtrTag);
        RegisterID targetGPR = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID diversityGPR = getCachedMemoryTempRegisterIDAndInvalidate();
        load64(address, targetGPR);
        move(TrustedImm64(tag), diversityGPR);
        m_assembler.brab(targetGPR, diversityGPR);
    }

    void farJump(Address address, RegisterID tag)
    {
        RegisterID targetGPR = getCachedDataTempRegisterIDAndInvalidate();
        ASSERT(tag != targetGPR);
        load64(address, targetGPR);
        m_assembler.brab(targetGPR, tag);
    }

    void farJump(BaseIndex address, PtrTag tag)
    {
        if (tag == NoPtrTag)
            return MacroAssemblerARM64::farJump(address, tag);

        ASSERT(tag != CFunctionPtrTag);
        RegisterID targetGPR = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID diversityGPR = getCachedMemoryTempRegisterIDAndInvalidate();
        load64(address, targetGPR);
        move(TrustedImm64(tag), diversityGPR);
        m_assembler.brab(targetGPR, diversityGPR);
    }

    void farJump(BaseIndex address, RegisterID tag)
    {
        RegisterID targetGPR = getCachedDataTempRegisterIDAndInvalidate();
        ASSERT(tag != targetGPR);
        load64(address, targetGPR);
        m_assembler.brab(targetGPR, tag);
    }

    void farJump(AbsoluteAddress address, PtrTag tag)
    {
        if (tag == NoPtrTag)
            return MacroAssemblerARM64::farJump(address, tag);

        RegisterID targetGPR = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID diversityGPR = getCachedMemoryTempRegisterIDAndInvalidate();
        move(TrustedImmPtr(address.m_ptr), targetGPR);
        load64(Address(targetGPR), targetGPR);
        move(TrustedImm64(tag), diversityGPR);
        m_assembler.brab(targetGPR, diversityGPR);
    }

    void farJump(AbsoluteAddress address, RegisterID tag)
    {
        RegisterID targetGPR = getCachedDataTempRegisterIDAndInvalidate();
        ASSERT(tag != targetGPR);
        move(TrustedImmPtr(address.m_ptr), targetGPR);
        load64(Address(targetGPR), targetGPR);
        m_assembler.brab(targetGPR, tag);
    }

    ALWAYS_INLINE void ret()
    {
        m_assembler.retab();
    }
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM64E)
