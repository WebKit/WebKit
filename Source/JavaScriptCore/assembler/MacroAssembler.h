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

#if ENABLE(ASSEMBLER)

#if CPU(ARM_THUMB2)
#include "MacroAssemblerARMv7.h"
namespace JSC { typedef MacroAssemblerARMv7 MacroAssemblerBase; };

#elif CPU(ARM_TRADITIONAL)
#include "MacroAssemblerARM.h"
namespace JSC { typedef MacroAssemblerARM MacroAssemblerBase; };

#elif CPU(MIPS)
#include "MacroAssemblerMIPS.h"
namespace JSC {
typedef MacroAssemblerMIPS MacroAssemblerBase;
};

#elif CPU(X86)
#include "MacroAssemblerX86.h"
namespace JSC { typedef MacroAssemblerX86 MacroAssemblerBase; };

#elif CPU(X86_64)
#include "MacroAssemblerX86_64.h"
namespace JSC { typedef MacroAssemblerX86_64 MacroAssemblerBase; };

#elif CPU(SH4)
#include "MacroAssemblerSH4.h"
namespace JSC {
typedef MacroAssemblerSH4 MacroAssemblerBase;
};

#else
#error "The MacroAssembler is not supported on this platform."
#endif

namespace JSC {

class MacroAssembler : public MacroAssemblerBase {
public:

    using MacroAssemblerBase::pop;
    using MacroAssemblerBase::jump;
    using MacroAssemblerBase::branch32;
#if CPU(X86_64)
    using MacroAssemblerBase::branchPtr;
    using MacroAssemblerBase::branchTestPtr;
#endif
    using MacroAssemblerBase::move;

#if ENABLE(JIT_CONSTANT_BLINDING)
    using MacroAssemblerBase::add32;
    using MacroAssemblerBase::and32;
    using MacroAssemblerBase::branchAdd32;
    using MacroAssemblerBase::branchMul32;
    using MacroAssemblerBase::branchSub32;
    using MacroAssemblerBase::lshift32;
    using MacroAssemblerBase::or32;
    using MacroAssemblerBase::rshift32;
    using MacroAssemblerBase::store32;
    using MacroAssemblerBase::sub32;
    using MacroAssemblerBase::urshift32;
    using MacroAssemblerBase::xor32;
#endif

    // Utilities used by the DFG JIT.
#if ENABLE(DFG_JIT)
    using MacroAssemblerBase::invert;
    
    static DoubleCondition invert(DoubleCondition cond)
    {
        switch (cond) {
        case DoubleEqual:
            return DoubleNotEqualOrUnordered;
        case DoubleNotEqual:
            return DoubleEqualOrUnordered;
        case DoubleGreaterThan:
            return DoubleLessThanOrEqualOrUnordered;
        case DoubleGreaterThanOrEqual:
            return DoubleLessThanOrUnordered;
        case DoubleLessThan:
            return DoubleGreaterThanOrEqualOrUnordered;
        case DoubleLessThanOrEqual:
            return DoubleGreaterThanOrUnordered;
        case DoubleEqualOrUnordered:
            return DoubleNotEqual;
        case DoubleNotEqualOrUnordered:
            return DoubleEqual;
        case DoubleGreaterThanOrUnordered:
            return DoubleLessThanOrEqual;
        case DoubleGreaterThanOrEqualOrUnordered:
            return DoubleLessThan;
        case DoubleLessThanOrUnordered:
            return DoubleGreaterThanOrEqual;
        case DoubleLessThanOrEqualOrUnordered:
            return DoubleGreaterThan;
        default:
            ASSERT_NOT_REACHED();
            return DoubleEqual; // make compiler happy
        }
    }
    
    static bool isInvertible(ResultCondition cond)
    {
        switch (cond) {
        case Zero:
        case NonZero:
            return true;
        default:
            return false;
        }
    }
    
    static ResultCondition invert(ResultCondition cond)
    {
        switch (cond) {
        case Zero:
            return NonZero;
        case NonZero:
            return Zero;
        default:
            ASSERT_NOT_REACHED();
            return Zero; // Make compiler happy for release builds.
        }
    }
#endif

    // Platform agnostic onvenience functions,
    // described in terms of other macro assembly methods.
    void pop()
    {
        addPtr(TrustedImm32(sizeof(void*)), stackPointerRegister);
    }
    
    void peek(RegisterID dest, int index = 0)
    {
        loadPtr(Address(stackPointerRegister, (index * sizeof(void*))), dest);
    }

    Address addressForPoke(int index)
    {
        return Address(stackPointerRegister, (index * sizeof(void*)));
    }
    
    void poke(RegisterID src, int index = 0)
    {
        storePtr(src, addressForPoke(index));
    }

    void poke(TrustedImm32 value, int index = 0)
    {
        store32(value, addressForPoke(index));
    }

    void poke(TrustedImmPtr imm, int index = 0)
    {
        storePtr(imm, addressForPoke(index));
    }


    // Backwards banches, these are currently all implemented using existing forwards branch mechanisms.
    void branchPtr(RelationalCondition cond, RegisterID op1, TrustedImmPtr imm, Label target)
    {
        branchPtr(cond, op1, imm).linkTo(target, this);
    }
    void branchPtr(RelationalCondition cond, RegisterID op1, ImmPtr imm, Label target)
    {
        branchPtr(cond, op1, imm).linkTo(target, this);
    }

    void branch32(RelationalCondition cond, RegisterID op1, RegisterID op2, Label target)
    {
        branch32(cond, op1, op2).linkTo(target, this);
    }

    void branch32(RelationalCondition cond, RegisterID op1, TrustedImm32 imm, Label target)
    {
        branch32(cond, op1, imm).linkTo(target, this);
    }
    
    void branch32(RelationalCondition cond, RegisterID op1, Imm32 imm, Label target)
    {
        branch32(cond, op1, imm).linkTo(target, this);
    }

    void branch32(RelationalCondition cond, RegisterID left, Address right, Label target)
    {
        branch32(cond, left, right).linkTo(target, this);
    }

    Jump branch32(RelationalCondition cond, TrustedImm32 left, RegisterID right)
    {
        return branch32(commute(cond), right, left);
    }

    Jump branch32(RelationalCondition cond, Imm32 left, RegisterID right)
    {
        return branch32(commute(cond), right, left);
    }

    void branchTestPtr(ResultCondition cond, RegisterID reg, Label target)
    {
        branchTestPtr(cond, reg).linkTo(target, this);
    }

#if !CPU(ARM_THUMB2)
    PatchableJump patchableBranchPtrWithPatch(RelationalCondition cond, Address left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(0))
    {
        return PatchableJump(branchPtrWithPatch(cond, left, dataLabel, initialRightValue));
    }

    PatchableJump patchableJump()
    {
        return PatchableJump(jump());
    }
#endif

    void jump(Label target)
    {
        jump().linkTo(target, this);
    }

    // Commute a relational condition, returns a new condition that will produce
    // the same results given the same inputs but with their positions exchanged.
    static RelationalCondition commute(RelationalCondition condition)
    {
        switch (condition) {
        case Above:
            return Below;
        case AboveOrEqual:
            return BelowOrEqual;
        case Below:
            return Above;
        case BelowOrEqual:
            return AboveOrEqual;
        case GreaterThan:
            return LessThan;
        case GreaterThanOrEqual:
            return LessThanOrEqual;
        case LessThan:
            return GreaterThan;
        case LessThanOrEqual:
            return GreaterThanOrEqual;
        default:
            break;
        }

        ASSERT(condition == Equal || condition == NotEqual);
        return condition;
    }
    

    // Ptr methods
    // On 32-bit platforms (i.e. x86), these methods directly map onto their 32-bit equivalents.
    // FIXME: should this use a test for 32-bitness instead of this specific exception?
#if !CPU(X86_64)
    void addPtr(RegisterID src, RegisterID dest)
    {
        add32(src, dest);
    }

    void addPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        add32(imm, srcDest);
    }

    void addPtr(TrustedImmPtr imm, RegisterID dest)
    {
        add32(TrustedImm32(imm), dest);
    }

    void addPtr(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        add32(imm, src, dest);
    }

    void addPtr(TrustedImm32 imm, AbsoluteAddress address)
    {
        add32(imm, address);
    }
    
    void andPtr(RegisterID src, RegisterID dest)
    {
        and32(src, dest);
    }

    void andPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        and32(imm, srcDest);
    }

    void orPtr(RegisterID src, RegisterID dest)
    {
        or32(src, dest);
    }

    void orPtr(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        or32(op1, op2, dest);
    }

    void orPtr(TrustedImmPtr imm, RegisterID dest)
    {
        or32(TrustedImm32(imm), dest);
    }

    void orPtr(TrustedImm32 imm, RegisterID dest)
    {
        or32(imm, dest);
    }

    void subPtr(RegisterID src, RegisterID dest)
    {
        sub32(src, dest);
    }
    
    void subPtr(TrustedImm32 imm, RegisterID dest)
    {
        sub32(imm, dest);
    }
    
    void subPtr(TrustedImmPtr imm, RegisterID dest)
    {
        sub32(TrustedImm32(imm), dest);
    }

    void xorPtr(RegisterID src, RegisterID dest)
    {
        xor32(src, dest);
    }

    void xorPtr(TrustedImm32 imm, RegisterID srcDest)
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

    void loadPtr(const void* address, RegisterID dest)
    {
        load32(address, dest);
    }

    DataLabel32 loadPtrWithAddressOffsetPatch(Address address, RegisterID dest)
    {
        return load32WithAddressOffsetPatch(address, dest);
    }
    
    DataLabelCompact loadPtrWithCompactAddressOffsetPatch(Address address, RegisterID dest)
    {
        return load32WithCompactAddressOffsetPatch(address, dest);
    }

    void move(ImmPtr imm, RegisterID dest)
    {
        move(Imm32(imm.asTrustedImmPtr()), dest);
    }

    void comparePtr(RelationalCondition cond, RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        compare32(cond, left, right, dest);
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

    void storePtr(TrustedImmPtr imm, ImplicitAddress address)
    {
        store32(TrustedImm32(imm), address);
    }
    
    void storePtr(ImmPtr imm, Address address)
    {
        store32(Imm32(imm.asTrustedImmPtr()), address);
    }

    void storePtr(TrustedImmPtr imm, void* address)
    {
        store32(TrustedImm32(imm), address);
    }

    DataLabel32 storePtrWithAddressOffsetPatch(RegisterID src, Address address)
    {
        return store32WithAddressOffsetPatch(src, address);
    }

    Jump branchPtr(RelationalCondition cond, RegisterID left, RegisterID right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, RegisterID left, TrustedImmPtr right)
    {
        return branch32(cond, left, TrustedImm32(right));
    }
    
    Jump branchPtr(RelationalCondition cond, RegisterID left, ImmPtr right)
    {
        return branch32(cond, left, Imm32(right.asTrustedImmPtr()));
    }

    Jump branchPtr(RelationalCondition cond, RegisterID left, Address right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, Address left, RegisterID right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, AbsoluteAddress left, RegisterID right)
    {
        return branch32(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, Address left, TrustedImmPtr right)
    {
        return branch32(cond, left, TrustedImm32(right));
    }
    
    Jump branchPtr(RelationalCondition cond, AbsoluteAddress left, TrustedImmPtr right)
    {
        return branch32(cond, left, TrustedImm32(right));
    }

    Jump branchTestPtr(ResultCondition cond, RegisterID reg, RegisterID mask)
    {
        return branchTest32(cond, reg, mask);
    }

    Jump branchTestPtr(ResultCondition cond, RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        return branchTest32(cond, reg, mask);
    }

    Jump branchTestPtr(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        return branchTest32(cond, address, mask);
    }

    Jump branchTestPtr(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        return branchTest32(cond, address, mask);
    }

    Jump branchAddPtr(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchAdd32(cond, src, dest);
    }

    Jump branchSubPtr(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchSub32(cond, imm, dest);
    }
    using MacroAssemblerBase::branchTest8;
    Jump branchTest8(ResultCondition cond, ExtendedAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        return MacroAssemblerBase::branchTest8(cond, Address(address.base, address.offset), mask);
    }
#else
    
#if ENABLE(JIT_CONSTANT_BLINDING)
    using MacroAssemblerBase::addPtr;
    using MacroAssemblerBase::andPtr;
    using MacroAssemblerBase::branchSubPtr;
    using MacroAssemblerBase::convertInt32ToDouble;
    using MacroAssemblerBase::storePtr;
    using MacroAssemblerBase::subPtr;
    using MacroAssemblerBase::xorPtr;
    
    bool shouldBlindDouble(double value)
    {
        // Don't trust NaN or +/-Infinity
        if (!isfinite(value))
            return true;

        // Try to force normalisation, and check that there's no change
        // in the bit pattern
        if (bitwise_cast<uintptr_t>(value * 1.0) != bitwise_cast<uintptr_t>(value))
            return true;

        value = abs(value);
        // Only allow a limited set of fractional components
        double scaledValue = value * 8;
        if (scaledValue / 8 != value)
            return true;
        double frac = scaledValue - floor(scaledValue);
        if (frac != 0.0)
            return true;

        return value > 0xff;
    }
    
    bool shouldBlind(ImmPtr imm)
    { 
#if !defined(NDEBUG)
        UNUSED_PARAM(imm);
        // Debug always blind all constants, if only so we know
        // if we've broken blinding during patch development.
        return true;        
#endif

        // First off we'll special case common, "safe" values to avoid hurting
        // performance too much
        uintptr_t value = imm.asTrustedImmPtr().asIntptr();
        switch (value) {
        case 0xffff:
        case 0xffffff:
        case 0xffffffffL:
        case 0xffffffffffL:
        case 0xffffffffffffL:
        case 0xffffffffffffffL:
        case 0xffffffffffffffffL:
            return false;
        default: {
            if (value <= 0xff)
                return false;
#if CPU(X86_64)
            JSValue jsValue = JSValue::decode(reinterpret_cast<void*>(value));
            if (jsValue.isInt32())
                return shouldBlind(Imm32(jsValue.asInt32()));
            if (jsValue.isDouble() && !shouldBlindDouble(jsValue.asDouble()))
                return false;

            if (!shouldBlindDouble(bitwise_cast<double>(value)))
                return false;
#endif 
        }
        }
        return shouldBlindForSpecificArch(value);
    }
    
    struct RotatedImmPtr {
        RotatedImmPtr(uintptr_t v1, uint8_t v2)
            : value(v1)
            , rotation(v2)
        {
        }
        TrustedImmPtr value;
        TrustedImm32 rotation;
    };
    
    RotatedImmPtr rotationBlindConstant(ImmPtr imm)
    {
        uint8_t rotation = random() % (sizeof(void*) * 8);
        uintptr_t value = imm.asTrustedImmPtr().asIntptr();
        value = (value << rotation) | (value >> (sizeof(void*) * 8 - rotation));
        return RotatedImmPtr(value, rotation);
    }
    
    void loadRotationBlindedConstant(RotatedImmPtr constant, RegisterID dest)
    {
        move(constant.value, dest);
        rotateRightPtr(constant.rotation, dest);
    }

    void convertInt32ToDouble(Imm32 imm, FPRegisterID dest)
    {
        if (shouldBlind(imm)) {
            RegisterID scratchRegister = scratchRegisterForBlinding();
            loadXorBlindedConstant(xorBlindConstant(imm), scratchRegister);
            convertInt32ToDouble(scratchRegister, dest);
        } else
            convertInt32ToDouble(imm.asTrustedImm32(), dest);
    }

    void move(ImmPtr imm, RegisterID dest)
    {
        if (shouldBlind(imm))
            loadRotationBlindedConstant(rotationBlindConstant(imm), dest);
        else
            move(imm.asTrustedImmPtr(), dest);
    }

    Jump branchPtr(RelationalCondition cond, RegisterID left, ImmPtr right)
    {
        if (shouldBlind(right)) {
            RegisterID scratchRegister = scratchRegisterForBlinding();
            loadRotationBlindedConstant(rotationBlindConstant(right), scratchRegister);
            return branchPtr(cond, left, scratchRegister);
        }
        return branchPtr(cond, left, right.asTrustedImmPtr());
    }
    
    void storePtr(ImmPtr imm, Address dest)
    {
        if (shouldBlind(imm)) {
            RegisterID scratchRegister = scratchRegisterForBlinding();
            loadRotationBlindedConstant(rotationBlindConstant(imm), scratchRegister);
            storePtr(scratchRegister, dest);
        } else
            storePtr(imm.asTrustedImmPtr(), dest);
    }

#endif

#endif // !CPU(X86_64)

#if ENABLE(JIT_CONSTANT_BLINDING)
    bool shouldBlind(Imm32 imm)
    { 
#if !defined(NDEBUG)
        UNUSED_PARAM(imm);
        // Debug always blind all constants, if only so we know
        // if we've broken blinding during patch development.
        return true;
#else

        // First off we'll special case common, "safe" values to avoid hurting
        // performance too much
        uint32_t value = imm.asTrustedImm32().m_value;
        switch (value) {
        case 0xffff:
        case 0xffffff:
        case 0xffffffff:
            return false;
        default:
            if (value <= 0xff)
                return false;
        }
        return shouldBlindForSpecificArch(value);
#endif
    }

    struct BlindedImm32 {
        BlindedImm32(int32_t v1, int32_t v2)
            : value1(v1)
            , value2(v2)
        {
        }
        TrustedImm32 value1;
        TrustedImm32 value2;
    };

    uint32_t keyForConstant(uint32_t value, uint32_t& mask)
    {
        uint32_t key = random();
        if (value <= 0xff)
            mask = 0xff;
        else if (value <= 0xffff)
            mask = 0xffff;
        else if (value <= 0xffffff)
            mask = 0xffffff;
        else
            mask = 0xffffffff;
        return key & mask;
    }

    uint32_t keyForConstant(uint32_t value)
    {
        uint32_t mask = 0;
        return keyForConstant(value, mask);
    }

    BlindedImm32 xorBlindConstant(Imm32 imm)
    {
        uint32_t baseValue = imm.asTrustedImm32().m_value;
        uint32_t key = keyForConstant(baseValue);
        return BlindedImm32(baseValue ^ key, key);
    }

    BlindedImm32 additionBlindedConstant(Imm32 imm)
    {
        // The addition immediate may be used as a pointer offset. Keep aligned based on "imm".
        static uint32_t maskTable[4] = { 0xfffffffc, 0xffffffff, 0xfffffffe, 0xffffffff };

        uint32_t baseValue = imm.asTrustedImm32().m_value;
        uint32_t key = keyForConstant(baseValue) & maskTable[baseValue & 3];
        if (key > baseValue)
            key = key - baseValue;
        return BlindedImm32(baseValue - key, key);
    }
    
    BlindedImm32 andBlindedConstant(Imm32 imm)
    {
        uint32_t baseValue = imm.asTrustedImm32().m_value;
        uint32_t mask = 0;
        uint32_t key = keyForConstant(baseValue, mask);
        ASSERT((baseValue & mask) == baseValue);
        return BlindedImm32(((baseValue & key) | ~key) & mask, ((baseValue & ~key) | key) & mask);
    }
    
    BlindedImm32 orBlindedConstant(Imm32 imm)
    {
        uint32_t baseValue = imm.asTrustedImm32().m_value;
        uint32_t mask = 0;
        uint32_t key = keyForConstant(baseValue, mask);
        ASSERT((baseValue & mask) == baseValue);
        return BlindedImm32((baseValue & key) & mask, (baseValue & ~key) & mask);
    }
    
    void loadXorBlindedConstant(BlindedImm32 constant, RegisterID dest)
    {
        move(constant.value1, dest);
        xor32(constant.value2, dest);
    }
    
    void add32(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = additionBlindedConstant(imm);
            add32(key.value1, dest);
            add32(key.value2, dest);
        } else
            add32(imm.asTrustedImm32(), dest);
    }
    
    void addPtr(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = additionBlindedConstant(imm);
            addPtr(key.value1, dest);
            addPtr(key.value2, dest);
        } else
            addPtr(imm.asTrustedImm32(), dest);
    }

    void and32(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = andBlindedConstant(imm);
            and32(key.value1, dest);
            and32(key.value2, dest);
        } else
            and32(imm.asTrustedImm32(), dest);
    }

    void andPtr(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = andBlindedConstant(imm);
            andPtr(key.value1, dest);
            andPtr(key.value2, dest);
        } else
            andPtr(imm.asTrustedImm32(), dest);
    }
    
    void and32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            if (src == dest)
                return and32(imm.asTrustedImm32(), dest);
            loadXorBlindedConstant(xorBlindConstant(imm), dest);
            and32(src, dest);
        } else
            and32(imm.asTrustedImm32(), src, dest);
    }

    void move(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm))
            loadXorBlindedConstant(xorBlindConstant(imm), dest);
        else
            move(imm.asTrustedImm32(), dest);
    }
    
    void or32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            if (src == dest)
                return or32(imm, dest);
            loadXorBlindedConstant(xorBlindConstant(imm), dest);
            or32(src, dest);
        } else
            or32(imm.asTrustedImm32(), src, dest);
    }
    
    void or32(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = orBlindedConstant(imm);
            or32(key.value1, dest);
            or32(key.value2, dest);
        } else
            or32(imm.asTrustedImm32(), dest);
    }
    
    void poke(Imm32 value, int index = 0)
    {
        store32(value, addressForPoke(index));
    }
    
    void poke(ImmPtr value, int index = 0)
    {
        storePtr(value, addressForPoke(index));
    }
    
    void store32(Imm32 imm, Address dest)
    {
        if (shouldBlind(imm)) {
#if CPU(X86) || CPU(X86_64)
            BlindedImm32 blind = xorBlindConstant(imm);
            store32(blind.value1, dest);
            xor32(blind.value2, dest);
#else
            if (RegisterID scratchRegister = (RegisterID)scratchRegisterForBlinding()) {
                loadXorBlindedConstant(xorBlindConstant(imm), scratchRegister);
                store32(scratchRegister, dest);
            } else {
                // If we don't have a scratch register available for use, we'll just 
                // place a random number of nops.
                uint32_t nopCount = random() & 3;
                while (nopCount--)
                    nop();
                store32(imm.asTrustedImm32(), dest);
            }
#endif
        } else
            store32(imm.asTrustedImm32(), dest);
    }
    
    void sub32(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = additionBlindedConstant(imm);
            sub32(key.value1, dest);
            sub32(key.value2, dest);
        } else
            sub32(imm.asTrustedImm32(), dest);
    }
    
    void subPtr(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = additionBlindedConstant(imm);
            subPtr(key.value1, dest);
            subPtr(key.value2, dest);
        } else
            subPtr(imm.asTrustedImm32(), dest);
    }
    
    void xor32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 blind = xorBlindConstant(imm);
            xor32(blind.value1, src, dest);
            xor32(blind.value2, dest);
        } else
            xor32(imm.asTrustedImm32(), src, dest);
    }
    
    void xor32(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 blind = xorBlindConstant(imm);
            xor32(blind.value1, dest);
            xor32(blind.value2, dest);
        } else
            xor32(imm.asTrustedImm32(), dest);
    }

    Jump branch32(RelationalCondition cond, RegisterID left, Imm32 right)
    {
        if (shouldBlind(right)) {
            if (RegisterID scratchRegister = (RegisterID)scratchRegisterForBlinding()) {
                loadXorBlindedConstant(xorBlindConstant(right), scratchRegister);
                return branch32(cond, left, scratchRegister);
            }
            // If we don't have a scratch register available for use, we'll just 
            // place a random number of nops.
            uint32_t nopCount = random() & 3;
            while (nopCount--)
                nop();
            return branch32(cond, left, right.asTrustedImm32());
        }
        
        return branch32(cond, left, right.asTrustedImm32());
    }

    Jump branchAdd32(ResultCondition cond, RegisterID src, Imm32 imm, RegisterID dest)
    {
        if (src == dest) {
            if (!scratchRegisterForBlinding()) {
                // Release mode ASSERT, if this fails we will perform incorrect codegen.
                CRASH();
            }
        }
        if (shouldBlind(imm)) {
            if (src == dest) {
                if (RegisterID scratchRegister = (RegisterID)scratchRegisterForBlinding()) {
                    move(src, scratchRegister);
                    src = scratchRegister;
                }
            }
            loadXorBlindedConstant(xorBlindConstant(imm), dest);
            return branchAdd32(cond, src, dest);  
        }
        return branchAdd32(cond, src, imm.asTrustedImm32(), dest);            
    }
    
    Jump branchMul32(ResultCondition cond, Imm32 imm, RegisterID src, RegisterID dest)
    {
        if (src == dest) {
            if (!scratchRegisterForBlinding()) {
                // Release mode ASSERT, if this fails we will perform incorrect codegen.
                CRASH();
            }
        }
        if (shouldBlind(imm)) {
            if (src == dest) {
                if (RegisterID scratchRegister = (RegisterID)scratchRegisterForBlinding()) {
                    move(src, scratchRegister);
                    src = scratchRegister;
                }
            }
            loadXorBlindedConstant(xorBlindConstant(imm), dest);
            return branchMul32(cond, src, dest);  
        }
        return branchMul32(cond, imm.asTrustedImm32(), src, dest);
    }

    // branchSub32 takes a scratch register as 32 bit platforms make use of this,
    // with src == dst, and on x86-32 we don't have a platform scratch register.
    Jump branchSub32(ResultCondition cond, RegisterID src, Imm32 imm, RegisterID dest, RegisterID scratch)
    {
        if (shouldBlind(imm)) {
            ASSERT(scratch != dest);
            ASSERT(scratch != src);
            loadXorBlindedConstant(xorBlindConstant(imm), scratch);
            return branchSub32(cond, src, scratch, dest);
        }
        return branchSub32(cond, src, imm.asTrustedImm32(), dest);            
    }
    
    // Immediate shifts only have 5 controllable bits
    // so we'll consider them safe for now.
    TrustedImm32 trustedImm32ForShift(Imm32 imm)
    {
        return TrustedImm32(imm.asTrustedImm32().m_value & 31);
    }

    void lshift32(Imm32 imm, RegisterID dest)
    {
        lshift32(trustedImm32ForShift(imm), dest);
    }
    
    void lshift32(RegisterID src, Imm32 amount, RegisterID dest)
    {
        lshift32(src, trustedImm32ForShift(amount), dest);
    }
    
    void rshift32(Imm32 imm, RegisterID dest)
    {
        rshift32(trustedImm32ForShift(imm), dest);
    }
    
    void rshift32(RegisterID src, Imm32 amount, RegisterID dest)
    {
        rshift32(src, trustedImm32ForShift(amount), dest);
    }
    
    void urshift32(Imm32 imm, RegisterID dest)
    {
        urshift32(trustedImm32ForShift(imm), dest);
    }
    
    void urshift32(RegisterID src, Imm32 amount, RegisterID dest)
    {
        urshift32(src, trustedImm32ForShift(amount), dest);
    }
#endif
};

} // namespace JSC

#else // ENABLE(ASSEMBLER)

// If there is no assembler for this platform, at least allow code to make references to
// some of the things it would otherwise define, albeit without giving that code any way
// of doing anything useful.
class MacroAssembler {
private:
    MacroAssembler() { }
    
public:
    
    enum RegisterID { NoRegister };
    enum FPRegisterID { NoFPRegister };
};

#endif // ENABLE(ASSEMBLER)

#endif // MacroAssembler_h
