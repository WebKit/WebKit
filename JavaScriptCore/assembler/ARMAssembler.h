/*
 * Copyright (C) 2009 University of Szeged
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ARMAssembler_h
#define ARMAssembler_h

#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER) && PLATFORM(ARM_TRADITIONAL)

#include "AssemblerBufferWithConstantPool.h"
#include <wtf/Assertions.h>
namespace JSC {

    typedef uint32_t ARMWord;

    namespace ARMRegisters {
        typedef enum {
            r0 = 0,
            r1,
            r2,
            r3,
            S0 = r3,
            r4,
            r5,
            r6,
            r7,
            r8,
            S1 = r8,
            r9,
            r10,
            r11,
            r12,
            r13,
            sp = r13,
            r14,
            lr = r14,
            r15,
            pc = r15
        } RegisterID;

        typedef enum {
            d0,
            d1,
            d2,
            d3,
            SD0 = d3
        } FPRegisterID;

    } // namespace ARMRegisters

    class ARMAssembler {
    public:
        typedef ARMRegisters::RegisterID RegisterID;
        typedef ARMRegisters::FPRegisterID FPRegisterID;
        typedef AssemblerBufferWithConstantPool<2048, 4, 4, ARMAssembler> ARMBuffer;
        typedef SegmentedVector<int, 64> Jumps;

        ARMAssembler() { }

        // ARM conditional constants
        typedef enum {
            EQ = 0x00000000, // Zero
            NE = 0x10000000, // Non-zero
            CS = 0x20000000,
            CC = 0x30000000,
            MI = 0x40000000,
            PL = 0x50000000,
            VS = 0x60000000,
            VC = 0x70000000,
            HI = 0x80000000,
            LS = 0x90000000,
            GE = 0xa0000000,
            LT = 0xb0000000,
            GT = 0xc0000000,
            LE = 0xd0000000,
            AL = 0xe0000000
        } Condition;

        // ARM instruction constants
        enum {
            AND = (0x0 << 21),
            EOR = (0x1 << 21),
            SUB = (0x2 << 21),
            RSB = (0x3 << 21),
            ADD = (0x4 << 21),
            ADC = (0x5 << 21),
            SBC = (0x6 << 21),
            RSC = (0x7 << 21),
            TST = (0x8 << 21),
            TEQ = (0x9 << 21),
            CMP = (0xa << 21),
            CMN = (0xb << 21),
            ORR = (0xc << 21),
            MOV = (0xd << 21),
            BIC = (0xe << 21),
            MVN = (0xf << 21),
            MUL = 0x00000090,
            MULL = 0x00c00090,
            FADDD = 0x0e300b00,
            FSUBD = 0x0e300b40,
            FMULD = 0x0e200b00,
            FCMPD = 0x0eb40b40,
            DTR = 0x05000000,
            LDRH = 0x00100090,
            STRH = 0x00000090,
            STMDB = 0x09200000,
            LDMIA = 0x08b00000,
            FDTR = 0x0d000b00,
            B = 0x0a000000,
            BL = 0x0b000000,
            FMSR = 0x0e000a10,
            FMRS = 0x0e100a10,
            FSITOD = 0x0eb80bc0,
            FTOSID = 0x0ebd0b40,
            FMSTAT = 0x0ef1fa10,
#if ARM_ARCH_VERSION >= 5
            CLZ = 0x016f0f10,
            BKPT = 0xe120070,
#endif
#if ARM_ARCH_VERSION >= 7
            MOVW = 0x03000000,
            MOVT = 0x03400000,
#endif
        };

        enum {
            OP2_IMM = (1 << 25),
            OP2_IMMh = (1 << 22),
            OP2_INV_IMM = (1 << 26),
            SET_CC = (1 << 20),
            OP2_OFSREG = (1 << 25),
            DT_UP = (1 << 23),
            DT_WB = (1 << 21),
            // This flag is inlcuded in LDR and STR
            DT_PRE = (1 << 24),
            HDT_UH = (1 << 5),
            DT_LOAD = (1 << 20),
        };

        // Masks of ARM instructions
        enum {
            BRANCH_MASK = 0x00ffffff,
            NONARM = 0xf0000000,
            SDT_MASK = 0x0c000000,
            SDT_OFFSET_MASK = 0xfff,
        };

        enum {
            BOFFSET_MIN = -0x00800000,
            BOFFSET_MAX = 0x007fffff,
            SDT = 0x04000000,
        };

        enum {
            padForAlign8  = 0x00,
            padForAlign16 = 0x0000,
            padForAlign32 = 0xee120070,
        };

        static const ARMWord INVALID_IMM = 0xf0000000;

        class JmpSrc {
            friend class ARMAssembler;
        public:
            JmpSrc()
                : m_offset(-1)
            {
            }

        private:
            JmpSrc(int offset)
                : m_offset(offset)
            {
            }

            int m_offset;
        };

        class JmpDst {
            friend class ARMAssembler;
        public:
            JmpDst()
                : m_offset(-1)
                , m_used(false)
            {
            }

            bool isUsed() const { return m_used; }
            void used() { m_used = true; }
        private:
            JmpDst(int offset)
                : m_offset(offset)
                , m_used(false)
            {
                ASSERT(m_offset == offset);
            }

            int m_offset : 31;
            int m_used : 1;
        };

        // Instruction formating

        void emitInst(ARMWord op, int rd, int rn, ARMWord op2)
        {
            ASSERT ( ((op2 & ~OP2_IMM) <= 0xfff) || (((op2 & ~OP2_IMMh) <= 0xfff)) );
            m_buffer.putInt(op | RN(rn) | RD(rd) | op2);
        }

        void and_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | AND, rd, rn, op2);
        }

        void ands_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | AND | SET_CC, rd, rn, op2);
        }

        void eor_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | EOR, rd, rn, op2);
        }

        void eors_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | EOR | SET_CC, rd, rn, op2);
        }

        void sub_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | SUB, rd, rn, op2);
        }

        void subs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | SUB | SET_CC, rd, rn, op2);
        }

        void rsb_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | RSB, rd, rn, op2);
        }

        void rsbs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | RSB | SET_CC, rd, rn, op2);
        }

        void add_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | ADD, rd, rn, op2);
        }

        void adds_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | ADD | SET_CC, rd, rn, op2);
        }

        void adc_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | ADC, rd, rn, op2);
        }

        void adcs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | ADC | SET_CC, rd, rn, op2);
        }

        void sbc_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | SBC, rd, rn, op2);
        }

        void sbcs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | SBC | SET_CC, rd, rn, op2);
        }

        void rsc_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | RSC, rd, rn, op2);
        }

        void rscs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | RSC | SET_CC, rd, rn, op2);
        }

        void tst_r(int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | TST | SET_CC, 0, rn, op2);
        }

        void teq_r(int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | TEQ | SET_CC, 0, rn, op2);
        }

        void cmp_r(int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | CMP | SET_CC, 0, rn, op2);
        }

        void orr_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | ORR, rd, rn, op2);
        }

        void orrs_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | ORR | SET_CC, rd, rn, op2);
        }

        void mov_r(int rd, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | MOV, rd, ARMRegisters::r0, op2);
        }

#if ARM_ARCH_VERSION >= 7
        void movw_r(int rd, ARMWord op2, Condition cc = AL)
        {
            ASSERT((op2 | 0xf0fff) == 0xf0fff);
            m_buffer.putInt(static_cast<ARMWord>(cc) | MOVW | RD(rd) | op2);
        }

        void movt_r(int rd, ARMWord op2, Condition cc = AL)
        {
            ASSERT((op2 | 0xf0fff) == 0xf0fff);
            m_buffer.putInt(static_cast<ARMWord>(cc) | MOVT | RD(rd) | op2);
        }
#endif

        void movs_r(int rd, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | MOV | SET_CC, rd, ARMRegisters::r0, op2);
        }

        void bic_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | BIC, rd, rn, op2);
        }

        void bics_r(int rd, int rn, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | BIC | SET_CC, rd, rn, op2);
        }

        void mvn_r(int rd, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | MVN, rd, ARMRegisters::r0, op2);
        }

        void mvns_r(int rd, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | MVN | SET_CC, rd, ARMRegisters::r0, op2);
        }

        void mul_r(int rd, int rn, int rm, Condition cc = AL)
        {
            m_buffer.putInt(static_cast<ARMWord>(cc) | MUL | RN(rd) | RS(rn) | RM(rm));
        }

        void muls_r(int rd, int rn, int rm, Condition cc = AL)
        {
            m_buffer.putInt(static_cast<ARMWord>(cc) | MUL | SET_CC | RN(rd) | RS(rn) | RM(rm));
        }

        void mull_r(int rdhi, int rdlo, int rn, int rm, Condition cc = AL)
        {
            m_buffer.putInt(static_cast<ARMWord>(cc) | MULL | RN(rdhi) | RD(rdlo) | RS(rn) | RM(rm));
        }

        void faddd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FADDD, dd, dn, dm);
        }

        void fsubd_r(int dd, int dn, int dm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FSUBD, dd, dn, dm);
        }

        void fmuld_r(int dd, int dn, int dm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FMULD, dd, dn, dm);
        }

        void fcmpd_r(int dd, int dm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FCMPD, dd, 0, dm);
        }

        void ldr_imm(int rd, ARMWord imm, Condition cc = AL)
        {
            m_buffer.putIntWithConstantInt(static_cast<ARMWord>(cc) | DTR | DT_LOAD | DT_UP | RN(ARMRegisters::pc) | RD(rd), imm, true);
        }

        void ldr_un_imm(int rd, ARMWord imm, Condition cc = AL)
        {
            m_buffer.putIntWithConstantInt(static_cast<ARMWord>(cc) | DTR | DT_LOAD | DT_UP | RN(ARMRegisters::pc) | RD(rd), imm);
        }

        void dtr_u(bool isLoad, int rd, int rb, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | DT_UP, rd, rb, op2);
        }

        void dtr_ur(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | DT_UP | OP2_OFSREG, rd, rb, rm);
        }

        void dtr_d(bool isLoad, int rd, int rb, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0), rd, rb, op2);
        }

        void dtr_dr(bool isLoad, int rd, int rb, int rm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | DTR | (isLoad ? DT_LOAD : 0) | OP2_OFSREG, rd, rb, rm);
        }

        void ldrh_r(int rd, int rn, int rm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_UP | DT_PRE, rd, rn, rm);
        }

        void ldrh_d(int rd, int rb, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_PRE, rd, rb, op2);
        }

        void ldrh_u(int rd, int rb, ARMWord op2, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | LDRH | HDT_UH | DT_UP | DT_PRE, rd, rb, op2);
        }

        void strh_r(int rn, int rm, int rd, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | STRH | HDT_UH | DT_UP | DT_PRE, rd, rn, rm);
        }

        void fdtr_u(bool isLoad, int rd, int rb, ARMWord op2, Condition cc = AL)
        {
            ASSERT(op2 <= 0xff);
            emitInst(static_cast<ARMWord>(cc) | FDTR | DT_UP | (isLoad ? DT_LOAD : 0), rd, rb, op2);
        }

        void fdtr_d(bool isLoad, int rd, int rb, ARMWord op2, Condition cc = AL)
        {
            ASSERT(op2 <= 0xff);
            emitInst(static_cast<ARMWord>(cc) | FDTR | (isLoad ? DT_LOAD : 0), rd, rb, op2);
        }

        void push_r(int reg, Condition cc = AL)
        {
            ASSERT(ARMWord(reg) <= 0xf);
            m_buffer.putInt(cc | DTR | DT_WB | RN(ARMRegisters::sp) | RD(reg) | 0x4);
        }

        void pop_r(int reg, Condition cc = AL)
        {
            ASSERT(ARMWord(reg) <= 0xf);
            m_buffer.putInt(cc | (DTR ^ DT_PRE) | DT_LOAD | DT_UP | RN(ARMRegisters::sp) | RD(reg) | 0x4);
        }

        inline void poke_r(int reg, Condition cc = AL)
        {
            dtr_d(false, ARMRegisters::sp, 0, reg, cc);
        }

        inline void peek_r(int reg, Condition cc = AL)
        {
            dtr_u(true, reg, ARMRegisters::sp, 0, cc);
        }

        void fmsr_r(int dd, int rn, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FMSR, rn, dd, 0);
        }

        void fmrs_r(int dd, int rn, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FMRS, rn, dd, 0);
        }

        void fsitod_r(int dd, int dm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FSITOD, dd, 0, dm);
        }

        void ftosid_r(int fd, int dm, Condition cc = AL)
        {
            emitInst(static_cast<ARMWord>(cc) | FTOSID, fd, 0, dm);
        }

        void fmstat(Condition cc = AL)
        {
            m_buffer.putInt(static_cast<ARMWord>(cc) | FMSTAT);
        }

#if ARM_ARCH_VERSION >= 5
        void clz_r(int rd, int rm, Condition cc = AL)
        {
            m_buffer.putInt(static_cast<ARMWord>(cc) | CLZ | RD(rd) | RM(rm));
        }
#endif

        void bkpt(ARMWord value)
        {
#if ARM_ARCH_VERSION >= 5
            m_buffer.putInt(BKPT | ((value & 0xff0) << 4) | (value & 0xf));
#else
            // Cannot access to Zero memory address
            dtr_dr(true, ARMRegisters::S0, ARMRegisters::S0, ARMRegisters::S0);
#endif
        }

        static ARMWord lsl(int reg, ARMWord value)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(value <= 0x1f);
            return reg | (value << 7) | 0x00;
        }

        static ARMWord lsr(int reg, ARMWord value)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(value <= 0x1f);
            return reg | (value << 7) | 0x20;
        }

        static ARMWord asr(int reg, ARMWord value)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(value <= 0x1f);
            return reg | (value << 7) | 0x40;
        }

        static ARMWord lsl_r(int reg, int shiftReg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(shiftReg <= ARMRegisters::pc);
            return reg | (shiftReg << 8) | 0x10;
        }

        static ARMWord lsr_r(int reg, int shiftReg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(shiftReg <= ARMRegisters::pc);
            return reg | (shiftReg << 8) | 0x30;
        }

        static ARMWord asr_r(int reg, int shiftReg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            ASSERT(shiftReg <= ARMRegisters::pc);
            return reg | (shiftReg << 8) | 0x50;
        }

        // General helpers

        int size()
        {
            return m_buffer.size();
        }

        void ensureSpace(int insnSpace, int constSpace)
        {
            m_buffer.ensureSpace(insnSpace, constSpace);
        }

        int sizeOfConstantPool()
        {
            return m_buffer.sizeOfConstantPool();
        }

        JmpDst label()
        {
            return JmpDst(m_buffer.size());
        }

        JmpDst align(int alignment)
        {
            while (!m_buffer.isAligned(alignment))
                mov_r(ARMRegisters::r0, ARMRegisters::r0);

            return label();
        }

        JmpSrc jmp(Condition cc = AL, int useConstantPool = 0)
        {
            ensureSpace(sizeof(ARMWord), sizeof(ARMWord));
            int s = m_buffer.uncheckedSize();
            ldr_un_imm(ARMRegisters::pc, 0xffffffff, cc);
            m_jumps.append(s | (useConstantPool & 0x1));
            return JmpSrc(s);
        }

        void* executableCopy(ExecutablePool* allocator);

        // Patching helpers

        static ARMWord* getLdrImmAddress(ARMWord* insn, uint32_t* constPool = 0);
        static void linkBranch(void* code, JmpSrc from, void* to, int useConstantPool = 0);

        static void patchPointerInternal(intptr_t from, void* to)
        {
            ARMWord* insn = reinterpret_cast<ARMWord*>(from);
            ARMWord* addr = getLdrImmAddress(insn);
            *addr = reinterpret_cast<ARMWord>(to);
            ExecutableAllocator::cacheFlush(addr, sizeof(ARMWord));
        }

        static ARMWord patchConstantPoolLoad(ARMWord load, ARMWord value)
        {
            value = (value << 1) + 1;
            ASSERT(!(value & ~0xfff));
            return (load & ~0xfff) | value;
        }

        static void patchConstantPoolLoad(void* loadAddr, void* constPoolAddr);

        // Patch pointers

        static void linkPointer(void* code, JmpDst from, void* to)
        {
            patchPointerInternal(reinterpret_cast<intptr_t>(code) + from.m_offset, to);
        }

        static void repatchInt32(void* from, int32_t to)
        {
            patchPointerInternal(reinterpret_cast<intptr_t>(from), reinterpret_cast<void*>(to));
        }

        static void repatchPointer(void* from, void* to)
        {
            patchPointerInternal(reinterpret_cast<intptr_t>(from), to);
        }

        static void repatchLoadPtrToLEA(void* from)
        {
            // On arm, this is a patch from LDR to ADD. It is restricted conversion,
            // from special case to special case, altough enough for its purpose
            ARMWord* insn = reinterpret_cast<ARMWord*>(from);
            ASSERT((*insn & 0x0ff00f00) == 0x05900000);

            *insn = (*insn & 0xf00ff0ff) | 0x02800000;
            ExecutableAllocator::cacheFlush(insn, sizeof(ARMWord));
        }

        // Linkers

        void linkJump(JmpSrc from, JmpDst to)
        {
            ARMWord* insn = reinterpret_cast<ARMWord*>(m_buffer.data()) + (from.m_offset / sizeof(ARMWord));
            *getLdrImmAddress(insn, m_buffer.poolAddress()) = static_cast<ARMWord>(to.m_offset);
        }

        static void linkJump(void* code, JmpSrc from, void* to)
        {
            linkBranch(code, from, to);
        }

        static void relinkJump(void* from, void* to)
        {
            patchPointerInternal(reinterpret_cast<intptr_t>(from) - sizeof(ARMWord), to);
        }

        static void linkCall(void* code, JmpSrc from, void* to)
        {
            linkBranch(code, from, to, true);
        }

        static void relinkCall(void* from, void* to)
        {
            relinkJump(from, to);
        }

        // Address operations

        static void* getRelocatedAddress(void* code, JmpSrc jump)
        {
            return reinterpret_cast<void*>(reinterpret_cast<ARMWord*>(code) + jump.m_offset / sizeof(ARMWord) + 1);
        }

        static void* getRelocatedAddress(void* code, JmpDst label)
        {
            return reinterpret_cast<void*>(reinterpret_cast<ARMWord*>(code) + label.m_offset / sizeof(ARMWord));
        }

        // Address differences

        static int getDifferenceBetweenLabels(JmpDst from, JmpSrc to)
        {
            return (to.m_offset + sizeof(ARMWord)) - from.m_offset;
        }

        static int getDifferenceBetweenLabels(JmpDst from, JmpDst to)
        {
            return to.m_offset - from.m_offset;
        }

        static unsigned getCallReturnOffset(JmpSrc call)
        {
            return call.m_offset + sizeof(ARMWord);
        }

        // Handle immediates

        static ARMWord getOp2Byte(ARMWord imm)
        {
            ASSERT(imm <= 0xff);
            return OP2_IMMh | (imm & 0x0f) | ((imm & 0xf0) << 4) ;
        }

        static ARMWord getOp2(ARMWord imm);

#if ARM_ARCH_VERSION >= 7
        static ARMWord getImm16Op2(ARMWord imm)
        {
            if (imm <= 0xffff)
                return (imm & 0xf000) << 4 | (imm & 0xfff);
            return INVALID_IMM;
        }
#endif
        ARMWord getImm(ARMWord imm, int tmpReg, bool invert = false);
        void moveImm(ARMWord imm, int dest);
        ARMWord encodeComplexImm(ARMWord imm, int dest);

        // Memory load/store helpers

        void dataTransfer32(bool isLoad, RegisterID srcDst, RegisterID base, int32_t offset);
        void baseIndexTransfer32(bool isLoad, RegisterID srcDst, RegisterID base, RegisterID index, int scale, int32_t offset);
        void doubleTransfer(bool isLoad, FPRegisterID srcDst, RegisterID base, int32_t offset);

        // Constant pool hnadlers

        static ARMWord placeConstantPoolBarrier(int offset)
        {
            offset = (offset - sizeof(ARMWord)) >> 2;
            ASSERT((offset <= BOFFSET_MAX && offset >= BOFFSET_MIN));
            return AL | B | (offset & BRANCH_MASK);
        }

    private:
        ARMWord RM(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg;
        }

        ARMWord RS(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 8;
        }

        ARMWord RD(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 12;
        }

        ARMWord RN(int reg)
        {
            ASSERT(reg <= ARMRegisters::pc);
            return reg << 16;
        }

        static ARMWord getConditionalField(ARMWord i)
        {
            return i & 0xf0000000;
        }

        int genInt(int reg, ARMWord imm, bool positive);

        ARMBuffer m_buffer;
        Jumps m_jumps;
    };

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && PLATFORM(ARM_TRADITIONAL)

#endif // ARMAssembler_h
