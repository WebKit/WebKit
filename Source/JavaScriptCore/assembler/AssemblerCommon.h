/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/OSCheck.h>
#include <JavaScriptCore/Options.h>
#include <optional>
#include <wtf/Atomics.h>
#include <wtf/MathExtras.h>
#include <wtf/OptionSet.h>

namespace JSC {

enum class RepatchingFlag : uint8_t {
    Atomic = 1 << 0,
    Memcpy = 1 << 1, // or JITMemcpy
    Flush = 1 << 2,
};

using RepatchingInfo = WTF::ConstexprOptionSet<RepatchingFlag>;
constexpr RepatchingInfo jitMemcpyRepatch = RepatchingInfo { };
constexpr RepatchingInfo jitMemcpyRepatchAtomic = RepatchingInfo { RepatchingFlag::Atomic };
constexpr RepatchingInfo jitMemcpyRepatchFlush = RepatchingInfo { RepatchingFlag::Flush };
constexpr RepatchingInfo memcpyRepatchFlush = RepatchingInfo { RepatchingFlag::Memcpy, RepatchingFlag::Flush };
constexpr RepatchingInfo memcpyRepatch = RepatchingInfo { RepatchingFlag::Memcpy };

ALWAYS_INLINE constexpr RepatchingInfo noFlush(RepatchingInfo i)
{
    auto tmp = *i;
    tmp.remove(RepatchingFlag::Flush);
    return { tmp };
}

template<size_t bits, typename Type>
ALWAYS_INLINE constexpr bool isInt(Type t)
{
    constexpr size_t shift = sizeof(Type) * CHAR_BIT - bits;
    static_assert(sizeof(Type) * CHAR_BIT > shift, "shift is larger than the size of the value");
    return ((t << shift) >> shift) == t;
}

ALWAYS_INLINE bool isInt9(int32_t value)
{
    return value == ((value << 23) >> 23);
}

template<typename Type>
ALWAYS_INLINE bool isUInt12(Type value)
{
    return !(value & ~static_cast<Type>(0xfff));
}

template<int datasize>
ALWAYS_INLINE bool isValidScaledUImm12(int32_t offset)
{
    int32_t maxPImm = 4095 * (datasize / 8);
    if (offset < 0)
        return false;
    if (offset > maxPImm)
        return false;
    if (offset & ((datasize / 8) - 1))
        return false;
    return true;
}

ALWAYS_INLINE bool isValidSignedImm9(int32_t value)
{
    return isInt9(value);
}

ALWAYS_INLINE bool isValidSignedImm7(int32_t value, int alignmentShiftAmount)
{
    constexpr int32_t disallowedHighBits = 32 - 7;
    int32_t shiftedValue = value >> alignmentShiftAmount;
    bool fitsIn7Bits = shiftedValue == ((shiftedValue << disallowedHighBits) >> disallowedHighBits);
    bool hasCorrectAlignment = value == (shiftedValue << alignmentShiftAmount);
    return fitsIn7Bits && hasCorrectAlignment;
}

class ARM64LogicalImmediate {
public:
    static ARM64LogicalImmediate create32(uint32_t value)
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

    static ARM64LogicalImmediate create64(uint64_t value)
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
    ARM64LogicalImmediate(int value)
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

    static constexpr int InvalidLogicalImmediate = -1;

    int m_value;
};

class ARM64FPImmediate {
public:
    static ARM64FPImmediate create64(uint64_t value)
    {
        uint8_t result = 0;
        for (unsigned i = 0; i < sizeof(double); ++i) {
            uint8_t slice = static_cast<uint8_t>(value >> (8 * i));
            if (!slice)
                continue;
            if (slice == UINT8_MAX) {
                result |= (1U << i);
                continue;
            }
            return { };
        }
        return ARM64FPImmediate(result);
    }

    bool isValid() const { return m_value.has_value(); }
    uint8_t value() const
    {
        ASSERT(isValid());
        return m_value.value();
    }

private:
    ARM64FPImmediate() = default;

    ARM64FPImmediate(uint8_t value)
        : m_value(value)
    {
    }

    std::optional<uint8_t> m_value;
};

// ARM64ShiftedImmediate32 encodes 32-bit values that can be represented as a single byte
// shifted left by 0, 8, 16, or 24 bits. This is used for ARM64 SIMD movi/mvni instructions.
//
// Examples of encodable patterns:
//   0x00000012 → immediate=0x12, shift=0
//   0x00001200 → immediate=0x12, shift=8
//   0x00120000 → immediate=0x12, shift=16
//   0x12000000 → immediate=0x12, shift=24
//   0x80000000 → immediate=0x80, shift=24  (commonly used sign bit pattern)
//   0x000000FF → immediate=0xFF, shift=0
//
// Non-encodable patterns:
//   0x12345678 → multiple non-zero bytes
//   0x00001234 → non-zero value wider than one byte
//
// This is used with:
//   movi Vd.2S, #imm8, lsl #shift    (materialized value = imm8 << shift)
//   mvni Vd.2S, #imm8, lsl #shift    (materialized value = ~(imm8 << shift))
class ARM64ShiftedImmediate32 {
public:
    static ARM64ShiftedImmediate32 create(uint32_t value)
    {
        // Check if value can be represented as (imm8 << shift) where shift is 0, 8, 16, or 24
        if (!value)
            return { };

        for (unsigned shift = 0; shift <= 24; shift += 8) {
            uint32_t mask = 0xFFU << shift;
            if ((value & ~mask) == 0) {
                // All bits outside the shifted byte are zero
                uint8_t imm = static_cast<uint8_t>(value >> shift);
                if (imm != 0) // Must have non-zero immediate
                    return ARM64ShiftedImmediate32(imm, shift);
            }
        }
        return { };
    }

    bool isValid() const { return m_immediate.has_value(); }
    uint8_t immediate() const
    {
        ASSERT(isValid());
        return m_immediate.value();
    }
    uint8_t shift() const
    {
        ASSERT(isValid());
        return m_shift;
    }

private:
    ARM64ShiftedImmediate32() = default;

    ARM64ShiftedImmediate32(uint8_t immediate, uint8_t shift)
        : m_immediate(immediate)
        , m_shift(shift)
    {
    }

    std::optional<uint8_t> m_immediate;
    uint8_t m_shift { 0 };
};

// ARM64ShiftedImmediateMSL32 encodes 32-bit values for ARM64 SIMD movi/mvni instructions
// using MSL (Mask Shift Left) mode, which shifts an 8-bit immediate and fills with ones.
//
// MSL patterns:
//   shift=8:  (imm8 << 8) | 0x000000FF
//   shift=16: (imm8 << 16) | 0x0000FFFF
//
// Examples of encodable patterns:
//   0x000042FF → immediate=0x42, shift=8   (movi with MSL #8)
//   0x0042FFFF → immediate=0x42, shift=16  (movi with MSL #16)
//   0xFFFFBD00 → ~0x000042FF → immediate=0x42, shift=8  (mvni with MSL #8)
//   0xFFBD0000 → ~0x0042FFFF → immediate=0x42, shift=16 (mvni with MSL #16)
//
// Common use cases:
//   Creating masks with specific byte set (e.g., 0x00FFFFFF for masking operations)
//
// This is used with:
//   movi Vd.2S, #imm8, MSL #shift    (materialized value = (imm8 << shift) | mask)
//   mvni Vd.2S, #imm8, MSL #shift    (materialized value = ~((imm8 << shift) | mask))
class ARM64ShiftedImmediateMSL32 {
public:
    static ARM64ShiftedImmediateMSL32 create(uint32_t value)
    {
        // MSL #8: (imm8 << 8) | 0xFF
        // Bits [7:0] must be 0xFF, bits [15:8] are imm8, bits [31:16] must be 0
        if ((value >> 16) == 0 && (value & 0xFF) == 0xFF) {
            uint8_t imm = static_cast<uint8_t>((value >> 8) & 0xFF);
            if (imm != 0)
                return ARM64ShiftedImmediateMSL32(imm, 8);
        }

        // MSL #16: (imm8 << 16) | 0xFFFF
        // Bits [15:0] must be 0xFFFF, bits [23:16] are imm8, bits [31:24] must be 0
        if ((value >> 24) == 0 && (value & 0xFFFF) == 0xFFFF) {
            uint8_t imm = static_cast<uint8_t>((value >> 16) & 0xFF);
            if (imm != 0)
                return ARM64ShiftedImmediateMSL32(imm, 16);
        }

        return { };
    }

    bool isValid() const { return m_immediate.has_value(); }
    uint8_t immediate() const
    {
        ASSERT(isValid());
        return m_immediate.value();
    }
    uint8_t shift() const
    {
        ASSERT(isValid());
        return m_shift;
    }

private:
    ARM64ShiftedImmediateMSL32() = default;

    ARM64ShiftedImmediateMSL32(uint8_t immediate, uint8_t shift)
        : m_immediate(immediate)
        , m_shift(shift)
    {
    }

    std::optional<uint8_t> m_immediate;
    uint8_t m_shift { 0 };
};

// ARM64ShiftedImmediate16 encodes 16-bit values that can be represented as a single byte
// shifted left by 0 or 8 bits. This is used for ARM64 SIMD movi/mvni instructions.
//
// Examples of encodable patterns:
//   0x0012 → immediate=0x12, shift=0
//   0x1200 → immediate=0x12, shift=8
//   0x00FF → immediate=0xFF, shift=0
//   0xFF00 → immediate=0xFF, shift=8
//
// Non-encodable patterns:
//   0x1234 → multiple non-zero bytes
//
// This is used with:
//   movi Vd.4H, #imm8, lsl #shift    (materialized value = imm8 << shift)
//   movi Vd.8H, #imm8, lsl #shift    (materialized value = imm8 << shift)
//   mvni Vd.4H, #imm8, lsl #shift    (materialized value = ~(imm8 << shift))
//   mvni Vd.8H, #imm8, lsl #shift    (materialized value = ~(imm8 << shift))
class ARM64ShiftedImmediate16 {
public:
    static ARM64ShiftedImmediate16 create(uint16_t value)
    {
        // Check if value can be represented as (imm8 << shift) where shift is 0 or 8
        if (!value)
            return { };

        for (unsigned shift = 0; shift <= 8; shift += 8) {
            uint16_t mask = 0xFFU << shift;
            if ((value & ~mask) == 0) {
                // All bits outside the shifted byte are zero
                uint8_t imm = static_cast<uint8_t>(value >> shift);
                if (imm != 0) // Must have non-zero immediate
                    return ARM64ShiftedImmediate16(imm, shift);
            }
        }
        return { };
    }

    bool isValid() const { return m_immediate.has_value(); }
    uint8_t immediate() const
    {
        ASSERT(isValid());
        return m_immediate.value();
    }
    uint8_t shift() const
    {
        ASSERT(isValid());
        return m_shift;
    }

private:
    ARM64ShiftedImmediate16() = default;

    ARM64ShiftedImmediate16(uint8_t immediate, uint8_t shift)
        : m_immediate(immediate)
        , m_shift(shift)
    {
    }

    std::optional<uint8_t> m_immediate;
    uint8_t m_shift { 0 };
};

// X86ContiguousBitPattern32 detects 32-bit values with contiguous set bits that can be
// synthesized using pcmpeqd (all-ones) + shifts on x86/x64.
//
// Pattern: a sequence of contiguous '1' bits surrounded by '0' bits
//
// Examples of encodable patterns:
//   0x80000000 → pcmpeqd + pslld #31 (shift left to isolate sign bit)
//   0xFF000000 → pcmpeqd + pslld #24 (shift left to isolate top byte)
//   0x00FFFFFF → pcmpeqd + psrld #8  (shift right to clear top byte)
//   0x7FFFFFFF → pcmpeqd + pslld #31 + psrld #1 (all ones except sign bit)
//
// Non-encodable patterns:
//   0x80000001 → non-contiguous bits
//   0xFF00FF00 → multiple separate ranges
//
// Usage: pcmpeqd xmm, xmm → pslld xmm, #leftShift → psrld xmm, #rightShift
class X86ContiguousBitPattern32 {
public:
    static X86ContiguousBitPattern32 create(uint32_t value)
    {
        if (!value || value == 0xFFFFFFFFU)
            return { }; // Zero and all-ones handled separately

        // Use bit manipulation to detect contiguous pattern
        // Technique: clz + ctz + popcount == 32 for contiguous bits
        unsigned leadingZeros = WTF::clz(value);
        unsigned trailingZeros = WTF::ctz(value);
        unsigned setBits = std::popcount(value);

        if (leadingZeros + trailingZeros + setBits != 32)
            return { }; // Not a contiguous pattern

        // Calculate shifts needed: shift left to position bits at top, then shift right
        uint8_t leftShift = static_cast<uint8_t>(32 - setBits);
        uint8_t rightShift = static_cast<uint8_t>(leadingZeros);

        return X86ContiguousBitPattern32(leftShift, rightShift);
    }

    bool isValid() const { return m_leftShift.has_value(); }
    uint8_t leftShift() const
    {
        ASSERT(isValid());
        return m_leftShift.value();
    }
    uint8_t rightShift() const
    {
        ASSERT(isValid());
        return m_rightShift;
    }

private:
    X86ContiguousBitPattern32() = default;

    X86ContiguousBitPattern32(uint8_t leftShift, uint8_t rightShift)
        : m_leftShift(leftShift)
        , m_rightShift(rightShift)
    {
    }

    std::optional<uint8_t> m_leftShift;
    uint8_t m_rightShift { 0 };
};

// X86ContiguousBitPattern64 - 64-bit version of contiguous bit pattern detection
class X86ContiguousBitPattern64 {
public:
    static X86ContiguousBitPattern64 create(uint64_t value)
    {
        if (!value || value == 0xFFFFFFFFFFFFFFFFULL)
            return { };

        unsigned leadingZeros = WTF::clz(value);
        unsigned trailingZeros = WTF::ctz(value);
        unsigned setBits = std::popcount(value);

        if (leadingZeros + trailingZeros + setBits != 64)
            return { };

        uint8_t leftShift = static_cast<uint8_t>(64 - setBits);
        uint8_t rightShift = static_cast<uint8_t>(leadingZeros);

        return X86ContiguousBitPattern64(leftShift, rightShift);
    }

    bool isValid() const { return m_leftShift.has_value(); }
    uint8_t leftShift() const
    {
        ASSERT(isValid());
        return m_leftShift.value();
    }
    uint8_t rightShift() const
    {
        ASSERT(isValid());
        return m_rightShift;
    }

private:
    X86ContiguousBitPattern64() = default;

    X86ContiguousBitPattern64(uint8_t leftShift, uint8_t rightShift)
        : m_leftShift(leftShift)
        , m_rightShift(rightShift)
    {
    }

    std::optional<uint8_t> m_leftShift;
    uint8_t m_rightShift { 0 };
};

ALWAYS_INLINE bool isValidARMThumb2Immediate(int64_t value)
{
    if (value < 0)
        return false;
    if (value > UINT32_MAX)
        return false;
    if (value < 256)
        return true;
    // If it can be expressed as an 8-bit number, left sifted by a constant
    const int64_t mask = (value ^ (value & (value - 1))) * 0xff;
    if ((value & mask) == value)
        return true;
    // FIXME: there are a few more valid forms, see section 4.2 in the Thumb-2 Supplement
    return false;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

ALWAYS_INLINE void* memcpyAtomic(void* dst, const void* src, size_t n)
{
    // This produces a much nicer error message for unaligned accesses.
    if constexpr (is32Bit())
        RELEASE_ASSERT(!(reinterpret_cast<uintptr_t>(dst) & static_cast<uintptr_t>(n - 1)));
    switch (n) {
    case 1:
        WTF::atomicStore(std::bit_cast<uint8_t*>(dst), *std::bit_cast<const uint8_t*>(src), std::memory_order_relaxed);
        return dst;
    case 2:
        WTF::atomicStore(std::bit_cast<uint16_t*>(dst), *std::bit_cast<const uint16_t*>(src), std::memory_order_relaxed);
        return dst;
    case 4:
        WTF::atomicStore(std::bit_cast<uint32_t*>(dst), *std::bit_cast<const uint32_t*>(src), std::memory_order_relaxed);
        return dst;
    case 8:
        WTF::atomicStore(std::bit_cast<uint64_t*>(dst), *std::bit_cast<const uint64_t*>(src), std::memory_order_relaxed);
        return dst;
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

ALWAYS_INLINE void* memcpyTearing(void* dst, const void* src, size_t n)
{
    // We should expect these instructions to be torn, so let's verify that.
    if (Options::fuzzAtomicJITMemcpy()) [[unlikely]] {
        auto* d = reinterpret_cast<uint8_t*>(dst);
        auto* s = reinterpret_cast<const uint8_t*>(src);
        for (size_t i = 0; i < n; ++i, ++s, ++d) {
            *d = *s;
            WTF::storeLoadFence();
        }
    }
    return memcpy(dst, src, n);
}

static ALWAYS_INLINE void* memcpyAtomicIfPossible(void* dst, const void* src, size_t n)
{
    if (isPowerOfTwo(n) && n <= sizeof(CPURegister))
        return memcpyAtomic(dst, src, n);
    return memcpyTearing(dst, src, n);
}

template<RepatchingInfo repatch>
void* performJITMemcpy(void* dst, const void* src, size_t n);

template<RepatchingInfo repatch>
ALWAYS_INLINE void* machineCodeCopy(void* dst, const void* src, size_t n)
{
    static_assert(!(*repatch).contains(RepatchingFlag::Flush));
    if constexpr (is32Bit()) {
        // Avoid unaligned accesses.
        if (WTF::isAligned(dst, n))
            return memcpyAtomicIfPossible(dst, src, n);
        return memcpyTearing(dst, src, n);
    }
    if constexpr ((*repatch).contains(RepatchingFlag::Memcpy) && (*repatch).contains(RepatchingFlag::Atomic))
        return memcpyAtomic(dst, src, n);
    else if constexpr ((*repatch).contains(RepatchingFlag::Memcpy))
        return memcpyAtomicIfPossible(dst, src, n);
    else
        return performJITMemcpy<repatch>(dst, src, n);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

} // namespace JSC.
