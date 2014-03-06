/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ARM64Assembler_h
#define ARM64Assembler_h

#if ENABLE(ASSEMBLER) && CPU(ARM64)

#include "AssemblerBuffer.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>
#include <stdint.h>

#define CHECK_DATASIZE_OF(datasize) ASSERT(datasize == 32 || datasize == 64)
#define DATASIZE_OF(datasize) ((datasize == 64) ? Datasize_64 : Datasize_32)
#define MEMOPSIZE_OF(datasize) ((datasize == 8 || datasize == 128) ? MemOpSize_8_or_128 : (datasize == 16) ? MemOpSize_16 : (datasize == 32) ? MemOpSize_32 : MemOpSize_64)
#define CHECK_DATASIZE() CHECK_DATASIZE_OF(datasize)
#define DATASIZE DATASIZE_OF(datasize)
#define MEMOPSIZE MEMOPSIZE_OF(datasize)
#define CHECK_FP_MEMOP_DATASIZE() ASSERT(datasize == 8 || datasize == 16 || datasize == 32 || datasize == 64 || datasize == 128)
#define MEMPAIROPSIZE_INT(datasize) ((datasize == 64) ? MemPairOp_64 : MemPairOp_32)
#define MEMPAIROPSIZE_FP(datasize) ((datasize == 128) ? MemPairOp_V128 : (datasize == 64) ? MemPairOp_V64 : MemPairOp_32)

namespace JSC {

ALWAYS_INLINE bool isInt7(int32_t value)
{
    return value == ((value << 25) >> 25);
}

ALWAYS_INLINE bool isInt9(int32_t value)
{
    return value == ((value << 23) >> 23);
}

ALWAYS_INLINE bool isInt11(int32_t value)
{
    return value == ((value << 21) >> 21);
}

ALWAYS_INLINE bool isUInt5(int32_t value)
{
    return !(value & ~0x1f);
}

ALWAYS_INLINE bool isUInt12(int32_t value)
{
    return !(value & ~0xfff);
}

ALWAYS_INLINE bool isUInt12(intptr_t value)
{
    return !(value & ~0xfffL);
}

class UInt5 {
public:
    explicit UInt5(int value)
        : m_value(value)
    {
        ASSERT(isUInt5(value));
    }

    operator int() { return m_value; }

private:
    int m_value;
};

class UInt12 {
public:
    explicit UInt12(int value)
        : m_value(value)
    {
        ASSERT(isUInt12(value));
    }

    operator int() { return m_value; }

private:
    int m_value;
};

class PostIndex {
public:
    explicit PostIndex(int value)
        : m_value(value)
    {
        ASSERT(isInt9(value));
    }

    operator int() { return m_value; }

private:
    int m_value;
};

class PreIndex {
public:
    explicit PreIndex(int value)
        : m_value(value)
    {
        ASSERT(isInt9(value));
    }

    operator int() { return m_value; }

private:
    int m_value;
};

class PairPostIndex {
public:
    explicit PairPostIndex(int value)
        : m_value(value)
    {
        ASSERT(isInt11(value));
    }

    operator int() { return m_value; }

private:
    int m_value;
};

class PairPreIndex {
public:
    explicit PairPreIndex(int value)
        : m_value(value)
    {
        ASSERT(isInt11(value));
    }

    operator int() { return m_value; }

private:
    int m_value;
};

class LogicalImmediate {
public:
    static LogicalImmediate create32(uint32_t value)
    {
        // Check for 0, -1 - these cannot be encoded.
        if (!value || !~value)
            return InvalidLogicalImmediate;

        // First look for a 32-bit pattern, then for repeating 16-bit
        // patterns, 8-bit, 4-bit, and finally 2-bit.

        unsigned hsb, lsb;
        bool inverted;
        if (findBitRange<32>(value, hsb, lsb, inverted))
            return encodeLogicalImmediate<32>(hsb, lsb, inverted);

        if ((value & 0xffff) != (value >> 16))
            return InvalidLogicalImmediate;
        value &= 0xffff;

        if (findBitRange<16>(value, hsb, lsb, inverted))
            return encodeLogicalImmediate<16>(hsb, lsb, inverted);

        if ((value & 0xff) != (value >> 8))
            return InvalidLogicalImmediate;
        value &= 0xff;

        if (findBitRange<8>(value, hsb, lsb, inverted))
            return encodeLogicalImmediate<8>(hsb, lsb, inverted);

        if ((value & 0xf) != (value >> 4))
            return InvalidLogicalImmediate;
        value &= 0xf;

        if (findBitRange<4>(value, hsb, lsb, inverted))
            return encodeLogicalImmediate<4>(hsb, lsb, inverted);

        if ((value & 0x3) != (value >> 2))
            return InvalidLogicalImmediate;
        value &= 0x3;

        if (findBitRange<2>(value, hsb, lsb, inverted))
            return encodeLogicalImmediate<2>(hsb, lsb, inverted);

        return InvalidLogicalImmediate;
    }

    static LogicalImmediate create64(uint64_t value)
    {
        // Check for 0, -1 - these cannot be encoded.
        if (!value || !~value)
            return InvalidLogicalImmediate;

        // Look for a contiguous bit range.
        unsigned hsb, lsb;
        bool inverted;
        if (findBitRange<64>(value, hsb, lsb, inverted))
            return encodeLogicalImmediate<64>(hsb, lsb, inverted);

        // If the high & low 32 bits are equal, we can try for a 32-bit (or narrower) pattern.
        if (static_cast<uint32_t>(value) == static_cast<uint32_t>(value >> 32))
            return create32(static_cast<uint32_t>(value));
        return InvalidLogicalImmediate;
    }

    int value() const
    {
        ASSERT(isValid());
        return m_value;
    }

    bool isValid() const
    {
        return m_value != InvalidLogicalImmediate;
    }

    bool is64bit() const
    {
        return m_value & (1 << 12);
    }

private:
    LogicalImmediate(int value)
        : m_value(value)
    {
    }

    // Generate a mask with bits in the range hsb..0 set, for example:
    //   hsb:63 = 0xffffffffffffffff
    //   hsb:42 = 0x000007ffffffffff
    //   hsb: 0 = 0x0000000000000001
    static uint64_t mask(unsigned hsb)
    {
        ASSERT(hsb < 64);
        return 0xffffffffffffffffull >> (63 - hsb);
    }

    template<unsigned N>
    static void partialHSB(uint64_t& value, unsigned&result)
    {
        if (value & (0xffffffffffffffffull << N)) {
            result += N;
            value >>= N;
        }
    }

    // Find the bit number of the highest bit set in a non-zero value, for example:
    //   0x8080808080808080 = hsb:63
    //   0x0000000000000001 = hsb: 0
    //   0x000007ffffe00000 = hsb:42
    static unsigned highestSetBit(uint64_t value)
    {
        ASSERT(value);
        unsigned hsb = 0;
        partialHSB<32>(value, hsb);
        partialHSB<16>(value, hsb);
        partialHSB<8>(value, hsb);
        partialHSB<4>(value, hsb);
        partialHSB<2>(value, hsb);
        partialHSB<1>(value, hsb);
        return hsb;
    }

    // This function takes a value and a bit width, where value obeys the following constraints:
    //   * bits outside of the width of the value must be zero.
    //   * bits within the width of value must neither be all clear or all set.
    // The input is inspected to detect values that consist of either two or three contiguous
    // ranges of bits. The output range hsb..lsb will describe the second range of the value.
    // if the range is set, inverted will be false, and if the range is clear, inverted will
    // be true. For example (with width 8):
    //   00001111 = hsb:3, lsb:0, inverted:false
    //   11110000 = hsb:3, lsb:0, inverted:true
    //   00111100 = hsb:5, lsb:2, inverted:false
    //   11000011 = hsb:5, lsb:2, inverted:true
    template<unsigned width>
    static bool findBitRange(uint64_t value, unsigned& hsb, unsigned& lsb, bool& inverted)
    {
        ASSERT(value & mask(width - 1));
        ASSERT(value != mask(width - 1));
        ASSERT(!(value & ~mask(width - 1)));

        // Detect cases where the top bit is set; if so, flip all the bits & set invert.
        // This halves the number of patterns we need to look for.
        const uint64_t msb = 1ull << (width - 1);
        if ((inverted = (value & msb)))
            value ^= mask(width - 1);

        // Find the highest set bit in value, generate a corresponding mask & flip all
        // bits under it.
        hsb = highestSetBit(value);
        value ^= mask(hsb);
        if (!value) {
            // If this cleared the value, then the range hsb..0 was all set.
            lsb = 0;
            return true;
        }

        // Try making one more mask, and flipping the bits!
        lsb = highestSetBit(value);
        value ^= mask(lsb);
        if (!value) {
            // Success - but lsb actually points to the hsb of a third range - add one
            // to get to the lsb of the mid range.
            ++lsb;
            return true;
        }

        return false;
    }

    // Encodes the set of immN:immr:imms fields found in a logical immediate.
    template<unsigned width>
    static int encodeLogicalImmediate(unsigned hsb, unsigned lsb, bool inverted)
    {
        // Check width is a power of 2!
        ASSERT(!(width & (width -1)));
        ASSERT(width <= 64 && width >= 2);
        ASSERT(hsb >= lsb);
        ASSERT(hsb < width);

        int immN = 0;
        int imms = 0;
        int immr = 0;

        // For 64-bit values this is easy - just set immN to true, and imms just
        // contains the bit number of the highest set bit of the set range. For
        // values with narrower widths, these are encoded by a leading set of
        // one bits, followed by a zero bit, followed by the remaining set of bits
        // being the high bit of the range. For a 32-bit immediate there are no
        // leading one bits, just a zero followed by a five bit number. For a
        // 16-bit immediate there is one one bit, a zero bit, and then a four bit
        // bit-position, etc.
        if (width == 64)
            immN = 1;
        else
            imms = 63 & ~(width + width - 1);

        if (inverted) {
            // if width is 64 & hsb is 62, then we have a value something like:
            //   0x80000000ffffffff (in this case with lsb 32).
            // The ror should be by 1, imms (effectively set width minus 1) is
            // 32. Set width is full width minus cleared width.
            immr = (width - 1) - hsb;
            imms |= (width - ((hsb - lsb) + 1)) - 1;
        } else {
            // if width is 64 & hsb is 62, then we have a value something like:
            //   0x7fffffff00000000 (in this case with lsb 32).
            // The value is effectively rol'ed by lsb, which is equivalent to
            // a ror by width - lsb (or 0, in the case where lsb is 0). imms
            // is hsb - lsb.
            immr = (width - lsb) & (width - 1);
            imms |= hsb - lsb;
        }

        return immN << 12 | immr << 6 | imms;
    }

    static const int InvalidLogicalImmediate = -1;

    int m_value;
};

inline uint16_t getHalfword(uint64_t value, int which)
{
    return value >> (which << 4);
}

namespace ARM64Registers {
    typedef enum {
        // ￼Parameter/result registers
        x0,
        x1,
        x2,
        x3,
        x4,
        x5,
        x6,
        x7,
        // Indirect result location register
        x8,
        // Temporary registers
        x9,
        x10,
        x11,
        x12,
        x13,
        x14,
        x15,
        // Intra-procedure-call scratch registers (temporary)
        x16, ip0 = x16,
        x17, ip1 = x17,
        // Platform Register (temporary)
        x18,
        // Callee-saved
        x19,
        x20,
        x21,
        x22,
        x23,
        x24,
        x25,
        x26,
        x27,
        x28,
        // Special
        x29, fp = x29,
        x30, lr = x30,
        sp,
        zr = 0x3f,
    } RegisterID;

    typedef enum {
        // Parameter/result registers
        q0,
        q1,
        q2,
        q3,
        q4,
        q5,
        q6,
        q7,
        // Callee-saved (up to 64-bits only!)
        q8,
        q9,
        q10,
        q11,
        q12,
        q13,
        q14,
        q15,
        // Temporary registers
        q16,
        q17,
        q18,
        q19,
        q20,
        q21,
        q22,
        q23,
        q24,
        q25,
        q26,
        q27,
        q28,
        q29,
        q30,
        q31,
    } FPRegisterID;

    static bool isSp(RegisterID reg) { return reg == sp; }
    static bool isZr(RegisterID reg) { return reg == zr; }
}

class ARM64Assembler {
public:
    typedef ARM64Registers::RegisterID RegisterID;
    typedef ARM64Registers::FPRegisterID FPRegisterID;
    
    static RegisterID firstRegister() { return ARM64Registers::x0; }
    static RegisterID lastRegister() { return ARM64Registers::x28; }
    
    static FPRegisterID firstFPRegister() { return ARM64Registers::q0; }
    static FPRegisterID lastFPRegister() { return ARM64Registers::q31; }

private:
    static bool isSp(RegisterID reg) { return ARM64Registers::isSp(reg); }
    static bool isZr(RegisterID reg) { return ARM64Registers::isZr(reg); }

public:
    ARM64Assembler()
        : m_indexOfLastWatchpoint(INT_MIN)
        , m_indexOfTailOfLastWatchpoint(INT_MIN)
    {
    }
    
    AssemblerBuffer& buffer() { return m_buffer; }

    // (HS, LO, HI, LS) -> (AE, B, A, BE)
    // (VS, VC) -> (O, NO)
    typedef enum {
        ConditionEQ,
        ConditionNE,
        ConditionHS, ConditionCS = ConditionHS,
        ConditionLO, ConditionCC = ConditionLO,
        ConditionMI,
        ConditionPL,
        ConditionVS,
        ConditionVC,
        ConditionHI,
        ConditionLS,
        ConditionGE,
        ConditionLT,
        ConditionGT,
        ConditionLE,
        ConditionAL,
        ConditionInvalid
    } Condition;

    static Condition invert(Condition cond)
    {
        return static_cast<Condition>(cond ^ 1);
    }

    typedef enum {
        LSL,
        LSR,
        ASR,
        ROR
    } ShiftType;

    typedef enum {
        UXTB,
        UXTH,
        UXTW,
        UXTX,
        SXTB,
        SXTH,
        SXTW,
        SXTX
    } ExtendType;

    enum SetFlags {
        DontSetFlags,
        S
    };

#define JUMP_ENUM_WITH_SIZE(index, value) (((value) << 4) | (index))
#define JUMP_ENUM_SIZE(jump) ((jump) >> 4)
    enum JumpType { JumpFixed = JUMP_ENUM_WITH_SIZE(0, 0),
        JumpNoCondition = JUMP_ENUM_WITH_SIZE(1, 1 * sizeof(uint32_t)),
        JumpCondition = JUMP_ENUM_WITH_SIZE(2, 2 * sizeof(uint32_t)),
        JumpCompareAndBranch = JUMP_ENUM_WITH_SIZE(3, 2 * sizeof(uint32_t)),
        JumpTestBit = JUMP_ENUM_WITH_SIZE(4, 2 * sizeof(uint32_t)),
        JumpNoConditionFixedSize = JUMP_ENUM_WITH_SIZE(5, 1 * sizeof(uint32_t)),
        JumpConditionFixedSize = JUMP_ENUM_WITH_SIZE(6, 2 * sizeof(uint32_t)),
        JumpCompareAndBranchFixedSize = JUMP_ENUM_WITH_SIZE(7, 2 * sizeof(uint32_t)),
        JumpTestBitFixedSize = JUMP_ENUM_WITH_SIZE(8, 2 * sizeof(uint32_t)),
    };
    enum JumpLinkType {
        LinkInvalid = JUMP_ENUM_WITH_SIZE(0, 0),
        LinkJumpNoCondition = JUMP_ENUM_WITH_SIZE(1, 1 * sizeof(uint32_t)),
        LinkJumpConditionDirect = JUMP_ENUM_WITH_SIZE(2, 1 * sizeof(uint32_t)),
        LinkJumpCondition = JUMP_ENUM_WITH_SIZE(3, 2 * sizeof(uint32_t)),
        LinkJumpCompareAndBranch = JUMP_ENUM_WITH_SIZE(4, 2 * sizeof(uint32_t)),
        LinkJumpCompareAndBranchDirect = JUMP_ENUM_WITH_SIZE(5, 1 * sizeof(uint32_t)),
        LinkJumpTestBit = JUMP_ENUM_WITH_SIZE(6, 2 * sizeof(uint32_t)),
        LinkJumpTestBitDirect = JUMP_ENUM_WITH_SIZE(7, 1 * sizeof(uint32_t)),
    };

    class LinkRecord {
    public:
        LinkRecord(intptr_t from, intptr_t to, JumpType type, Condition condition)
        {
            data.realTypes.m_from = from;
            data.realTypes.m_to = to;
            data.realTypes.m_type = type;
            data.realTypes.m_linkType = LinkInvalid;
            data.realTypes.m_condition = condition;
        }
        LinkRecord(intptr_t from, intptr_t to, JumpType type, Condition condition, bool is64Bit, RegisterID compareRegister)
        {
            data.realTypes.m_from = from;
            data.realTypes.m_to = to;
            data.realTypes.m_type = type;
            data.realTypes.m_linkType = LinkInvalid;
            data.realTypes.m_condition = condition;
            data.realTypes.m_is64Bit = is64Bit;
            data.realTypes.m_compareRegister = compareRegister;
        }
        LinkRecord(intptr_t from, intptr_t to, JumpType type, Condition condition, unsigned bitNumber, RegisterID compareRegister)
        {
            data.realTypes.m_from = from;
            data.realTypes.m_to = to;
            data.realTypes.m_type = type;
            data.realTypes.m_linkType = LinkInvalid;
            data.realTypes.m_condition = condition;
            data.realTypes.m_bitNumber = bitNumber;
            data.realTypes.m_compareRegister = compareRegister;
        }
        void operator=(const LinkRecord& other)
        {
            data.copyTypes.content[0] = other.data.copyTypes.content[0];
            data.copyTypes.content[1] = other.data.copyTypes.content[1];
            data.copyTypes.content[2] = other.data.copyTypes.content[2];
        }
        intptr_t from() const { return data.realTypes.m_from; }
        void setFrom(intptr_t from) { data.realTypes.m_from = from; }
        intptr_t to() const { return data.realTypes.m_to; }
        JumpType type() const { return data.realTypes.m_type; }
        JumpLinkType linkType() const { return data.realTypes.m_linkType; }
        void setLinkType(JumpLinkType linkType) { ASSERT(data.realTypes.m_linkType == LinkInvalid); data.realTypes.m_linkType = linkType; }
        Condition condition() const { return data.realTypes.m_condition; }
        bool is64Bit() const { return data.realTypes.m_is64Bit; }
        unsigned bitNumber() const { return data.realTypes.m_bitNumber; }
        RegisterID compareRegister() const { return data.realTypes.m_compareRegister; }

    private:
        union {
            struct RealTypes {
                intptr_t m_from : 48;
                intptr_t m_to : 48;
                JumpType m_type : 8;
                JumpLinkType m_linkType : 8;
                Condition m_condition : 4;
                bool m_is64Bit : 1;
                unsigned m_bitNumber : 6;
                RegisterID m_compareRegister : 5;
            } realTypes;
            struct CopyTypes {
                uint64_t content[3];
            } copyTypes;
            COMPILE_ASSERT(sizeof(RealTypes) == sizeof(CopyTypes), LinkRecordCopyStructSizeEqualsRealStruct);
        } data;
    };

    // bits(N) VFPExpandImm(bits(8) imm8);
    //
    // Encoding of floating point immediates is a litte complicated. Here's a
    // high level description:
    //     +/-m*2-n where m and n are integers, 16 <= m <= 31, 0 <= n <= 7
    // and the algirithm for expanding to a single precision float:
    //     return imm8<7>:NOT(imm8<6>):Replicate(imm8<6>,5):imm8<5:0>:Zeros(19);
    //
    // The trickiest bit is how the exponent is handled. The following table
    // may help clarify things a little:
    //     654
    //     100 01111100 124 -3 1020 01111111100
    //     101 01111101 125 -2 1021 01111111101
    //     110 01111110 126 -1 1022 01111111110
    //     111 01111111 127  0 1023 01111111111
    //     000 10000000 128  1 1024 10000000000
    //     001 10000001 129  2 1025 10000000001
    //     010 10000010 130  3 1026 10000000010
    //     011 10000011 131  4 1027 10000000011
    // The first column shows the bit pattern stored in bits 6-4 of the arm
    // encoded immediate. The second column shows the 8-bit IEEE 754 single
    // -precision exponent in binary, the third column shows the raw decimal
    // value. IEEE 754 single-precision numbers are stored with a bias of 127
    // to the exponent, so the fourth column shows the resulting exponent.
    // From this was can see that the exponent can be in the range -3..4,
    // which agrees with the high level description given above. The fifth
    // and sixth columns shows the value stored in a IEEE 754 double-precision
    // number to represent these exponents in decimal and binary, given the
    // bias of 1023.
    //
    // Ultimately, detecting doubles that can be encoded as immediates on arm
    // and encoding doubles is actually not too bad. A floating point value can
    // be encoded by retaining the sign bit, the low three bits of the exponent
    // and the high 4 bits of the mantissa. To validly be able to encode an
    // immediate the remainder of the mantissa must be zero, and the high part
    // of the exponent must match the top bit retained, bar the highest bit
    // which must be its inverse.
    static bool canEncodeFPImm(double d)
    {
        // Discard the sign bit, the low two bits of the exponent & the highest
        // four bits of the mantissa.
        uint64_t masked = bitwise_cast<uint64_t>(d) & 0x7fc0ffffffffffffull;
        return (masked == 0x3fc0000000000000ull) || (masked == 0x4000000000000000ull);
    }

    template<int datasize>
    static bool canEncodePImmOffset(int32_t offset)
    {
        int32_t maxPImm = 4095 * (datasize / 8);
        if (offset < 0)
            return false;
        if (offset > maxPImm)
            return false;
        if (offset & ((datasize / 8 ) - 1))
            return false;
        return true;
    }

    static bool canEncodeSImmOffset(int32_t offset)
    {
        return isInt9(offset);
    }

private:
    int encodeFPImm(double d)
    {
        ASSERT(canEncodeFPImm(d));
        uint64_t u64 = bitwise_cast<uint64_t>(d);
        return (static_cast<int>(u64 >> 56) & 0x80) | (static_cast<int>(u64 >> 48) & 0x7f);
    }

    template<int datasize>
    int encodeShiftAmount(int amount)
    {
        ASSERT(!amount || datasize == (8 << amount));
        return amount;
    }

    template<int datasize>
    static int encodePositiveImmediate(unsigned pimm)
    {
        ASSERT(!(pimm & ((datasize / 8) - 1)));
        return pimm / (datasize / 8);
    }

    enum Datasize {
        Datasize_32,
        Datasize_64,
        Datasize_64_top,
        Datasize_16
    };

    enum MemOpSize {
        MemOpSize_8_or_128,
        MemOpSize_16,
        MemOpSize_32,
        MemOpSize_64,
    };

    enum BranchType {
        BranchType_JMP,
        BranchType_CALL,
        BranchType_RET
    };

    enum AddOp {
        AddOp_ADD,
        AddOp_SUB
    };

    enum BitfieldOp {
        BitfieldOp_SBFM,
        BitfieldOp_BFM,
        BitfieldOp_UBFM
    };

    enum DataOp1Source {
        DataOp_RBIT,
        DataOp_REV16,
        DataOp_REV32,
        DataOp_REV64,
        DataOp_CLZ,
        DataOp_CLS
    };

    enum DataOp2Source {
        DataOp_UDIV = 2,
        DataOp_SDIV = 3,
        DataOp_LSLV = 8,
        DataOp_LSRV = 9,
        DataOp_ASRV = 10,
        DataOp_RORV = 11
    };

    enum DataOp3Source {
        DataOp_MADD = 0,
        DataOp_MSUB = 1,
        DataOp_SMADDL = 2,
        DataOp_SMSUBL = 3,
        DataOp_SMULH = 4,
        DataOp_UMADDL = 10,
        DataOp_UMSUBL = 11,
        DataOp_UMULH = 12
    };

    enum ExcepnOp {
        ExcepnOp_EXCEPTION = 0,
        ExcepnOp_BREAKPOINT = 1,
        ExcepnOp_HALT = 2,
        ExcepnOp_DCPS = 5
    };

    enum FPCmpOp {
        FPCmpOp_FCMP = 0x00,
        FPCmpOp_FCMP0 = 0x08,
        FPCmpOp_FCMPE = 0x10,
        FPCmpOp_FCMPE0 = 0x18
    };

    enum FPCondCmpOp {
        FPCondCmpOp_FCMP,
        FPCondCmpOp_FCMPE
    };

    enum FPDataOp1Source {
        FPDataOp_FMOV = 0,
        FPDataOp_FABS = 1,
        FPDataOp_FNEG = 2,
        FPDataOp_FSQRT = 3,
        FPDataOp_FCVT_toSingle = 4,
        FPDataOp_FCVT_toDouble = 5,
        FPDataOp_FCVT_toHalf = 7,
        FPDataOp_FRINTN = 8,
        FPDataOp_FRINTP = 9,
        FPDataOp_FRINTM = 10,
        FPDataOp_FRINTZ = 11,
        FPDataOp_FRINTA = 12,
        FPDataOp_FRINTX = 14,
        FPDataOp_FRINTI = 15
    };

    enum FPDataOp2Source {
        FPDataOp_FMUL,
        FPDataOp_FDIV,
        FPDataOp_FADD,
        FPDataOp_FSUB,
        FPDataOp_FMAX,
        FPDataOp_FMIN,
        FPDataOp_FMAXNM,
        FPDataOp_FMINNM,
        FPDataOp_FNMUL
    };

    enum FPIntConvOp {
        FPIntConvOp_FCVTNS = 0x00,
        FPIntConvOp_FCVTNU = 0x01,
        FPIntConvOp_SCVTF = 0x02,
        FPIntConvOp_UCVTF = 0x03,
        FPIntConvOp_FCVTAS = 0x04,
        FPIntConvOp_FCVTAU = 0x05,
        FPIntConvOp_FMOV_QtoX = 0x06,
        FPIntConvOp_FMOV_XtoQ = 0x07,
        FPIntConvOp_FCVTPS = 0x08,
        FPIntConvOp_FCVTPU = 0x09,
        FPIntConvOp_FMOV_QtoX_top = 0x0e,
        FPIntConvOp_FMOV_XtoQ_top = 0x0f,
        FPIntConvOp_FCVTMS = 0x10,
        FPIntConvOp_FCVTMU = 0x11,
        FPIntConvOp_FCVTZS = 0x18,
        FPIntConvOp_FCVTZU = 0x19,
    };

    enum LogicalOp {
        LogicalOp_AND,
        LogicalOp_ORR,
        LogicalOp_EOR,
        LogicalOp_ANDS
    };

    enum MemOp {
        MemOp_STORE,
        MemOp_LOAD,
        MemOp_STORE_V128, 
        MemOp_LOAD_V128,
        MemOp_PREFETCH = 2, // size must be 3
        MemOp_LOAD_signed64 = 2, // size may be 0, 1 or 2
        MemOp_LOAD_signed32 = 3 // size may be 0 or 1
    };

    enum MemPairOpSize {
        MemPairOp_32 = 0,
        MemPairOp_LoadSigned_32 = 1,
        MemPairOp_64 = 2,

        MemPairOp_V32 = MemPairOp_32,
        MemPairOp_V64 = 1,
        MemPairOp_V128 = 2
    };

    enum MoveWideOp {
        MoveWideOp_N = 0,
        MoveWideOp_Z = 2,
        MoveWideOp_K = 3 
    };

    enum LdrLiteralOp {
        LdrLiteralOp_32BIT = 0,
        LdrLiteralOp_64BIT = 1,
        LdrLiteralOp_LDRSW = 2,
        LdrLiteralOp_128BIT = 2
    };

    static unsigned memPairOffsetShift(bool V, MemPairOpSize size)
    {
        // return the log2 of the size in bytes, e.g. 64 bit size returns 3
        if (V)
            return size + 2;
        return (size >> 1) + 2;
    }

public:
    // Integer Instructions:

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void adc(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(addSubtractWithCarry(DATASIZE, AddOp_ADD, setFlags, rm, rn, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void add(RegisterID rd, RegisterID rn, UInt12 imm12, int shift = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!shift || shift == 12);
        insn(addSubtractImmediate(DATASIZE, AddOp_ADD, setFlags, shift == 12, imm12, rn, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void add(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        add<datasize, setFlags>(rd, rn, rm, LSL, 0);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void add(RegisterID rd, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_DATASIZE();
        insn(addSubtractExtendedRegister(DATASIZE, AddOp_ADD, setFlags, rm, extend, amount, rn, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void add(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        if (isSp(rd) || isSp(rn)) {
            ASSERT(shift == LSL);
            ASSERT(!isSp(rm));
            add<datasize, setFlags>(rd, rn, rm, UXTX, amount);
        } else
            insn(addSubtractShiftedRegister(DATASIZE, AddOp_ADD, setFlags, shift, rm, amount, rn, rd));
    }

    ALWAYS_INLINE void adr(RegisterID rd, int offset)
    {
        insn(pcRelative(false, offset, rd));
    }

    ALWAYS_INLINE void adrp(RegisterID rd, int offset)
    {
        ASSERT(!(offset & 0xfff));
        insn(pcRelative(true, offset >> 12, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void and_(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        and_<datasize, setFlags>(rd, rn, rm, LSL, 0);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void and_(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        insn(logicalShiftedRegister(DATASIZE, setFlags ? LogicalOp_ANDS : LogicalOp_AND, shift, false, rm, amount, rn, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void and_(RegisterID rd, RegisterID rn, LogicalImmediate imm)
    {
        CHECK_DATASIZE();
        insn(logicalImmediate(DATASIZE, setFlags ? LogicalOp_ANDS : LogicalOp_AND, imm.value(), rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void asr(RegisterID rd, RegisterID rn, int shift)
    {
        ASSERT(shift < datasize);
        sbfm<datasize>(rd, rn, shift, datasize - 1);
    }

    template<int datasize>
    ALWAYS_INLINE void asr(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        asrv<datasize>(rd, rn, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void asrv(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(dataProcessing2Source(DATASIZE, rm, DataOp_ASRV, rn, rd));
    }

    ALWAYS_INLINE void b(int32_t offset = 0)
    {
        ASSERT(!(offset & 3));
        offset >>= 2;
        ASSERT(offset == (offset << 6) >> 6);
        insn(unconditionalBranchImmediate(false, offset));
    }

    ALWAYS_INLINE void b_cond(Condition cond, int32_t offset = 0)
    {
        ASSERT(!(offset & 3));
        offset >>= 2;
        ASSERT(offset == (offset << 13) >> 13);
        insn(conditionalBranchImmediate(offset, cond));
    }

    template<int datasize>
    ALWAYS_INLINE void bfi(RegisterID rd, RegisterID rn, int lsb, int width)
    {
        bfm<datasize>(rd, rn, (datasize - lsb) & (datasize - 1), width - 1);
    }

    template<int datasize>
    ALWAYS_INLINE void bfm(RegisterID rd, RegisterID rn, int immr, int imms)
    {
        CHECK_DATASIZE();
        insn(bitfield(DATASIZE, BitfieldOp_BFM, immr, imms, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void bfxil(RegisterID rd, RegisterID rn, int lsb, int width)
    {
        bfm<datasize>(rd, rn, lsb, lsb + width - 1);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void bic(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        bic<datasize, setFlags>(rd, rn, rm, LSL, 0);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void bic(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        insn(logicalShiftedRegister(DATASIZE, setFlags ? LogicalOp_ANDS : LogicalOp_AND, shift, true, rm, amount, rn, rd));
    }

    ALWAYS_INLINE void bl(int32_t offset = 0)
    {
        ASSERT(!(offset & 3));
        offset >>= 2;
        insn(unconditionalBranchImmediate(true, offset));
    }

    ALWAYS_INLINE void blr(RegisterID rn)
    {
        insn(unconditionalBranchRegister(BranchType_CALL, rn));
    }

    ALWAYS_INLINE void br(RegisterID rn)
    {
        insn(unconditionalBranchRegister(BranchType_JMP, rn));
    }

    ALWAYS_INLINE void brk(uint16_t imm)
    {
        insn(excepnGeneration(ExcepnOp_BREAKPOINT, imm, 0));
    }
    
    template<int datasize>
    ALWAYS_INLINE void cbnz(RegisterID rt, int32_t offset = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!(offset & 3));
        offset >>= 2;
        insn(compareAndBranchImmediate(DATASIZE, true, offset, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void cbz(RegisterID rt, int32_t offset = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!(offset & 3));
        offset >>= 2;
        insn(compareAndBranchImmediate(DATASIZE, false, offset, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ccmn(RegisterID rn, RegisterID rm, int nzcv, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalCompareRegister(DATASIZE, AddOp_ADD, rm, cond, rn, nzcv));
    }

    template<int datasize>
    ALWAYS_INLINE void ccmn(RegisterID rn, UInt5 imm, int nzcv, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalCompareImmediate(DATASIZE, AddOp_ADD, imm, cond, rn, nzcv));
    }

    template<int datasize>
    ALWAYS_INLINE void ccmp(RegisterID rn, RegisterID rm, int nzcv, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalCompareRegister(DATASIZE, AddOp_SUB, rm, cond, rn, nzcv));
    }

    template<int datasize>
    ALWAYS_INLINE void ccmp(RegisterID rn, UInt5 imm, int nzcv, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalCompareImmediate(DATASIZE, AddOp_SUB, imm, cond, rn, nzcv));
    }

    template<int datasize>
    ALWAYS_INLINE void cinc(RegisterID rd, RegisterID rn, Condition cond)
    {
        csinc<datasize>(rd, rn, rn, invert(cond));
    }

    template<int datasize>
    ALWAYS_INLINE void cinv(RegisterID rd, RegisterID rn, Condition cond)
    {
        csinv<datasize>(rd, rn, rn, invert(cond));
    }

    template<int datasize>
    ALWAYS_INLINE void cls(RegisterID rd, RegisterID rn)
    {
        CHECK_DATASIZE();
        insn(dataProcessing1Source(DATASIZE, DataOp_CLS, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void clz(RegisterID rd, RegisterID rn)
    {
        CHECK_DATASIZE();
        insn(dataProcessing1Source(DATASIZE, DataOp_CLZ, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void cmn(RegisterID rn, UInt12 imm12, int shift = 0)
    {
        add<datasize, S>(ARM64Registers::zr, rn, imm12, shift);
    }

    template<int datasize>
    ALWAYS_INLINE void cmn(RegisterID rn, RegisterID rm)
    {
        add<datasize, S>(ARM64Registers::zr, rn, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void cmn(RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        add<datasize, S>(ARM64Registers::zr, rn, rm, extend, amount);
    }

    template<int datasize>
    ALWAYS_INLINE void cmn(RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        add<datasize, S>(ARM64Registers::zr, rn, rm, shift, amount);
    }

    template<int datasize>
    ALWAYS_INLINE void cmp(RegisterID rn, UInt12 imm12, int shift = 0)
    {
        sub<datasize, S>(ARM64Registers::zr, rn, imm12, shift);
    }

    template<int datasize>
    ALWAYS_INLINE void cmp(RegisterID rn, RegisterID rm)
    {
        sub<datasize, S>(ARM64Registers::zr, rn, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void cmp(RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        sub<datasize, S>(ARM64Registers::zr, rn, rm, extend, amount);
    }

    template<int datasize>
    ALWAYS_INLINE void cmp(RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        sub<datasize, S>(ARM64Registers::zr, rn, rm, shift, amount);
    }

    template<int datasize>
    ALWAYS_INLINE void cneg(RegisterID rd, RegisterID rn, Condition cond)
    {
        csneg<datasize>(rd, rn, rn, invert(cond));
    }

    template<int datasize>
    ALWAYS_INLINE void csel(RegisterID rd, RegisterID rn, RegisterID rm, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalSelect(DATASIZE, false, rm, cond, false, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void cset(RegisterID rd, Condition cond)
    {
        csinc<datasize>(rd, ARM64Registers::zr, ARM64Registers::zr, invert(cond));
    }

    template<int datasize>
    ALWAYS_INLINE void csetm(RegisterID rd, Condition cond)
    {
        csinv<datasize>(rd, ARM64Registers::zr, ARM64Registers::zr, invert(cond));
    }

    template<int datasize>
    ALWAYS_INLINE void csinc(RegisterID rd, RegisterID rn, RegisterID rm, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalSelect(DATASIZE, false, rm, cond, true, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void csinv(RegisterID rd, RegisterID rn, RegisterID rm, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalSelect(DATASIZE, true, rm, cond, false, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void csneg(RegisterID rd, RegisterID rn, RegisterID rm, Condition cond)
    {
        CHECK_DATASIZE();
        insn(conditionalSelect(DATASIZE, true, rm, cond, true, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void eon(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        eon<datasize>(rd, rn, rm, LSL, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void eon(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        insn(logicalShiftedRegister(DATASIZE, LogicalOp_EOR, shift, true, rm, amount, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void eor(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        eor<datasize>(rd, rn, rm, LSL, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void eor(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        insn(logicalShiftedRegister(DATASIZE, LogicalOp_EOR, shift, false, rm, amount, rn, rd));
    }
    
    template<int datasize>
    ALWAYS_INLINE void eor(RegisterID rd, RegisterID rn, LogicalImmediate imm)
    {
        CHECK_DATASIZE();
        insn(logicalImmediate(DATASIZE, LogicalOp_EOR, imm.value(), rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void extr(RegisterID rd, RegisterID rn, RegisterID rm, int lsb)
    {
        CHECK_DATASIZE();
        insn(extract(DATASIZE, rm, lsb, rn, rd));
    }

    ALWAYS_INLINE void hint(int imm)
    {
        insn(hintPseudo(imm));
    }

    ALWAYS_INLINE void hlt(uint16_t imm)
    {
        insn(excepnGeneration(ExcepnOp_HALT, imm, 0));
    }

    template<int datasize>
    ALWAYS_INLINE void ldp(RegisterID rt, RegisterID rt2, RegisterID rn, PairPostIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPairPostIndex(MEMPAIROPSIZE_INT(datasize), false, MemOp_LOAD, simm, rn, rt, rt2));
    }

    template<int datasize>
    ALWAYS_INLINE void ldp(RegisterID rt, RegisterID rt2, RegisterID rn, PairPreIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPairPreIndex(MEMPAIROPSIZE_INT(datasize), false, MemOp_LOAD, simm, rn, rt, rt2));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        ldr<datasize>(rt, rn, rm, UXTX, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterRegisterOffset(MEMOPSIZE, false, MemOp_LOAD, rm, extend, encodeShiftAmount<datasize>(amount), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnsignedImmediate(MEMOPSIZE, false, MemOp_LOAD, encodePositiveImmediate<datasize>(pimm), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPostIndex(MEMOPSIZE, false, MemOp_LOAD, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPreIndex(MEMOPSIZE, false, MemOp_LOAD, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr_literal(RegisterID rt, int offset = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!(offset & 3));
        insn(loadRegisterLiteral(datasize == 64 ? LdrLiteralOp_64BIT : LdrLiteralOp_32BIT, false, offset >> 2, rt));
    }

    ALWAYS_INLINE void ldrb(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        // Not calling the 5 argument form of ldrb, since is amount is ommitted S is false.
        insn(loadStoreRegisterRegisterOffset(MemOpSize_8_or_128, false, MemOp_LOAD, rm, UXTX, false, rn, rt));
    }

    ALWAYS_INLINE void ldrb(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        ASSERT_UNUSED(amount, !amount);
        insn(loadStoreRegisterRegisterOffset(MemOpSize_8_or_128, false, MemOp_LOAD, rm, extend, true, rn, rt));
    }

    ALWAYS_INLINE void ldrb(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        insn(loadStoreRegisterUnsignedImmediate(MemOpSize_8_or_128, false, MemOp_LOAD, encodePositiveImmediate<8>(pimm), rn, rt));
    }

    ALWAYS_INLINE void ldrb(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        insn(loadStoreRegisterPostIndex(MemOpSize_8_or_128, false, MemOp_LOAD, simm, rn, rt));
    }

    ALWAYS_INLINE void ldrb(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        insn(loadStoreRegisterPreIndex(MemOpSize_8_or_128, false, MemOp_LOAD, simm, rn, rt));
    }

    ALWAYS_INLINE void ldrh(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        ldrh(rt, rn, rm, UXTX, 0);
    }

    ALWAYS_INLINE void ldrh(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        ASSERT(!amount || amount == 1);
        insn(loadStoreRegisterRegisterOffset(MemOpSize_16, false, MemOp_LOAD, rm, extend, amount == 1, rn, rt));
    }

    ALWAYS_INLINE void ldrh(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        insn(loadStoreRegisterUnsignedImmediate(MemOpSize_16, false, MemOp_LOAD, encodePositiveImmediate<16>(pimm), rn, rt));
    }

    ALWAYS_INLINE void ldrh(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        insn(loadStoreRegisterPostIndex(MemOpSize_16, false, MemOp_LOAD, simm, rn, rt));
    }

    ALWAYS_INLINE void ldrh(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        insn(loadStoreRegisterPreIndex(MemOpSize_16, false, MemOp_LOAD, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsb(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        // Not calling the 5 argument form of ldrsb, since is amount is ommitted S is false.
        insn(loadStoreRegisterRegisterOffset(MemOpSize_8_or_128, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, rm, UXTX, false, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsb(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_DATASIZE();
        ASSERT_UNUSED(amount, !amount);
        insn(loadStoreRegisterRegisterOffset(MemOpSize_8_or_128, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, rm, extend, true, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsb(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnsignedImmediate(MemOpSize_8_or_128, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, encodePositiveImmediate<8>(pimm), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsb(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPostIndex(MemOpSize_8_or_128, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsb(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPreIndex(MemOpSize_8_or_128, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsh(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        ldrsh<datasize>(rt, rn, rm, UXTX, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsh(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_DATASIZE();
        ASSERT(!amount || amount == 1);
        insn(loadStoreRegisterRegisterOffset(MemOpSize_16, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, rm, extend, amount == 1, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsh(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnsignedImmediate(MemOpSize_16, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, encodePositiveImmediate<16>(pimm), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsh(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPostIndex(MemOpSize_16, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldrsh(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPreIndex(MemOpSize_16, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, simm, rn, rt));
    }

    ALWAYS_INLINE void ldrsw(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        ldrsw(rt, rn, rm, UXTX, 0);
    }

    ALWAYS_INLINE void ldrsw(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        ASSERT(!amount || amount == 2);
        insn(loadStoreRegisterRegisterOffset(MemOpSize_32, false, MemOp_LOAD_signed64, rm, extend, amount == 2, rn, rt));
    }

    ALWAYS_INLINE void ldrsw(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        insn(loadStoreRegisterUnsignedImmediate(MemOpSize_32, false, MemOp_LOAD_signed64, encodePositiveImmediate<32>(pimm), rn, rt));
    }

    ALWAYS_INLINE void ldrsw(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        insn(loadStoreRegisterPostIndex(MemOpSize_32, false, MemOp_LOAD_signed64, simm, rn, rt));
    }

    ALWAYS_INLINE void ldrsw(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        insn(loadStoreRegisterPreIndex(MemOpSize_32, false, MemOp_LOAD_signed64, simm, rn, rt));
    }

    ALWAYS_INLINE void ldrsw_literal(RegisterID rt, int offset = 0)
    {
        ASSERT(!(offset & 3));
        insn(loadRegisterLiteral(LdrLiteralOp_LDRSW, false, offset >> 2, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldur(RegisterID rt, RegisterID rn, int simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnscaledImmediate(MEMOPSIZE, false, MemOp_LOAD, simm, rn, rt));
    }

    ALWAYS_INLINE void ldurb(RegisterID rt, RegisterID rn, int simm)
    {
        insn(loadStoreRegisterUnscaledImmediate(MemOpSize_8_or_128, false, MemOp_LOAD, simm, rn, rt));
    }

    ALWAYS_INLINE void ldurh(RegisterID rt, RegisterID rn, int simm)
    {
        insn(loadStoreRegisterUnscaledImmediate(MemOpSize_16, false, MemOp_LOAD, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldursb(RegisterID rt, RegisterID rn, int simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnscaledImmediate(MemOpSize_8_or_128, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldursh(RegisterID rt, RegisterID rn, int simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnscaledImmediate(MemOpSize_16, false, (datasize == 64) ? MemOp_LOAD_signed64 : MemOp_LOAD_signed32, simm, rn, rt));
    }

    ALWAYS_INLINE void ldursw(RegisterID rt, RegisterID rn, int simm)
    {
        insn(loadStoreRegisterUnscaledImmediate(MemOpSize_32, false, MemOp_LOAD_signed64, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void lsl(RegisterID rd, RegisterID rn, int shift)
    {
        ASSERT(shift < datasize);
        ubfm<datasize>(rd, rn, (datasize - shift) & (datasize - 1), datasize - 1 - shift);
    }

    template<int datasize>
    ALWAYS_INLINE void lsl(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        lslv<datasize>(rd, rn, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void lslv(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(dataProcessing2Source(DATASIZE, rm, DataOp_LSLV, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void lsr(RegisterID rd, RegisterID rn, int shift)
    {
        ASSERT(shift < datasize);
        ubfm<datasize>(rd, rn, shift, datasize - 1);
    }

    template<int datasize>
    ALWAYS_INLINE void lsr(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        lsrv<datasize>(rd, rn, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void lsrv(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(dataProcessing2Source(DATASIZE, rm, DataOp_LSRV, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void madd(RegisterID rd, RegisterID rn, RegisterID rm, RegisterID ra)
    {
        CHECK_DATASIZE();
        insn(dataProcessing3Source(DATASIZE, DataOp_MADD, rm, ra, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void mneg(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        msub<datasize>(rd, rn, rm, ARM64Registers::zr);
    }

    template<int datasize>
    ALWAYS_INLINE void mov(RegisterID rd, RegisterID rm)
    {
        if (isSp(rd) || isSp(rm))
            add<datasize>(rd, rm, UInt12(0));
        else
            orr<datasize>(rd, ARM64Registers::zr, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void movi(RegisterID rd, LogicalImmediate imm)
    {
        orr<datasize>(rd, ARM64Registers::zr, imm);
    }

    template<int datasize>
    ALWAYS_INLINE void movk(RegisterID rd, uint16_t value, int shift = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!(shift & 0xf));
        insn(moveWideImediate(DATASIZE, MoveWideOp_K, shift >> 4, value, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void movn(RegisterID rd, uint16_t value, int shift = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!(shift & 0xf));
        insn(moveWideImediate(DATASIZE, MoveWideOp_N, shift >> 4, value, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void movz(RegisterID rd, uint16_t value, int shift = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!(shift & 0xf));
        insn(moveWideImediate(DATASIZE, MoveWideOp_Z, shift >> 4, value, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void msub(RegisterID rd, RegisterID rn, RegisterID rm, RegisterID ra)
    {
        CHECK_DATASIZE();
        insn(dataProcessing3Source(DATASIZE, DataOp_MSUB, rm, ra, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void mul(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        madd<datasize>(rd, rn, rm, ARM64Registers::zr);
    }

    template<int datasize>
    ALWAYS_INLINE void mvn(RegisterID rd, RegisterID rm)
    {
        orn<datasize>(rd, ARM64Registers::zr, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void mvn(RegisterID rd, RegisterID rm, ShiftType shift, int amount)
    {
        orn<datasize>(rd, ARM64Registers::zr, rm, shift, amount);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void neg(RegisterID rd, RegisterID rm)
    {
        sub<datasize, setFlags>(rd, ARM64Registers::zr, rm);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void neg(RegisterID rd, RegisterID rm, ShiftType shift, int amount)
    {
        sub<datasize, setFlags>(rd, ARM64Registers::zr, rm, shift, amount);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void ngc(RegisterID rd, RegisterID rm)
    {
        sbc<datasize, setFlags>(rd, ARM64Registers::zr, rm);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void ngc(RegisterID rd, RegisterID rm, ShiftType shift, int amount)
    {
        sbc<datasize, setFlags>(rd, ARM64Registers::zr, rm, shift, amount);
    }

    ALWAYS_INLINE void nop()
    {
        insn(nopPseudo());
    }
    
    static void fillNops(void* base, size_t size)
    {
        RELEASE_ASSERT(!(size % sizeof(int32_t)));
        size_t n = size / sizeof(int32_t);
        for (int32_t* ptr = static_cast<int32_t*>(base); n--;)
            *ptr++ = nopPseudo();
    }
    
    ALWAYS_INLINE void dmbSY()
    {
        insn(0xd5033fbf);
    }

    template<int datasize>
    ALWAYS_INLINE void orn(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        orn<datasize>(rd, rn, rm, LSL, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void orn(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        insn(logicalShiftedRegister(DATASIZE, LogicalOp_ORR, shift, true, rm, amount, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void orr(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        orr<datasize>(rd, rn, rm, LSL, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void orr(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        insn(logicalShiftedRegister(DATASIZE, LogicalOp_ORR, shift, false, rm, amount, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void orr(RegisterID rd, RegisterID rn, LogicalImmediate imm)
    {
        CHECK_DATASIZE();
        insn(logicalImmediate(DATASIZE, LogicalOp_ORR, imm.value(), rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void rbit(RegisterID rd, RegisterID rn)
    {
        CHECK_DATASIZE();
        insn(dataProcessing1Source(DATASIZE, DataOp_RBIT, rn, rd));
    }

    ALWAYS_INLINE void ret(RegisterID rn = ARM64Registers::lr)
    {
        insn(unconditionalBranchRegister(BranchType_RET, rn));
    }

    template<int datasize>
    ALWAYS_INLINE void rev(RegisterID rd, RegisterID rn)
    {
        CHECK_DATASIZE();
        if (datasize == 32) // 'rev' mnemonic means REV32 or REV64 depending on the operand width.
            insn(dataProcessing1Source(Datasize_32, DataOp_REV32, rn, rd));
        else
            insn(dataProcessing1Source(Datasize_64, DataOp_REV64, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void rev16(RegisterID rd, RegisterID rn)
    {
        CHECK_DATASIZE();
        insn(dataProcessing1Source(DATASIZE, DataOp_REV16, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void rev32(RegisterID rd, RegisterID rn)
    {
        ASSERT(datasize == 64); // 'rev32' only valid with 64-bit operands.
        insn(dataProcessing1Source(Datasize_64, DataOp_REV32, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void ror(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        rorv<datasize>(rd, rn, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void ror(RegisterID rd, RegisterID rs, int shift)
    {
        extr<datasize>(rd, rs, rs, shift);
    }

    template<int datasize>
    ALWAYS_INLINE void rorv(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(dataProcessing2Source(DATASIZE, rm, DataOp_RORV, rn, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void sbc(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(addSubtractWithCarry(DATASIZE, AddOp_SUB, setFlags, rm, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void sbfiz(RegisterID rd, RegisterID rn, int lsb, int width)
    {
        sbfm<datasize>(rd, rn, (datasize - lsb) & (datasize - 1), width - 1);
    }

    template<int datasize>
    ALWAYS_INLINE void sbfm(RegisterID rd, RegisterID rn, int immr, int imms)
    {
        CHECK_DATASIZE();
        insn(bitfield(DATASIZE, BitfieldOp_SBFM, immr, imms, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void sbfx(RegisterID rd, RegisterID rn, int lsb, int width)
    {
        sbfm<datasize>(rd, rn, lsb, lsb + width - 1);
    }

    template<int datasize>
    ALWAYS_INLINE void sdiv(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(dataProcessing2Source(DATASIZE, rm, DataOp_SDIV, rn, rd));
    }

    ALWAYS_INLINE void smaddl(RegisterID rd, RegisterID rn, RegisterID rm, RegisterID ra)
    {
        insn(dataProcessing3Source(Datasize_64, DataOp_SMADDL, rm, ra, rn, rd));
    }

    ALWAYS_INLINE void smnegl(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        smsubl(rd, rn, rm, ARM64Registers::zr);
    }

    ALWAYS_INLINE void smsubl(RegisterID rd, RegisterID rn, RegisterID rm, RegisterID ra)
    {
        insn(dataProcessing3Source(Datasize_64, DataOp_SMSUBL, rm, ra, rn, rd));
    }

    ALWAYS_INLINE void smulh(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        insn(dataProcessing3Source(Datasize_64, DataOp_SMULH, rm, ARM64Registers::zr, rn, rd));
    }

    ALWAYS_INLINE void smull(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        smaddl(rd, rn, rm, ARM64Registers::zr);
    }

    template<int datasize>
    ALWAYS_INLINE void stp(RegisterID rt, RegisterID rt2, RegisterID rn, PairPostIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPairPostIndex(MEMPAIROPSIZE_INT(datasize), false, MemOp_STORE, simm, rn, rt, rt2));
    }

    template<int datasize>
    ALWAYS_INLINE void stp(RegisterID rt, RegisterID rt2, RegisterID rn, PairPreIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPairPreIndex(MEMPAIROPSIZE_INT(datasize), false, MemOp_STORE, simm, rn, rt, rt2));
    }

    template<int datasize>
    ALWAYS_INLINE void str(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        str<datasize>(rt, rn, rm, UXTX, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void str(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterRegisterOffset(MEMOPSIZE, false, MemOp_STORE, rm, extend, encodeShiftAmount<datasize>(amount), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void str(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnsignedImmediate(MEMOPSIZE, false, MemOp_STORE, encodePositiveImmediate<datasize>(pimm), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void str(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPostIndex(MEMOPSIZE, false, MemOp_STORE, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void str(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterPreIndex(MEMOPSIZE, false, MemOp_STORE, simm, rn, rt));
    }

    ALWAYS_INLINE void strb(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        // Not calling the 5 argument form of strb, since is amount is ommitted S is false.
        insn(loadStoreRegisterRegisterOffset(MemOpSize_8_or_128, false, MemOp_STORE, rm, UXTX, false, rn, rt));
    }

    ALWAYS_INLINE void strb(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        ASSERT_UNUSED(amount, !amount);
        insn(loadStoreRegisterRegisterOffset(MemOpSize_8_or_128, false, MemOp_STORE, rm, extend, true, rn, rt));
    }

    ALWAYS_INLINE void strb(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        insn(loadStoreRegisterUnsignedImmediate(MemOpSize_8_or_128, false, MemOp_STORE, encodePositiveImmediate<8>(pimm), rn, rt));
    }

    ALWAYS_INLINE void strb(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        insn(loadStoreRegisterPostIndex(MemOpSize_8_or_128, false, MemOp_STORE, simm, rn, rt));
    }

    ALWAYS_INLINE void strb(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        insn(loadStoreRegisterPreIndex(MemOpSize_8_or_128, false, MemOp_STORE, simm, rn, rt));
    }

    ALWAYS_INLINE void strh(RegisterID rt, RegisterID rn, RegisterID rm)
    {
        strh(rt, rn, rm, UXTX, 0);
    }

    ALWAYS_INLINE void strh(RegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        ASSERT(!amount || amount == 1);
        insn(loadStoreRegisterRegisterOffset(MemOpSize_16, false, MemOp_STORE, rm, extend, amount == 1, rn, rt));
    }

    ALWAYS_INLINE void strh(RegisterID rt, RegisterID rn, unsigned pimm)
    {
        insn(loadStoreRegisterUnsignedImmediate(MemOpSize_16, false, MemOp_STORE, encodePositiveImmediate<16>(pimm), rn, rt));
    }

    ALWAYS_INLINE void strh(RegisterID rt, RegisterID rn, PostIndex simm)
    {
        insn(loadStoreRegisterPostIndex(MemOpSize_16, false, MemOp_STORE, simm, rn, rt));
    }

    ALWAYS_INLINE void strh(RegisterID rt, RegisterID rn, PreIndex simm)
    {
        insn(loadStoreRegisterPreIndex(MemOpSize_16, false, MemOp_STORE, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void stur(RegisterID rt, RegisterID rn, int simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnscaledImmediate(MEMOPSIZE, false, MemOp_STORE, simm, rn, rt));
    }

    ALWAYS_INLINE void sturb(RegisterID rt, RegisterID rn, int simm)
    {
        insn(loadStoreRegisterUnscaledImmediate(MemOpSize_8_or_128, false, MemOp_STORE, simm, rn, rt));
    }

    ALWAYS_INLINE void sturh(RegisterID rt, RegisterID rn, int simm)
    {
        insn(loadStoreRegisterUnscaledImmediate(MemOpSize_16, false, MemOp_STORE, simm, rn, rt));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void sub(RegisterID rd, RegisterID rn, UInt12 imm12, int shift = 0)
    {
        CHECK_DATASIZE();
        ASSERT(!shift || shift == 12);
        insn(addSubtractImmediate(DATASIZE, AddOp_SUB, setFlags, shift == 12, imm12, rn, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void sub(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        sub<datasize, setFlags>(rd, rn, rm, LSL, 0);
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void sub(RegisterID rd, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_DATASIZE();
        insn(addSubtractExtendedRegister(DATASIZE, AddOp_SUB, setFlags, rm, extend, amount, rn, rd));
    }

    template<int datasize, SetFlags setFlags = DontSetFlags>
    ALWAYS_INLINE void sub(RegisterID rd, RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        CHECK_DATASIZE();
        if (isSp(rd) || isSp(rn)) {
            ASSERT(shift == LSL);
            ASSERT(!isSp(rm));
            sub<datasize, setFlags>(rd, rn, rm, UXTX, amount);
        } else
            insn(addSubtractShiftedRegister(DATASIZE, AddOp_SUB, setFlags, shift, rm, amount, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void sxtb(RegisterID rd, RegisterID rn)
    {
        sbfm<datasize>(rd, rn, 0, 7);
    }

    template<int datasize>
    ALWAYS_INLINE void sxth(RegisterID rd, RegisterID rn)
    {
        sbfm<datasize>(rd, rn, 0, 15);
    }

    ALWAYS_INLINE void sxtw(RegisterID rd, RegisterID rn)
    {
        sbfm<64>(rd, rn, 0, 31);
    }

    ALWAYS_INLINE void tbz(RegisterID rt, int imm, int offset = 0)
    {
        ASSERT(!(offset & 3));
        offset >>= 2;
        insn(testAndBranchImmediate(false, imm, offset, rt));
    }

    ALWAYS_INLINE void tbnz(RegisterID rt, int imm, int offset = 0)
    {
        ASSERT(!(offset & 3));
        offset >>= 2;
        insn(testAndBranchImmediate(true, imm, offset, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void tst(RegisterID rn, RegisterID rm)
    {
        and_<datasize, S>(ARM64Registers::zr, rn, rm);
    }

    template<int datasize>
    ALWAYS_INLINE void tst(RegisterID rn, RegisterID rm, ShiftType shift, int amount)
    {
        and_<datasize, S>(ARM64Registers::zr, rn, rm, shift, amount);
    }

    template<int datasize>
    ALWAYS_INLINE void tst(RegisterID rn, LogicalImmediate imm)
    {
        and_<datasize, S>(ARM64Registers::zr, rn, imm);
    }

    template<int datasize>
    ALWAYS_INLINE void ubfiz(RegisterID rd, RegisterID rn, int lsb, int width)
    {
        ubfm<datasize>(rd, rn, (datasize - lsb) & (datasize - 1), width - 1);
    }

    template<int datasize>
    ALWAYS_INLINE void ubfm(RegisterID rd, RegisterID rn, int immr, int imms)
    {
        CHECK_DATASIZE();
        insn(bitfield(DATASIZE, BitfieldOp_UBFM, immr, imms, rn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void ubfx(RegisterID rd, RegisterID rn, int lsb, int width)
    {
        ubfm<datasize>(rd, rn, lsb, lsb + width - 1);
    }

    template<int datasize>
    ALWAYS_INLINE void udiv(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        CHECK_DATASIZE();
        insn(dataProcessing2Source(DATASIZE, rm, DataOp_UDIV, rn, rd));
    }

    ALWAYS_INLINE void umaddl(RegisterID rd, RegisterID rn, RegisterID rm, RegisterID ra)
    {
        insn(dataProcessing3Source(Datasize_64, DataOp_UMADDL, rm, ra, rn, rd));
    }

    ALWAYS_INLINE void umnegl(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        umsubl(rd, rn, rm, ARM64Registers::zr);
    }

    ALWAYS_INLINE void umsubl(RegisterID rd, RegisterID rn, RegisterID rm, RegisterID ra)
    {
        insn(dataProcessing3Source(Datasize_64, DataOp_UMSUBL, rm, ra, rn, rd));
    }

    ALWAYS_INLINE void umulh(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        insn(dataProcessing3Source(Datasize_64, DataOp_UMULH, rm, ARM64Registers::zr, rn, rd));
    }

    ALWAYS_INLINE void umull(RegisterID rd, RegisterID rn, RegisterID rm)
    {
        umaddl(rd, rn, rm, ARM64Registers::zr);
    }

    template<int datasize>
    ALWAYS_INLINE void uxtb(RegisterID rd, RegisterID rn)
    {
        ubfm<datasize>(rd, rn, 0, 7);
    }

    template<int datasize>
    ALWAYS_INLINE void uxth(RegisterID rd, RegisterID rn)
    {
        ubfm<datasize>(rd, rn, 0, 15);
    }

    ALWAYS_INLINE void uxtw(RegisterID rd, RegisterID rn)
    {
        ubfm<64>(rd, rn, 0, 31);
    }

    // Floating Point Instructions:

    template<int datasize>
    ALWAYS_INLINE void fabs(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FABS, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fadd(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FADD, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fccmp(FPRegisterID vn, FPRegisterID vm, int nzcv, Condition cond)
    {
        CHECK_DATASIZE();
        insn(floatingPointConditionalCompare(DATASIZE, vm, cond, vn, FPCondCmpOp_FCMP, nzcv));
    }

    template<int datasize>
    ALWAYS_INLINE void fccmpe(FPRegisterID vn, FPRegisterID vm, int nzcv, Condition cond)
    {
        CHECK_DATASIZE();
        insn(floatingPointConditionalCompare(DATASIZE, vm, cond, vn, FPCondCmpOp_FCMPE, nzcv));
    }

    template<int datasize>
    ALWAYS_INLINE void fcmp(FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointCompare(DATASIZE, vm, vn, FPCmpOp_FCMP));
    }

    template<int datasize>
    ALWAYS_INLINE void fcmp_0(FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointCompare(DATASIZE, static_cast<FPRegisterID>(0), vn, FPCmpOp_FCMP0));
    }

    template<int datasize>
    ALWAYS_INLINE void fcmpe(FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointCompare(DATASIZE, vm, vn, FPCmpOp_FCMPE));
    }

    template<int datasize>
    ALWAYS_INLINE void fcmpe_0(FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointCompare(DATASIZE, static_cast<FPRegisterID>(0), vn, FPCmpOp_FCMPE0));
    }

    template<int datasize>
    ALWAYS_INLINE void fcsel(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm, Condition cond)
    {
        CHECK_DATASIZE();
        insn(floatingPointConditionalSelect(DATASIZE, vm, cond, vn, vd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvt(FPRegisterID vd, FPRegisterID vn)
    {
        ASSERT(dstsize == 16 || dstsize == 32 || dstsize == 64);
        ASSERT(srcsize == 16 || srcsize == 32 || srcsize == 64);
        ASSERT(dstsize != srcsize);
        Datasize type = (srcsize == 64) ? Datasize_64 : (srcsize == 32) ? Datasize_32 : Datasize_16;
        FPDataOp1Source opcode = (dstsize == 64) ? FPDataOp_FCVT_toDouble : (dstsize == 32) ? FPDataOp_FCVT_toSingle : FPDataOp_FCVT_toHalf;
        insn(floatingPointDataProcessing1Source(type, opcode, vn, vd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtas(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTAS, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtau(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTAU, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtms(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTMS, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtmu(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTMU, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtns(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTNS, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtnu(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTNU, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtps(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTPS, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtpu(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTPU, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtzs(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTZS, vn, rd));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void fcvtzu(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(dstsize), DATASIZE_OF(srcsize), FPIntConvOp_FCVTZU, vn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void fdiv(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FDIV, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmadd(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm, FPRegisterID va)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing3Source(DATASIZE, false, vm, AddOp_ADD, va, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmax(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FMAX, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmaxnm(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FMAXNM, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmin(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FMIN, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fminnm(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FMINNM, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmov(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FMOV, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmov(FPRegisterID vd, RegisterID rn)
    {
        CHECK_DATASIZE();
        insn(floatingPointIntegerConversions(DATASIZE, DATASIZE, FPIntConvOp_FMOV_XtoQ, rn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmov(RegisterID rd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointIntegerConversions(DATASIZE, DATASIZE, FPIntConvOp_FMOV_QtoX, vn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmov(FPRegisterID vd, double imm)
    {
        CHECK_DATASIZE();
        insn(floatingPointImmediate(DATASIZE, encodeFPImm(imm), vd));
    }

    ALWAYS_INLINE void fmov_top(FPRegisterID vd, RegisterID rn)
    {
        insn(floatingPointIntegerConversions(Datasize_64, Datasize_64, FPIntConvOp_FMOV_XtoQ_top, rn, vd));
    }

    ALWAYS_INLINE void fmov_top(RegisterID rd, FPRegisterID vn)
    {
        insn(floatingPointIntegerConversions(Datasize_64, Datasize_64, FPIntConvOp_FMOV_QtoX_top, vn, rd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmsub(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm, FPRegisterID va)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing3Source(DATASIZE, false, vm, AddOp_SUB, va, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fmul(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FMUL, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fneg(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FNEG, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fnmadd(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm, FPRegisterID va)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing3Source(DATASIZE, true, vm, AddOp_ADD, va, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fnmsub(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm, FPRegisterID va)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing3Source(DATASIZE, true, vm, AddOp_SUB, va, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fnmul(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FNMUL, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void frinta(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FRINTA, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void frinti(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FRINTI, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void frintm(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FRINTM, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void frintn(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FRINTN, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void frintp(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FRINTP, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void frintx(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FRINTX, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void frintz(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FRINTZ, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fsqrt(FPRegisterID vd, FPRegisterID vn)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing1Source(DATASIZE, FPDataOp_FSQRT, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void fsub(FPRegisterID vd, FPRegisterID vn, FPRegisterID vm)
    {
        CHECK_DATASIZE();
        insn(floatingPointDataProcessing2Source(DATASIZE, vm, FPDataOp_FSUB, vn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(FPRegisterID rt, RegisterID rn, RegisterID rm)
    {
        ldr<datasize>(rt, rn, rm, UXTX, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(FPRegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterRegisterOffset(MEMOPSIZE, true, datasize == 128 ? MemOp_LOAD_V128 : MemOp_LOAD, rm, extend, encodeShiftAmount<datasize>(amount), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(FPRegisterID rt, RegisterID rn, unsigned pimm)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterUnsignedImmediate(MEMOPSIZE, true, datasize == 128 ? MemOp_LOAD_V128 : MemOp_LOAD, encodePositiveImmediate<datasize>(pimm), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(FPRegisterID rt, RegisterID rn, PostIndex simm)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterPostIndex(MEMOPSIZE, true, datasize == 128 ? MemOp_LOAD_V128 : MemOp_LOAD, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr(FPRegisterID rt, RegisterID rn, PreIndex simm)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterPreIndex(MEMOPSIZE, true, datasize == 128 ? MemOp_LOAD_V128 : MemOp_LOAD, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldr_literal(FPRegisterID rt, int offset = 0)
    {
        CHECK_FP_MEMOP_DATASIZE();
        ASSERT(datasize >= 32);
        ASSERT(!(offset & 3));
        insn(loadRegisterLiteral(datasize == 128 ? LdrLiteralOp_128BIT : datasize == 64 ? LdrLiteralOp_64BIT : LdrLiteralOp_32BIT, true, offset >> 2, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void ldur(FPRegisterID rt, RegisterID rn, int simm)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterUnscaledImmediate(MEMOPSIZE, true, datasize == 128 ? MemOp_LOAD_V128 : MemOp_LOAD, simm, rn, rt));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void scvtf(FPRegisterID vd, RegisterID rn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(srcsize), DATASIZE_OF(dstsize), FPIntConvOp_SCVTF, rn, vd));
    }

    template<int datasize>
    ALWAYS_INLINE void str(FPRegisterID rt, RegisterID rn, RegisterID rm)
    {
        str<datasize>(rt, rn, rm, UXTX, 0);
    }

    template<int datasize>
    ALWAYS_INLINE void str(FPRegisterID rt, RegisterID rn, RegisterID rm, ExtendType extend, int amount)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterRegisterOffset(MEMOPSIZE, true, datasize == 128 ? MemOp_STORE_V128 : MemOp_STORE, rm, extend, encodeShiftAmount<datasize>(amount), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void str(FPRegisterID rt, RegisterID rn, unsigned pimm)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterUnsignedImmediate(MEMOPSIZE, true, datasize == 128 ? MemOp_STORE_V128 : MemOp_STORE, encodePositiveImmediate<datasize>(pimm), rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void str(FPRegisterID rt, RegisterID rn, PostIndex simm)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterPostIndex(MEMOPSIZE, true, datasize == 128 ? MemOp_STORE_V128 : MemOp_STORE, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void str(FPRegisterID rt, RegisterID rn, PreIndex simm)
    {
        CHECK_FP_MEMOP_DATASIZE();
        insn(loadStoreRegisterPreIndex(MEMOPSIZE, true, datasize == 128 ? MemOp_STORE_V128 : MemOp_STORE, simm, rn, rt));
    }

    template<int datasize>
    ALWAYS_INLINE void stur(FPRegisterID rt, RegisterID rn, int simm)
    {
        CHECK_DATASIZE();
        insn(loadStoreRegisterUnscaledImmediate(MEMOPSIZE, true, datasize == 128 ? MemOp_STORE_V128 : MemOp_STORE, simm, rn, rt));
    }

    template<int dstsize, int srcsize>
    ALWAYS_INLINE void ucvtf(FPRegisterID vd, RegisterID rn)
    {
        CHECK_DATASIZE_OF(dstsize);
        CHECK_DATASIZE_OF(srcsize);
        insn(floatingPointIntegerConversions(DATASIZE_OF(srcsize), DATASIZE_OF(dstsize), FPIntConvOp_UCVTF, rn, vd));
    }

    // Admin methods:

    AssemblerLabel labelIgnoringWatchpoints()
    {
        return m_buffer.label();
    }

    AssemblerLabel labelForWatchpoint()
    {
        AssemblerLabel result = m_buffer.label();
        if (static_cast<int>(result.m_offset) != m_indexOfLastWatchpoint)
            result = label();
        m_indexOfLastWatchpoint = result.m_offset;
        m_indexOfTailOfLastWatchpoint = result.m_offset + maxJumpReplacementSize();
        return result;
    }

    AssemblerLabel label()
    {
        AssemblerLabel result = m_buffer.label();
        while (UNLIKELY(static_cast<int>(result.m_offset) < m_indexOfTailOfLastWatchpoint)) {
            nop();
            result = m_buffer.label();
        }
        return result;
    }

    AssemblerLabel align(int alignment)
    {
        ASSERT(!(alignment & 3));
        while (!m_buffer.isAligned(alignment))
            brk(0);
        return label();
    }
    
    static void* getRelocatedAddress(void* code, AssemblerLabel label)
    {
        ASSERT(label.isSet());
        return reinterpret_cast<void*>(reinterpret_cast<ptrdiff_t>(code) + label.m_offset);
    }
    
    static int getDifferenceBetweenLabels(AssemblerLabel a, AssemblerLabel b)
    {
        return b.m_offset - a.m_offset;
    }

    void* unlinkedCode() { return m_buffer.data(); }
    size_t codeSize() const { return m_buffer.codeSize(); }

    static unsigned getCallReturnOffset(AssemblerLabel call)
    {
        ASSERT(call.isSet());
        return call.m_offset;
    }

    // Linking & patching:
    //
    // 'link' and 'patch' methods are for use on unprotected code - such as the code
    // within the AssemblerBuffer, and code being patched by the patch buffer. Once
    // code has been finalized it is (platform support permitting) within a non-
    // writable region of memory; to modify the code in an execute-only execuable
    // pool the 'repatch' and 'relink' methods should be used.

    void linkJump(AssemblerLabel from, AssemblerLabel to, JumpType type, Condition condition)
    {
        ASSERT(to.isSet());
        ASSERT(from.isSet());
        m_jumpsToLink.append(LinkRecord(from.m_offset, to.m_offset, type, condition));
    }

    void linkJump(AssemblerLabel from, AssemblerLabel to, JumpType type, Condition condition, bool is64Bit, RegisterID compareRegister)
    {
        ASSERT(to.isSet());
        ASSERT(from.isSet());
        m_jumpsToLink.append(LinkRecord(from.m_offset, to.m_offset, type, condition, is64Bit, compareRegister));
    }

    void linkJump(AssemblerLabel from, AssemblerLabel to, JumpType type, Condition condition, unsigned bitNumber, RegisterID compareRegister)
    {
        ASSERT(to.isSet());
        ASSERT(from.isSet());
        m_jumpsToLink.append(LinkRecord(from.m_offset, to.m_offset, type, condition, bitNumber, compareRegister));
    }

    void linkJump(AssemblerLabel from, AssemblerLabel to)
    {
        ASSERT(from.isSet());
        ASSERT(to.isSet());
        relinkJumpOrCall<false>(addressOf(from), addressOf(to));
    }
    
    static void linkJump(void* code, AssemblerLabel from, void* to)
    {
        ASSERT(from.isSet());
        relinkJumpOrCall<false>(addressOf(code, from), to);
    }

    static void linkCall(void* code, AssemblerLabel from, void* to)
    {
        ASSERT(from.isSet());
        linkJumpOrCall<true>(addressOf(code, from) - 1, to);
    }

    static void linkPointer(void* code, AssemblerLabel where, void* valuePtr)
    {
        linkPointer(addressOf(code, where), valuePtr);
    }

    static void replaceWithJump(void* where, void* to)
    {
        intptr_t offset = (reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(where)) >> 2;
        ASSERT(static_cast<int>(offset) == offset);
        *static_cast<int*>(where) = unconditionalBranchImmediate(false, static_cast<int>(offset));
        cacheFlush(where, sizeof(int));
    }
    
    static ptrdiff_t maxJumpReplacementSize()
    {
        return 4;
    }
    
    static void replaceWithLoad(void* where)
    {
        Datasize sf;
        AddOp op;
        SetFlags S;
        int shift;
        int imm12;
        RegisterID rn;
        RegisterID rd;
        if (disassembleAddSubtractImmediate(where, sf, op, S, shift, imm12, rn, rd)) {
            ASSERT(sf == Datasize_64);
            ASSERT(op == AddOp_ADD);
            ASSERT(!S);
            ASSERT(!shift);
            ASSERT(!(imm12 & ~0xff8));
            *static_cast<int*>(where) = loadStoreRegisterUnsignedImmediate(MemOpSize_64, false, MemOp_LOAD, encodePositiveImmediate<64>(imm12), rn, rd);
            cacheFlush(where, sizeof(int));
        }
#if !ASSERT_DISABLED
        else {
            MemOpSize size;
            bool V;
            MemOp opc;
            int imm12;
            RegisterID rn;
            RegisterID rt;
            ASSERT(disassembleLoadStoreRegisterUnsignedImmediate(where, size, V, opc, imm12, rn, rt));
            ASSERT(size == MemOpSize_64);
            ASSERT(!V);
            ASSERT(opc == MemOp_LOAD);
            ASSERT(!(imm12 & ~0x1ff));
        }
#endif
    }

    static void replaceWithAddressComputation(void* where)
    {
        MemOpSize size;
        bool V;
        MemOp opc;
        int imm12;
        RegisterID rn;
        RegisterID rt;
        if (disassembleLoadStoreRegisterUnsignedImmediate(where, size, V, opc, imm12, rn, rt)) {
            ASSERT(size == MemOpSize_64);
            ASSERT(!V);
            ASSERT(opc == MemOp_LOAD);
            ASSERT(!(imm12 & ~0x1ff));
            *static_cast<int*>(where) = addSubtractImmediate(Datasize_64, AddOp_ADD, DontSetFlags, 0, imm12 * sizeof(void*), rn, rt);
            cacheFlush(where, sizeof(int));
        }
#if !ASSERT_DISABLED
        else {
            Datasize sf;
            AddOp op;
            SetFlags S;
            int shift;
            int imm12;
            RegisterID rn;
            RegisterID rd;
            ASSERT(disassembleAddSubtractImmediate(where, sf, op, S, shift, imm12, rn, rd));
            ASSERT(sf == Datasize_64);
            ASSERT(op == AddOp_ADD);
            ASSERT(!S);
            ASSERT(!shift);
            ASSERT(!(imm12 & ~0xff8));
        }
#endif
    }

    static void repatchPointer(void* where, void* valuePtr)
    {
        linkPointer(static_cast<int*>(where), valuePtr, true);
    }

    static void setPointer(int* address, void* valuePtr, RegisterID rd, bool flush)
    {
        uintptr_t value = reinterpret_cast<uintptr_t>(valuePtr);
        address[0] = moveWideImediate(Datasize_64, MoveWideOp_Z, 0, getHalfword(value, 0), rd);
        address[1] = moveWideImediate(Datasize_64, MoveWideOp_K, 1, getHalfword(value, 1), rd);
        address[2] = moveWideImediate(Datasize_64, MoveWideOp_K, 2, getHalfword(value, 2), rd);

        if (flush)
            cacheFlush(address, sizeof(int) * 3);
    }

    static void repatchInt32(void* where, int32_t value)
    {
        int* address = static_cast<int*>(where);

        Datasize sf;
        MoveWideOp opc;
        int hw;
        uint16_t imm16;
        RegisterID rd;
        bool expected = disassembleMoveWideImediate(address, sf, opc, hw, imm16, rd);
        ASSERT_UNUSED(expected, expected && !sf && (opc == MoveWideOp_Z || opc == MoveWideOp_N) && !hw);
        ASSERT(checkMovk<Datasize_32>(address[1], 1, rd));

        if (value >= 0) {
            address[0] = moveWideImediate(Datasize_32, MoveWideOp_Z, 0, getHalfword(value, 0), rd);
            address[1] = moveWideImediate(Datasize_32, MoveWideOp_K, 1, getHalfword(value, 1), rd);
        } else {
            address[0] = moveWideImediate(Datasize_32, MoveWideOp_N, 0, ~getHalfword(value, 0), rd);
            address[1] = moveWideImediate(Datasize_32, MoveWideOp_K, 1, getHalfword(value, 1), rd);
        }

        cacheFlush(where, sizeof(int) * 2);
    }

    static void* readPointer(void* where)
    {
        int* address = static_cast<int*>(where);

        Datasize sf;
        MoveWideOp opc;
        int hw;
        uint16_t imm16;
        RegisterID rdFirst, rd;

        bool expected = disassembleMoveWideImediate(address, sf, opc, hw, imm16, rdFirst);
        ASSERT_UNUSED(expected, expected && sf && opc == MoveWideOp_Z && !hw);
        uintptr_t result = imm16;

        expected = disassembleMoveWideImediate(address + 1, sf, opc, hw, imm16, rd);
        ASSERT_UNUSED(expected, expected && sf && opc == MoveWideOp_K && hw == 1 && rd == rdFirst);
        result |= static_cast<uintptr_t>(imm16) << 16;

        expected = disassembleMoveWideImediate(address + 2, sf, opc, hw, imm16, rd);
        ASSERT_UNUSED(expected, expected && sf && opc == MoveWideOp_K && hw == 2 && rd == rdFirst);
        result |= static_cast<uintptr_t>(imm16) << 32;

        return reinterpret_cast<void*>(result);
    }

    static void* readCallTarget(void* from)
    {
        return readPointer(reinterpret_cast<int*>(from) - 4);
    }

    static void relinkJump(void* from, void* to)
    {
        relinkJumpOrCall<false>(reinterpret_cast<int*>(from), to);
        cacheFlush(from, sizeof(int));
    }
    
    static void relinkCall(void* from, void* to)
    {
        relinkJumpOrCall<true>(reinterpret_cast<int*>(from) - 1, to);
        cacheFlush(reinterpret_cast<int*>(from) - 1, sizeof(int));
    }
    
    static void repatchCompact(void* where, int32_t value)
    {
        ASSERT(!(value & ~0x3ff8));

        MemOpSize size;
        bool V;
        MemOp opc;
        int imm12;
        RegisterID rn;
        RegisterID rt;
        bool expected = disassembleLoadStoreRegisterUnsignedImmediate(where, size, V, opc, imm12, rn, rt);
        ASSERT_UNUSED(expected, expected && size >= MemOpSize_32 && !V && opc == MemOp_LOAD); // expect 32/64 bit load to GPR.

        if (size == MemOpSize_32)
            imm12 = encodePositiveImmediate<32>(value);
        else
            imm12 = encodePositiveImmediate<64>(value);
        *static_cast<int*>(where) = loadStoreRegisterUnsignedImmediate(size, V, opc, imm12, rn, rt);

        cacheFlush(where, sizeof(int));
    }

    unsigned debugOffset() { return m_buffer.debugOffset(); }

    static void cacheFlush(void* code, size_t size)
    {
#if OS(IOS)
        sys_cache_control(kCacheFunctionPrepareForExecution, code, size);
#else
#error "The cacheFlush support is missing on this platform."
#endif
    }

    // Assembler admin methods:

    int jumpSizeDelta(JumpType jumpType, JumpLinkType jumpLinkType) { return JUMP_ENUM_SIZE(jumpType) - JUMP_ENUM_SIZE(jumpLinkType); }

    static ALWAYS_INLINE bool linkRecordSourceComparator(const LinkRecord& a, const LinkRecord& b)
    {
        return a.from() < b.from();
    }

    bool canCompact(JumpType jumpType)
    {
        // Fixed jumps cannot be compacted
        return (jumpType == JumpNoCondition) || (jumpType == JumpCondition) || (jumpType == JumpCompareAndBranch) || (jumpType == JumpTestBit);
    }

    JumpLinkType computeJumpType(JumpType jumpType, const uint8_t* from, const uint8_t* to)
    {
        switch (jumpType) {
        case JumpFixed:
            return LinkInvalid;
        case JumpNoConditionFixedSize:
            return LinkJumpNoCondition;
        case JumpConditionFixedSize:
            return LinkJumpCondition;
        case JumpCompareAndBranchFixedSize:
            return LinkJumpCompareAndBranch;
        case JumpTestBitFixedSize:
            return LinkJumpTestBit;
        case JumpNoCondition:
            return LinkJumpNoCondition;
        case JumpCondition: {
            ASSERT(!(reinterpret_cast<intptr_t>(from) & 0x3));
            ASSERT(!(reinterpret_cast<intptr_t>(to) & 0x3));
            intptr_t relative = reinterpret_cast<intptr_t>(to) - (reinterpret_cast<intptr_t>(from));

            if (((relative << 43) >> 43) == relative)
                return LinkJumpConditionDirect;

            return LinkJumpCondition;
            }
        case JumpCompareAndBranch:  {
            ASSERT(!(reinterpret_cast<intptr_t>(from) & 0x3));
            ASSERT(!(reinterpret_cast<intptr_t>(to) & 0x3));
            intptr_t relative = reinterpret_cast<intptr_t>(to) - (reinterpret_cast<intptr_t>(from));

            if (((relative << 43) >> 43) == relative)
                return LinkJumpCompareAndBranchDirect;

            return LinkJumpCompareAndBranch;
        }
        case JumpTestBit:   {
            ASSERT(!(reinterpret_cast<intptr_t>(from) & 0x3));
            ASSERT(!(reinterpret_cast<intptr_t>(to) & 0x3));
            intptr_t relative = reinterpret_cast<intptr_t>(to) - (reinterpret_cast<intptr_t>(from));

            if (((relative << 50) >> 50) == relative)
                return LinkJumpTestBitDirect;

            return LinkJumpTestBit;
        }
        default:
            ASSERT_NOT_REACHED();
        }

        return LinkJumpNoCondition;
    }

    JumpLinkType computeJumpType(LinkRecord& record, const uint8_t* from, const uint8_t* to)
    {
        JumpLinkType linkType = computeJumpType(record.type(), from, to);
        record.setLinkType(linkType);
        return linkType;
    }

    void recordLinkOffsets(int32_t regionStart, int32_t regionEnd, int32_t offset)
    {
        int32_t ptr = regionStart / sizeof(int32_t);
        const int32_t end = regionEnd / sizeof(int32_t);
        int32_t* offsets = static_cast<int32_t*>(m_buffer.data());
        while (ptr < end)
            offsets[ptr++] = offset;
    }

    Vector<LinkRecord, 0, UnsafeVectorOverflow>& jumpsToLink()
    {
        std::sort(m_jumpsToLink.begin(), m_jumpsToLink.end(), linkRecordSourceComparator);
        return m_jumpsToLink;
    }

    void ALWAYS_INLINE link(LinkRecord& record, uint8_t* from, uint8_t* to)
    {
        switch (record.linkType()) {
        case LinkJumpNoCondition:
            linkJumpOrCall<false>(reinterpret_cast<int*>(from), to);
            break;
        case LinkJumpConditionDirect:
            linkConditionalBranch<true>(record.condition(), reinterpret_cast<int*>(from), to);
            break;
        case LinkJumpCondition:
            linkConditionalBranch<false>(record.condition(), reinterpret_cast<int*>(from) - 1, to);
            break;
        case LinkJumpCompareAndBranchDirect:
            linkCompareAndBranch<true>(record.condition(), record.is64Bit(), record.compareRegister(), reinterpret_cast<int*>(from), to);
            break;
        case LinkJumpCompareAndBranch:
            linkCompareAndBranch<false>(record.condition(), record.is64Bit(), record.compareRegister(), reinterpret_cast<int*>(from) - 1, to);
            break;
        case LinkJumpTestBitDirect:
            linkTestAndBranch<true>(record.condition(), record.bitNumber(), record.compareRegister(), reinterpret_cast<int*>(from), to);
            break;
        case LinkJumpTestBit:
            linkTestAndBranch<false>(record.condition(), record.bitNumber(), record.compareRegister(), reinterpret_cast<int*>(from) - 1, to);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

private:
    template<Datasize size>
    static bool checkMovk(int insn, int _hw, RegisterID _rd)
    {
        Datasize sf;
        MoveWideOp opc;
        int hw;
        uint16_t imm16;
        RegisterID rd;
        bool expected = disassembleMoveWideImediate(&insn, sf, opc, hw, imm16, rd);

        return expected
            && sf == size
            && opc == MoveWideOp_K
            && hw == _hw
            && rd == _rd;
    }

    static void linkPointer(int* address, void* valuePtr, bool flush = false)
    {
        Datasize sf;
        MoveWideOp opc;
        int hw;
        uint16_t imm16;
        RegisterID rd;
        bool expected = disassembleMoveWideImediate(address, sf, opc, hw, imm16, rd);
        ASSERT_UNUSED(expected, expected && sf && opc == MoveWideOp_Z && !hw);
        ASSERT(checkMovk<Datasize_64>(address[1], 1, rd));
        ASSERT(checkMovk<Datasize_64>(address[2], 2, rd));

        setPointer(address, valuePtr, rd, flush);
    }

    template<bool isCall>
    static void linkJumpOrCall(int* from, void* to)
    {
        bool link;
        int imm26;
        bool isUnconditionalBranchImmediateOrNop = disassembleUnconditionalBranchImmediate(from, link, imm26) || disassembleNop(from);

        ASSERT_UNUSED(isUnconditionalBranchImmediateOrNop, isUnconditionalBranchImmediateOrNop);
        ASSERT_UNUSED(isCall, (link == isCall) || disassembleNop(from));
        ASSERT(!(reinterpret_cast<intptr_t>(from) & 3));
        ASSERT(!(reinterpret_cast<intptr_t>(to) & 3));
        intptr_t offset = (reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(from)) >> 2;
        ASSERT(static_cast<int>(offset) == offset);

        *from = unconditionalBranchImmediate(isCall, static_cast<int>(offset));
    }

    template<bool isDirect>
    static void linkCompareAndBranch(Condition condition, bool is64Bit, RegisterID rt, int* from, void* to)
    {
        ASSERT(!(reinterpret_cast<intptr_t>(from) & 3));
        ASSERT(!(reinterpret_cast<intptr_t>(to) & 3));
        intptr_t offset = (reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(from)) >> 2;
        ASSERT(((offset << 38) >> 38) == offset);

        bool useDirect = ((offset << 45) >> 45) == offset; // Fits in 19 bits
        ASSERT(!isDirect || useDirect);

        if (useDirect || isDirect) {
            *from = compareAndBranchImmediate(is64Bit ? Datasize_64 : Datasize_32, condition == ConditionNE, static_cast<int>(offset), rt);
            if (!isDirect)
                *(from + 1) = nopPseudo();
        } else {
            *from = compareAndBranchImmediate(is64Bit ? Datasize_64 : Datasize_32, invert(condition) == ConditionNE, 2, rt);
            linkJumpOrCall<false>(from + 1, to);
        }
    }

    template<bool isDirect>
    static void linkConditionalBranch(Condition condition, int* from, void* to)
    {
        ASSERT(!(reinterpret_cast<intptr_t>(from) & 3));
        ASSERT(!(reinterpret_cast<intptr_t>(to) & 3));
        intptr_t offset = (reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(from)) >> 2;
        ASSERT(((offset << 38) >> 38) == offset);

        bool useDirect = ((offset << 45) >> 45) == offset; // Fits in 19 bits
        ASSERT(!isDirect || useDirect);

        if (useDirect || isDirect) {
            *from = conditionalBranchImmediate(static_cast<int>(offset), condition);
            if (!isDirect)
                *(from + 1) = nopPseudo();
        } else {
            *from = conditionalBranchImmediate(2, invert(condition));
            linkJumpOrCall<false>(from + 1, to);
        }
    }

    template<bool isDirect>
    static void linkTestAndBranch(Condition condition, unsigned bitNumber, RegisterID rt, int* from, void* to)
    {
        ASSERT(!(reinterpret_cast<intptr_t>(from) & 3));
        ASSERT(!(reinterpret_cast<intptr_t>(to) & 3));
        intptr_t offset = (reinterpret_cast<intptr_t>(to) - reinterpret_cast<intptr_t>(from)) >> 2;
        ASSERT(static_cast<int>(offset) == offset);
        ASSERT(((offset << 38) >> 38) == offset);

        bool useDirect = ((offset << 50) >> 50) == offset; // Fits in 14 bits
        ASSERT(!isDirect || useDirect);

        if (useDirect || isDirect) {
            *from = testAndBranchImmediate(condition == ConditionNE, static_cast<int>(bitNumber), static_cast<int>(offset), rt);
            if (!isDirect)
                *(from + 1) = nopPseudo();
        } else {
            *from = testAndBranchImmediate(invert(condition) == ConditionNE, static_cast<int>(bitNumber), 2, rt);
            linkJumpOrCall<false>(from + 1, to);
        }
    }

    template<bool isCall>
    static void relinkJumpOrCall(int* from, void* to)
    {
        if (!isCall && disassembleNop(from)) {
            unsigned op01;
            int imm19;
            Condition condition;
            bool isConditionalBranchImmediate = disassembleConditionalBranchImmediate(from - 1, op01, imm19, condition);

            if (isConditionalBranchImmediate) {
                ASSERT_UNUSED(op01, !op01);
                ASSERT_UNUSED(isCall, !isCall);

                if (imm19 == 8)
                    condition = invert(condition);

                linkConditionalBranch<false>(condition, from - 1, to);
                return;
            }

            Datasize opSize;
            bool op;
            RegisterID rt;
            bool isCompareAndBranchImmediate = disassembleCompareAndBranchImmediate(from - 1, opSize, op, imm19, rt);

            if (isCompareAndBranchImmediate) {
                if (imm19 == 8)
                    op = !op;

                linkCompareAndBranch<false>(op ? ConditionNE : ConditionEQ, opSize == Datasize_64, rt, from - 1, to);
                return;
            }

            int imm14;
            unsigned bitNumber;
            bool isTestAndBranchImmediate = disassembleTestAndBranchImmediate(from - 1, op, bitNumber, imm14, rt);

            if (isTestAndBranchImmediate) {
                if (imm14 == 8)
                    op = !op;

                linkTestAndBranch<false>(op ? ConditionNE : ConditionEQ, bitNumber, rt, from - 1, to);
                return;
            }
        }

        linkJumpOrCall<isCall>(from, to);
    }

    static int* addressOf(void* code, AssemblerLabel label)
    {
        return reinterpret_cast<int*>(static_cast<char*>(code) + label.m_offset);
    }

    int* addressOf(AssemblerLabel label)
    {
        return addressOf(m_buffer.data(), label);
    }

    static RegisterID disassembleXOrSp(int reg) { return reg == 31 ? ARM64Registers::sp : static_cast<RegisterID>(reg); }
    static RegisterID disassembleXOrZr(int reg) { return reg == 31 ? ARM64Registers::zr : static_cast<RegisterID>(reg); }
    static RegisterID disassembleXOrZrOrSp(bool useZr, int reg) { return reg == 31 ? (useZr ? ARM64Registers::zr : ARM64Registers::sp) : static_cast<RegisterID>(reg); }

    static bool disassembleAddSubtractImmediate(void* address, Datasize& sf, AddOp& op, SetFlags& S, int& shift, int& imm12, RegisterID& rn, RegisterID& rd)
    {
        int insn = *static_cast<int*>(address);
        sf = static_cast<Datasize>((insn >> 31) & 1);
        op = static_cast<AddOp>((insn >> 30) & 1);
        S = static_cast<SetFlags>((insn >> 29) & 1);
        shift = (insn >> 22) & 3;
        imm12 = (insn >> 10) & 0x3ff;
        rn = disassembleXOrSp((insn >> 5) & 0x1f);
        rd = disassembleXOrZrOrSp(S, insn & 0x1f);
        return (insn & 0x1f000000) == 0x11000000;
    }

    static bool disassembleLoadStoreRegisterUnsignedImmediate(void* address, MemOpSize& size, bool& V, MemOp& opc, int& imm12, RegisterID& rn, RegisterID& rt)
    {
        int insn = *static_cast<int*>(address);
        size = static_cast<MemOpSize>((insn >> 30) & 3);
        V = (insn >> 26) & 1;
        opc = static_cast<MemOp>((insn >> 22) & 3);
        imm12 = (insn >> 10) & 0xfff;
        rn = disassembleXOrSp((insn >> 5) & 0x1f);
        rt = disassembleXOrZr(insn & 0x1f);
        return (insn & 0x3b000000) == 0x39000000;
    }

    static bool disassembleMoveWideImediate(void* address, Datasize& sf, MoveWideOp& opc, int& hw, uint16_t& imm16, RegisterID& rd)
    {
        int insn = *static_cast<int*>(address);
        sf = static_cast<Datasize>((insn >> 31) & 1);
        opc = static_cast<MoveWideOp>((insn >> 29) & 3);
        hw = (insn >> 21) & 3;
        imm16 = insn >> 5;
        rd = disassembleXOrZr(insn & 0x1f);
        return (insn & 0x1f800000) == 0x12800000;
    }

    static bool disassembleNop(void* address)
    {
        unsigned insn = *static_cast<unsigned*>(address);
        return insn == 0xd503201f;
    }

    static bool disassembleCompareAndBranchImmediate(void* address, Datasize& sf, bool& op, int& imm19, RegisterID& rt)
    {
        int insn = *static_cast<int*>(address);
        sf = static_cast<Datasize>((insn >> 31) & 1);
        op = (insn >> 24) & 0x1;
        imm19 = (insn << 8) >> 13;
        rt = static_cast<RegisterID>(insn & 0x1f);
        return (insn & 0x7e000000) == 0x34000000;
        
    }

    static bool disassembleConditionalBranchImmediate(void* address, unsigned& op01, int& imm19, Condition &condition)
    {
        int insn = *static_cast<int*>(address);
        op01 = ((insn >> 23) & 0x2) | ((insn >> 4) & 0x1);
        imm19 = (insn << 8) >> 13;
        condition = static_cast<Condition>(insn & 0xf);
        return (insn & 0xfe000000) == 0x54000000;
    }

    static bool disassembleTestAndBranchImmediate(void* address, bool& op, unsigned& bitNumber, int& imm14, RegisterID& rt)
    {
        int insn = *static_cast<int*>(address);
        op = (insn >> 24) & 0x1;
        imm14 = (insn << 13) >> 18;
        bitNumber = static_cast<unsigned>((((insn >> 26) & 0x20)) | ((insn > 19) & 0x1f));
        rt = static_cast<RegisterID>(insn & 0x1f);
        return (insn & 0x7e000000) == 0x36000000;
        
    }

    static bool disassembleUnconditionalBranchImmediate(void* address, bool& op, int& imm26)
    {
        int insn = *static_cast<int*>(address);
        op = (insn >> 31) & 1;
        imm26 = (insn << 6) >> 6;
        return (insn & 0x7c000000) == 0x14000000;
    }

    static int xOrSp(RegisterID reg) { ASSERT(!isZr(reg)); return reg; }
    static int xOrZr(RegisterID reg) { ASSERT(!isSp(reg)); return reg & 31; }
    static FPRegisterID xOrZrAsFPR(RegisterID reg) { return static_cast<FPRegisterID>(xOrZr(reg)); }
    static int xOrZrOrSp(bool useZr, RegisterID reg) { return useZr ? xOrZr(reg) : xOrSp(reg); }

    ALWAYS_INLINE void insn(int instruction)
    {
        m_buffer.putInt(instruction);
    }

    ALWAYS_INLINE static int addSubtractExtendedRegister(Datasize sf, AddOp op, SetFlags S, RegisterID rm, ExtendType option, int imm3, RegisterID rn, RegisterID rd)
    {
        ASSERT(imm3 < 5);
        // The only allocated values for opt is 0.
        const int opt = 0;
        return (0x0b200000 | sf << 31 | op << 30 | S << 29 | opt << 22 | xOrZr(rm) << 16 | option << 13 | (imm3 & 0x7) << 10 | xOrSp(rn) << 5 | xOrZrOrSp(S, rd));
    }

    ALWAYS_INLINE static int addSubtractImmediate(Datasize sf, AddOp op, SetFlags S, int shift, int imm12, RegisterID rn, RegisterID rd)
    {
        ASSERT(shift < 2);
        ASSERT(isUInt12(imm12));
        return (0x11000000 | sf << 31 | op << 30 | S << 29 | shift << 22 | (imm12 & 0xfff) << 10 | xOrSp(rn) << 5 | xOrZrOrSp(S, rd));
    }

    ALWAYS_INLINE static int addSubtractShiftedRegister(Datasize sf, AddOp op, SetFlags S, ShiftType shift, RegisterID rm, int imm6, RegisterID rn, RegisterID rd)
    {
        ASSERT(shift < 3);
        ASSERT(!(imm6 & (sf ? ~63 : ~31)));
        return (0x0b000000 | sf << 31 | op << 30 | S << 29 | shift << 22 | xOrZr(rm) << 16 | (imm6 & 0x3f) << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int addSubtractWithCarry(Datasize sf, AddOp op, SetFlags S, RegisterID rm, RegisterID rn, RegisterID rd)
    {
        const int opcode2 = 0;
        return (0x1a000000 | sf << 31 | op << 30 | S << 29 | xOrZr(rm) << 16 | opcode2 << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int bitfield(Datasize sf, BitfieldOp opc, int immr, int imms, RegisterID rn, RegisterID rd)
    {
        ASSERT(immr < (sf ? 64 : 32));
        ASSERT(imms < (sf ? 64 : 32));
        const int N = sf;
        return (0x13000000 | sf << 31 | opc << 29 | N << 22 | immr << 16 | imms << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    // 'op' means negate
    ALWAYS_INLINE static int compareAndBranchImmediate(Datasize sf, bool op, int32_t imm19, RegisterID rt)
    {
        ASSERT(imm19 == (imm19 << 13) >> 13);
        return (0x34000000 | sf << 31 | op << 24 | (imm19 & 0x7ffff) << 5 | xOrZr(rt));
    }

    ALWAYS_INLINE static int conditionalBranchImmediate(int32_t imm19, Condition cond)
    {
        ASSERT(imm19 == (imm19 << 13) >> 13);
        ASSERT(!(cond & ~15));
        // The only allocated values for o1 & o0 are 0.
        const int o1 = 0;
        const int o0 = 0;
        return (0x54000000 | o1 << 24 | (imm19 & 0x7ffff) << 5 | o0 << 4 | cond);
    }

    ALWAYS_INLINE static int conditionalCompareImmediate(Datasize sf, AddOp op, int imm5, Condition cond, RegisterID rn, int nzcv)
    {
        ASSERT(!(imm5 & ~0x1f));
        ASSERT(nzcv < 16);
        const int S = 1;
        const int o2 = 0;
        const int o3 = 0;
        return (0x1a400800 | sf << 31 | op << 30 | S << 29 | (imm5 & 0x1f) << 16 | cond << 12 | o2 << 10 | xOrZr(rn) << 5 | o3 << 4 | nzcv);
    }

    ALWAYS_INLINE static int conditionalCompareRegister(Datasize sf, AddOp op, RegisterID rm, Condition cond, RegisterID rn, int nzcv)
    {
        ASSERT(nzcv < 16);
        const int S = 1;
        const int o2 = 0;
        const int o3 = 0;
        return (0x1a400000 | sf << 31 | op << 30 | S << 29 | xOrZr(rm) << 16 | cond << 12 | o2 << 10 | xOrZr(rn) << 5 | o3 << 4 | nzcv);
    }

    // 'op' means negate
    // 'op2' means increment
    ALWAYS_INLINE static int conditionalSelect(Datasize sf, bool op, RegisterID rm, Condition cond, bool op2, RegisterID rn, RegisterID rd)
    {
        const int S = 0;
        return (0x1a800000 | sf << 31 | op << 30 | S << 29 | xOrZr(rm) << 16 | cond << 12 | op2 << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int dataProcessing1Source(Datasize sf, DataOp1Source opcode, RegisterID rn, RegisterID rd)
    {
        const int S = 0;
        const int opcode2 = 0;
        return (0x5ac00000 | sf << 31 | S << 29 | opcode2 << 16 | opcode << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int dataProcessing2Source(Datasize sf, RegisterID rm, DataOp2Source opcode, RegisterID rn, RegisterID rd)
    {
        const int S = 0;
        return (0x1ac00000 | sf << 31 | S << 29 | xOrZr(rm) << 16 | opcode << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int dataProcessing3Source(Datasize sf, DataOp3Source opcode, RegisterID rm, RegisterID ra, RegisterID rn, RegisterID rd)
    {
        int op54 = opcode >> 4;
        int op31 = (opcode >> 1) & 7;
        int op0 = opcode & 1;
        return (0x1b000000 | sf << 31 | op54 << 29 | op31 << 21 | xOrZr(rm) << 16 | op0 << 15 | xOrZr(ra) << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int excepnGeneration(ExcepnOp opc, uint16_t imm16, int LL)
    {
        ASSERT((opc == ExcepnOp_BREAKPOINT || opc == ExcepnOp_HALT) ? !LL : (LL && (LL < 4)));
        const int op2 = 0;
        return (0xd4000000 | opc << 21 | imm16 << 5 | op2 << 2 | LL);
    }

    ALWAYS_INLINE static int extract(Datasize sf, RegisterID rm, int imms, RegisterID rn, RegisterID rd)
    {
        ASSERT(imms < (sf ? 64 : 32));
        const int op21 = 0;
        const int N = sf;
        const int o0 = 0;
        return (0x13800000 | sf << 31 | op21 << 29 | N << 22 | o0 << 21 | xOrZr(rm) << 16 | imms << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int floatingPointCompare(Datasize type, FPRegisterID rm, FPRegisterID rn, FPCmpOp opcode2)
    {
        const int M = 0;
        const int S = 0;
        const int op = 0;
        return (0x1e202000 | M << 31 | S << 29 | type << 22 | rm << 16 | op << 14 | rn << 5 | opcode2);
    }

    ALWAYS_INLINE static int floatingPointConditionalCompare(Datasize type, FPRegisterID rm, Condition cond, FPRegisterID rn, FPCondCmpOp op, int nzcv)
    {
        ASSERT(nzcv < 16);
        const int M = 0;
        const int S = 0;
        return (0x1e200400 | M << 31 | S << 29 | type << 22 | rm << 16 | cond << 12 | rn << 5 | op << 4 | nzcv);
    }

    ALWAYS_INLINE static int floatingPointConditionalSelect(Datasize type, FPRegisterID rm, Condition cond, FPRegisterID rn, FPRegisterID rd)
    {
        const int M = 0;
        const int S = 0;
        return (0x1e200c00 | M << 31 | S << 29 | type << 22 | rm << 16 | cond << 12 | rn << 5 | rd);
    }

    ALWAYS_INLINE static int floatingPointImmediate(Datasize type, int imm8, FPRegisterID rd)
    {
        const int M = 0;
        const int S = 0;
        const int imm5 = 0;
        return (0x1e201000 | M << 31 | S << 29 | type << 22 | (imm8 & 0xff) << 13 | imm5 << 5 | rd);
    }

    ALWAYS_INLINE static int floatingPointIntegerConversions(Datasize sf, Datasize type, FPIntConvOp rmodeOpcode, FPRegisterID rn, FPRegisterID rd)
    {
        const int S = 0;
        return (0x1e200000 | sf << 31 | S << 29 | type << 22 | rmodeOpcode << 16 | rn << 5 | rd);
    }

    ALWAYS_INLINE static int floatingPointIntegerConversions(Datasize sf, Datasize type, FPIntConvOp rmodeOpcode, FPRegisterID rn, RegisterID rd)
    {
        return floatingPointIntegerConversions(sf, type, rmodeOpcode, rn, xOrZrAsFPR(rd));
    }

    ALWAYS_INLINE static int floatingPointIntegerConversions(Datasize sf, Datasize type, FPIntConvOp rmodeOpcode, RegisterID rn, FPRegisterID rd)
    {
        return floatingPointIntegerConversions(sf, type, rmodeOpcode, xOrZrAsFPR(rn), rd);
    }

    ALWAYS_INLINE static int floatingPointDataProcessing1Source(Datasize type, FPDataOp1Source opcode, FPRegisterID rn, FPRegisterID rd)
    {
        const int M = 0;
        const int S = 0;
        return (0x1e204000 | M << 31 | S << 29 | type << 22 | opcode << 15 | rn << 5 | rd);
    }

    ALWAYS_INLINE static int floatingPointDataProcessing2Source(Datasize type, FPRegisterID rm, FPDataOp2Source opcode, FPRegisterID rn, FPRegisterID rd)
    {
        const int M = 0;
        const int S = 0;
        return (0x1e200800 | M << 31 | S << 29 | type << 22 | rm << 16 | opcode << 12 | rn << 5 | rd);
    }

    // 'o1' means negate
    ALWAYS_INLINE static int floatingPointDataProcessing3Source(Datasize type, bool o1, FPRegisterID rm, AddOp o2, FPRegisterID ra, FPRegisterID rn, FPRegisterID rd)
    {
        const int M = 0;
        const int S = 0;
        return (0x1f000000 | M << 31 | S << 29 | type << 22 | o1 << 21 | rm << 16 | o2 << 15 | ra << 10 | rn << 5 | rd);
    }

    // 'V' means vector
    ALWAYS_INLINE static int loadRegisterLiteral(LdrLiteralOp opc, bool V, int imm19, FPRegisterID rt)
    {
        ASSERT(((imm19 << 13) >> 13) == imm19);
        return (0x18000000 | opc << 30 | V << 26 | (imm19 & 0x7ffff) << 5 | rt);
    }

    ALWAYS_INLINE static int loadRegisterLiteral(LdrLiteralOp opc, bool V, int imm19, RegisterID rt)
    {
        return loadRegisterLiteral(opc, V, imm19, xOrZrAsFPR(rt));
    }

    // 'V' means vector
    ALWAYS_INLINE static int loadStoreRegisterPostIndex(MemOpSize size, bool V, MemOp opc, int imm9, RegisterID rn, FPRegisterID rt)
    {
        ASSERT(!(size && V && (opc & 2))); // Maximum vector size is 128 bits.
        ASSERT(!((size & 2) && !V && (opc == 3))); // signed 32-bit load must be extending from 8/16 bits.
        ASSERT(isInt9(imm9));
        return (0x38000400 | size << 30 | V << 26 | opc << 22 | (imm9 & 0x1ff) << 12 | xOrSp(rn) << 5 | rt);
    }

    ALWAYS_INLINE static int loadStoreRegisterPostIndex(MemOpSize size, bool V, MemOp opc, int imm9, RegisterID rn, RegisterID rt)
    {
        return loadStoreRegisterPostIndex(size, V, opc, imm9, rn, xOrZrAsFPR(rt));
    }

    // 'V' means vector
    ALWAYS_INLINE static int loadStoreRegisterPairPostIndex(MemPairOpSize size, bool V, MemOp opc, int immediate, RegisterID rn, FPRegisterID rt, FPRegisterID rt2)
    {
        ASSERT(size < 3);
        ASSERT(opc == (opc & 1)); // Only load or store, load signed 64 is handled via size.
        ASSERT(V || (size != MemPairOp_LoadSigned_32) || (opc == MemOp_LOAD)); // There isn't an integer store signed.
        unsigned immedShiftAmount = memPairOffsetShift(V, size);
        int imm7 = immediate >> immedShiftAmount;
        ASSERT((imm7 << immedShiftAmount) == immediate && isInt7(imm7));
        return (0x28800000 | size << 30 | V << 26 | opc << 22 | (imm7 & 0x7f) << 15 | rt2 << 10 | xOrSp(rn) << 5 | rt);
    }

    ALWAYS_INLINE static int loadStoreRegisterPairPostIndex(MemPairOpSize size, bool V, MemOp opc, int immediate, RegisterID rn, RegisterID rt, RegisterID rt2)
    {
        return loadStoreRegisterPairPostIndex(size, V, opc, immediate, rn, xOrZrAsFPR(rt), xOrZrAsFPR(rt2));
    }

    // 'V' means vector
    ALWAYS_INLINE static int loadStoreRegisterPreIndex(MemOpSize size, bool V, MemOp opc, int imm9, RegisterID rn, FPRegisterID rt)
    {
        ASSERT(!(size && V && (opc & 2))); // Maximum vector size is 128 bits.
        ASSERT(!((size & 2) && !V && (opc == 3))); // signed 32-bit load must be extending from 8/16 bits.
        ASSERT(isInt9(imm9));
        return (0x38000c00 | size << 30 | V << 26 | opc << 22 | (imm9 & 0x1ff) << 12 | xOrSp(rn) << 5 | rt);
    }

    ALWAYS_INLINE static int loadStoreRegisterPreIndex(MemOpSize size, bool V, MemOp opc, int imm9, RegisterID rn, RegisterID rt)
    {
        return loadStoreRegisterPreIndex(size, V, opc, imm9, rn, xOrZrAsFPR(rt));
    }

    // 'V' means vector
    ALWAYS_INLINE static int loadStoreRegisterPairPreIndex(MemPairOpSize size, bool V, MemOp opc, int immediate, RegisterID rn, FPRegisterID rt, FPRegisterID rt2)
    {
        ASSERT(size < 3);
        ASSERT(opc == (opc & 1)); // Only load or store, load signed 64 is handled via size.
        ASSERT(V || (size != MemPairOp_LoadSigned_32) || (opc == MemOp_LOAD)); // There isn't an integer store signed.
        unsigned immedShiftAmount = memPairOffsetShift(V, size);
        int imm7 = immediate >> immedShiftAmount;
        ASSERT((imm7 << immedShiftAmount) == immediate && isInt7(imm7));
        return (0x29800000 | size << 30 | V << 26 | opc << 22 | (imm7 & 0x7f) << 15 | rt2 << 10 | xOrSp(rn) << 5 | rt);
    }

    ALWAYS_INLINE static int loadStoreRegisterPairPreIndex(MemPairOpSize size, bool V, MemOp opc, int immediate, RegisterID rn, RegisterID rt, RegisterID rt2)
    {
        return loadStoreRegisterPairPreIndex(size, V, opc, immediate, rn, xOrZrAsFPR(rt), xOrZrAsFPR(rt2));
    }

    // 'V' means vector
    // 'S' means shift rm
    ALWAYS_INLINE static int loadStoreRegisterRegisterOffset(MemOpSize size, bool V, MemOp opc, RegisterID rm, ExtendType option, bool S, RegisterID rn, FPRegisterID rt)
    {
        ASSERT(!(size && V && (opc & 2))); // Maximum vector size is 128 bits.
        ASSERT(!((size & 2) && !V && (opc == 3))); // signed 32-bit load must be extending from 8/16 bits.
        ASSERT(option & 2); // The ExtendType for the address must be 32/64 bit, signed or unsigned - not 8/16bit.
        return (0x38200800 | size << 30 | V << 26 | opc << 22 | xOrZr(rm) << 16 | option << 13 | S << 12 | xOrSp(rn) << 5 | rt);
    }

    ALWAYS_INLINE static int loadStoreRegisterRegisterOffset(MemOpSize size, bool V, MemOp opc, RegisterID rm, ExtendType option, bool S, RegisterID rn, RegisterID rt)
    {
        return loadStoreRegisterRegisterOffset(size, V, opc, rm, option, S, rn, xOrZrAsFPR(rt));
    }

    // 'V' means vector
    ALWAYS_INLINE static int loadStoreRegisterUnscaledImmediate(MemOpSize size, bool V, MemOp opc, int imm9, RegisterID rn, FPRegisterID rt)
    {
        ASSERT(!(size && V && (opc & 2))); // Maximum vector size is 128 bits.
        ASSERT(!((size & 2) && !V && (opc == 3))); // signed 32-bit load must be extending from 8/16 bits.
        ASSERT(isInt9(imm9));
        return (0x38000000 | size << 30 | V << 26 | opc << 22 | (imm9 & 0x1ff) << 12 | xOrSp(rn) << 5 | rt);
    }

    ALWAYS_INLINE static int loadStoreRegisterUnscaledImmediate(MemOpSize size, bool V, MemOp opc, int imm9, RegisterID rn, RegisterID rt)
    {
        ASSERT(isInt9(imm9));
        return loadStoreRegisterUnscaledImmediate(size, V, opc, imm9, rn, xOrZrAsFPR(rt));
    }

    // 'V' means vector
    ALWAYS_INLINE static int loadStoreRegisterUnsignedImmediate(MemOpSize size, bool V, MemOp opc, int imm12, RegisterID rn, FPRegisterID rt)
    {
        ASSERT(!(size && V && (opc & 2))); // Maximum vector size is 128 bits.
        ASSERT(!((size & 2) && !V && (opc == 3))); // signed 32-bit load must be extending from 8/16 bits.
        ASSERT(isUInt12(imm12));
        return (0x39000000 | size << 30 | V << 26 | opc << 22 | (imm12 & 0xfff) << 10 | xOrSp(rn) << 5 | rt);
    }

    ALWAYS_INLINE static int loadStoreRegisterUnsignedImmediate(MemOpSize size, bool V, MemOp opc, int imm12, RegisterID rn, RegisterID rt)
    {
        return loadStoreRegisterUnsignedImmediate(size, V, opc, imm12, rn, xOrZrAsFPR(rt));
    }

    ALWAYS_INLINE static int logicalImmediate(Datasize sf, LogicalOp opc, int N_immr_imms, RegisterID rn, RegisterID rd)
    {
        ASSERT(!(N_immr_imms & (sf ? ~0x1fff : ~0xfff)));
        return (0x12000000 | sf << 31 | opc << 29 | N_immr_imms << 10 | xOrZr(rn) << 5 | xOrZrOrSp(opc == LogicalOp_ANDS, rd));
    }

    // 'N' means negate rm
    ALWAYS_INLINE static int logicalShiftedRegister(Datasize sf, LogicalOp opc, ShiftType shift, bool N, RegisterID rm, int imm6, RegisterID rn, RegisterID rd)
    {
        ASSERT(!(imm6 & (sf ? ~63 : ~31)));
        return (0x0a000000 | sf << 31 | opc << 29 | shift << 22 | N << 21 | xOrZr(rm) << 16 | (imm6 & 0x3f) << 10 | xOrZr(rn) << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int moveWideImediate(Datasize sf, MoveWideOp opc, int hw, uint16_t imm16, RegisterID rd)
    {
        ASSERT(hw < (sf ? 4 : 2));
        return (0x12800000 | sf << 31 | opc << 29 | hw << 21 | (int)imm16 << 5 | xOrZr(rd));
    }

    // 'op' means link
    ALWAYS_INLINE static int unconditionalBranchImmediate(bool op, int32_t imm26)
    {
        ASSERT(imm26 == (imm26 << 6) >> 6);
        return (0x14000000 | op << 31 | (imm26 & 0x3ffffff));
    }

    // 'op' means page
    ALWAYS_INLINE static int pcRelative(bool op, int32_t imm21, RegisterID rd)
    {
        ASSERT(imm21 == (imm21 << 11) >> 11);
        int32_t immlo = imm21 & 3;
        int32_t immhi = (imm21 >> 2) & 0x7ffff;
        return (0x10000000 | op << 31 | immlo << 29 | immhi << 5 | xOrZr(rd));
    }

    ALWAYS_INLINE static int system(bool L, int op0, int op1, int crn, int crm, int op2, RegisterID rt)
    {
        return (0xd5000000 | L << 21 | op0 << 19 | op1 << 16 | crn << 12 | crm << 8 | op2 << 5 | xOrZr(rt));
    }

    ALWAYS_INLINE static int hintPseudo(int imm)
    {
        ASSERT(!(imm & ~0x7f));
        return system(0, 0, 3, 2, (imm >> 3) & 0xf, imm & 0x7, ARM64Registers::zr);
    }

    ALWAYS_INLINE static int nopPseudo()
    {
        return hintPseudo(0);
    }
    
    // 'op' means negate
    ALWAYS_INLINE static int testAndBranchImmediate(bool op, int b50, int imm14, RegisterID rt)
    {
        ASSERT(!(b50 & ~0x3f));
        ASSERT(imm14 == (imm14 << 18) >> 18);
        int b5 = b50 >> 5;
        int b40 = b50 & 0x1f;
        return (0x36000000 | b5 << 31 | op << 24 | b40 << 19 | (imm14 & 0x3fff) << 5 | xOrZr(rt));
    }

    ALWAYS_INLINE static int unconditionalBranchRegister(BranchType opc, RegisterID rn)
    {
        // The only allocated values for op2 is 0x1f, for op3 & op4 are 0.
        const int op2 = 0x1f;
        const int op3 = 0;
        const int op4 = 0;
        return (0xd6000000 | opc << 21 | op2 << 16 | op3 << 10 | xOrZr(rn) << 5 | op4);
    }

    AssemblerBuffer m_buffer;
    Vector<LinkRecord, 0, UnsafeVectorOverflow> m_jumpsToLink;
    int m_indexOfLastWatchpoint;
    int m_indexOfTailOfLastWatchpoint;
};

} // namespace JSC

#undef CHECK_DATASIZE_OF
#undef DATASIZE_OF
#undef MEMOPSIZE_OF
#undef CHECK_DATASIZE
#undef DATASIZE
#undef MEMOPSIZE
#undef CHECK_FP_MEMOP_DATASIZE

#endif // ENABLE(ASSEMBLER) && CPU(ARM64)

#endif // ARM64Assembler_h
