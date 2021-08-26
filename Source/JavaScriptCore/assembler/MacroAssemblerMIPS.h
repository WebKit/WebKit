/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2010 MIPS Technologies, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MIPS TECHNOLOGIES, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MIPS TECHNOLOGIES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ASSEMBLER) && CPU(MIPS)

#include "MIPSAssembler.h"
#include "AbstractMacroAssembler.h"

namespace JSC {

using Assembler = TARGET_ASSEMBLER;

class MacroAssemblerMIPS : public AbstractMacroAssembler<Assembler> {
public:
    typedef MIPSRegisters::FPRegisterID FPRegisterID;
    static constexpr unsigned numGPRs = 32;
    static constexpr unsigned numFPRs = 32;

    MacroAssemblerMIPS()
        : m_fixedWidth(false)
    {
    }

    static bool isCompactPtrAlignedAddressOffset(ptrdiff_t value)
    {
        return value >= -2147483647 - 1 && value <= 2147483647;
    }

    inline bool isPowerOf2(int32_t v)
    {
        return hasOneBitSet(v);
    }

    inline int bitPosition(int32_t v)
    {
        return getLSBSet(v);
    }

    // For storing immediate number
    static constexpr RegisterID immTempRegister = MIPSRegisters::t0;
    // For storing data loaded from the memory
    static constexpr RegisterID dataTempRegister = MIPSRegisters::t1;
    // For storing address base
    static constexpr RegisterID addrTempRegister = MIPSRegisters::t7;
    // For storing compare result
    static constexpr RegisterID cmpTempRegister = MIPSRegisters::t8;

    // FP temp register
    static constexpr FPRegisterID fpTempRegister = MIPSRegisters::f16;

    RegisterID scratchRegister() { return dataTempRegister; }

    static constexpr int MaximumCompactPtrAlignedAddressOffset = 0x7FFFFFFF;

    enum RelationalCondition {
        Equal,
        NotEqual,
        Above,
        AboveOrEqual,
        Below,
        BelowOrEqual,
        GreaterThan,
        GreaterThanOrEqual,
        LessThan,
        LessThanOrEqual
    };

    enum ResultCondition {
        Overflow,
        Signed,
        PositiveOrZero,
        Zero,
        NonZero
    };

    enum DoubleCondition {
        DoubleEqualAndOrdered,
        DoubleNotEqualAndOrdered,
        DoubleGreaterThanAndOrdered,
        DoubleGreaterThanOrEqualAndOrdered,
        DoubleLessThanAndOrdered,
        DoubleLessThanOrEqualAndOrdered,
        DoubleEqualOrUnordered,
        DoubleNotEqualOrUnordered,
        DoubleGreaterThanOrUnordered,
        DoubleGreaterThanOrEqualOrUnordered,
        DoubleLessThanOrUnordered,
        DoubleLessThanOrEqualOrUnordered
    };

    enum class LoadAddressMode {
        ScaleAndAddOffsetIfOffsetIsOutOfBounds,
        Scale
    };

    static constexpr RegisterID stackPointerRegister = MIPSRegisters::sp;
    static constexpr RegisterID framePointerRegister = MIPSRegisters::fp;
    static constexpr RegisterID returnAddressRegister = MIPSRegisters::ra;

    // Integer arithmetic operations:
    //
    // Operations are typically two operand - operation(source, srcDst)
    // For many operations the source may be an TrustedImm32, the srcDst operand
    // may often be a memory location (explictly described using an Address
    // object).

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.addu(dest, dest, src);
    }

    void add32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.addu(dest, op1, op2);
    }

    void add32(TrustedImm32 imm, RegisterID dest)
    {
        add32(imm, dest, dest);
    }

    void add32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (imm.m_value >= -32768 && imm.m_value <= 32767
            && !m_fixedWidth) {
            /*
              addiu     dest, src, imm
            */
            m_assembler.addiu(dest, src, imm.m_value);
        } else {
            /*
              li        immTemp, imm
              addu      dest, src, immTemp
            */
            move(imm, immTempRegister);
            m_assembler.addu(dest, src, immTempRegister);
        }
    }

    void add32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        add32(imm, src, dest);
    }

    void add32(TrustedImm32 imm, Address address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            /*
              lw        dataTemp, offset(base)
              li        immTemp, imm
              addu      dataTemp, dataTemp, immTemp
              sw        dataTemp, offset(base)
            */
            m_assembler.lw(dataTempRegister, address.base, address.offset);
            if (imm.m_value >= -32768 && imm.m_value <= 32767
                && !m_fixedWidth)
                m_assembler.addiu(dataTempRegister, dataTempRegister, imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.addu(dataTempRegister, dataTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, address.base, address.offset);
        } else {
            /*
              lui       addrTemp, (offset + 0x8000) >> 16
              addu      addrTemp, addrTemp, base
              lw        dataTemp, (offset & 0xffff)(addrTemp)
              li        immtemp, imm
              addu      dataTemp, dataTemp, immTemp
              sw        dataTemp, (offset & 0xffff)(addrTemp)
            */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lw(dataTempRegister, addrTempRegister, address.offset);

            if (imm.m_value >= -32768 && imm.m_value <= 32767 && !m_fixedWidth)
                m_assembler.addiu(dataTempRegister, dataTempRegister, imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.addu(dataTempRegister, dataTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, addrTempRegister, address.offset);
        }
    }

    void add32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        add32(dataTempRegister, dest);
    }

    void add32(AbsoluteAddress src, RegisterID dest)
    {
        load32(src.m_ptr, dataTempRegister);
        add32(dataTempRegister, dest);
    }

    void add32(RegisterID src, Address dest)
    {
        if (dest.offset >= -32768 && dest.offset <= 32767 && !m_fixedWidth) {
            /*
              lw        dataTemp, offset(base)
              addu      dataTemp, dataTemp, src
              sw        dataTemp, offset(base)
            */
            m_assembler.lw(dataTempRegister, dest.base, dest.offset);
            m_assembler.addu(dataTempRegister, dataTempRegister, src);
            m_assembler.sw(dataTempRegister, dest.base, dest.offset);
        } else {
            /*
              lui       addrTemp, (offset + 0x8000) >> 16
              addu      addrTemp, addrTemp, base
              lw        dataTemp, (offset & 0xffff)(addrTemp)
              addu      dataTemp, dataTemp, src
              sw        dataTemp, (offset & 0xffff)(addrTemp)
            */
            m_assembler.lui(addrTempRegister, (dest.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, dest.base);
            m_assembler.lw(dataTempRegister, addrTempRegister, dest.offset);
            m_assembler.addu(dataTempRegister, dataTempRegister, src);
            m_assembler.sw(dataTempRegister, addrTempRegister, dest.offset);
        }
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        if (!m_fixedWidth) {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address.m_ptr);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lw(cmpTempRegister, addrTempRegister, adr & 0xffff);
            if (imm.m_value >= -32768 && imm.m_value <= 32767)
                m_assembler.addiu(dataTempRegister, cmpTempRegister, imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.addu(dataTempRegister, cmpTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
        } else {
            /*
               li   addrTemp, address
               li   immTemp, imm
               lw   cmpTemp, 0(addrTemp)
               addu dataTemp, cmpTemp, immTemp
               sw   dataTemp, 0(addrTemp)
            */
            move(TrustedImmPtr(address.m_ptr), addrTempRegister);
            m_assembler.lw(cmpTempRegister, addrTempRegister, 0);
            move(imm, immTempRegister);
            m_assembler.addu(dataTempRegister, cmpTempRegister, immTempRegister);
            m_assembler.sw(dataTempRegister, addrTempRegister, 0);
        }
    }

    void add64(TrustedImm32 imm, AbsoluteAddress address)
    {
        if (!m_fixedWidth) {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address.m_ptr);
            if ((adr >> 15) == ((adr + 4) >> 15)) {
                m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
                m_assembler.lw(cmpTempRegister, addrTempRegister, adr & 0xffff);
                if (imm.m_value >= -32768 && imm.m_value <= 32767)
                    m_assembler.addiu(dataTempRegister, cmpTempRegister, imm.m_value);
                else {
                    move(imm, immTempRegister);
                    m_assembler.addu(dataTempRegister, cmpTempRegister, immTempRegister);
                }
                m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
                m_assembler.sltu(immTempRegister, dataTempRegister, cmpTempRegister);
                m_assembler.lw(dataTempRegister, addrTempRegister, (adr + 4) & 0xffff);
                if (imm.m_value >> 31)
                    m_assembler.addiu(dataTempRegister, dataTempRegister, -1);
                m_assembler.addu(dataTempRegister, dataTempRegister, immTempRegister);
                m_assembler.sw(dataTempRegister, addrTempRegister, (adr + 4) & 0xffff);
                return;
            }
        }
        /*
            add32(imm, address)
            sltu  immTemp, dataTemp, cmpTemp    # set carry-in bit
            lw    dataTemp, 4(addrTemp)
            addiu dataTemp, imm.m_value >> 31 ? -1 : 0
            addu  dataTemp, dataTemp, immTemp
            sw    dataTemp, 4(addrTemp)
        */
        move(TrustedImmPtr(address.m_ptr), addrTempRegister);
        m_assembler.lw(cmpTempRegister, addrTempRegister, 0);
        if (imm.m_value >= -32768 && imm.m_value <= 32767 && !m_fixedWidth)
            m_assembler.addiu(dataTempRegister, cmpTempRegister, imm.m_value);
        else {
            move(imm, immTempRegister);
            m_assembler.addu(dataTempRegister, cmpTempRegister, immTempRegister);
        }
        m_assembler.sw(dataTempRegister, addrTempRegister, 0);
        m_assembler.sltu(immTempRegister, dataTempRegister, cmpTempRegister);
        m_assembler.lw(dataTempRegister, addrTempRegister, 4);
        if (imm.m_value >> 31)
            m_assembler.addiu(dataTempRegister, dataTempRegister, -1);
        m_assembler.addu(dataTempRegister, dataTempRegister, immTempRegister);
        m_assembler.sw(dataTempRegister, addrTempRegister, 4);
    }

    void loadAddress(BaseIndex address, LoadAddressMode mode)
    {
        if (mode == LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds) {
            if (!address.scale)
                m_assembler.addu(addrTempRegister, address.index, address.base);
            else {
                m_assembler.sll(addrTempRegister, address.index, address.scale);
                m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            }
            if (address.offset < -32768 || address.offset > 32767) {
                m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
                m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            }
        } else {
            if (!address.scale)
                m_assembler.addu(addrTempRegister, address.index, address.base);
            else {
                m_assembler.sll(addrTempRegister, address.index, address.scale);
                m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            }
        }
    }

    void getEffectiveAddress(BaseIndex address, RegisterID dest)
    {
        if (!address.scale && !m_fixedWidth)
            m_assembler.addu(dest, address.index, address.base);
        else {
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(dest, addrTempRegister, address.base);
        }
        if (address.offset)
            add32(TrustedImm32(address.offset), dest);
    }

    void and16(Address src, RegisterID dest)
    {
        load16(src, dataTempRegister);
        and32(dataTempRegister, dest);
    }

    void and32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        and32(dataTempRegister, dest);
    }

    void and32(RegisterID src, RegisterID dest)
    {
        m_assembler.andInsn(dest, dest, src);
    }

    void and32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.andInsn(dest, op1, op2);
    }

    void and32(TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            move(MIPSRegisters::zero, dest);
        else if (imm.m_value > 0 && imm.m_value <= 65535 && !m_fixedWidth)
            m_assembler.andi(dest, dest, imm.m_value);
        else {
            /*
              li        immTemp, imm
              and       dest, dest, immTemp
            */
            move(imm, immTempRegister);
            m_assembler.andInsn(dest, dest, immTempRegister);
        }
    }

    void and32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            move(MIPSRegisters::zero, dest);
        else if (imm.m_value > 0 && imm.m_value <= 65535 && !m_fixedWidth)
            m_assembler.andi(dest, src, imm.m_value);
        else {
            move(imm, immTempRegister);
            m_assembler.andInsn(dest, src, immTempRegister);
        }
    }

    void countLeadingZeros32(RegisterID src, RegisterID dest)
    {
#if WTF_MIPS_ISA_AT_LEAST(32)
        m_assembler.clz(dest, src);
#else
        static_assert(false, "CLZ opcode is not available for this ISA");
#endif
    }

    void lshift32(RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.sllv(dest, dest, shiftAmount);
    }

    void lshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.sllv(dest, src, shiftAmount);
    }

    void lshift32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.sll(dest, dest, imm.m_value);
    }

    void lshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.sll(dest, src, imm.m_value);
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        m_assembler.mul(dest, dest, src);
    }

    void mul32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.mul(dest, op1, op2);
    }

    void mul32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            move(MIPSRegisters::zero, dest);
        else if (imm.m_value == 1 && !m_fixedWidth)
            move(src, dest);
        else {
            /*
                li      dataTemp, imm
                mul     dest, src, dataTemp
            */
            move(imm, dataTempRegister);
            m_assembler.mul(dest, src, dataTempRegister);
        }
    }

    void neg32(RegisterID srcDest)
    {
        m_assembler.subu(srcDest, MIPSRegisters::zero, srcDest);
    }

    void neg32(RegisterID src, RegisterID dest)
    {
        m_assembler.subu(dest, MIPSRegisters::zero, src);
    }


    void or8(TrustedImm32 imm, AbsoluteAddress dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            return;

        if (m_fixedWidth) {
            load8(dest.m_ptr, immTempRegister);
            or32(imm, immTempRegister);
            store8(immTempRegister, dest.m_ptr);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(dest.m_ptr);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lbu(immTempRegister, addrTempRegister, adr & 0xffff);
            or32(imm, immTempRegister);
            m_assembler.sb(immTempRegister, addrTempRegister, adr & 0xffff);            
        }
    }
    
    void or16(TrustedImm32 imm, AbsoluteAddress dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            return;

        if (m_fixedWidth) {
            // TODO: Swap dataTempRegister and immTempRegister usage
            load16(dest.m_ptr, immTempRegister);
            or32(imm, immTempRegister);
            store16(immTempRegister, dest.m_ptr);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(dest.m_ptr);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lhu(immTempRegister, addrTempRegister, adr & 0xffff);
            or32(imm, immTempRegister);
            m_assembler.sh(immTempRegister, addrTempRegister, adr & 0xffff);
        }
    }

    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orInsn(dest, dest, src);
    }

    void or32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.orInsn(dest, op1, op2);
    }

    void or32(TrustedImm32 imm, AbsoluteAddress dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            return;

        if (m_fixedWidth) {
            // TODO: Swap dataTempRegister and immTempRegister usage
            load32(dest.m_ptr, immTempRegister);
            or32(imm, immTempRegister);
            store32(immTempRegister, dest.m_ptr);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(dest.m_ptr);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lw(immTempRegister, addrTempRegister, adr & 0xffff);
            or32(imm, immTempRegister);
            m_assembler.sw(immTempRegister, addrTempRegister, adr & 0xffff);
        }
    }

    void or32(TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            return;

        if (imm.m_value > 0 && imm.m_value <= 65535
            && !m_fixedWidth) {
            m_assembler.ori(dest, dest, imm.m_value);
            return;
        }

        /*
            li      dataTemp, imm
            or      dest, dest, dataTemp
        */
        move(imm, dataTempRegister);
        m_assembler.orInsn(dest, dest, dataTempRegister);
    }

    void or32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (!imm.m_value && !m_fixedWidth) {
            move(src, dest);
            return;
        }

        if (imm.m_value > 0 && imm.m_value <= 65535 && !m_fixedWidth) {
            m_assembler.ori(dest, src, imm.m_value);
            return;
        }

        /*
            li      dataTemp, imm
            or      dest, src, dataTemp
        */
        move(imm, dataTempRegister);
        m_assembler.orInsn(dest, src, dataTempRegister);
    }

    void or32(RegisterID src, AbsoluteAddress dest)
    {
        if (m_fixedWidth) {
            load32(dest.m_ptr, dataTempRegister);
            m_assembler.orInsn(dataTempRegister, dataTempRegister, src);
            store32(dataTempRegister, dest.m_ptr);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(dest.m_ptr);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lw(dataTempRegister, addrTempRegister, adr & 0xffff);
            m_assembler.orInsn(dataTempRegister, dataTempRegister, src);
            m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
        }
    }

    void or32(TrustedImm32 imm, Address address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            /*
              lw        dataTemp, offset(base)
              li        immTemp, imm
              or        dataTemp, dataTemp, immTemp
              sw        dataTemp, offset(base)
            */
            m_assembler.lw(dataTempRegister, address.base, address.offset);
            if (imm.m_value >= 0 && imm.m_value <= 65535 && !m_fixedWidth)
                m_assembler.ori(dataTempRegister, dataTempRegister, imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.orInsn(dataTempRegister, dataTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, address.base, address.offset);
        } else {
            /*
              lui       addrTemp, (offset + 0x8000) >> 16
              addu      addrTemp, addrTemp, base
              lw        dataTemp, (offset & 0xffff)(addrTemp)
              li        immtemp, imm
              or        dataTemp, dataTemp, immTemp
              sw        dataTemp, (offset & 0xffff)(addrTemp)
            */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lw(dataTempRegister, addrTempRegister, address.offset);

            if (imm.m_value >= 0 && imm.m_value <= 65535 && !m_fixedWidth)
                m_assembler.ori(dataTempRegister, dataTempRegister, imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.orInsn(dataTempRegister, dataTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, addrTempRegister, address.offset);
        }
    }

    // This is only referenced by code intented for ARM64_32.
    void rotateRight32(TrustedImm32, RegisterID)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    void rshift32(RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srav(dest, dest, shiftAmount);
    }

    void rshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srav(dest, src, shiftAmount);
    }

    void rshift32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.sra(dest, dest, imm.m_value);
    }

    void rshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.sra(dest, src, imm.m_value);
    }

    void urshift32(RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srlv(dest, dest, shiftAmount);
    }

    void urshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srlv(dest, src, shiftAmount);
    }

    void urshift32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.srl(dest, dest, imm.m_value);
    }

    void urshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.srl(dest, src, imm.m_value);
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        m_assembler.subu(dest, dest, src);
    }

    void sub32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.subu(dest, op1, op2);
    }

    void sub32(TrustedImm32 imm, RegisterID dest)
    {
        if (imm.m_value >= -32767 && imm.m_value <= 32768
            && !m_fixedWidth) {
            /*
              addiu     dest, src, imm
            */
            m_assembler.addiu(dest, dest, -imm.m_value);
        } else {
            /*
              li        immTemp, imm
              subu      dest, src, immTemp
            */
            move(imm, immTempRegister);
            m_assembler.subu(dest, dest, immTempRegister);
        }
    }

    void sub32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (imm.m_value >= -32767 && imm.m_value <= 32768
            && !m_fixedWidth) {
            /*
              addiu     dest, src, imm
            */
            m_assembler.addiu(dest, src, -imm.m_value);
        } else {
            /*
              li        immTemp, imm
              subu      dest, src, immTemp
            */
            move(imm, immTempRegister);
            m_assembler.subu(dest, src, immTempRegister);
        }
    }

    void sub32(TrustedImm32 imm, Address address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            /*
              lw        dataTemp, offset(base)
              li        immTemp, imm
              subu      dataTemp, dataTemp, immTemp
              sw        dataTemp, offset(base)
            */
            m_assembler.lw(dataTempRegister, address.base, address.offset);
            if (imm.m_value >= -32767 && imm.m_value <= 32768 && !m_fixedWidth)
                m_assembler.addiu(dataTempRegister, dataTempRegister, -imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.subu(dataTempRegister, dataTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, address.base, address.offset);
        } else {
            /*
              lui       addrTemp, (offset + 0x8000) >> 16
              addu      addrTemp, addrTemp, base
              lw        dataTemp, (offset & 0xffff)(addrTemp)
              li        immtemp, imm
              subu      dataTemp, dataTemp, immTemp
              sw        dataTemp, (offset & 0xffff)(addrTemp)
            */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lw(dataTempRegister, addrTempRegister, address.offset);

            if (imm.m_value >= -32767 && imm.m_value <= 32768
                && !m_fixedWidth)
                m_assembler.addiu(dataTempRegister, dataTempRegister, -imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.subu(dataTempRegister, dataTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, addrTempRegister, address.offset);
        }
    }

    void sub32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        sub32(dataTempRegister, dest);
    }

    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        if (!m_fixedWidth) {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address.m_ptr);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lw(cmpTempRegister, addrTempRegister, adr & 0xffff);
            if (imm.m_value >= -32767 && imm.m_value <= 32768)
                m_assembler.addiu(dataTempRegister, cmpTempRegister, -imm.m_value);
            else {
                move(imm, immTempRegister);
                m_assembler.subu(dataTempRegister, cmpTempRegister, immTempRegister);
            }
            m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
        } else {
            /*
               li   addrTemp, address
               lw   dataTemp, 0(addrTemp)
               li   immTemp, imm
               subu dataTemp, dataTemp, immTemp
               sw   dataTemp, 0(addrTemp)
            */
            move(TrustedImmPtr(address.m_ptr), addrTempRegister);
            m_assembler.lw(cmpTempRegister, addrTempRegister, 0);
            move(imm, immTempRegister);
            m_assembler.subu(dataTempRegister, cmpTempRegister, immTempRegister);
            m_assembler.sw(dataTempRegister, addrTempRegister, 0);
        }
    }

    void xor32(RegisterID src, RegisterID dest)
    {
        m_assembler.xorInsn(dest, dest, src);
    }

    void xor32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.xorInsn(dest, op1, op2);
    }

    void xor32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        xor32(dataTempRegister, dest);
    }

    void xor32(TrustedImm32 imm, RegisterID dest)
    {
        if (!m_fixedWidth) {
            if (imm.m_value == -1) {
                m_assembler.nor(dest, dest, MIPSRegisters::zero);
                return;
            }
            if (imm.m_value >= 0 && imm.m_value <= 65535) {
                m_assembler.xori(dest, dest, imm.m_value);
                return;
            }
        }
        /*
            li  immTemp, imm
            xor dest, dest, immTemp
        */
        move(imm, immTempRegister);
        m_assembler.xorInsn(dest, dest, immTempRegister);
    }

    void xor32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (!m_fixedWidth) {
            if (imm.m_value == -1) {
                m_assembler.nor(dest, src, MIPSRegisters::zero);
                return;
            }
            if (imm.m_value >= 0 && imm.m_value <= 65535) {
                m_assembler.xori(dest, src, imm.m_value);
                return;
            }
        }
        /*
            li  immTemp, imm
            xor dest, src, immTemp
        */
        move(imm, immTempRegister);
        m_assembler.xorInsn(dest, src, immTempRegister);
    }

    void not32(RegisterID srcDest)
    {
        m_assembler.nor(srcDest, srcDest, MIPSRegisters::zero);
    }

    void sqrtDouble(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.sqrtd(dst, src);
    }

    void absDouble(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.absd(dst, src);
    }

    NO_RETURN_DUE_TO_CRASH void ceilDouble(FPRegisterID, FPRegisterID)
    {
        ASSERT(!supportsFloatingPointRounding());
        CRASH();
    }

    NO_RETURN_DUE_TO_CRASH void floorDouble(FPRegisterID, FPRegisterID)
    {
        ASSERT(!supportsFloatingPointRounding());
        CRASH();
    }

    NO_RETURN_DUE_TO_CRASH void roundTowardZeroDouble(FPRegisterID, FPRegisterID)
    {
        ASSERT(!supportsFloatingPointRounding());
        CRASH();
    }

    ConvertibleLoadLabel convertibleLoadPtr(Address address, RegisterID dest)
    {
        ConvertibleLoadLabel result(this);
        /*
            lui     addrTemp, (offset + 0x8000) >> 16
            addu    addrTemp, addrTemp, base
            lw      dest, (offset & 0xffff)(addrTemp)
        */
        m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
        m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
        m_assembler.lw(dest, addrTempRegister, address.offset);
        return result;
    }

    // Memory access operations:
    //
    // Loads are of the form load(address, destination) and stores of the form
    // store(source, address). The source for a store may be an TrustedImm32. Address
    // operand objects to loads and store will be implicitly constructed if a
    // register is passed.

    /* Need to use zero-extened load byte for load8.  */
    void load8(ImplicitAddress address, RegisterID dest)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.lbu(dest, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                lbu     dest, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lbu(dest, addrTempRegister, address.offset);
        }
    }

    void load8(BaseIndex address, RegisterID dest)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.lbu(dest, addrTempRegister, address.offset);
        } else {
            /*
             sll     addrTemp, address.index, address.scale
             addu    addrTemp, addrTemp, address.base
             lui     immTemp, (address.offset + 0x8000) >> 16
             addu    addrTemp, addrTemp, immTemp
             lbu     dest, (address.offset & 0xffff)(at)
             */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.lbu(dest, addrTempRegister, address.offset);
        }
    }

    ALWAYS_INLINE void load8(AbsoluteAddress address, RegisterID dest)
    {
        load8(address.m_ptr, dest);
    }

    void load8(const void* address, RegisterID dest)
    {
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                lbu dest, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.lbu(dest, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lbu(dest, addrTempRegister, adr & 0xffff);
        }
    }

    void load8SignedExtendTo32(ImplicitAddress address, RegisterID dest)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.lb(dest, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                lb      dest, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lb(dest, addrTempRegister, address.offset);
        }
    }

    void load8SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.lb(dest, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                lb     dest, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.lb(dest, addrTempRegister, address.offset);
        }
    }

    ALWAYS_INLINE void load8SignedExtendTo32(AbsoluteAddress address, RegisterID dest)
    {
        load8SignedExtendTo32(address.m_ptr, dest);
    }

    void load8SignedExtendTo32(const void* address, RegisterID dest)
    {
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                lb dest, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.lb(dest, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lb(dest, addrTempRegister, adr & 0xffff);
        }
    }


    void load32(ImplicitAddress address, RegisterID dest)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.lw(dest, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                lw      dest, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lw(dest, addrTempRegister, address.offset);
        }
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.lw(dest, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                lw      dest, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.lw(dest, addrTempRegister, address.offset);
        }
    }

    void load16Unaligned(BaseIndex address, RegisterID dest)
    {
        if (address.offset >= -32768 && address.offset <= 32767 && !m_fixedWidth) {
            /*
                sll     addrtemp, address.index, address.scale
                addu    addrtemp, addrtemp, address.base
                lbu     immTemp, address.offset+x(addrtemp) (x=0 for LE, x=1 for BE)
                lbu     dest, address.offset+x(addrtemp)    (x=1 for LE, x=0 for BE)
                sll     dest, dest, 8
                or      dest, dest, immTemp
            */
            loadAddress(address, LoadAddressMode::Scale);
#if CPU(BIG_ENDIAN)
            m_assembler.lbu(immTempRegister, addrTempRegister, address.offset + 1);
            m_assembler.lbu(dest, addrTempRegister, address.offset);
#else
            m_assembler.lbu(immTempRegister, addrTempRegister, address.offset);
            m_assembler.lbu(dest, addrTempRegister, address.offset + 1);
#endif
            m_assembler.sll(dest, dest, 8);
            m_assembler.orInsn(dest, dest, immTempRegister);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, address.offset >> 16
                ori     immTemp, immTemp, address.offset & 0xffff
                addu    addrTemp, addrTemp, immTemp
                lbu     immTemp, x(addrtemp) (x=0 for LE, x=1 for BE)
                lbu     dest, x(addrtemp)    (x=1 for LE, x=0 for BE)
                sll     dest, dest, 8
                or      dest, dest, immTemp
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, address.offset >> 16);
            m_assembler.ori(immTempRegister, immTempRegister, address.offset);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
#if CPU(BIG_ENDIAN)
            m_assembler.lbu(immTempRegister, addrTempRegister, 1);
            m_assembler.lbu(dest, addrTempRegister, 0);
#else
            m_assembler.lbu(immTempRegister, addrTempRegister, 0);
            m_assembler.lbu(dest, addrTempRegister, 1);
#endif
            m_assembler.sll(dest, dest, 8);
            m_assembler.orInsn(dest, dest, immTempRegister);
        }
    }

    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
    {
        if (address.offset >= -32768 && address.offset <= 32764
            && !m_fixedWidth) {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                (Big-Endian)
                lwl     dest, address.offset(addrTemp)
                lwr     dest, address.offset+3(addrTemp)
                (Little-Endian)
                lwl     dest, address.offset+3(addrTemp)
                lwr     dest, address.offset(addrTemp)
            */
            loadAddress(address, LoadAddressMode::Scale);
#if CPU(BIG_ENDIAN)
            m_assembler.lwl(dest, addrTempRegister, address.offset);
            m_assembler.lwr(dest, addrTempRegister, address.offset + 3);
#else
            m_assembler.lwl(dest, addrTempRegister, address.offset + 3);
            m_assembler.lwr(dest, addrTempRegister, address.offset);

#endif
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, address.offset >> 16
                ori     immTemp, immTemp, address.offset & 0xffff
                addu    addrTemp, addrTemp, immTemp
                (Big-Endian)
                lw      dest, 0(at)
                lw      dest, 3(at)
                (Little-Endian)
                lw      dest, 3(at)
                lw      dest, 0(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, address.offset >> 16);
            m_assembler.ori(immTempRegister, immTempRegister, address.offset);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
#if CPU(BIG_ENDIAN)
            m_assembler.lwl(dest, addrTempRegister, 0);
            m_assembler.lwr(dest, addrTempRegister, 3);
#else
            m_assembler.lwl(dest, addrTempRegister, 3);
            m_assembler.lwr(dest, addrTempRegister, 0);
#endif
        }
    }

    void load32(const void* address, RegisterID dest)
    {
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                lw  dest, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.lw(dest, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lw(dest, addrTempRegister, adr & 0xffff);
        }
    }

    DataLabel32 load32WithAddressOffsetPatch(Address address, RegisterID dest)
    {
        m_fixedWidth = true;
        /*
            lui addrTemp, address.offset >> 16
            ori addrTemp, addrTemp, address.offset & 0xffff
            addu        addrTemp, addrTemp, address.base
            lw  dest, 0(addrTemp)
        */
        DataLabel32 dataLabel(this);
        move(TrustedImm32(address.offset), addrTempRegister);
        m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
        m_assembler.lw(dest, addrTempRegister, 0);
        m_fixedWidth = false;
        return dataLabel;
    }
    
    DataLabelCompact load32WithCompactAddressOffsetPatch(Address address, RegisterID dest)
    {
        DataLabelCompact dataLabel(this);
        load32WithAddressOffsetPatch(address, dest);
        return dataLabel;
    }

    void load16(const void* address, RegisterID dest)
    {
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                lhu  dest, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.lhu(dest, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lhu(dest, addrTempRegister, adr & 0xffff);
        }
    }

    /* Need to use zero-extened load half-word for load16.  */
    void load16(ImplicitAddress address, RegisterID dest)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.lhu(dest, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                lhu     dest, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lhu(dest, addrTempRegister, address.offset);
        }
    }

    /* Need to use zero-extened load half-word for load16.  */
    void load16(BaseIndex address, RegisterID dest)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.lhu(dest, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                lhu     dest, (address.offset & 0xffff)(addrTemp)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.lhu(dest, addrTempRegister, address.offset);
        }
    }

    void load16SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.lh(dest, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                lh     dest, (address.offset & 0xffff)(addrTemp)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.lh(dest, addrTempRegister, address.offset);
        }
    }

    void loadPair32(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src, TrustedImm32(0), dest1, dest2);
    }

    void loadPair32(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        ASSERT(dest1 != dest2); // If it is the same, ldp becomes illegal instruction.
        if (src == dest1) {
            load32(Address(src, offset.m_value + 4), dest2);
            load32(Address(src, offset.m_value), dest1);
        } else {
            load32(Address(src, offset.m_value), dest1);
            load32(Address(src, offset.m_value + 4), dest2);
        }
    }

    DataLabel32 store32WithAddressOffsetPatch(RegisterID src, Address address)
    {
        m_fixedWidth = true;
        /*
            lui addrTemp, address.offset >> 16
            ori addrTemp, addrTemp, address.offset & 0xffff
            addu        addrTemp, addrTemp, address.base
            sw  src, 0(addrTemp)
        */
        DataLabel32 dataLabel(this);
        move(TrustedImm32(address.offset), addrTempRegister);
        m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
        m_assembler.sw(src, addrTempRegister, 0);
        m_fixedWidth = false;
        return dataLabel;
    }

    void store8(RegisterID src, ImplicitAddress address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.sb(src, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                sb      src, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.sb(src, addrTempRegister, address.offset);
        }
    }

    void store8(RegisterID src, BaseIndex address)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.sb(src, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                sb      src, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.sb(src, addrTempRegister, address.offset);
        }
    }

    void store8(RegisterID src, const void* address)
    {
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                sb  src, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.sb(src, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.sb(src, addrTempRegister, adr & 0xffff);
        }
    }

    void store8(TrustedImm32 imm, void* address)
    {
        if (m_fixedWidth) {
            /*
                li  immTemp, imm
                li  addrTemp, address
                sb  src, 0(addrTemp)
            */
            TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
            move(imm8, immTempRegister);
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.sb(immTempRegister, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            if (!imm.m_value)
                m_assembler.sb(MIPSRegisters::zero, addrTempRegister, adr & 0xffff);
            else {
                TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
                move(imm8, immTempRegister);
                m_assembler.sb(immTempRegister, addrTempRegister, adr & 0xffff);
            }
        }
    }

    void store8(TrustedImm32 imm, ImplicitAddress address)
    {
        TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            if (!imm8.m_value)
                m_assembler.sb(MIPSRegisters::zero, address.base, address.offset);
            else {
                move(imm8, immTempRegister);
                m_assembler.sb(immTempRegister, address.base, address.offset);
            }
        } else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                sb      immTemp, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            if (!imm8.m_value && !m_fixedWidth)
                m_assembler.sb(MIPSRegisters::zero, addrTempRegister, address.offset);
            else {
                move(imm8, immTempRegister);
                m_assembler.sb(immTempRegister, addrTempRegister, address.offset);
            }
        }
    }

    void store16(RegisterID src, const void* address)
    {
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                sh  src, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.sh(src, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.sh(src, addrTempRegister, adr & 0xffff);
        }
    }

    void store16(RegisterID src, ImplicitAddress address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            m_assembler.sh(src, address.base, address.offset);
        } else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                sh      src, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.sh(src, addrTempRegister, address.offset);
        }
    }

    void store16(RegisterID src, BaseIndex address)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.sh(src, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                sh      src, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.sh(src, addrTempRegister, address.offset);
        }
    }

    void store32(RegisterID src, ImplicitAddress address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.sw(src, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                sw      src, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.sw(src, addrTempRegister, address.offset);
        }
    }

    void store32(RegisterID src, BaseIndex address)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.sw(src, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                sw      src, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.sw(src, addrTempRegister, address.offset);
        }
    }

    void store32(TrustedImm32 imm, ImplicitAddress address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            if (!imm.m_value)
                m_assembler.sw(MIPSRegisters::zero, address.base, address.offset);
            else {
                move(imm, immTempRegister);
                m_assembler.sw(immTempRegister, address.base, address.offset);
            }
        } else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                sw      immTemp, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            if (!imm.m_value && !m_fixedWidth)
                m_assembler.sw(MIPSRegisters::zero, addrTempRegister, address.offset);
            else {
                move(imm, immTempRegister);
                m_assembler.sw(immTempRegister, addrTempRegister, address.offset);
            }
        }
    }

    void store32(TrustedImm32 imm, BaseIndex address)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            if (!imm.m_value)
                m_assembler.sw(MIPSRegisters::zero, addrTempRegister, address.offset);
            else {
                move(imm, immTempRegister);
                m_assembler.sw(immTempRegister, addrTempRegister, address.offset);
            }
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                sw      src, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            move(imm, immTempRegister);
            m_assembler.sw(immTempRegister, addrTempRegister, address.offset);
        }
    }


    void store32(RegisterID src, const void* address)
    {
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                sw  src, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.sw(src, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.sw(src, addrTempRegister, adr & 0xffff);
        }
    }

    void store32(TrustedImm32 imm, const void* address)
    {
        if (m_fixedWidth) {
            /*
                li  immTemp, imm
                li  addrTemp, address
                sw  src, 0(addrTemp)
            */
            move(imm, immTempRegister);
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.sw(immTempRegister, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            if (!imm.m_value)
                m_assembler.sw(MIPSRegisters::zero, addrTempRegister, adr & 0xffff);
            else {
                move(imm, immTempRegister);
                m_assembler.sw(immTempRegister, addrTempRegister, adr & 0xffff);
            }
        }
    }

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        storePair32(src1, src2, dest, TrustedImm32(0));
    }

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        store32(src1, Address(dest, offset.m_value));
        store32(src2, Address(dest, offset.m_value + 4));
    }

    // Floating-point operations:

    static bool supportsFloatingPoint()
    {
#if WTF_MIPS_DOUBLE_FLOAT
        return true;
#else
        return false;
#endif
    }

    static bool supportsFloatingPointTruncate()
    {
#if WTF_MIPS_DOUBLE_FLOAT && WTF_MIPS_ISA_AT_LEAST(2)
        return true;
#else
        return false;
#endif
    }

    static bool supportsFloatingPointSqrt()
    {
#if WTF_MIPS_DOUBLE_FLOAT && WTF_MIPS_ISA_AT_LEAST(2)
        return true;
#else
        return false;
#endif
    }

    static bool supportsFloatingPointAbs()
    {
#if WTF_MIPS_DOUBLE_FLOAT && WTF_MIPS_ISA_AT_LEAST(2)
        return true;
#else
        return false;
#endif
    }

    static bool supportsFloatingPointRounding() { return false; }

    // Stack manipulation operations:
    //
    // The ABI is assumed to provide a stack abstraction to memory,
    // containing machine word sized units of data. Push and pop
    // operations add and remove a single register sized unit of data
    // to or from the stack. Peek and poke operations read or write
    // values on the stack, without moving the current stack position.

    void pop(RegisterID dest)
    {
        m_assembler.lw(dest, MIPSRegisters::sp, 0);
        m_assembler.addiu(MIPSRegisters::sp, MIPSRegisters::sp, 4);
    }

    void popPair(RegisterID dest1, RegisterID dest2)
    {
        m_assembler.lw(dest1, MIPSRegisters::sp, 0);
        m_assembler.lw(dest2, MIPSRegisters::sp, 4);
        m_assembler.addiu(MIPSRegisters::sp, MIPSRegisters::sp, 8);
    }

    void push(RegisterID src)
    {
        m_assembler.addiu(MIPSRegisters::sp, MIPSRegisters::sp, -4);
        m_assembler.sw(src, MIPSRegisters::sp, 0);
    }

    void push(Address address)
    {
        load32(address, dataTempRegister);
        push(dataTempRegister);
    }

    void push(TrustedImm32 imm)
    {
        move(imm, immTempRegister);
        push(immTempRegister);
    }

    void pushPair(RegisterID src1, RegisterID src2)
    {
        m_assembler.addiu(MIPSRegisters::sp, MIPSRegisters::sp, -8);
        m_assembler.sw(src2, MIPSRegisters::sp, 4);
        m_assembler.sw(src1, MIPSRegisters::sp, 0);
    }

    // Register move operations:
    //
    // Move values in registers.

    void move(TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value && !m_fixedWidth)
            move(MIPSRegisters::zero, dest);
        else if (m_fixedWidth) {
            m_assembler.lui(dest, imm.m_value >> 16);
            m_assembler.ori(dest, dest, imm.m_value);
        } else
            m_assembler.li(dest, imm.m_value);
    }

    void move(RegisterID src, RegisterID dest)
    {
        if (src != dest || m_fixedWidth)
            m_assembler.move(dest, src);
    }

    void move(TrustedImmPtr imm, RegisterID dest)
    {
        move(TrustedImm32(imm), dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        move(reg1, immTempRegister);
        move(reg2, reg1);
        move(immTempRegister, reg2);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest || m_fixedWidth)
            move(src, dest);
    }

    void zeroExtend32ToWord(RegisterID src, RegisterID dest)
    {
        if (src != dest || m_fixedWidth)
            move(src, dest);
    }

    // Forwards / external control flow operations:
    //
    // This set of jump and conditional branch operations return a Jump
    // object which may linked at a later point, allow forwards jump,
    // or jumps that will require external linkage (after the code has been
    // relocated).
    //
    // For branches, signed <, >, <= and >= are denoted as l, g, le, and ge
    // respecitvely, for unsigned comparisons the names b, a, be, and ae are
    // used (representing the names 'below' and 'above').
    //
    // Operands to the comparision are provided in the expected order, e.g.
    // jle32(reg1, TrustedImm32(5)) will branch if the value held in reg1, when
    // treated as a signed 32bit value, is less than or equal to 5.
    //
    // jz and jnz test whether the first operand is equal to zero, and take
    // an optional second operand of a mask under which to perform the test.

    Jump branch8(RelationalCondition cond, Address left, TrustedImm32 right)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, dataTempRegister);
        return branch32(cond, dataTempRegister, right8);
    }

    Jump branch8(RelationalCondition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, dataTempRegister);
        return branch32(cond, dataTempRegister, right8);
    }

    void compare8(RelationalCondition cond, Address left, TrustedImm32 right, RegisterID dest)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, dataTempRegister);
        compare32(cond, dataTempRegister, right8, dest);
    }

    Jump branch8(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, dataTempRegister);
        return branch32(cond, dataTempRegister, right8);
    }

    Jump branchPtr(RelationalCondition cond, BaseIndex left, RegisterID right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, RegisterID left, RegisterID right)
    {
        if (cond == Equal)
            return branchEqual(left, right);
        if (cond == NotEqual)
            return branchNotEqual(left, right);
        if (cond == Above) {
            m_assembler.sltu(cmpTempRegister, right, left);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == AboveOrEqual) {
            m_assembler.sltu(cmpTempRegister, left, right);
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Below) {
            m_assembler.sltu(cmpTempRegister, left, right);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == BelowOrEqual) {
            m_assembler.sltu(cmpTempRegister, right, left);
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == GreaterThan) {
            m_assembler.slt(cmpTempRegister, right, left);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == GreaterThanOrEqual) {
            m_assembler.slt(cmpTempRegister, left, right);
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == LessThan) {
            m_assembler.slt(cmpTempRegister, left, right);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == LessThanOrEqual) {
            m_assembler.slt(cmpTempRegister, right, left);
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        ASSERT(0);

        return Jump();
    }

    Jump branch32(RelationalCondition cond, RegisterID left, TrustedImm32 right)
    {
        if (!m_fixedWidth) {
            if (!right.m_value)
                return branch32(cond, left, MIPSRegisters::zero);
            if (right.m_value >= -32768 && right.m_value <= 32767) {
                if (cond == AboveOrEqual) {
                    m_assembler.sltiu(cmpTempRegister, left, right.m_value);
                    return branchEqual(cmpTempRegister, MIPSRegisters::zero);
                }
                if (cond == Below) {
                    m_assembler.sltiu(cmpTempRegister, left, right.m_value);
                    return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
                }
                if (cond == GreaterThanOrEqual) {
                    m_assembler.slti(cmpTempRegister, left, right.m_value);
                    return branchEqual(cmpTempRegister, MIPSRegisters::zero);
                }
                if (cond == LessThan) {
                    m_assembler.slti(cmpTempRegister, left, right.m_value);
                    return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
                }
            }
        }
        move(right, immTempRegister);
        return branch32(cond, left, immTempRegister);
    }

    Jump branch32(RelationalCondition cond, RegisterID left, Address right)
    {
        load32(right, dataTempRegister);
        return branch32(cond, left, dataTempRegister);
    }

    Jump branch32(RelationalCondition cond, Address left, RegisterID right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, Address left, TrustedImm32 right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32WithUnalignedHalfWords(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        load32WithUnalignedHalfWords(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, RegisterID right)
    {
        load32(left.m_ptr, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        load32(left.m_ptr, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branchTest32(ResultCondition cond, RegisterID reg, RegisterID mask)
    {
        ASSERT((cond == Zero) || (cond == NonZero) || (cond == Signed));
        m_assembler.andInsn(cmpTempRegister, reg, mask);
        switch (cond) {
        case Zero:
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        case NonZero:
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        case Signed:
            m_assembler.slt(cmpTempRegister, cmpTempRegister, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    Jump branchTest32(ResultCondition cond, RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero) || (cond == Signed));
        if (!m_fixedWidth) {
            if (mask.m_value == -1) {
                switch (cond) {
                case Zero:
                    return branchEqual(reg, MIPSRegisters::zero);
                case NonZero:
                    return branchNotEqual(reg, MIPSRegisters::zero);
                case Signed:
                    m_assembler.slt(cmpTempRegister, reg, MIPSRegisters::zero);
                    return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
#if WTF_MIPS_ISA_REV(2)
            if (isPowerOf2(mask.m_value)) {
                uint16_t pos= bitPosition(mask.m_value);
                m_assembler.ext(cmpTempRegister, reg, pos, 1);
                switch (cond) {
                case Zero:
                    return branchEqual(cmpTempRegister, MIPSRegisters::zero);
                case NonZero:
                    return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
                case Signed:
                    m_assembler.slt(cmpTempRegister, cmpTempRegister, MIPSRegisters::zero);
                    return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
#endif
            if (mask.m_value >= 0 && mask.m_value <= 65535) {
                m_assembler.andi(cmpTempRegister, reg, mask.m_value);
                switch (cond) {
                case Zero:
                    return branchEqual(cmpTempRegister, MIPSRegisters::zero);
                case NonZero:
                    return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
                case Signed:
                    m_assembler.slt(cmpTempRegister, cmpTempRegister, MIPSRegisters::zero);
                    return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
        }
        move(mask, immTempRegister);
        return branchTest32(cond, reg, immTempRegister);
    }

    Jump branchTest32(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        load32(address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask);
    }

    Jump branchTest32(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        load32(address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask);
    }

    Jump branchTest32(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        load32(address.m_ptr, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask);
    }

    TrustedImm32 mask8OnTest(ResultCondition cond, TrustedImm32 mask)
    {
        if (mask.m_value == -1 && !m_fixedWidth)
            return TrustedImm32(-1);
        return MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
    }

    Jump branchTest8(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        TrustedImm32 mask8 = mask8OnTest(cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask8);
    }

    Jump branchTest8(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        TrustedImm32 mask8 = mask8OnTest(cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask8);
    }

    Jump branchTest8(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        TrustedImm32 mask8 = mask8OnTest(cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask8);
    }

    Jump jump()
    {
        return branchEqual(MIPSRegisters::zero, MIPSRegisters::zero);
    }

    void farJump(RegisterID target, PtrTag)
    {
        move(target, MIPSRegisters::t9);
        m_assembler.jr(MIPSRegisters::t9);
        m_assembler.nop();
    }

    void farJump(TrustedImmPtr target, PtrTag)
    {
        move(target, MIPSRegisters::t9);
        m_assembler.jr(MIPSRegisters::t9);
        m_assembler.nop();
    }

    void farJump(Address address, PtrTag)
    {
        m_fixedWidth = true;
        load32(address, MIPSRegisters::t9);
        m_assembler.jr(MIPSRegisters::t9);
        m_assembler.nop();
        m_fixedWidth = false;
    }

    void farJump(AbsoluteAddress address, PtrTag)
    {
        m_fixedWidth = true;
        load32(address.m_ptr, MIPSRegisters::t9);
        m_assembler.jr(MIPSRegisters::t9);
        m_assembler.nop();
        m_fixedWidth = false;
    }

    ALWAYS_INLINE void farJump(RegisterID target, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), farJump(target, NoPtrTag); }
    ALWAYS_INLINE void farJump(Address address, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), farJump(address, NoPtrTag); }
    ALWAYS_INLINE void farJump(AbsoluteAddress address, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), farJump(address, NoPtrTag); }

    void moveDoubleToInts(FPRegisterID src, RegisterID dest1, RegisterID dest2)
    {
        m_assembler.vmov(dest1, dest2, src);
    }

    void moveIntsToDouble(RegisterID src1, RegisterID src2, FPRegisterID dest)
    {
        m_assembler.vmov(dest, src1, src2);
    }

    // Arithmetic control flow operations:
    //
    // This set of conditional branch operations branch based
    // on the result of an arithmetic operation. The operation
    // is performed as normal, storing the result.
    //
    // * jz operations branch if the result is zero.
    // * jo operations branch if the (signed) arithmetic
    //   operation caused an overflow to occur.

    Jump branchAdd32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == PositiveOrZero) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            /*
                move    dest, dataTemp
                xor     cmpTemp, dataTemp, src
                bltz    cmpTemp, No_overflow    # diff sign bit -> no overflow
                addu    dest, dataTemp, src
                xor     cmpTemp, dest, dataTemp
                bgez    cmpTemp, No_overflow    # same sign big -> no overflow
                nop
                b       Overflow
                nop
                b       No_overflow
                nop
                nop
                nop
            No_overflow:
            */
            move(dest, dataTempRegister);
            m_assembler.xorInsn(cmpTempRegister, dataTempRegister, src);
            m_assembler.bltz(cmpTempRegister, 10);
            m_assembler.addu(dest, dataTempRegister, src);
            m_assembler.xorInsn(cmpTempRegister, dest, dataTempRegister);
            m_assembler.bgez(cmpTempRegister, 7);
            m_assembler.nop();
            return jump();
        }
        if (cond == Signed) {
            add32(src, dest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == PositiveOrZero) {
            add32(src, dest);
            // Check if dest is not negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            add32(src, dest);
            return branchEqual(dest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            add32(src, dest);
            return branchNotEqual(dest, MIPSRegisters::zero);
        }
        ASSERT(0);
        return Jump();
    }

    Jump branchAdd32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == PositiveOrZero) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            /*
                move    dataTemp, op1
                xor     cmpTemp, dataTemp, op2
                bltz    cmpTemp, No_overflow    # diff sign bit -> no overflow
                addu    dest, dataTemp, op2
                xor     cmpTemp, dest, dataTemp
                bgez    cmpTemp, No_overflow    # same sign big -> no overflow
                nop
                b       Overflow
                nop
                b       No_overflow
                nop
                nop
                nop
            No_overflow:
            */
            move(op1, dataTempRegister);
            m_assembler.xorInsn(cmpTempRegister, dataTempRegister, op2);
            m_assembler.bltz(cmpTempRegister, 10);
            m_assembler.addu(dest, dataTempRegister, op2);
            m_assembler.xorInsn(cmpTempRegister, dest, dataTempRegister);
            m_assembler.bgez(cmpTempRegister, 7);
            m_assembler.nop();
            return jump();
        }
        if (cond == Signed) {
            add32(op1, op2, dest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == PositiveOrZero) {
            add32(op1, op2, dest);
            // Check if dest is not negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            add32(op1, op2, dest);
            return branchEqual(dest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            add32(op1, op2, dest);
            return branchNotEqual(dest, MIPSRegisters::zero);
        }
        ASSERT(0);
        return Jump();
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchAdd32(cond, dest, imm, dest);
    }

    Jump branchAdd32(ResultCondition cond, Address address, RegisterID dest)
    {
        load32(address, immTempRegister);
        return branchAdd32(cond, immTempRegister, dest);
    }

    Jump branchAdd32(ResultCondition cond, RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (imm.m_value >= -32768 && imm.m_value <= 32767 && !m_fixedWidth) {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == PositiveOrZero) || (cond == Zero) || (cond == NonZero));
            if (cond == Overflow) {
                if (imm.m_value >= 0) {
                    m_assembler.bltz(src, 9);
                    m_assembler.addiu(dest, src, imm.m_value);
                    m_assembler.bgez(dest, 7);
                    m_assembler.nop();
                } else {
                    m_assembler.bgez(src, 9);
                    m_assembler.addiu(dest, src, imm.m_value);
                    m_assembler.bltz(dest, 7);
                    m_assembler.nop();
                }
                return jump();
            }
            m_assembler.addiu(dest, src, imm.m_value);
            if (cond == Signed) {
                // Check if dest is negative.
                m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
                return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
            }
            if (cond == PositiveOrZero) {
                // Check if dest is not negative.
                m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
                return branchEqual(cmpTempRegister, MIPSRegisters::zero);
            }
            if (cond == Zero)
                return branchEqual(dest, MIPSRegisters::zero);
            if (cond == NonZero)
                return branchNotEqual(dest, MIPSRegisters::zero);
            ASSERT_NOT_REACHED();
            return Jump();
        }
        move(imm, immTempRegister);
        return branchAdd32(cond, src, immTempRegister, dest);
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, AbsoluteAddress dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == PositiveOrZero) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            if (m_fixedWidth) {
                /*
                    load    dest, dataTemp
                    move    imm, immTemp
                    xor     cmpTemp, dataTemp, immTemp
                    addu    dataTemp, dataTemp, immTemp
                    store   dataTemp, dest
                    bltz    cmpTemp, No_overflow    # diff sign bit -> no overflow
                    xor     cmpTemp, dataTemp, immTemp
                    bgez    cmpTemp, No_overflow    # same sign big -> no overflow
                    nop
                    b       Overflow
                    nop
                    b       No_overflow
                    nop
                    nop
                    nop
                No_overflow:
                */
                load32(dest.m_ptr, dataTempRegister);
                move(imm, immTempRegister);
                m_assembler.xorInsn(cmpTempRegister, dataTempRegister, immTempRegister);
                m_assembler.addu(dataTempRegister, dataTempRegister, immTempRegister);
                store32(dataTempRegister, dest.m_ptr);
                m_assembler.bltz(cmpTempRegister, 9);
                m_assembler.xorInsn(cmpTempRegister, dataTempRegister, immTempRegister);
                m_assembler.bgez(cmpTempRegister, 7);
                m_assembler.nop();
            } else {
                uintptr_t adr = reinterpret_cast<uintptr_t>(dest.m_ptr);
                m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
                m_assembler.lw(dataTempRegister, addrTempRegister, adr & 0xffff);
                if (imm.m_value >= 0 && imm.m_value  <= 32767) {
                    move(dataTempRegister, cmpTempRegister);
                    m_assembler.addiu(dataTempRegister, dataTempRegister, imm.m_value);
                    m_assembler.bltz(cmpTempRegister, 9);
                    m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
                    m_assembler.bgez(dataTempRegister, 7);
                    m_assembler.nop();
                } else if (imm.m_value >= -32768 && imm.m_value < 0) {
                    move(dataTempRegister, cmpTempRegister);
                    m_assembler.addiu(dataTempRegister, dataTempRegister, imm.m_value);
                    m_assembler.bgez(cmpTempRegister, 9);
                    m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
                    m_assembler.bltz(cmpTempRegister, 7);
                    m_assembler.nop();
                } else {
                    move(imm, immTempRegister);
                    m_assembler.xorInsn(cmpTempRegister, dataTempRegister, immTempRegister);
                    m_assembler.addu(dataTempRegister, dataTempRegister, immTempRegister);
                    m_assembler.bltz(cmpTempRegister, 10);
                    m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
                    m_assembler.xorInsn(cmpTempRegister, dataTempRegister, immTempRegister);
                    m_assembler.bgez(cmpTempRegister, 7);
                    m_assembler.nop();
                }
            }
            return jump();
        }
        if (m_fixedWidth) {
            move(imm, immTempRegister);
            load32(dest.m_ptr, dataTempRegister);
            add32(immTempRegister, dataTempRegister);
            store32(dataTempRegister, dest.m_ptr);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(dest.m_ptr);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.lw(dataTempRegister, addrTempRegister, adr & 0xffff);
            add32(imm, dataTempRegister);
            m_assembler.sw(dataTempRegister, addrTempRegister, adr & 0xffff);
        }
        if (cond == Signed) {
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dataTempRegister, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == PositiveOrZero) {
            // Check if dest is not negative.
            m_assembler.slt(cmpTempRegister, dataTempRegister, MIPSRegisters::zero);
            return branchEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero)
            return branchEqual(dataTempRegister, MIPSRegisters::zero);
        if (cond == NonZero)
            return branchNotEqual(dataTempRegister, MIPSRegisters::zero);
        ASSERT(0);
        return Jump();
    }

    Jump branchMul32(ResultCondition cond, RegisterID src1, RegisterID src2, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            /*
                mult    src, dest
                mfhi    dataTemp
                mflo    dest
                sra     addrTemp, dest, 31
                beq     dataTemp, addrTemp, No_overflow # all sign bits (bit 63 to bit 31) are the same -> no overflow
                nop
                b       Overflow
                nop
                b       No_overflow
                nop
                nop
                nop
            No_overflow:
            */
            m_assembler.mult(src1, src2);
            m_assembler.mfhi(dataTempRegister);
            m_assembler.mflo(dest);
            m_assembler.sra(addrTempRegister, dest, 31);
            m_assembler.beq(dataTempRegister, addrTempRegister, 7);
            m_assembler.nop();
            return jump();
        }
        if (cond == Signed) {
            mul32(src1, src2, dest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            mul32(src1, src2, dest);
            return branchEqual(dest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            mul32(src1, src2, dest);
            return branchNotEqual(dest, MIPSRegisters::zero);
        }
        ASSERT(0);
        return Jump();
    }

    Jump branchMul32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            /*
                mult    src, dest
                mfhi    dataTemp
                mflo    dest
                sra     addrTemp, dest, 31
                beq     dataTemp, addrTemp, No_overflow # all sign bits (bit 63 to bit 31) are the same -> no overflow
                nop
                b       Overflow
                nop
                b       No_overflow
                nop
                nop
                nop
            No_overflow:
            */
            m_assembler.mult(src, dest);
            m_assembler.mfhi(dataTempRegister);
            m_assembler.mflo(dest);
            m_assembler.sra(addrTempRegister, dest, 31);
            m_assembler.beq(dataTempRegister, addrTempRegister, 7);
            m_assembler.nop();
            return jump();
        }
        if (cond == Signed) {
            mul32(src, dest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            mul32(src, dest);
            return branchEqual(dest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            mul32(src, dest);
            return branchNotEqual(dest, MIPSRegisters::zero);
        }
        ASSERT(0);
        return Jump();
    }

    Jump branchMul32(ResultCondition cond, RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        move(imm, immTempRegister);
        return branchMul32(cond, immTempRegister, src, dest);
    }

    Jump branchSub32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            /*
                move    dest, dataTemp
                xor     cmpTemp, dataTemp, src
                bgez    cmpTemp, No_overflow    # same sign bit -> no overflow
                subu    dest, dataTemp, src
                xor     cmpTemp, dest, dataTemp
                bgez    cmpTemp, No_overflow    # same sign bit -> no overflow
                nop
                b       Overflow
                nop
                b       No_overflow
                nop
                nop
                nop
            No_overflow:
            */
            move(dest, dataTempRegister);
            m_assembler.xorInsn(cmpTempRegister, dataTempRegister, src);
            m_assembler.bgez(cmpTempRegister, 10);
            m_assembler.subu(dest, dataTempRegister, src);
            m_assembler.xorInsn(cmpTempRegister, dest, dataTempRegister);
            m_assembler.bgez(cmpTempRegister, 7);
            m_assembler.nop();
            return jump();
        }
        if (cond == Signed) {
            sub32(src, dest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            sub32(src, dest);
            return branchEqual(dest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            sub32(src, dest);
            return branchNotEqual(dest, MIPSRegisters::zero);
        }
        ASSERT(0);
        return Jump();
    }

    Jump branchSub32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        move(imm, immTempRegister);
        return branchSub32(cond, immTempRegister, dest);
    }

    Jump branchSub32(ResultCondition cond, RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        move(imm, immTempRegister);
        return branchSub32(cond, src, immTempRegister, dest);
    }

    Jump branchSub32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            /*
                move    dataTemp, op1
                xor     cmpTemp, dataTemp, op2
                bgez    cmpTemp, No_overflow    # same sign bit -> no overflow
                subu    dest, dataTemp, op2
                xor     cmpTemp, dest, dataTemp
                bgez    cmpTemp, No_overflow    # same sign bit -> no overflow
                nop
                b       Overflow
                nop
                b       No_overflow
                nop
                nop
                nop
            No_overflow:
            */
            move(op1, dataTempRegister);
            m_assembler.xorInsn(cmpTempRegister, dataTempRegister, op2);
            m_assembler.bgez(cmpTempRegister, 10);
            m_assembler.subu(dest, dataTempRegister, op2);
            m_assembler.xorInsn(cmpTempRegister, dest, dataTempRegister);
            m_assembler.bgez(cmpTempRegister, 7);
            m_assembler.nop();
            return jump();
        }
        if (cond == Signed) {
            sub32(op1, op2, dest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            sub32(op1, op2, dest);
            return branchEqual(dest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            sub32(op1, op2, dest);
            return branchNotEqual(dest, MIPSRegisters::zero);
        }
        ASSERT(0);
        return Jump();
    }

    Jump branchNeg32(ResultCondition cond, RegisterID srcDest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            /*
                bgez    srcDest, No_overflow    # positive input -> no overflow
                subu    srcDest, zero, srcDest
                bgez    srcDest, No_overflow    # negative input, positive output -> no overflow
                nop
                b       Overflow
                nop
                b       No_overflow
                nop
                nop
                nop
            No_overflow:
            */
            m_assembler.bgez(srcDest, 9);
            m_assembler.subu(srcDest, MIPSRegisters::zero, srcDest);
            m_assembler.bgez(srcDest, 7);
            m_assembler.nop();
            return jump();
        }
        if (cond == Signed) {
            m_assembler.subu(srcDest, MIPSRegisters::zero, srcDest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, srcDest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            m_assembler.subu(srcDest, MIPSRegisters::zero, srcDest);
            return branchEqual(srcDest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            m_assembler.subu(srcDest, MIPSRegisters::zero, srcDest);
            return branchNotEqual(srcDest, MIPSRegisters::zero);
        }
        ASSERT_NOT_REACHED();
        return Jump();
    }

    Jump branchOr32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Signed) {
            or32(src, dest);
            // Check if dest is negative.
            m_assembler.slt(cmpTempRegister, dest, MIPSRegisters::zero);
            return branchNotEqual(cmpTempRegister, MIPSRegisters::zero);
        }
        if (cond == Zero) {
            or32(src, dest);
            return branchEqual(dest, MIPSRegisters::zero);
        }
        if (cond == NonZero) {
            or32(src, dest);
            return branchNotEqual(dest, MIPSRegisters::zero);
        }
        ASSERT(0);
        return Jump();
    }

    // Miscellaneous operations:

    void breakpoint()
    {
        m_assembler.bkpt();
    }

    static bool isBreakpoint(void* address) { return MIPSAssembler::isBkpt(address); }

    Call nearCall()
    {
        /* We need two words for relaxation. */
        m_assembler.nop();
        m_assembler.nop();
        m_assembler.jal();
        m_assembler.nop();
        return Call(m_assembler.label(), Call::LinkableNear);
    }

    Call nearTailCall()
    {
        m_assembler.nop();
        m_assembler.nop();
        m_assembler.beq(MIPSRegisters::zero, MIPSRegisters::zero, 0);
        m_assembler.nop();
        insertRelaxationWords();
        return Call(m_assembler.label(), Call::LinkableNearTail);
    }

    Call call(PtrTag)
    {
        m_assembler.lui(MIPSRegisters::t9, 0);
        m_assembler.ori(MIPSRegisters::t9, MIPSRegisters::t9, 0);
        m_assembler.jalr(MIPSRegisters::t9);
        m_assembler.nop();
        return Call(m_assembler.label(), Call::Linkable);
    }

    Call call(RegisterID target, PtrTag)
    {
        move(target, MIPSRegisters::t9);
        m_assembler.jalr(MIPSRegisters::t9);
        m_assembler.nop();
        return Call(m_assembler.label(), Call::None);
    }

    Call call(Address address, PtrTag)
    {
        m_fixedWidth = true;
        load32(address, MIPSRegisters::t9);
        m_assembler.jalr(MIPSRegisters::t9);
        m_assembler.nop();
        m_fixedWidth = false;
        return Call(m_assembler.label(), Call::None);
    }

    ALWAYS_INLINE Call call(RegisterID callTag) { return UNUSED_PARAM(callTag), call(NoPtrTag); }
    ALWAYS_INLINE Call call(RegisterID target, RegisterID callTag) { return UNUSED_PARAM(callTag), call(target, NoPtrTag); }
    ALWAYS_INLINE Call call(Address address, RegisterID callTag) { return UNUSED_PARAM(callTag), call(address, NoPtrTag); }

    void ret()
    {
        m_assembler.jr(MIPSRegisters::ra);
        m_assembler.nop();
    }

    void compare32(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        if (cond == Equal) {
            if (right == MIPSRegisters::zero && !m_fixedWidth)
                m_assembler.sltiu(dest, left, 1);
            else {
                m_assembler.xorInsn(dest, left, right);
                m_assembler.sltiu(dest, dest, 1);
            }
        } else if (cond == NotEqual) {
            if (right == MIPSRegisters::zero && !m_fixedWidth)
                m_assembler.sltu(dest, MIPSRegisters::zero, left);
            else {
                m_assembler.xorInsn(dest, left, right);
                m_assembler.sltu(dest, MIPSRegisters::zero, dest);
            }
        } else if (cond == Above)
            m_assembler.sltu(dest, right, left);
        else if (cond == AboveOrEqual) {
            m_assembler.sltu(dest, left, right);
            m_assembler.xori(dest, dest, 1);
        } else if (cond == Below)
            m_assembler.sltu(dest, left, right);
        else if (cond == BelowOrEqual) {
            m_assembler.sltu(dest, right, left);
            m_assembler.xori(dest, dest, 1);
        } else if (cond == GreaterThan)
            m_assembler.slt(dest, right, left);
        else if (cond == GreaterThanOrEqual) {
            m_assembler.slt(dest, left, right);
            m_assembler.xori(dest, dest, 1);
        } else if (cond == LessThan)
            m_assembler.slt(dest, left, right);
        else if (cond == LessThanOrEqual) {
            m_assembler.slt(dest, right, left);
            m_assembler.xori(dest, dest, 1);
        }
    }

    void compare32(RelationalCondition cond, RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        if (!right.m_value && !m_fixedWidth)
            compare32(cond, left, MIPSRegisters::zero, dest);
        else {
            move(right, immTempRegister);
            compare32(cond, left, immTempRegister, dest);
        }
    }

    void test8(ResultCondition cond, Address address, TrustedImm32 mask, RegisterID dest)
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        TrustedImm32 mask8 = mask8OnTest(cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, dataTempRegister);
        if ((mask8.m_value & 0xff) == 0xff && !m_fixedWidth) {
            if (cond == Zero)
                m_assembler.sltiu(dest, dataTempRegister, 1);
            else
                m_assembler.sltu(dest, MIPSRegisters::zero, dataTempRegister);
        } else {
            move(mask8, immTempRegister);
            m_assembler.andInsn(cmpTempRegister, dataTempRegister, immTempRegister);
            if (cond == Zero)
                m_assembler.sltiu(dest, cmpTempRegister, 1);
            else
                m_assembler.sltu(dest, MIPSRegisters::zero, cmpTempRegister);
        }
    }

    void test32(ResultCondition cond, Address address, TrustedImm32 mask, RegisterID dest)
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        load32(address, dataTempRegister);
        if (mask.m_value == -1 && !m_fixedWidth) {
            if (cond == Zero)
                m_assembler.sltiu(dest, dataTempRegister, 1);
            else
                m_assembler.sltu(dest, MIPSRegisters::zero, dataTempRegister);
        } else {
            move(mask, immTempRegister);
            m_assembler.andInsn(cmpTempRegister, dataTempRegister, immTempRegister);
            if (cond == Zero)
                m_assembler.sltiu(dest, cmpTempRegister, 1);
            else
                m_assembler.sltu(dest, MIPSRegisters::zero, cmpTempRegister);
        }
    }

    DataLabel32 moveWithPatch(TrustedImm32 imm, RegisterID dest)
    {
        m_fixedWidth = true;
        DataLabel32 label(this);
        move(imm, dest);
        m_fixedWidth = false;
        return label;
    }

    DataLabelPtr moveWithPatch(TrustedImmPtr initialValue, RegisterID dest)
    {
        m_fixedWidth = true;
        DataLabelPtr label(this);
        move(initialValue, dest);
        m_fixedWidth = false;
        return label;
    }

    Jump branchPtrWithPatch(RelationalCondition cond, RegisterID left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        m_fixedWidth = true;
        dataLabel = moveWithPatch(initialRightValue, immTempRegister);
        m_assembler.nop();
        m_assembler.nop();
        Jump temp = branch32(cond, left, immTempRegister);
        m_fixedWidth = false;
        return temp;
    }

    Jump branchPtrWithPatch(RelationalCondition cond, Address left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        m_fixedWidth = true;
        load32(left, dataTempRegister);
        dataLabel = moveWithPatch(initialRightValue, immTempRegister);
        m_assembler.nop();
        m_assembler.nop();
        Jump temp = branch32(cond, dataTempRegister, immTempRegister);
        m_fixedWidth = false;
        return temp;
    }

    Jump branch32WithPatch(RelationalCondition cond, Address left, DataLabel32& dataLabel, TrustedImm32 initialRightValue = TrustedImm32(0))
    {
        m_fixedWidth = true;
        load32(left, dataTempRegister);
        dataLabel = moveWithPatch(initialRightValue, immTempRegister);
        Jump temp = branch32(cond, dataTempRegister, immTempRegister);
        m_fixedWidth = false;
        return temp;
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, ImplicitAddress address)
    {
        m_fixedWidth = true;
        DataLabelPtr dataLabel = moveWithPatch(initialValue, dataTempRegister);
        store32(dataTempRegister, address);
        m_fixedWidth = false;
        return dataLabel;
    }

    DataLabelPtr storePtrWithPatch(ImplicitAddress address)
    {
        return storePtrWithPatch(TrustedImmPtr(nullptr), address);
    }

    void loadFloat(BaseIndex address, FPRegisterID dest)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.lwc1(dest, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                lwc1    dest, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.lwc1(dest, addrTempRegister, address.offset);
        }
    }

    void loadFloat(ImplicitAddress address, FPRegisterID dest)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            m_assembler.lwc1(dest, address.base, address.offset);
        } else {
            /*
               lui     addrTemp, (offset + 0x8000) >> 16
               addu    addrTemp, addrTemp, base
               lwc1    dest, (offset & 0xffff)(addrTemp)
               */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lwc1(dest, addrTempRegister, address.offset);
        }
    }

    void loadDouble(ImplicitAddress address, FPRegisterID dest)
    {
#if WTF_MIPS_ISA(1)
        /*
            li          addrTemp, address.offset
            addu        addrTemp, addrTemp, base
            lwc1        dest, 0(addrTemp)
            lwc1        dest+1, 4(addrTemp)
         */
        move(TrustedImm32(address.offset), addrTempRegister);
        m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
        m_assembler.lwc1(dest, addrTempRegister, 0);
        m_assembler.lwc1(FPRegisterID(dest + 1), addrTempRegister, 4);
#else
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth) {
            m_assembler.ldc1(dest, address.base, address.offset);
        } else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                ldc1    dest, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.ldc1(dest, addrTempRegister, address.offset);
        }
#endif
    }

    void loadDouble(BaseIndex address, FPRegisterID dest)
    {
#if WTF_MIPS_ISA(1)
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.lwc1(dest, addrTempRegister, address.offset);
            m_assembler.lwc1(FPRegisterID(dest + 1), addrTempRegister, address.offset + 4);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                lwc1    dest, (address.offset & 0xffff)(at)
                lwc1    dest+1, (address.offset & 0xffff + 4)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.lwc1(dest, addrTempRegister, address.offset);
            m_assembler.lwc1(FPRegisterID(dest + 1), addrTempRegister, address.offset + 4);
        }
#else
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.ldc1(dest, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                ldc1    dest, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.ldc1(dest, addrTempRegister, address.offset);
        }
#endif
    }

    void loadDouble(TrustedImmPtr address, FPRegisterID dest)
    {
#if WTF_MIPS_ISA(1)
        /*
            li          addrTemp, address
            lwc1        dest, 0(addrTemp)
            lwc1        dest+1, 4(addrTemp)
         */
        move(address, addrTempRegister);
        m_assembler.lwc1(dest, addrTempRegister, 0);
        m_assembler.lwc1(FPRegisterID(dest + 1), addrTempRegister, 4);
#else
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                ldc1        dest, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.ldc1(dest, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address.m_value);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.ldc1(dest, addrTempRegister, adr & 0xffff);
        }
#endif
    }

    void storeFloat(FPRegisterID src, BaseIndex address)
    {
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.swc1(src, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                swc1    src, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.swc1(src, addrTempRegister, address.offset);
        }
    }

    void storeFloat(FPRegisterID src, ImplicitAddress address)
    {
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.swc1(src, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                swc1    src, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.swc1(src, addrTempRegister, address.offset);
        }
    }

    void storeDouble(FPRegisterID src, ImplicitAddress address)
    {
#if WTF_MIPS_ISA(1)
        /*
            li          addrTemp, address.offset
            addu        addrTemp, addrTemp, base
            swc1        dest, 0(addrTemp)
            swc1        dest+1, 4(addrTemp)
         */
        move(TrustedImm32(address.offset), addrTempRegister);
        m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
        m_assembler.swc1(src, addrTempRegister, 0);
        m_assembler.swc1(FPRegisterID(src + 1), addrTempRegister, 4);
#else
        if (address.offset >= -32768 && address.offset <= 32767
            && !m_fixedWidth)
            m_assembler.sdc1(src, address.base, address.offset);
        else {
            /*
                lui     addrTemp, (offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, base
                sdc1    src, (offset & 0xffff)(addrTemp)
              */
            m_assembler.lui(addrTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.sdc1(src, addrTempRegister, address.offset);
        }
#endif
    }

    void storeDouble(FPRegisterID src, BaseIndex address)
    {
#if WTF_MIPS_ISA(1)
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.swc1(src, addrTempRegister, address.offset);
            m_assembler.swc1(FPRegisterID(src + 1), addrTempRegister, address.offset + 4);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                swc1    src, (address.offset & 0xffff)(at)
                swc1    src+1, (address.offset & 0xffff + 4)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.swc1(src, addrTempRegister, address.offset);
            m_assembler.swc1(FPRegisterID(src + 1), addrTempRegister, address.offset + 4);
        }
#else
        if (!m_fixedWidth) {
            loadAddress(address, LoadAddressMode::ScaleAndAddOffsetIfOffsetIsOutOfBounds);
            m_assembler.sdc1(src, addrTempRegister, address.offset);
        } else {
            /*
                sll     addrTemp, address.index, address.scale
                addu    addrTemp, addrTemp, address.base
                lui     immTemp, (address.offset + 0x8000) >> 16
                addu    addrTemp, addrTemp, immTemp
                sdc1    src, (address.offset & 0xffff)(at)
            */
            m_assembler.sll(addrTempRegister, address.index, address.scale);
            m_assembler.addu(addrTempRegister, addrTempRegister, address.base);
            m_assembler.lui(immTempRegister, (address.offset + 0x8000) >> 16);
            m_assembler.addu(addrTempRegister, addrTempRegister, immTempRegister);
            m_assembler.sdc1(src, addrTempRegister, address.offset);
        }
#endif
    }

    void storeDouble(FPRegisterID src, TrustedImmPtr address)
    {
#if WTF_MIPS_ISA(1)
        move(address, addrTempRegister);
        m_assembler.swc1(src, addrTempRegister, 0);
        m_assembler.swc1(FPRegisterID(src + 1), addrTempRegister, 4);
#else
        if (m_fixedWidth) {
            /*
                li  addrTemp, address
                sdc1  src, 0(addrTemp)
            */
            move(TrustedImmPtr(address), addrTempRegister);
            m_assembler.sdc1(src, addrTempRegister, 0);
        } else {
            uintptr_t adr = reinterpret_cast<uintptr_t>(address.m_value);
            m_assembler.lui(addrTempRegister, (adr + 0x8000) >> 16);
            m_assembler.sdc1(src, addrTempRegister, adr & 0xffff);
        }
#endif
    }

    void moveDouble(FPRegisterID src, FPRegisterID dest)
    {
        if (src != dest || m_fixedWidth)
            m_assembler.movd(dest, src);
    }

    void moveDouble(FPRegisterID src, RegisterID dest)
    {
        m_assembler.mfc1(dest, src);
        m_assembler.mfc1(RegisterID(dest + 1), FPRegisterID(src + 1));
    }

    void moveZeroToDouble(FPRegisterID reg)
    {
        convertInt32ToDouble(MIPSRegisters::zero, reg);
    }

    void swap(FPRegisterID fr1, FPRegisterID fr2)
    {
        moveDouble(fr1, fpTempRegister);
        moveDouble(fr2, fr1);
        moveDouble(fpTempRegister, fr2);
    }

    void addDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.addd(dest, dest, src);
    }

    void addDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.addd(dest, op1, op2);
    }

    void addDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        m_assembler.addd(dest, dest, fpTempRegister);
    }

    void addDouble(AbsoluteAddress address, FPRegisterID dest)
    {
        loadDouble(TrustedImmPtr(address.m_ptr), fpTempRegister);
        m_assembler.addd(dest, dest, fpTempRegister);
    }

    // andDouble and orDouble are a bit convoluted to implement
    // because we don't have FP instructions for those
    // operations. That means we'll have to go back and forth between
    // the FPU and the CPU, which accounts for most of the code here.
    void andDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.mfc1(immTempRegister, op1);
        m_assembler.mfc1(dataTempRegister, op2);
        m_assembler.andInsn(cmpTempRegister, immTempRegister, dataTempRegister);
        m_assembler.mtc1(cmpTempRegister, dest);

#if WTF_MIPS_ISA_REV(2) && WTF_MIPS_FP64
        m_assembler.mfhc1(immTempRegister, op1);
        m_assembler.mfhc1(dataTempRegister, op2);
#else
        m_assembler.mfc1(immTempRegister, FPRegisterID(op1+1));
        m_assembler.mfc1(dataTempRegister, FPRegisterID(op2+1));
#endif
        m_assembler.andInsn(cmpTempRegister, immTempRegister, dataTempRegister);
#if WTF_MIPS_ISA_REV(2) && WTF_MIPS_FP64
        m_assembler.mthc1(cmpTempRegister, dest);
#else
        m_assembler.mtc1(cmpTempRegister, FPRegisterID(dest+1));
#endif
    }

    void orDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.mfc1(immTempRegister, op1);
        m_assembler.mfc1(dataTempRegister, op2);
        m_assembler.orInsn(cmpTempRegister, immTempRegister, dataTempRegister);
        m_assembler.mtc1(cmpTempRegister, dest);

#if WTF_MIPS_ISA_REV(2) && WTF_MIPS_FP64
        m_assembler.mfhc1(immTempRegister, op1);
        m_assembler.mfhc1(dataTempRegister, op2);
#else
        m_assembler.mfc1(immTempRegister, FPRegisterID(op1+1));
        m_assembler.mfc1(dataTempRegister, FPRegisterID(op2+1));
#endif
        m_assembler.orInsn(cmpTempRegister, immTempRegister, dataTempRegister);
#if WTF_MIPS_ISA_REV(2) && WTF_MIPS_FP64
        m_assembler.mthc1(cmpTempRegister, dest);
#else
        m_assembler.mtc1(cmpTempRegister, FPRegisterID(dest+1));
#endif
    }

    void subDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.subd(dest, dest, src);
    }

    void subDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.subd(dest, op1, op2);
    }

    void subDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        m_assembler.subd(dest, dest, fpTempRegister);
    }

    void mulDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.muld(dest, dest, src);
    }

    void mulDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        m_assembler.muld(dest, dest, fpTempRegister);
    }

    void mulDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.muld(dest, op1, op2);
    }

    void divDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.divd(dest, dest, src);
    }

    void divDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.divd(dest, op1, op2);
    }

    void divDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        m_assembler.divd(dest, dest, fpTempRegister);
    }

    void negateDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.negd(dest, src);
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.mtc1(src, fpTempRegister);
        m_assembler.cvtdw(dest, fpTempRegister);
    }

    void convertInt32ToDouble(Address src, FPRegisterID dest)
    {
        load32(src, dataTempRegister);
        m_assembler.mtc1(dataTempRegister, fpTempRegister);
        m_assembler.cvtdw(dest, fpTempRegister);
    }

    void convertInt32ToDouble(AbsoluteAddress src, FPRegisterID dest)
    {
        load32(src.m_ptr, dataTempRegister);
        m_assembler.mtc1(dataTempRegister, fpTempRegister);
        m_assembler.cvtdw(dest, fpTempRegister);
    }

    void convertFloatToDouble(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.cvtds(dst, src);
    }

    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.cvtsd(dst, src);
    }

    void insertRelaxationWords()
    {
        /* We need four words for relaxation. */
        m_assembler.beq(MIPSRegisters::zero, MIPSRegisters::zero, 3); // Jump over nops;
        m_assembler.nop();
        m_assembler.nop();
        m_assembler.nop();
    }

    Jump branchTrue()
    {
        m_assembler.appendJump();
        m_assembler.bc1t();
        m_assembler.nop();
        insertRelaxationWords();
        return Jump(m_assembler.label());
    }

    Jump branchFalse()
    {
        m_assembler.appendJump();
        m_assembler.bc1f();
        m_assembler.nop();
        insertRelaxationWords();
        return Jump(m_assembler.label());
    }

    Jump branchEqual(RegisterID rs, RegisterID rt)
    {
        m_assembler.appendJump();
        m_assembler.beq(rs, rt, 0);
        m_assembler.nop();
        insertRelaxationWords();
        return Jump(m_assembler.label());
    }

    Jump branchNotEqual(RegisterID rs, RegisterID rt)
    {
        m_assembler.appendJump();
        m_assembler.bne(rs, rt, 0);
        m_assembler.nop();
        insertRelaxationWords();
        return Jump(m_assembler.label());
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        if (cond == DoubleEqualAndOrdered) {
            m_assembler.ceqd(left, right);
            return branchTrue();
        }
        if (cond == DoubleNotEqualAndOrdered) {
            m_assembler.cueqd(left, right);
            return branchFalse(); // false
        }
        if (cond == DoubleGreaterThanAndOrdered) {
            m_assembler.cngtd(left, right);
            return branchFalse(); // false
        }
        if (cond == DoubleGreaterThanOrEqualAndOrdered) {
            m_assembler.cnged(left, right);
            return branchFalse(); // false
        }
        if (cond == DoubleLessThanAndOrdered) {
            m_assembler.cltd(left, right);
            return branchTrue();
        }
        if (cond == DoubleLessThanOrEqualAndOrdered) {
            m_assembler.cled(left, right);
            return branchTrue();
        }
        if (cond == DoubleEqualOrUnordered) {
            m_assembler.cueqd(left, right);
            return branchTrue();
        }
        if (cond == DoubleNotEqualOrUnordered) {
            m_assembler.ceqd(left, right);
            return branchFalse(); // false
        }
        if (cond == DoubleGreaterThanOrUnordered) {
            m_assembler.coled(left, right);
            return branchFalse(); // false
        }
        if (cond == DoubleGreaterThanOrEqualOrUnordered) {
            m_assembler.coltd(left, right);
            return branchFalse(); // false
        }
        if (cond == DoubleLessThanOrUnordered) {
            m_assembler.cultd(left, right);
            return branchTrue();
        }
        if (cond == DoubleLessThanOrEqualOrUnordered) {
            m_assembler.culed(left, right);
            return branchTrue();
        }
        ASSERT(0);

        return Jump();
    }

    // Truncates 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    enum BranchTruncateType { BranchIfTruncateFailed, BranchIfTruncateSuccessful };

    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest, BranchTruncateType branchType = BranchIfTruncateFailed)
    {
        m_assembler.truncwd(fpTempRegister, src);
        m_assembler.cfc1(dataTempRegister, MIPSRegisters::fcsr);
        m_assembler.mfc1(dest, fpTempRegister);
        and32(TrustedImm32(MIPSAssembler::FP_CAUSE_INVALID_OPERATION), dataTempRegister);
        return branch32(branchType == BranchIfTruncateFailed ? NotEqual : Equal, dataTempRegister, MIPSRegisters::zero);
    }

    // Result is undefined if the value is outside of the integer range.
    void truncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.truncwd(fpTempRegister, src);
        m_assembler.mfc1(dest, fpTempRegister);
    }

    // Result is undefined if src > 2^31
    void truncateDoubleToUint32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.truncwd(fpTempRegister, src);
        m_assembler.mfc1(dest, fpTempRegister);
    }

    // Convert 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, 0).
    void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID fpTemp, bool negZeroCheck = true)
    {
        m_assembler.cvtwd(fpTempRegister, src);
        m_assembler.mfc1(dest, fpTempRegister);

        // If the result is zero, it might have been -0.0, and the double comparison won't catch this!
        if (negZeroCheck)
            failureCases.append(branch32(Equal, dest, MIPSRegisters::zero));

        // Convert the integer result back to float & compare to the original value - if not equal or unordered (NaN) then jump.
        convertInt32ToDouble(dest, fpTemp);
        failureCases.append(branchDouble(DoubleNotEqualOrUnordered, fpTemp, src));
    }

    Jump branchDoubleNonZero(FPRegisterID reg, FPRegisterID scratch)
    {
        m_assembler.vmov(scratch, MIPSRegisters::zero, MIPSRegisters::zero);
        return branchDouble(DoubleNotEqualAndOrdered, reg, scratch);
    }

    Jump branchDoubleZeroOrNaN(FPRegisterID reg, FPRegisterID scratch)
    {
        m_assembler.vmov(scratch, MIPSRegisters::zero, MIPSRegisters::zero);
        return branchDouble(DoubleEqualOrUnordered, reg, scratch);
    }

    // Invert a relational condition, e.g. == becomes !=, < becomes >=, etc.
    static RelationalCondition invert(RelationalCondition cond)
    {
        RelationalCondition r;
        if (cond == Equal)
            r = NotEqual;
        else if (cond == NotEqual)
            r = Equal;
        else if (cond == Above)
            r = BelowOrEqual;
        else if (cond == AboveOrEqual)
            r = Below;
        else if (cond == Below)
            r = AboveOrEqual;
        else if (cond == BelowOrEqual)
            r = Above;
        else if (cond == GreaterThan)
            r = LessThanOrEqual;
        else if (cond == GreaterThanOrEqual)
            r = LessThan;
        else if (cond == LessThan)
            r = GreaterThanOrEqual;
        else if (cond == LessThanOrEqual)
            r = GreaterThan;
        return r;
    }

    void nop()
    {
        m_assembler.nop();
    }

    void memoryFence()
    {
        m_assembler.sync();
    }

    void abortWithReason(AbortReason reason)
    {
        move(TrustedImm32(reason), dataTempRegister);
        breakpoint();
    }

    void storeFence()
    {
        m_assembler.sync();
    }

    void abortWithReason(AbortReason reason, intptr_t misc)
    {
        move(TrustedImm32(misc), immTempRegister);
        abortWithReason(reason);
    }

    template<PtrTag resultTag, PtrTag locationTag>
    static FunctionPtr<resultTag> readCallTarget(CodeLocationCall<locationTag> call)
    {
        return FunctionPtr<resultTag>(reinterpret_cast<void(*)()>(MIPSAssembler::readCallTarget(call.dataLocation())));
    }

    template<PtrTag startTag, PtrTag destTag>
    static void replaceWithJump(CodeLocationLabel<startTag> instructionStart, CodeLocationLabel<destTag> destination)
    {
        MIPSAssembler::replaceWithJump(instructionStart.dataLocation(), destination.dataLocation());
    }
    
    static ptrdiff_t maxJumpReplacementSize()
    {
        MIPSAssembler::maxJumpReplacementSize();
        return 0;
    }

    static ptrdiff_t patchableJumpSize()
    {
        return MIPSAssembler::patchableJumpSize();
    }

    static bool canJumpReplacePatchableBranchPtrWithPatch() { return false; }
    static bool canJumpReplacePatchableBranch32WithPatch() { return false; }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranch32WithPatchOnAddress(CodeLocationDataLabel32<tag>)
    {
        UNREACHABLE_FOR_PLATFORM();
        return CodeLocationLabel<tag>();
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfBranchPtrWithPatchOnRegister(CodeLocationDataLabelPtr<tag> label)
    {
        return label.labelAtOffset(0);
    }

    template<PtrTag tag>
    static void revertJumpReplacementToBranchPtrWithPatch(CodeLocationLabel<tag> instructionStart, RegisterID, void* initialValue)
    {
        MIPSAssembler::revertJumpToMove(instructionStart.dataLocation(), immTempRegister, reinterpret_cast<int>(initialValue) & 0xffff);
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranchPtrWithPatchOnAddress(CodeLocationDataLabelPtr<tag>)
    {
        UNREACHABLE_FOR_PLATFORM();
        return CodeLocationLabel<tag>();
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranch32WithPatch(CodeLocationLabel<tag>, Address, int32_t)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranchPtrWithPatch(CodeLocationLabel<tag>, Address, void*)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, CodeLocationLabel<destTag> destination)
    {
        MIPSAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, FunctionPtr<destTag> destination)
    {
        MIPSAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

private:
    // If m_fixedWidth is true, we will generate a fixed number of instructions.
    // Otherwise, we can emit any number of instructions.
    bool m_fixedWidth;

    friend class LinkBuffer;

    template<PtrTag tag>
    static void linkCall(void* code, Call call, FunctionPtr<tag> function)
    {
        if (call.isFlagSet(Call::Tail))
            MIPSAssembler::linkJump(code, call.m_label, function.executableAddress());
        else
            MIPSAssembler::linkCall(code, call.m_label, function.executableAddress());
    }

};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(MIPS)
