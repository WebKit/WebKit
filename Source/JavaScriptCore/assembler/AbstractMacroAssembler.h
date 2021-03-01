/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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

#include "AbortReason.h"
#include "AssemblerBuffer.h"
#include "AssemblerCommon.h"
#include "CPU.h"
#include "CodeLocation.h"
#include "JSCJSValue.h"
#include "JSCPtrTag.h"
#include "MacroAssemblerCodeRef.h"
#include "MacroAssemblerHelpers.h"
#include "Options.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Noncopyable.h>
#include <wtf/SharedTask.h>
#include <wtf/Vector.h>
#include <wtf/WeakRandom.h>

namespace JSC {

#if ENABLE(ASSEMBLER)

class AllowMacroScratchRegisterUsage;
class LinkBuffer;
class Watchpoint;

template<typename T> class DisallowMacroScratchRegisterUsage;

namespace DFG {
struct OSRExit;
}

class AbstractMacroAssemblerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum StatusCondition {
        Success,
        Failure
    };
    
    static StatusCondition invert(StatusCondition condition)
    {
        switch (condition) {
        case Success:
            return Failure;
        case Failure:
            return Success;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return Success;
    }
};

template <class AssemblerType>
class AbstractMacroAssembler : public AbstractMacroAssemblerBase {
public:
    typedef AbstractMacroAssembler<AssemblerType> AbstractMacroAssemblerType;
    typedef AssemblerType AssemblerType_T;

    template<PtrTag tag> using CodePtr = MacroAssemblerCodePtr<tag>;
    template<PtrTag tag> using CodeRef = MacroAssemblerCodeRef<tag>;

    enum class CPUIDCheckState {
        NotChecked,
        Clear,
        Set
    };

    class Jump;

    typedef typename AssemblerType::RegisterID RegisterID;
    typedef typename AssemblerType::SPRegisterID SPRegisterID;
    typedef typename AssemblerType::FPRegisterID FPRegisterID;
    
    static constexpr RegisterID firstRegister() { return AssemblerType::firstRegister(); }
    static constexpr RegisterID lastRegister() { return AssemblerType::lastRegister(); }
    static constexpr unsigned numberOfRegisters() { return AssemblerType::numberOfRegisters(); }
    static const char* gprName(RegisterID id) { return AssemblerType::gprName(id); }

    static constexpr SPRegisterID firstSPRegister() { return AssemblerType::firstSPRegister(); }
    static constexpr SPRegisterID lastSPRegister() { return AssemblerType::lastSPRegister(); }
    static constexpr unsigned numberOfSPRegisters() { return AssemblerType::numberOfSPRegisters(); }
    static const char* sprName(SPRegisterID id) { return AssemblerType::sprName(id); }

    static constexpr FPRegisterID firstFPRegister() { return AssemblerType::firstFPRegister(); }
    static constexpr FPRegisterID lastFPRegister() { return AssemblerType::lastFPRegister(); }
    static constexpr unsigned numberOfFPRegisters() { return AssemblerType::numberOfFPRegisters(); }
    static const char* fprName(FPRegisterID id) { return AssemblerType::fprName(id); }

    // Section 1: MacroAssembler operand types
    //
    // The following types are used as operands to MacroAssembler operations,
    // describing immediate  and memory operands to the instructions to be planted.

    enum Scale {
        TimesOne,
        TimesTwo,
        TimesFour,
        TimesEight,
        ScalePtr = isAddress64Bit() ? TimesEight : TimesFour,
    };
    
    struct BaseIndex;
    
    static RegisterID withSwappedRegister(RegisterID original, RegisterID left, RegisterID right)
    {
        if (original == left)
            return right;
        if (original == right)
            return left;
        return original;
    }
    
    // Address:
    //
    // Describes a simple base-offset address.
    struct Address {
        explicit Address(RegisterID base, int32_t offset = 0)
            : base(base)
            , offset(offset)
        {
        }
        
        Address withOffset(int32_t additionalOffset)
        {
            return Address(base, offset + additionalOffset);
        }
        
        Address withSwappedRegister(RegisterID left, RegisterID right)
        {
            return Address(AbstractMacroAssembler::withSwappedRegister(base, left, right), offset);
        }
        
        BaseIndex indexedBy(RegisterID index, Scale) const;
        
        RegisterID base;
        int32_t offset;
    };

    struct ExtendedAddress {
        explicit ExtendedAddress(RegisterID base, intptr_t offset = 0)
            : base(base)
            , offset(offset)
        {
        }
        
        RegisterID base;
        intptr_t offset;
    };

    // ImplicitAddress:
    //
    // This class is used for explicit 'load' and 'store' operations
    // (as opposed to situations in which a memory operand is provided
    // to a generic operation, such as an integer arithmetic instruction).
    //
    // In the case of a load (or store) operation we want to permit
    // addresses to be implicitly constructed, e.g. the two calls:
    //
    //     load32(Address(addrReg), destReg);
    //     load32(addrReg, destReg);
    //
    // Are equivalent, and the explicit wrapping of the Address in the former
    // is unnecessary.
    struct ImplicitAddress {
        ImplicitAddress(RegisterID base)
            : base(base)
            , offset(0)
        {
            ASSERT(base != RegisterID::InvalidGPRReg);
        }

        ImplicitAddress(Address address)
            : base(address.base)
            , offset(address.offset)
        {
            ASSERT(base != RegisterID::InvalidGPRReg);
        }

        RegisterID base;
        int32_t offset;
    };

    // BaseIndex:
    //
    // Describes a complex addressing mode.
    struct BaseIndex {
        BaseIndex(RegisterID base, RegisterID index, Scale scale, int32_t offset = 0)
            : base(base)
            , index(index)
            , scale(scale)
            , offset(offset)
        {
        }
        
        RegisterID base;
        RegisterID index;
        Scale scale;
        int32_t offset;
        
        BaseIndex withOffset(int32_t additionalOffset)
        {
            return BaseIndex(base, index, scale, offset + additionalOffset);
        }

        BaseIndex withSwappedRegister(RegisterID left, RegisterID right)
        {
            return BaseIndex(AbstractMacroAssembler::withSwappedRegister(base, left, right), AbstractMacroAssembler::withSwappedRegister(index, left, right), scale, offset);
        }
    };

    // AbsoluteAddress:
    //
    // Describes an memory operand given by a pointer.  For regular load & store
    // operations an unwrapped void* will be used, rather than using this.
    struct AbsoluteAddress {
        explicit AbsoluteAddress(const void* ptr)
            : m_ptr(ptr)
        {
        }

        const void* m_ptr;
    };

    // TrustedImm:
    //
    // An empty super class of each of the TrustedImm types. This class is used for template overloads
    // on a TrustedImm type via std::is_base_of.
    struct TrustedImm { };

    // TrustedImmPtr:
    //
    // A pointer sized immediate operand to an instruction - this is wrapped
    // in a class requiring explicit construction in order to differentiate
    // from pointers used as absolute addresses to memory operations
    struct TrustedImmPtr : public TrustedImm {
        TrustedImmPtr() { }
        
        explicit TrustedImmPtr(const void* value)
            : m_value(value)
        {
        }

        template<typename ReturnType, typename... Arguments>
        explicit TrustedImmPtr(ReturnType(*value)(Arguments...))
            : m_value(reinterpret_cast<void*>(value))
        {
        }

        explicit TrustedImmPtr(std::nullptr_t)
        {
        }

        explicit TrustedImmPtr(size_t value)
            : m_value(reinterpret_cast<void*>(value))
        {
        }

        intptr_t asIntptr()
        {
            return reinterpret_cast<intptr_t>(m_value);
        }

        void* asPtr()
        {
            return const_cast<void*>(m_value);
        }

        const void* m_value { nullptr };
    };

    struct ImmPtr : private TrustedImmPtr
    {
        explicit ImmPtr(const void* value)
            : TrustedImmPtr(value)
        {
        }

        TrustedImmPtr asTrustedImmPtr() { return *this; }
    };

    // TrustedImm32:
    //
    // A 32bit immediate operand to an instruction - this is wrapped in a
    // class requiring explicit construction in order to prevent RegisterIDs
    // (which are implemented as an enum) from accidentally being passed as
    // immediate values.
    struct TrustedImm32 : public TrustedImm {
        TrustedImm32() { }
        
        explicit TrustedImm32(int32_t value)
            : m_value(value)
        {
        }

#if !CPU(X86_64)
        explicit TrustedImm32(TrustedImmPtr ptr)
            : m_value(ptr.asIntptr())
        {
        }
#endif

        int32_t m_value;
    };


    struct Imm32 : private TrustedImm32 {
        explicit Imm32(int32_t value)
            : TrustedImm32(value)
        {
        }
#if !CPU(X86_64)
        explicit Imm32(TrustedImmPtr ptr)
            : TrustedImm32(ptr)
        {
        }
#endif
        const TrustedImm32& asTrustedImm32() const { return *this; }

    };
    
    // TrustedImm64:
    //
    // A 64bit immediate operand to an instruction - this is wrapped in a
    // class requiring explicit construction in order to prevent RegisterIDs
    // (which are implemented as an enum) from accidentally being passed as
    // immediate values.
    struct TrustedImm64 : TrustedImm {
        TrustedImm64() { }
        
        explicit TrustedImm64(int64_t value)
            : m_value(value)
        {
        }

#if CPU(X86_64) || CPU(ARM64)
        explicit TrustedImm64(TrustedImmPtr ptr)
            : m_value(ptr.asIntptr())
        {
        }
#endif

        int64_t m_value;
    };

    struct Imm64 : private TrustedImm64
    {
        explicit Imm64(int64_t value)
            : TrustedImm64(value)
        {
        }
#if CPU(X86_64) || CPU(ARM64)
        explicit Imm64(TrustedImmPtr ptr)
            : TrustedImm64(ptr)
        {
        }
#endif
        const TrustedImm64& asTrustedImm64() const { return *this; }
    };
    
    // Section 2: MacroAssembler code buffer handles
    //
    // The following types are used to reference items in the code buffer
    // during JIT code generation.  For example, the type Jump is used to
    // track the location of a jump instruction so that it may later be
    // linked to a label marking its destination.


    // Label:
    //
    // A Label records a point in the generated instruction stream, typically such that
    // it may be used as a destination for a jump.
    class Label {
        friend class AbstractMacroAssembler<AssemblerType>;
        friend struct DFG::OSRExit;
        friend class Jump;
        template<PtrTag> friend class MacroAssemblerCodeRef;
        friend class LinkBuffer;
        friend class Watchpoint;

    public:
        Label()
        {
        }

        Label(AbstractMacroAssemblerType* masm)
            : m_label(masm->m_assembler.label())
        {
            masm->invalidateAllTempRegisters();
        }

        bool operator==(const Label& other) const { return m_label == other.m_label; }

        bool isSet() const { return m_label.isSet(); }
    private:
        AssemblerLabel m_label;
    };
    
    // ConvertibleLoadLabel:
    //
    // A ConvertibleLoadLabel records a loadPtr instruction that can be patched to an addPtr
    // so that:
    //
    // loadPtr(Address(a, i), b)
    //
    // becomes:
    //
    // addPtr(TrustedImmPtr(i), a, b)
    class ConvertibleLoadLabel {
        friend class AbstractMacroAssembler<AssemblerType>;
        friend class LinkBuffer;
        
    public:
        ConvertibleLoadLabel()
        {
        }
        
        ConvertibleLoadLabel(AbstractMacroAssemblerType* masm)
            : m_label(masm->m_assembler.labelIgnoringWatchpoints())
        {
        }
        
        bool isSet() const { return m_label.isSet(); }
    private:
        AssemblerLabel m_label;
    };

    // DataLabelPtr:
    //
    // A DataLabelPtr is used to refer to a location in the code containing a pointer to be
    // patched after the code has been generated.
    class DataLabelPtr {
        friend class AbstractMacroAssembler<AssemblerType>;
        friend class LinkBuffer;
    public:
        DataLabelPtr()
        {
        }

        DataLabelPtr(AbstractMacroAssemblerType* masm)
            : m_label(masm->m_assembler.label())
        {
        }

        bool isSet() const { return m_label.isSet(); }
        
    private:
        AssemblerLabel m_label;
    };

    // DataLabel32:
    //
    // A DataLabel32 is used to refer to a location in the code containing a 32-bit constant to be
    // patched after the code has been generated.
    class DataLabel32 {
        friend class AbstractMacroAssembler<AssemblerType>;
        friend class LinkBuffer;
    public:
        DataLabel32()
        {
        }

        DataLabel32(AbstractMacroAssemblerType* masm)
            : m_label(masm->m_assembler.label())
        {
        }

        AssemblerLabel label() const { return m_label; }

    private:
        AssemblerLabel m_label;
    };

    // DataLabelCompact:
    //
    // A DataLabelCompact is used to refer to a location in the code containing a
    // compact immediate to be patched after the code has been generated.
    class DataLabelCompact {
        friend class AbstractMacroAssembler<AssemblerType>;
        friend class LinkBuffer;
    public:
        DataLabelCompact()
        {
        }
        
        DataLabelCompact(AbstractMacroAssemblerType* masm)
            : m_label(masm->m_assembler.label())
        {
        }

        DataLabelCompact(AssemblerLabel label)
            : m_label(label)
        {
        }

        AssemblerLabel label() const { return m_label; }

    private:
        AssemblerLabel m_label;
    };

    // Call:
    //
    // A Call object is a reference to a call instruction that has been planted
    // into the code buffer - it is typically used to link the call, setting the
    // relative offset such that when executed it will call to the desired
    // destination.
    class Call {
        friend class AbstractMacroAssembler<AssemblerType>;

    public:
        enum Flags {
            None = 0x0,
            Linkable = 0x1,
            Near = 0x2,
            Tail = 0x4,
            LinkableNear = 0x3,
            LinkableNearTail = 0x7,
        };

        Call()
            : m_flags(None)
        {
        }
        
        Call(AssemblerLabel jmp, Flags flags)
            : m_label(jmp)
            , m_flags(flags)
        {
        }

        bool isFlagSet(Flags flag)
        {
            return m_flags & flag;
        }

        static Call fromTailJump(Jump jump)
        {
            return Call(jump.m_label, Linkable);
        }

        AssemblerLabel m_label;
    private:
        Flags m_flags;
    };

    // Jump:
    //
    // A jump object is a reference to a jump instruction that has been planted
    // into the code buffer - it is typically used to link the jump, setting the
    // relative offset such that when executed it will jump to the desired
    // destination.
    class Jump {
        friend class AbstractMacroAssembler<AssemblerType>;
        friend class Call;
        friend struct DFG::OSRExit;
        friend class LinkBuffer;
    public:
        Jump() = default;

#if CPU(ARM_THUMB2)
        // Fixme: this information should be stored in the instruction stream, not in the Jump object.
        Jump(AssemblerLabel jmp, ARMv7Assembler::JumpType type = ARMv7Assembler::JumpNoCondition, ARMv7Assembler::Condition condition = ARMv7Assembler::ConditionInvalid)
            : m_label(jmp)
            , m_type(type)
            , m_condition(condition)
        {
        }
#elif CPU(ARM64)
        Jump(AssemblerLabel jmp, ARM64Assembler::JumpType type = ARM64Assembler::JumpNoCondition, ARM64Assembler::Condition condition = ARM64Assembler::ConditionInvalid)
            : m_label(jmp)
            , m_type(type)
            , m_condition(condition)
        {
        }

        Jump(AssemblerLabel jmp, ARM64Assembler::JumpType type, ARM64Assembler::Condition condition, bool is64Bit, ARM64Assembler::RegisterID compareRegister)
            : m_label(jmp)
            , m_type(type)
            , m_condition(condition)
            , m_is64Bit(is64Bit)
            , m_compareRegister(compareRegister)
        {
            ASSERT((type == ARM64Assembler::JumpCompareAndBranch) || (type == ARM64Assembler::JumpCompareAndBranchFixedSize));
        }

        Jump(AssemblerLabel jmp, ARM64Assembler::JumpType type, ARM64Assembler::Condition condition, unsigned bitNumber, ARM64Assembler::RegisterID compareRegister)
            : m_label(jmp)
            , m_type(type)
            , m_condition(condition)
            , m_bitNumber(bitNumber)
            , m_compareRegister(compareRegister)
        {
            ASSERT((type == ARM64Assembler::JumpTestBit) || (type == ARM64Assembler::JumpTestBitFixedSize));
        }
#else
        Jump(AssemblerLabel jmp)    
            : m_label(jmp)
        {
        }
#endif
        
        Label label() const
        {
            Label result;
            result.m_label = m_label;
            return result;
        }

        void link(AbstractMacroAssemblerType* masm) const
        {
            masm->invalidateAllTempRegisters();

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
            masm->checkRegisterAllocationAgainstBranchRange(m_label.offset(), masm->debugOffset());
#endif

#if CPU(ARM_THUMB2)
            masm->m_assembler.linkJump(m_label, masm->m_assembler.label(), m_type, m_condition);
#elif CPU(ARM64)
            if ((m_type == ARM64Assembler::JumpCompareAndBranch) || (m_type == ARM64Assembler::JumpCompareAndBranchFixedSize))
                masm->m_assembler.linkJump(m_label, masm->m_assembler.label(), m_type, m_condition, m_is64Bit, m_compareRegister);
            else if ((m_type == ARM64Assembler::JumpTestBit) || (m_type == ARM64Assembler::JumpTestBitFixedSize))
                masm->m_assembler.linkJump(m_label, masm->m_assembler.label(), m_type, m_condition, m_bitNumber, m_compareRegister);
            else
                masm->m_assembler.linkJump(m_label, masm->m_assembler.label(), m_type, m_condition);
#else
            masm->m_assembler.linkJump(m_label, masm->m_assembler.label());
#endif
        }
        
        void linkTo(Label label, AbstractMacroAssemblerType* masm) const
        {
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
            masm->checkRegisterAllocationAgainstBranchRange(label.m_label.offset(), m_label.offset());
#endif

#if CPU(ARM_THUMB2)
            masm->m_assembler.linkJump(m_label, label.m_label, m_type, m_condition);
#elif CPU(ARM64)
            if ((m_type == ARM64Assembler::JumpCompareAndBranch) || (m_type == ARM64Assembler::JumpCompareAndBranchFixedSize))
                masm->m_assembler.linkJump(m_label, label.m_label, m_type, m_condition, m_is64Bit, m_compareRegister);
            else if ((m_type == ARM64Assembler::JumpTestBit) || (m_type == ARM64Assembler::JumpTestBitFixedSize))
                masm->m_assembler.linkJump(m_label, label.m_label, m_type, m_condition, m_bitNumber, m_compareRegister);
            else
                masm->m_assembler.linkJump(m_label, label.m_label, m_type, m_condition);
#else
            masm->m_assembler.linkJump(m_label, label.m_label);
#endif
        }

        bool isSet() const { return m_label.isSet(); }

    private:
        AssemblerLabel m_label;
#if CPU(ARM_THUMB2)
        ARMv7Assembler::JumpType m_type { ARMv7Assembler::JumpNoCondition };
        ARMv7Assembler::Condition m_condition { ARMv7Assembler::ConditionInvalid };
#elif CPU(ARM64)
        ARM64Assembler::JumpType m_type { ARM64Assembler::JumpNoCondition };
        ARM64Assembler::Condition m_condition { ARM64Assembler::ConditionInvalid };
        bool m_is64Bit { false };
        unsigned m_bitNumber { 0 };
        ARM64Assembler::RegisterID m_compareRegister { ARM64Registers::InvalidGPRReg };
#endif
    };

    struct PatchableJump {
        PatchableJump()
        {
        }

        explicit PatchableJump(Jump jump)
            : m_jump(jump)
        {
        }

        operator Jump&() { return m_jump; }

        Jump m_jump;
    };

    // JumpList:
    //
    // A JumpList is a set of Jump objects.
    // All jumps in the set will be linked to the same destination.
    class JumpList {
    public:
        typedef Vector<Jump, 2> JumpVector;
        
        JumpList() { }
        
        JumpList(Jump jump)
        {
            if (jump.isSet())
                append(jump);
        }

        void link(AbstractMacroAssemblerType* masm) const
        {
            size_t size = m_jumps.size();
            for (size_t i = 0; i < size; ++i)
                m_jumps[i].link(masm);
        }
        
        void linkTo(Label label, AbstractMacroAssemblerType* masm) const
        {
            size_t size = m_jumps.size();
            for (size_t i = 0; i < size; ++i)
                m_jumps[i].linkTo(label, masm);
        }
        
        void append(Jump jump)
        {
            if (jump.isSet())
                m_jumps.append(jump);
        }
        
        void append(const JumpList& other)
        {
            m_jumps.append(other.m_jumps.begin(), other.m_jumps.size());
        }

        bool empty() const
        {
            return !m_jumps.size();
        }
        
        void clear()
        {
            m_jumps.clear();
        }
        
        const JumpVector& jumps() const { return m_jumps; }

    private:
        JumpVector m_jumps;
    };


    // Section 3: Misc admin methods
#if ENABLE(DFG_JIT)
    Label labelIgnoringWatchpoints()
    {
        Label result;
        result.m_label = m_assembler.labelIgnoringWatchpoints();
        return result;
    }
#else
    Label labelIgnoringWatchpoints()
    {
        return label();
    }
#endif
    
    Label label()
    {
        return Label(this);
    }
    
    void padBeforePatch()
    {
        // Rely on the fact that asking for a label already does the padding.
        (void)label();
    }
    
    Label watchpointLabel()
    {
        Label result;
        result.m_label = m_assembler.labelForWatchpoint();
        return result;
    }
    
    Label align()
    {
        m_assembler.align(16);
        return Label(this);
    }

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
    class RegisterAllocationOffset {
    public:
        RegisterAllocationOffset(unsigned offset)
            : m_offset(offset)
        {
        }

        void checkOffsets(unsigned low, unsigned high)
        {
            RELEASE_ASSERT_WITH_MESSAGE(!(low <= m_offset && m_offset <= high), "Unsafe branch over register allocation at instruction offset %u in jump offset range %u..%u", m_offset, low, high);
        }

    private:
        unsigned m_offset;
    };

    void addRegisterAllocationAtOffset(unsigned offset)
    {
        m_registerAllocationForOffsets.append(RegisterAllocationOffset(offset));
    }

    void clearRegisterAllocationOffsets()
    {
        m_registerAllocationForOffsets.clear();
    }

    void checkRegisterAllocationAgainstBranchRange(unsigned offset1, unsigned offset2)
    {
        if (offset1 > offset2)
            std::swap(offset1, offset2);

        size_t size = m_registerAllocationForOffsets.size();
        for (size_t i = 0; i < size; ++i)
            m_registerAllocationForOffsets[i].checkOffsets(offset1, offset2);
    }
#endif

    template<typename T, typename U>
    static ptrdiff_t differenceBetween(T from, U to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    template<PtrTag aTag, PtrTag bTag>
    static ptrdiff_t differenceBetweenCodePtr(const MacroAssemblerCodePtr<aTag>& a, const MacroAssemblerCodePtr<bTag>& b)
    {
        return b.template dataLocation<ptrdiff_t>() - a.template dataLocation<ptrdiff_t>();
    }

    unsigned debugOffset() { return m_assembler.debugOffset(); }

    ALWAYS_INLINE static void cacheFlush(void* code, size_t size)
    {
        AssemblerType::cacheFlush(code, size);
    }

    template<PtrTag tag>
    static void linkJump(void* code, Jump jump, CodeLocationLabel<tag> target)
    {
        AssemblerType::linkJump(code, jump.m_label, target.dataLocation());
    }

    static void linkPointer(void* code, AssemblerLabel label, void* value)
    {
        AssemblerType::linkPointer(code, label, value);
    }

    template<PtrTag tag>
    static void linkPointer(void* code, AssemblerLabel label, MacroAssemblerCodePtr<tag> value)
    {
        AssemblerType::linkPointer(code, label, value.executableAddress());
    }

    template<PtrTag tag>
    static void* getLinkerAddress(void* code, AssemblerLabel label)
    {
        return tagCodePtr<tag>(AssemblerType::getRelocatedAddress(code, label));
    }

    static unsigned getLinkerCallReturnOffset(Call call)
    {
        return AssemblerType::getCallReturnOffset(call.m_label);
    }

    template<PtrTag jumpTag, PtrTag destTag>
    static void repatchJump(CodeLocationJump<jumpTag> jump, CodeLocationLabel<destTag> destination)
    {
        AssemblerType::relinkJump(jump.dataLocation(), destination.dataLocation());
    }
    
    template<PtrTag jumpTag>
    static void repatchJumpToNop(CodeLocationJump<jumpTag> jump)
    {
        AssemblerType::relinkJumpToNop(jump.dataLocation());
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchNearCall(CodeLocationNearCall<callTag> nearCall, CodeLocationLabel<destTag> destination)
    {
        switch (nearCall.callMode()) {
        case NearCallMode::Tail:
            AssemblerType::relinkJump(nearCall.dataLocation(), destination.dataLocation());
            return;
        case NearCallMode::Regular:
            AssemblerType::relinkCall(nearCall.dataLocation(), destination.untaggedExecutableAddress());
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<PtrTag callTag, PtrTag destTag>
    static CodeLocationLabel<destTag> prepareForAtomicRepatchNearCallConcurrently(CodeLocationNearCall<callTag> nearCall, CodeLocationLabel<destTag> destination)
    {
#if ENABLE(JUMP_ISLANDS)
        switch (nearCall.callMode()) {
        case NearCallMode::Tail:
            return CodeLocationLabel<destTag>(tagCodePtr<destTag>(AssemblerType::prepareForAtomicRelinkJumpConcurrently(nearCall.dataLocation(), destination.dataLocation())));
        case NearCallMode::Regular:
            return CodeLocationLabel<destTag>(tagCodePtr<destTag>(AssemblerType::prepareForAtomicRelinkCallConcurrently(nearCall.dataLocation(), destination.untaggedExecutableAddress())));
        }
#else
        UNUSED_PARAM(nearCall);
        return destination;
#endif
    }

    template<PtrTag tag>
    static void repatchCompact(CodeLocationDataLabelCompact<tag> dataLabelCompact, int32_t value)
    {
        AssemblerType::repatchCompact(dataLabelCompact.template dataLocation(), value);
    }

    template<PtrTag tag>
    static void repatchInt32(CodeLocationDataLabel32<tag> dataLabel32, int32_t value)
    {
        AssemblerType::repatchInt32(dataLabel32.dataLocation(), value);
    }

    template<PtrTag tag>
    static void repatchPointer(CodeLocationDataLabelPtr<tag> dataLabelPtr, void* value)
    {
        AssemblerType::repatchPointer(dataLabelPtr.dataLocation(), value);
    }

    template<PtrTag tag>
    static void* readPointer(CodeLocationDataLabelPtr<tag> dataLabelPtr)
    {
        return AssemblerType::readPointer(dataLabelPtr.dataLocation());
    }
    
    template<PtrTag tag>
    static void replaceWithLoad(CodeLocationConvertibleLoad<tag> label)
    {
        AssemblerType::replaceWithLoad(label.dataLocation());
    }

    template<PtrTag tag>
    static void replaceWithAddressComputation(CodeLocationConvertibleLoad<tag> label)
    {
        AssemblerType::replaceWithAddressComputation(label.dataLocation());
    }

    template<typename Functor>
    void addLinkTask(const Functor& functor)
    {
        m_linkTasks.append(createSharedTask<void(LinkBuffer&)>(functor));
    }

#if COMPILER(GCC)
    // Workaround for GCC demanding that memcpy "must be the name of a function with external linkage".
    static void* memcpy(void* dst, const void* src, size_t size)
    {
        return std::memcpy(dst, src, size);
    }
#endif

    void emitNops(size_t memoryToFillWithNopsInBytes)
    {
#if CPU(ARM64)
        RELEASE_ASSERT(memoryToFillWithNopsInBytes % 4 == 0);
        for (unsigned i = 0; i < memoryToFillWithNopsInBytes / 4; ++i)
            m_assembler.nop();
#else
        AssemblerBuffer& buffer = m_assembler.buffer();
        size_t startCodeSize = buffer.codeSize();
        size_t targetCodeSize = startCodeSize + memoryToFillWithNopsInBytes;
        buffer.ensureSpace(memoryToFillWithNopsInBytes);
        AssemblerType::template fillNops<memcpy>(static_cast<char*>(buffer.data()) + startCodeSize, memoryToFillWithNopsInBytes);
        buffer.setCodeSize(targetCodeSize);
#endif
    }

    ALWAYS_INLINE void tagReturnAddress() { }
    ALWAYS_INLINE void untagReturnAddress(RegisterID = RegisterID::InvalidGPRReg) { }

    ALWAYS_INLINE void tagPtr(PtrTag, RegisterID) { }
    ALWAYS_INLINE void tagPtr(RegisterID, RegisterID) { }
    ALWAYS_INLINE void untagPtr(PtrTag, RegisterID) { }
    ALWAYS_INLINE void untagPtr(RegisterID, RegisterID) { }
    ALWAYS_INLINE void removePtrTag(RegisterID) { }
    ALWAYS_INLINE void validateUntaggedPtr(RegisterID, RegisterID = RegisterID::InvalidGPRReg) { }

protected:
    AbstractMacroAssembler()
        : m_randomSource(0)
        , m_assembler()
    {
        invalidateAllTempRegisters();
    }

    uint32_t random()
    {
        if (!m_randomSourceIsInitialized) {
            m_randomSourceIsInitialized = true;
            m_randomSource.setSeed(cryptographicallyRandomNumber());
        }
        return m_randomSource.getUint32();
    }

    bool m_randomSourceIsInitialized { false };
    WeakRandom m_randomSource;
public:
    AssemblerType m_assembler;
protected:

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
    Vector<RegisterAllocationOffset, 10> m_registerAllocationForOffsets;
#endif

    static bool haveScratchRegisterForBlinding()
    {
        return false;
    }
    static RegisterID scratchRegisterForBlinding()
    {
        UNREACHABLE_FOR_PLATFORM();
        return firstRegister();
    }
    static bool canBlind() { return false; }
    static bool shouldBlindForSpecificArch(uint32_t) { return false; }
    static bool shouldBlindForSpecificArch(uint64_t) { return false; }

    class CachedTempRegister {
        friend class DataLabelPtr;
        friend class DataLabel32;
        friend class DataLabelCompact;
        friend class Jump;
        friend class Label;

    public:
        CachedTempRegister(AbstractMacroAssemblerType* masm, RegisterID registerID)
            : m_masm(masm)
            , m_registerID(registerID)
            , m_value(0)
            , m_validBit(1 << static_cast<unsigned>(registerID))
        {
            ASSERT(static_cast<unsigned>(registerID) < (sizeof(unsigned) * 8));
        }

        ALWAYS_INLINE RegisterID registerIDInvalidate() { invalidate(); return m_registerID; }

        ALWAYS_INLINE RegisterID registerIDNoInvalidate() { return m_registerID; }

        bool value(intptr_t& value)
        {
            value = m_value;
            return m_masm->isTempRegisterValid(m_validBit);
        }

        void setValue(intptr_t value)
        {
            m_value = value;
            m_masm->setTempRegisterValid(m_validBit);
        }

        ALWAYS_INLINE void invalidate() { m_masm->clearTempRegisterValid(m_validBit); }

    private:
        AbstractMacroAssemblerType* m_masm;
        RegisterID m_registerID;
        intptr_t m_value;
        unsigned m_validBit;
    };

    ALWAYS_INLINE void invalidateAllTempRegisters()
    {
        m_tempRegistersValidBits = 0;
    }

    ALWAYS_INLINE bool isTempRegisterValid(unsigned registerMask)
    {
        return (m_tempRegistersValidBits & registerMask);
    }

    ALWAYS_INLINE void clearTempRegisterValid(unsigned registerMask)
    {
        m_tempRegistersValidBits &=  ~registerMask;
    }

    ALWAYS_INLINE void setTempRegisterValid(unsigned registerMask)
    {
        m_tempRegistersValidBits |= registerMask;
    }

    friend class AllowMacroScratchRegisterUsage;
    friend class AllowMacroScratchRegisterUsageIf;
    template<typename T> friend class DisallowMacroScratchRegisterUsage;
    unsigned m_tempRegistersValidBits;
    bool m_allowScratchRegister { true };

    Vector<RefPtr<SharedTask<void(LinkBuffer&)>>> m_linkTasks;

    friend class LinkBuffer;
}; // class AbstractMacroAssembler

template <class AssemblerType>
inline typename AbstractMacroAssembler<AssemblerType>::BaseIndex
AbstractMacroAssembler<AssemblerType>::Address::indexedBy(
    typename AbstractMacroAssembler<AssemblerType>::RegisterID index,
    typename AbstractMacroAssembler<AssemblerType>::Scale scale) const
{
    return BaseIndex(base, index, scale, offset);
}

#endif // ENABLE(ASSEMBLER)

} // namespace JSC

#if ENABLE(ASSEMBLER)

namespace WTF {

class PrintStream;

void printInternal(PrintStream& out, JSC::AbstractMacroAssemblerBase::StatusCondition);

} // namespace WTF

#endif // ENABLE(ASSEMBLER)

