/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef MacroAssembler_h
#define MacroAssembler_h

#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER)

#if CPU(ARM_THUMB2)
#include "MacroAssemblerARMv7.h"
namespace JSC { typedef MacroAssemblerARMv7 MacroAssemblerBase; };

#elif CPU(ARM_TRADITIONAL)
#include "MacroAssemblerARM.h"
namespace JSC { typedef MacroAssemblerARM MacroAssemblerBase; };

#elif CPU(X86)
#include "MacroAssemblerX86.h"
namespace JSC { typedef MacroAssemblerX86 MacroAssemblerBase; };

#elif CPU(X86_64)
#include "MacroAssemblerX86_64.h"
namespace JSC { typedef MacroAssemblerX86_64 MacroAssemblerBase; };

#else
#error "The MacroAssembler is not supported on this platform."
#endif


namespace JSC {

class MacroAssembler : public MacroAssemblerBase {
public:

    using MacroAssemblerBase::pop;
    using MacroAssemblerBase::jump;
    using MacroAssemblerBase::branch32;
    using MacroAssemblerBase::branch16;
#if CPU(X86_64)
    using MacroAssemblerBase::branchPtr;
    using MacroAssemblerBase::branchTestPtr;
#endif


    // Platform agnostic onvenience functions,
    // described in terms of other macro assembly methods.
    void pop()
    {
        addPtr(Imm32(sizeof(void*)), stackPointerRegister);
    }
    
    void peek(RegisterID dest, int index = 0)
    {
        loadPtr(Address(stackPointerRegister, (index * sizeof(void*))), dest);
    }

    void poke(RegisterID src, int index = 0)
    {
        storePtr(src, Address(stackPointerRegister, (index * sizeof(void*))));
    }

    void poke(Imm32 value, int index = 0)
    {
        store32(value, Address(stackPointerRegister, (index * sizeof(void*))));
    }

    void poke(ImmPtr imm, int index = 0)
    {
        storePtr(imm, Address(stackPointerRegister, (index * sizeof(void*))));
    }


    // Backwards banches, these are currently all implemented using existing forwards branch mechanisms.
    void branchPtr(Condition cond, RegisterID op1, ImmPtr imm, Label target)
    {
        branchPtr(cond, op1, imm).linkTo(target, this);
    }

    void branch32(Condition cond, RegisterID op1, RegisterID op2, Label target)
    {
        branch32(cond, op1, op2).linkTo(target, this);
    }

    void branch32(Condition cond, RegisterID op1, Imm32 imm, Label target)
    {
        branch32(cond, op1, imm).linkTo(target, this);
    }

    void branch32(Condition cond, RegisterID left, Address right, Label target)
    {
        branch32(cond, left, right).linkTo(target, this);
    }

    void branch16(Condition cond, BaseIndex left, RegisterID right, Label target)
    {
        branch16(cond, left, right).linkTo(target, this);
    }
    
    void branchTestPtr(Condition cond, RegisterID reg, Label target)
    {
        branchTestPtr(cond, reg).linkTo(target, this);
    }

    void jump(Label target)
    {
        jump().linkTo(target, this);
    }


    // Ptr methods
    // On 32-bit platforms (i.e. x86), these methods directly map onto their 32-bit equivalents.
    // FIXME: should this use a test for 32-bitness instead of this specific exception?
#if !CPU(X86_64)
    void addPtr(RegisterID src, RegisterID dest)
    {
        add32(src, dest);
    }

    void addPtr(Imm32 imm, RegisterID srcDest)
    {
        add32(imm, srcDest);
    }

    void addPtr(ImmPtr imm, RegisterID dest)
    {
        add32(Imm32(imm), dest);
    }

    void addPtr(Imm32 imm, RegisterID src, RegisterID dest)
    {
        add32(imm, src, dest);
    }

    void andPtr(RegisterID src, RegisterID dest)
    {
        and32(src, dest);
    }

    void andPtr(Imm32 imm, RegisterID srcDest)
    {
        and32(imm, srcDest);
    }

    void orPtr(RegisterID src, RegisterID dest)
    {
        or32(src, dest);
    }

    void orPtr(ImmPtr imm, RegisterID dest)
    {
        or32(Imm32(imm), dest);
    }

    void orPtr(Imm32 imm, RegisterID dest)
    {
        or32(imm, dest);
    }

    void subPtr(RegisterID src, RegisterID dest)
    {
        sub32(src, dest);
    }
    
    void subPtr(Imm32 imm, RegisterID dest)
    {
        sub32(imm, dest);
    }
    
    void subPtr(ImmPtr imm, RegisterID dest)
    {
        sub32(Imm32(imm), dest);
    }

    void xorPtr(RegisterID src, RegisterID dest)
    {
        xor32(src, dest);
    }

    void xorPtr(Imm32 imm, RegisterID srcDest)
    {
        xor32(imm, srcDest);
    }


    void loadPtr(ImplicitAddress address, RegisterID dest)
    {
        load32(address, dest);
    }

    void loadPtr(BaseIndex address, RegisterID dest)
    {
        load32(address, dest);
    }

    void loadPtr(void* address, RegisterID dest)
    {
        load32(address, dest);
    }

    DataLabel32 loadPtrWithAddressOffsetPatch(Address address, RegisterID dest)
    {
        return load32WithAddressOffsetPatch(address, dest);
    }

    void setPtr(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        set32(cond, left, right, dest);
    }

    void storePtr(RegisterID src, ImplicitAddress address)
    {
        store32(src, address);
    }

    void storePtr(RegisterID src, BaseIndex address)
    {
        store32(src, address);
    }

    void storePtr(RegisterID src, void* address)
    {
        store32(src, address);
    }

    void storePtr(ImmPtr imm, ImplicitAddress address)
    {
        store32(Imm32(imm), address);
    }

    void storePtr(ImmPtr imm, void* address)
    {
        store32(Imm32(imm), address);
    }

    DataLabel32 storePtrWithAddressOffsetPatch(RegisterID src, Address address)
    {
        return store32WithAddressOffsetPatch(src, address);
    }


    Jump branchPtr(Condition cond, RegisterID left, RegisterID right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(Condition cond, RegisterID left, ImmPtr right)
    {
        return branch32(cond, left, Imm32(right));
    }

    Jump branchPtr(Condition cond, RegisterID left, Address right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(Condition cond, Address left, RegisterID right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(Condition cond, AbsoluteAddress left, RegisterID right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(Condition cond, Address left, ImmPtr right)
    {
        return branch32(cond, left, Imm32(right));
    }

    Jump branchPtr(Condition cond, AbsoluteAddress left, ImmPtr right)
    {
        return branch32(cond, left, Imm32(right));
    }

    Jump branchTestPtr(Condition cond, RegisterID reg, RegisterID mask)
    {
        return branchTest32(cond, reg, mask);
    }

    Jump branchTestPtr(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
        return branchTest32(cond, reg, mask);
    }

    Jump branchTestPtr(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        return branchTest32(cond, address, mask);
    }

    Jump branchTestPtr(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        return branchTest32(cond, address, mask);
    }


    Jump branchAddPtr(Condition cond, RegisterID src, RegisterID dest)
    {
        return branchAdd32(cond, src, dest);
    }

    Jump branchSubPtr(Condition cond, Imm32 imm, RegisterID dest)
    {
        return branchSub32(cond, imm, dest);
    }
#endif

};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // MacroAssembler_h
