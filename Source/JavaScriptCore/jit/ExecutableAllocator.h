/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#include "JSExportMacros.h"
#include "ExecutableMemoryHandle.h"
#include "FastJITPermissions.h"
#include "JITCompilationEffort.h"
#include "JSCConfig.h"
#include "JSCPtrTag.h"
#include "CodeLocation.h"
#include "Options.h"
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>

#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/FastBitVector.h>
#include <wtf/FileSystem.h>
#include <wtf/FixedVector.h>
#include <wtf/IterationStatus.h>
#include <wtf/PageReservation.h>
#include <wtf/ProcessID.h>
#include <wtf/RedBlackTree.h>
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>
#include <wtf/WorkQueue.h>

#if ENABLE(LIBPAS_JIT_HEAP)
#include <bmalloc/jit_heap.h>
#include <bmalloc/jit_heap_config.h>
#else
#include <wtf/MetaAllocator.h>
#endif

#if HAVE(IOS_JIT_RESTRICTIONS) || HAVE(MAC_JIT_RESTRICTIONS)
#include <wtf/cocoa/Entitlements.h>
#endif


#if !ENABLE(LIBPAS_JIT_HEAP)
#include <wtf/MetaAllocator.h>
#endif

#if OS(DARWIN)
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#endif

#if CPU(MIPS) && OS(LINUX)
#include <sys/cachectl.h>
#endif

#define EXECUTABLE_POOL_WRITABLE true

namespace JSC {

static constexpr unsigned jitAllocationGranule = 32;

class ExecutableAllocatorBase {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ExecutableAllocatorBase);
public:
    bool isValid() const { return false; }

    static bool underMemoryPressure() { return false; }

    static double memoryPressureMultiplier(size_t) { return 1.0; }

    static void dumpProfile() { }

    RefPtr<ExecutableMemoryHandle> allocate(size_t, JITCompilationEffort) { return nullptr; }

    static void disableJIT() { };
    
    bool isValidExecutableMemory(const AbstractLocker&, void*) { return false; }

    static size_t committedByteCount() { return 0; }

    Lock& getLock() const WTF_RETURNS_LOCK(m_lock)
    {
        return m_lock;
    }

protected:
    ExecutableAllocatorBase() = default;
    ~ExecutableAllocatorBase() = default;

private:
    mutable Lock m_lock;
};

#if ENABLE(JIT)

JS_EXPORT_PRIVATE ALWAYS_INLINE void* startOfFixedExecutableMemoryPoolImpl()
{
    auto* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return nullptr;
    return g_jscConfig.startExecutableMemory;
}

JS_EXPORT_PRIVATE ALWAYS_INLINE void* endOfFixedExecutableMemoryPoolImpl()
{
    auto* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        return nullptr;
    return g_jscConfig.endExecutableMemory;
}

template<typename T = void*>
T startOfFixedExecutableMemoryPool()
{
    return bitwise_cast<T>(startOfFixedExecutableMemoryPoolImpl());
}

template<typename T = void*>
T endOfFixedExecutableMemoryPool()
{
    return bitwise_cast<T>(endOfFixedExecutableMemoryPoolImpl());
}

ALWAYS_INLINE bool isJITPC(void* pc)
{
    return g_jscConfig.startExecutableMemory <= pc && pc < g_jscConfig.endExecutableMemory;
}

JS_EXPORT_PRIVATE void dumpJITMemory(const void*, const void*, size_t);

static ALWAYS_INLINE void* performJITMemcpy(void *dst, const void *src, size_t n)
{
#if CPU(ARM64)
    static constexpr size_t instructionSize = sizeof(unsigned);
    RELEASE_ASSERT(roundUpToMultipleOf<instructionSize>(dst) == dst);
    RELEASE_ASSERT(roundUpToMultipleOf<instructionSize>(src) == src);
#endif
    if (isJITPC(dst)) {
        RELEASE_ASSERT(!Gigacage::contains(src));
        RELEASE_ASSERT(reinterpret_cast<uint8_t*>(dst) + n <= endOfFixedExecutableMemoryPool());

        if (UNLIKELY(Options::dumpJITMemoryPath()))
            dumpJITMemory(dst, src, n);

        if (g_jscConfig.useFastJITPermissions) {
            threadSelfRestrictRWXToRW();
            memcpy(dst, src, n);
            threadSelfRestrictRWXToRX();
            return dst;
        }

#if ENABLE(SEPARATED_WX_HEAP)
        if (g_jscConfig.jitWriteSeparateHeaps) {
            // Use execute-only write thunk for writes inside the JIT region. This is a variant of
            // memcpy that takes an offset into the JIT region as its destination (first) parameter.
            off_t offset = (off_t)((uintptr_t)dst - startOfFixedExecutableMemoryPool<uintptr_t>());
            retagCodePtr<JITThunkPtrTag, CFunctionPtrTag>(g_jscConfig.jitWriteSeparateHeaps)(offset, src, n);
            RELEASE_ASSERT(!Gigacage::contains(src));
            return dst;
        }
#endif
    }

    // Use regular memcpy for writes outside the JIT region.
    return memcpy(dst, src, n);
}

class ExecutableAllocator : private ExecutableAllocatorBase {
public:
    using Base = ExecutableAllocatorBase;

    JS_EXPORT_PRIVATE ALWAYS_INLINE static ExecutableAllocator& singleton()
    {
        ASSERT(g_jscConfig.executableAllocator);
        return *g_jscConfig.executableAllocator;
    }

    static void initialize();
    static void initializeUnderlyingAllocator();

    bool isValid() const;

    static bool underMemoryPressure();
    
    static double memoryPressureMultiplier(size_t addedMemoryUsage);
    
#if ENABLE(META_ALLOCATOR_PROFILE)
    static void dumpProfile();
#else
    static void dumpProfile() { }
#endif
    
    JS_EXPORT_PRIVATE static void disableJIT();

    RefPtr<ExecutableMemoryHandle> allocate(size_t sizeInBytes, JITCompilationEffort);

    bool isValidExecutableMemory(const AbstractLocker&, void* address);

    static size_t committedByteCount();

    Lock& getLock() const;

#if ENABLE(JUMP_ISLANDS)
    JS_EXPORT_PRIVATE ALWAYS_INLINE void* getJumpIslandTo(void* from, void* newDestination);
    JS_EXPORT_PRIVATE inline void* getJumpIslandToConcurrently(void* from, void* newDestination);
#endif

private:
    ExecutableAllocator() = default;
    ~ExecutableAllocator() = default;
};

#else

class ExecutableAllocator : public ExecutableAllocatorBase {
public:
    static ExecutableAllocator& singleton();
    static void initialize();
    static void initializeUnderlyingAllocator() { }

private:
    ExecutableAllocator() = default;
    ~ExecutableAllocator() = default;
};

static inline void* performJITMemcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

inline bool isJITPC(void*) { return false; }
#endif // ENABLE(JIT)

}

#include "JSExportMacros.h"
#include "ARM64Assembler.h"
#include "AbortReason.h"
#include "AssemblerBuffer.h"
#include "AssemblerCommon.h"
#include "AssemblyComments.h"
#include "CPU.h"
#include "CodeLocation.h"
#include "JSCJSValue.h"
#include "JSCPtrTag.h"
#include "MacroAssemblerCodeRef.h"
#include "MacroAssemblerHelpers.h"
#include "Options.h"
#include <wtf/Noncopyable.h>
#include <wtf/SharedTask.h>
#include <wtf/StringPrintStream.h>
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

#define JIT_COMMENT(jit, ...) do { if (UNLIKELY(Options::needDisassemblySupport())) { (jit).comment(__VA_ARGS__); } else { (void) jit; } } while (0)

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

protected:
    uint32_t random()
    {
        if (!m_randomSource)
            initializeRandom();
        return m_randomSource->getUint32();
    }

private:
    JS_EXPORT_PRIVATE void initializeRandom();

    std::optional<WeakRandom> m_randomSource;
};

template <class AssemblerType>
class AbstractMacroAssembler : public AbstractMacroAssemblerBase {
public:
    typedef AbstractMacroAssembler<AssemblerType> AbstractMacroAssemblerType;
    typedef AssemblerType AssemblerType_T;

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
        ScaleRegWord = isRegister64Bit() ? TimesEight : TimesFour,
    };

    enum class Extend : uint8_t {
        ZExt32,
        SExt32,
        None
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

    // BaseIndex:
    //
    // Describes a complex addressing mode.
    struct BaseIndex {
        BaseIndex(RegisterID base, RegisterID index, Scale scale, int32_t offset = 0, Extend extend = Extend::None)
            : base(base)
            , index(index)
            , scale(scale)
            , offset(offset)
            , extend(extend)
        {
#if !CPU(ARM64)
            ASSERT(extend == Extend::None);
#endif
        }
        
        RegisterID base;
        RegisterID index;
        Scale scale;
        int32_t offset;
        Extend extend;
        
        BaseIndex withOffset(int32_t additionalOffset)
        {
            return BaseIndex(base, index, scale, offset + additionalOffset);
        }

        BaseIndex withSwappedRegister(RegisterID left, RegisterID right)
        {
            return BaseIndex(AbstractMacroAssembler::withSwappedRegister(base, left, right), AbstractMacroAssembler::withSwappedRegister(index, left, right), scale, offset);
        }
    };

    // PreIndexAddress:
    //
    // Describes an address with base address and pre-increment/decrement index.
    struct PreIndexAddress {
        PreIndexAddress(RegisterID base, int index)
            : base(base)
            , index(index)
        {
        }

        RegisterID base;
        int index;
    };

    // PostIndexAddress:
    //
    // Describes an address with base address and post-increment/decrement index.
    struct PostIndexAddress {
        PostIndexAddress(RegisterID base, int index)
            : base(base)
            , index(index)
        {
        }

        RegisterID base;
        int index;
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
        constexpr TrustedImmPtr() { }
        
        explicit constexpr TrustedImmPtr(const void* value)
            : m_value(value)
        {
        }

        template<typename ReturnType, typename... Arguments>
        explicit TrustedImmPtr(ReturnType(*value)(Arguments...))
            : m_value(reinterpret_cast<void*>(value))
        {
        }

        explicit constexpr TrustedImmPtr(std::nullptr_t)
        {
        }

        explicit constexpr TrustedImmPtr(size_t value)
            : m_value(reinterpret_cast<void*>(value))
        {
        }

        constexpr intptr_t asIntptr()
        {
            return reinterpret_cast<intptr_t>(m_value);
        }

        constexpr void* asPtr()
        {
            return const_cast<void*>(m_value);
        }

        const void* m_value { nullptr };
    };

    struct ImmPtr : private TrustedImmPtr
    {
        explicit constexpr ImmPtr(const void* value)
            : TrustedImmPtr(value)
        {
        }

        constexpr TrustedImmPtr asTrustedImmPtr() { return *this; }
    };

    // TrustedImm32:
    //
    // A 32bit immediate operand to an instruction - this is wrapped in a
    // class requiring explicit construction in order to prevent RegisterIDs
    // (which are implemented as an enum) from accidentally being passed as
    // immediate values.
    struct TrustedImm32 : public TrustedImm {
        constexpr TrustedImm32() = default;
        
        explicit constexpr TrustedImm32(int32_t value)
            : m_value(value)
        {
        }

#if !CPU(X86_64)
        explicit constexpr TrustedImm32(TrustedImmPtr ptr)
            : m_value(ptr.asIntptr())
        {
        }
#endif

        int32_t m_value { 0 };
    };


    struct Imm32 : private TrustedImm32 {
        explicit constexpr Imm32(int32_t value)
            : TrustedImm32(value)
        {
        }
#if !CPU(X86_64)
        explicit constexpr Imm32(TrustedImmPtr ptr)
            : TrustedImm32(ptr)
        {
        }
#endif
        constexpr const TrustedImm32& asTrustedImm32() const { return *this; }

    };
    
    // TrustedImm64:
    //
    // A 64bit immediate operand to an instruction - this is wrapped in a
    // class requiring explicit construction in order to prevent RegisterIDs
    // (which are implemented as an enum) from accidentally being passed as
    // immediate values.
    struct TrustedImm64 : TrustedImm {
        constexpr TrustedImm64() { }
        
        explicit constexpr TrustedImm64(int64_t value)
            : m_value(value)
        {
        }

#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
        explicit constexpr TrustedImm64(TrustedImmPtr ptr)
            : m_value(ptr.asIntptr())
        {
        }
#endif

        int64_t m_value;
    };

    struct Imm64 : private TrustedImm64
    {
        explicit constexpr Imm64(int64_t value)
            : TrustedImm64(value)
        {
        }
#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
        explicit constexpr Imm64(TrustedImmPtr ptr)
            : TrustedImm64(ptr)
        {
        }
#endif
        constexpr const TrustedImm64& asTrustedImm64() const { return *this; }
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
        Label() = default;

        Label(AbstractMacroAssemblerType* masm)
            : m_label(masm->m_assembler.label())
        {
            masm->invalidateAllTempRegisters();
        }

        friend bool operator==(const Label&, const Label&) = default;

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
            LinkableNear = Linkable | Near,
            LinkableNearTail = Linkable | Near | Tail,
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
            , m_bitNumber(bitNumber)
            , m_type(type)
            , m_condition(condition)
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
        unsigned m_bitNumber { 0 };
        ARM64Assembler::JumpType m_type { ARM64Assembler::JumpNoCondition };
        ARM64Assembler::Condition m_condition { ARM64Assembler::ConditionInvalid };
        bool m_is64Bit { false };
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
    static ptrdiff_t differenceBetweenCodePtr(const CodePtr<aTag>& a, const CodePtr<bTag>& b)
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
    static void linkPointer(void* code, AssemblerLabel label, CodePtr<tag> value)
    {
        AssemblerType::linkPointer(code, label, value.taggedPtr());
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
    
    template<PtrTag callTag, PtrTag destTag>
    static void repatchNearCall(CodeLocationNearCall<callTag> nearCall, CodeLocationLabel<destTag> destination)
    {
        switch (nearCall.callMode()) {
        case NearCallMode::Tail:
            AssemblerType::relinkTailCall(nearCall.dataLocation(), destination.dataLocation());
            return;
        case NearCallMode::Regular:
            AssemblerType::relinkCall(nearCall.dataLocation(), destination.untaggedPtr());
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
            return CodeLocationLabel<destTag>(tagCodePtr<destTag>(AssemblerType::prepareForAtomicRelinkCallConcurrently(nearCall.dataLocation(), destination.untaggedPtr())));
        }
        RELEASE_ASSERT_NOT_REACHED();
#else
        UNUSED_PARAM(nearCall);
        return destination;
#endif
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

    template<typename Functor>
    void addLinkTask(const Functor& functor)
    {
        m_linkTasks.append(createSharedTask<void(LinkBuffer&)>(functor));
    }

    template<typename Functor>
    void addLateLinkTask(const Functor& functor) // Run after all link tasks
    {
        m_lateLinkTasks.append(createSharedTask<void(LinkBuffer&)>(functor));
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

    template<typename... Types>
    void comment(const Types&... values)
    {
        if (LIKELY(!Options::needDisassemblySupport()))
            return;
        StringPrintStream s;
        s.print(values...);
        commentImpl(s.toString());
    }

protected:
    AbstractMacroAssembler()
        : m_assembler()
    {
        invalidateAllTempRegisters();
    }

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
    static constexpr bool canBlind() { return false; }
    static constexpr bool shouldBlindForSpecificArch(uint32_t) { return false; }
    static constexpr bool shouldBlindForSpecificArch(uint64_t) { return false; }

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

        WARN_UNUSED_RETURN bool value(intptr_t& value)
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

    void commentImpl(String&& str) { m_comments.append({ labelIgnoringWatchpoints(), WTFMove(str) }); }

    friend class AllowMacroScratchRegisterUsage;
    friend class AllowMacroScratchRegisterUsageIf;
    template<typename T> friend class DisallowMacroScratchRegisterUsage;
    unsigned m_tempRegistersValidBits;
    bool m_allowScratchRegister { true };

    Vector<std::pair<Label, String>> m_comments;

    Vector<RefPtr<SharedTask<void(LinkBuffer&)>>> m_linkTasks;
    Vector<RefPtr<SharedTask<void(LinkBuffer&)>>> m_lateLinkTasks;

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

#if ENABLE(ASSEMBLER)

#include "JSCJSValue.h"

#define DEFINE_SIMD_FUNC(name, func, lane) \
    template <typename ...Args> \
    void name(Args&&... args) { func(lane, std::forward<Args>(args)...); }

#define DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name, func, lane, mode) \
    template <typename ...Args> \
    void name(Args&&... args) { func(lane, mode, std::forward<Args>(args)...); }

#define DEFINE_SIMD_FUNCS(name) \
    DEFINE_SIMD_FUNC(name##Int8, name, SIMDLane::i8x16) \
    DEFINE_SIMD_FUNC(name##Int16, name, SIMDLane::i16x8) \
    DEFINE_SIMD_FUNC(name##Int32, name, SIMDLane::i32x4) \
    DEFINE_SIMD_FUNC(name##Int64, name, SIMDLane::i64x2) \
    DEFINE_SIMD_FUNC(name##Float32, name, SIMDLane::f32x4) \
    DEFINE_SIMD_FUNC(name##Float64, name, SIMDLane::f64x2)

#define DEFINE_SIGNED_SIMD_FUNCS(name) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##SignedInt8, name, SIMDLane::i8x16, SIMDSignMode::Signed) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##UnsignedInt8, name, SIMDLane::i8x16, SIMDSignMode::Unsigned) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##SignedInt16, name, SIMDLane::i16x8, SIMDSignMode::Signed) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##UnsignedInt16, name, SIMDLane::i16x8, SIMDSignMode::Unsigned) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##Int32, name, SIMDLane::i32x4, SIMDSignMode::None) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##Int64, name, SIMDLane::i64x2, SIMDSignMode::None) \
    DEFINE_SIMD_FUNC(name##Float32, name, SIMDLane::f32x4) \
    DEFINE_SIMD_FUNC(name##Float64, name, SIMDLane::f64x2)

#if CPU(ARM_THUMB2)
#define TARGET_ASSEMBLER ARMv7Assembler
#define TARGET_MACROASSEMBLER MacroAssemblerARMv7
#include "MacroAssemblerARMv7.h"
namespace JSC { typedef MacroAssemblerARMv7 MacroAssemblerBase; };

#elif CPU(ARM64E)
#define TARGET_ASSEMBLER ARM64EAssembler
#define TARGET_MACROASSEMBLER MacroAssemblerARM64E
#include "MacroAssemblerARM64E.h"

#elif CPU(ARM64)
#define TARGET_ASSEMBLER ARM64Assembler
#define TARGET_MACROASSEMBLER MacroAssemblerARM64
#include "MacroAssemblerARM64.h"

#elif CPU(MIPS)
#define TARGET_ASSEMBLER MIPSAssembler
#define TARGET_MACROASSEMBLER MacroAssemblerMIPS
#include "MacroAssemblerMIPS.h"

#elif CPU(X86_64)
#define TARGET_ASSEMBLER X86Assembler
#define TARGET_MACROASSEMBLER MacroAssemblerX86_64
#include "MacroAssemblerX86_64.h"

#elif CPU(RISCV64)
#define TARGET_ASSEMBLER RISCV64Assembler
#define TARGET_MACROASSEMBLER MacroAssemblerRISCV64
#include "MacroAssemblerRISCV64.h"

#else
#error "The MacroAssembler is not supported on this platform."
#endif

#include "MacroAssemblerHelpers.h"

namespace WTF {

template<typename FunctionType>
class ScopedLambda;

} // namespace WTF

namespace JSC {

namespace Probe {

enum class SavedFPWidth {
    SaveVectors,
    DontSaveVectors
};

class Context;
typedef void (*Function)(Context&);

} // namespace Probe

using Probe::SavedFPWidth;

namespace Printer {

struct PrintRecord;
typedef Vector<PrintRecord> PrintRecordList;

} // namespace Printer

using MacroAssemblerBase = TARGET_MACROASSEMBLER;

class MacroAssembler : public MacroAssemblerBase {
public:
    using Base = MacroAssemblerBase;

    static constexpr RegisterID nextRegister(RegisterID reg)
    {
        return static_cast<RegisterID>(reg + 1);
    }
    
    static constexpr FPRegisterID nextFPRegister(FPRegisterID reg)
    {
        return static_cast<FPRegisterID>(reg + 1);
    }
    
    static constexpr unsigned registerIndex(RegisterID reg)
    {
        return reg - firstRegister();
    }
    
    static constexpr unsigned fpRegisterIndex(FPRegisterID reg)
    {
        return reg - firstFPRegister();
    }
    
    static constexpr unsigned registerIndex(FPRegisterID reg)
    {
        return fpRegisterIndex(reg) + numberOfRegisters();
    }
    
    static constexpr unsigned totalNumberOfRegisters()
    {
        return numberOfRegisters() + numberOfFPRegisters();
    }

    using MacroAssemblerBase::pop;
    using MacroAssemblerBase::jump;
    using MacroAssemblerBase::farJump;
    using MacroAssemblerBase::branch32;
    using MacroAssemblerBase::compare32;
    using MacroAssemblerBase::move;
    using MacroAssemblerBase::moveDouble;
    using MacroAssemblerBase::add32;
    using MacroAssemblerBase::mul32;
    using MacroAssemblerBase::and32;
    using MacroAssemblerBase::branchAdd32;
    using MacroAssemblerBase::branchMul32;
#if CPU(ARM64) || CPU(ARM_THUMB2) || CPU(X86_64) || CPU(MIPS) || CPU(RISCV64)
    using MacroAssemblerBase::branchPtr;
#endif
#if CPU(X86_64)
    using MacroAssemblerBase::branch64;
#endif
    using MacroAssemblerBase::branchSub32;
    using MacroAssemblerBase::lshift32;
    using MacroAssemblerBase::or32;
    using MacroAssemblerBase::rshift32;
    using MacroAssemblerBase::store32;
    using MacroAssemblerBase::sub32;
    using MacroAssemblerBase::urshift32;
    using MacroAssemblerBase::xor32;

#if CPU(ARM64) || CPU(X86_64) || CPU(RISCV64)
    using MacroAssemblerBase::and64;
    using MacroAssemblerBase::or64;
    using MacroAssemblerBase::xor64;
    using MacroAssemblerBase::convertInt32ToDouble;
    using MacroAssemblerBase::store64;
#endif

    static bool isPtrAlignedAddressOffset(ptrdiff_t value)
    {
        return value == static_cast<int32_t>(value);
    }

    static const double twoToThe32; // This is super useful for some double code.

    // Utilities used by the DFG JIT.
    using AbstractMacroAssemblerBase::invert;
    using MacroAssemblerBase::invert;
    
    static DoubleCondition invert(DoubleCondition cond)
    {
        switch (cond) {
        case DoubleEqualAndOrdered:
            return DoubleNotEqualOrUnordered;
        case DoubleNotEqualAndOrdered:
            return DoubleEqualOrUnordered;
        case DoubleGreaterThanAndOrdered:
            return DoubleLessThanOrEqualOrUnordered;
        case DoubleGreaterThanOrEqualAndOrdered:
            return DoubleLessThanOrUnordered;
        case DoubleLessThanAndOrdered:
            return DoubleGreaterThanOrEqualOrUnordered;
        case DoubleLessThanOrEqualAndOrdered:
            return DoubleGreaterThanOrUnordered;
        case DoubleEqualOrUnordered:
            return DoubleNotEqualAndOrdered;
        case DoubleNotEqualOrUnordered:
            return DoubleEqualAndOrdered;
        case DoubleGreaterThanOrUnordered:
            return DoubleLessThanOrEqualAndOrdered;
        case DoubleGreaterThanOrEqualOrUnordered:
            return DoubleLessThanAndOrdered;
        case DoubleLessThanOrUnordered:
            return DoubleGreaterThanOrEqualAndOrdered;
        case DoubleLessThanOrEqualOrUnordered:
            return DoubleGreaterThanAndOrdered;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return DoubleEqualAndOrdered; // make compiler happy
    }
    
    static bool isInvertible(ResultCondition cond)
    {
        switch (cond) {
        case Zero:
        case NonZero:
        case Signed:
        case PositiveOrZero:
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
        case Signed:
            return PositiveOrZero;
        case PositiveOrZero:
            return Signed;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return Zero; // Make compiler happy for release builds.
        }
    }

    static RelationalCondition flip(RelationalCondition cond)
    {
        switch (cond) {
        case Equal:
        case NotEqual:
            return cond;
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
        }

        RELEASE_ASSERT_NOT_REACHED();
        return Equal;
    }

    static bool isSigned(RelationalCondition cond)
    {
        return MacroAssemblerHelpers::isSigned<MacroAssembler>(cond);
    }

    static bool isUnsigned(RelationalCondition cond)
    {
        return MacroAssemblerHelpers::isUnsigned<MacroAssembler>(cond);
    }

    static bool isSigned(ResultCondition cond)
    {
        return MacroAssemblerHelpers::isSigned<MacroAssembler>(cond);
    }

    static bool isUnsigned(ResultCondition cond)
    {
        return MacroAssemblerHelpers::isUnsigned<MacroAssembler>(cond);
    }

    // Platform agnostic convenience functions,
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

    void poke(FPRegisterID src, int index = 0)
    {
        storeDouble(src, addressForPoke(index));
    }

#if !CPU(ARM64)
    void pushToSave(RegisterID src)
    {
        push(src);
    }
    void pushToSaveImmediateWithoutTouchingRegisters(TrustedImm32 imm)
    {
        push(imm);
    }
    void popToRestore(RegisterID dest)
    {
        pop(dest);
    }
    void pushToSave(FPRegisterID src)
    {
        subPtr(TrustedImm32(sizeof(double)), stackPointerRegister);
        storeDouble(src, Address(stackPointerRegister));
    }
    void popToRestore(FPRegisterID dest)
    {
        loadDouble(Address(stackPointerRegister), dest);
        addPtr(TrustedImm32(sizeof(double)), stackPointerRegister);
    }
    
    static constexpr ptrdiff_t pushToSaveByteOffset() { return sizeof(void*); }
#endif // !CPU(ARM64)

#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
    void peek64(RegisterID dest, int index = 0)
    {
        load64(Address(stackPointerRegister, (index * sizeof(void*))), dest);
    }

    void poke(TrustedImm64 value, int index = 0)
    {
        store64(value, addressForPoke(index));
    }

    void poke64(RegisterID src, int index = 0)
    {
        store64(src, addressForPoke(index));
    }
#endif

    // Immediate shifts only have 5 controllable bits
    // so we'll consider them safe for now.
    TrustedImm32 trustedImm32ForShift(Imm32 imm)
    {
        return TrustedImm32(imm.asTrustedImm32().m_value & 31);
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

    Jump branch32(RelationalCondition cond, RegisterID left, AbsoluteAddress right)
    {
        return branch32(flip(cond), right, left);
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

    void compare32(RelationalCondition cond, Imm32 left, RegisterID right, RegisterID dest)
    {
        compare32(commute(cond), right, left, dest);
    }

    void branchTestPtr(ResultCondition cond, RegisterID reg, Label target)
    {
        branchTestPtr(cond, reg).linkTo(target, this);
    }

#if !CPU(ARM_THUMB2) && !CPU(ARM64)
    PatchableJump patchableBranchPtr(RelationalCondition cond, Address left, TrustedImmPtr right = TrustedImmPtr(nullptr))
    {
        padBeforePatch();
        return PatchableJump(branchPtr(cond, left, right));
    }
    
    PatchableJump patchableBranchPtrWithPatch(RelationalCondition cond, Address left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        padBeforePatch();
        return PatchableJump(branchPtrWithPatch(cond, left, dataLabel, initialRightValue));
    }

    PatchableJump patchableBranch32WithPatch(RelationalCondition cond, Address left, DataLabel32& dataLabel, TrustedImm32 initialRightValue = TrustedImm32(0))
    {
        padBeforePatch();
        return PatchableJump(branch32WithPatch(cond, left, dataLabel, initialRightValue));
    }

    PatchableJump patchableJump()
    {
        padBeforePatch();
        return PatchableJump(jump());
    }

    PatchableJump patchableBranchTest32(ResultCondition cond, RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        padBeforePatch();
        return PatchableJump(branchTest32(cond, reg, mask));
    }

    PatchableJump patchableBranch32(RelationalCondition cond, RegisterID reg, TrustedImm32 imm)
    {
        padBeforePatch();
        return PatchableJump(branch32(cond, reg, imm));
    }

    PatchableJump patchableBranch8(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        padBeforePatch();
        return PatchableJump(branch8(cond, address, imm));
    }

    PatchableJump patchableBranch32(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        padBeforePatch();
        return PatchableJump(branch32(cond, address, imm));
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

    void oops()
    {
        abortWithReason(B3Oops);
    }

    // B3 has additional pseudo-opcodes for returning, when it wants to signal that the return
    // consumes some register in some way.
    void retVoid() { ret(); }
    void ret32(RegisterID) { ret(); }
    void ret64(RegisterID) { ret(); }
    void retFloat(FPRegisterID) { ret(); }
    void retDouble(FPRegisterID) { ret(); }

    static constexpr unsigned BlindingModulus = 64;
    bool shouldConsiderBlinding()
    {
        return !(random() & (BlindingModulus - 1));
    }

    void move(Address src, Address dest, RegisterID scratch)
    {
        loadPtr(src, scratch);
        storePtr(scratch, dest);
    }
    
    void move32(Address src, Address dest, RegisterID scratch)
    {
        load32(src, scratch);
        store32(scratch, dest);
    }
    
    void moveFloat(Address src, Address dest, FPRegisterID scratch)
    {
        loadFloat(src, scratch);
        storeFloat(scratch, dest);
    }

    void moveDouble(Address src, Address dest, FPRegisterID scratch)
    {
        loadDouble(src, scratch);
        storeDouble(scratch, dest);
    }

    // Ptr methods
    // On 32-bit platforms (i.e. x86), these methods directly map onto their 32-bit equivalents.
#if !CPU(ADDRESS64)
    void addPtr(Address src, RegisterID dest)
    {
        add32(src, dest);
    }

    void addPtr(AbsoluteAddress src, RegisterID dest)
    {
        add32(src, dest);
    }

    void addPtr(RegisterID src, RegisterID dest)
    {
        add32(src, dest);
    }

    void addPtr(RegisterID left, RegisterID right, RegisterID dest)
    {
        add32(left, right, dest);
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

    void andPtr(TrustedImmPtr imm, RegisterID srcDest)
    {
        and32(TrustedImm32(imm), srcDest);
    }

    void lshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        lshift32(trustedImm32ForShift(imm), srcDest);
    }
    
    void lshiftPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        lshift32(imm, srcDest);
    }
    
    void rshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        rshift32(trustedImm32ForShift(imm), srcDest);
    }

    void rshiftPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        rshift32(imm, srcDest);
    }

    void urshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        urshift32(trustedImm32ForShift(imm), srcDest);
    }

    void urshiftPtr(RegisterID shiftAmmount, RegisterID srcDest)
    {
        urshift32(shiftAmmount, srcDest);
    }

    void negPtr(RegisterID dest)
    {
        neg32(dest);
    }

    void negPtr(RegisterID src, RegisterID dest)
    {
        neg32(src, dest);
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

    void rotateRightPtr(TrustedImm32 imm, RegisterID srcDst)
    {
        rotateRight32(imm, srcDst);
    }

    void subPtr(RegisterID src, RegisterID dest)
    {
        sub32(src, dest);
    }

    void subPtr(RegisterID left, RegisterID right, RegisterID dest)
    {
        sub32(left, right, dest);
    }

    void subPtr(TrustedImm32 imm, RegisterID dest)
    {
        sub32(imm, dest);
    }

    void subPtr(RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        sub32(left, right, dest);
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

    void xorPtr(TrustedImmPtr imm, RegisterID srcDest)
    {
        xor32(TrustedImm32(imm), srcDest);
    }

    void xorPtr(Address src, RegisterID dest)
    {
        xor32(src, dest);
    }

    void loadPtr(Address address, RegisterID dest)
    {
        load32(address, dest);
    }

    void loadPtr(BaseIndex address, RegisterID dest)
    {
#if CPU(NEEDS_ALIGNED_ACCESS)
        ASSERT(address.scale == ScalePtr || address.scale == TimesOne);
#endif
        load32(address, dest);
    }

    void loadPtr(const void* address, RegisterID dest)
    {
        load32(address, dest);
    }

    void loadPairPtr(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src, dest1, dest2);
    }

    void loadPairPtr(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src, offset, dest1, dest2);
    }

    void loadPairPtr(Address src, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src, dest1, dest2);
    }

#if ENABLE(FAST_TLS_JIT)
    void loadFromTLSPtr(uint32_t offset, RegisterID dst)
    {
        loadFromTLS32(offset, dst);
    }

    void storeToTLSPtr(RegisterID src, uint32_t offset)
    {
        storeToTLS32(src, offset);
    }
#endif

    void comparePtr(RelationalCondition cond, RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        compare32(cond, left, right, dest);
    }

    void comparePtr(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        compare32(cond, left, right, dest);
    }
    
    void storePtr(RegisterID src, Address address)
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

    void storePtr(TrustedImmPtr imm, Address address)
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

    void storePtr(TrustedImm32 imm, Address address)
    {
        store32(imm, address);
    }

    void storePtr(TrustedImmPtr imm, BaseIndex address)
    {
        store32(TrustedImm32(imm), address);
    }

    void storePairPtr(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        storePair32(src1, src2, dest);
    }

    void storePairPtr(RegisterID src1, RegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        storePair32(src1, src2, dest, offset);
    }

    void storePairPtr(RegisterID src1, RegisterID src2, Address dest)
    {
        storePair32(src1, src2, dest);
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

    Jump branchSubPtr(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchSub32(cond, src, dest);
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

    using MacroAssemblerBase::branchTest16;
    Jump branchTest16(ResultCondition cond, ExtendedAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        return MacroAssemblerBase::branchTest16(cond, Address(address.base, address.offset), mask);
    }

#else // !CPU(ADDRESS64)

    void addPtr(RegisterID src, RegisterID dest)
    {
        add64(src, dest);
    }

    void addPtr(RegisterID left, RegisterID right, RegisterID dest)
    {
        add64(left, right, dest);
    }
    
    void addPtr(Address src, RegisterID dest)
    {
        add64(src, dest);
    }

    void addPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        add64(imm, srcDest);
    }

    void addPtr(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        add64(imm, src, dest);
    }

    void addPtr(TrustedImm32 imm, Address address)
    {
        add64(imm, address);
    }

    void addPtr(AbsoluteAddress src, RegisterID dest)
    {
        add64(src, dest);
    }

    void addPtr(TrustedImmPtr imm, RegisterID dest)
    {
        add64(TrustedImm64(imm), dest);
    }

    void addPtr(TrustedImm32 imm, AbsoluteAddress address)
    {
        add64(imm, address);
    }

    void andPtr(RegisterID src, RegisterID dest)
    {
        and64(src, dest);
    }

    void andPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        and64(imm, srcDest);
    }
    
    void andPtr(TrustedImmPtr imm, RegisterID srcDest)
    {
        and64(imm, srcDest);
    }
    
    void lshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        lshift64(trustedImm32ForShift(imm), srcDest);
    }

    void lshiftPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        lshift64(imm, srcDest);
    }

    void rshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        rshift64(trustedImm32ForShift(imm), srcDest);
    }

    void rshiftPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        rshift64(imm, srcDest);
    }

    void urshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        urshift64(trustedImm32ForShift(imm), srcDest);
    }

    void urshiftPtr(RegisterID shiftAmmount, RegisterID srcDest)
    {
        urshift64(shiftAmmount, srcDest);
    }

    void negPtr(RegisterID dest)
    {
        neg64(dest);
    }

    void negPtr(RegisterID src, RegisterID dest)
    {
        neg64(src, dest);
    }

    void orPtr(RegisterID src, RegisterID dest)
    {
        or64(src, dest);
    }

    void orPtr(TrustedImm32 imm, RegisterID dest)
    {
        or64(imm, dest);
    }

    void orPtr(TrustedImmPtr imm, RegisterID dest)
    {
        or64(TrustedImm64(imm), dest);
    }

    void orPtr(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        or64(op1, op2, dest);
    }

    void orPtr(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        or64(imm, src, dest);
    }
    
    void rotateRightPtr(TrustedImm32 imm, RegisterID srcDst)
    {
        rotateRight64(imm, srcDst);
    }

    void subPtr(RegisterID src, RegisterID dest)
    {
        sub64(src, dest);
    }

    void subPtr(RegisterID left, RegisterID right, RegisterID dest)
    {
        sub64(left, right, dest);
    }

    void subPtr(TrustedImm32 imm, RegisterID dest)
    {
        sub64(imm, dest);
    }

    void subPtr(RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        sub64(left, right, dest);
    }

    void subPtr(TrustedImmPtr imm, RegisterID dest)
    {
        sub64(TrustedImm64(imm), dest);
    }

    void xorPtr(RegisterID src, RegisterID dest)
    {
        xor64(src, dest);
    }
    
    void xorPtr(Address src, RegisterID dest)
    {
        xor64(src, dest);
    }
    
    void xorPtr(RegisterID src, Address dest)
    {
        xor64(src, dest);
    }

    void xorPtr(TrustedImm32 imm, RegisterID srcDest)
    {
        xor64(imm, srcDest);
    }

    // FIXME: Look into making the need for a scratch register explicit, or providing the option to specify a scratch register.
    void xorPtr(TrustedImmPtr imm, RegisterID srcDest)
    {
        xor64(TrustedImm64(imm), srcDest);
    }

    void loadPtr(Address address, RegisterID dest)
    {
        load64(address, dest);
    }

    void loadPtr(BaseIndex address, RegisterID dest)
    {
#if CPU(NEEDS_ALIGNED_ACCESS)
        ASSERT(address.scale == ScalePtr || address.scale == TimesOne);
#endif
        load64(address, dest);
    }

    void loadPtr(const void* address, RegisterID dest)
    {
        load64(address, dest);
    }

    void loadPairPtr(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair64(src, dest1, dest2);
    }

    void loadPairPtr(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        loadPair64(src, offset, dest1, dest2);
    }

    void loadPairPtr(Address src, RegisterID dest1, RegisterID dest2)
    {
        loadPair64(src, dest1, dest2);
    }

#if ENABLE(FAST_TLS_JIT)
    void loadFromTLSPtr(uint32_t offset, RegisterID dst)
    {
        loadFromTLS64(offset, dst);
    }
    void storeToTLSPtr(RegisterID src, uint32_t offset)
    {
        storeToTLS64(src, offset);
    }
#endif

    void storePtr(RegisterID src, Address address)
    {
        store64(src, address);
    }

    void storePtr(RegisterID src, BaseIndex address)
    {
        store64(src, address);
    }
    
    void storePtr(RegisterID src, void* address)
    {
        store64(src, address);
    }

    void storePtr(TrustedImmPtr imm, Address address)
    {
        store64(TrustedImm64(imm), address);
    }

    void storePtr(TrustedImm32 imm, Address address)
    {
        store64(imm, address);
    }

    void storePtr(TrustedImmPtr imm, BaseIndex address)
    {
        store64(TrustedImm64(imm), address);
    }

    void storePairPtr(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        storePair64(src1, src2, dest);
    }

    void storePairPtr(RegisterID src1, RegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        storePair64(src1, src2, dest, offset);
    }

    void storePairPtr(RegisterID src1, RegisterID src2, Address dest)
    {
        storePair64(src1, src2, dest);
    }

    void comparePtr(RelationalCondition cond, RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        compare64(cond, left, right, dest);
    }
    
    void comparePtr(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        compare64(cond, left, right, dest);
    }
    
    void testPtr(ResultCondition cond, RegisterID reg, TrustedImm32 mask, RegisterID dest)
    {
        test64(cond, reg, mask, dest);
    }

    void testPtr(ResultCondition cond, RegisterID reg, RegisterID mask, RegisterID dest)
    {
        test64(cond, reg, mask, dest);
    }

    Jump branchPtr(RelationalCondition cond, RegisterID left, RegisterID right)
    {
        return branch64(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, RegisterID left, TrustedImmPtr right)
    {
        return branch64(cond, left, TrustedImm64(right));
    }
    
    Jump branchPtr(RelationalCondition cond, RegisterID left, Address right)
    {
        return branch64(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, Address left, RegisterID right)
    {
        return branch64(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, AbsoluteAddress left, RegisterID right)
    {
        return branch64(cond, left, right);
    }

    Jump branchPtr(RelationalCondition cond, Address left, TrustedImmPtr right)
    {
        return branch64(cond, left, TrustedImm64(right));
    }

    Jump branchTestPtr(ResultCondition cond, RegisterID reg, RegisterID mask)
    {
        return branchTest64(cond, reg, mask);
    }
    
    Jump branchTestPtr(ResultCondition cond, RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        return branchTest64(cond, reg, mask);
    }

    Jump branchTestPtr(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        return branchTest64(cond, address, mask);
    }

    Jump branchTestPtr(ResultCondition cond, Address address, RegisterID reg)
    {
        return branchTest64(cond, address, reg);
    }

    Jump branchTestPtr(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        return branchTest64(cond, address, mask);
    }

    Jump branchTestPtr(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        return branchTest64(cond, address, mask);
    }

    Jump branchAddPtr(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchAdd64(cond, imm, dest);
    }

    Jump branchAddPtr(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchAdd64(cond, src, dest);
    }

    Jump branchSubPtr(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchSub64(cond, imm, dest);
    }

    Jump branchSubPtr(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchSub64(cond, src, dest);
    }

    Jump branchSubPtr(ResultCondition cond, RegisterID src1, TrustedImm32 src2, RegisterID dest)
    {
        return branchSub64(cond, src1, src2, dest);
    }

    Jump branchPtr(RelationalCondition cond, RegisterID left, ImmPtr right)
    {
        if (shouldBlind(right) && haveScratchRegisterForBlinding()) {
            RegisterID scratchRegister = scratchRegisterForBlinding();
            loadRotationBlindedConstant(rotationBlindConstant(right), scratchRegister);
            return branchPtr(cond, left, scratchRegister);
        }
        return branchPtr(cond, left, right.asTrustedImmPtr());
    }

    void storePtr(ImmPtr imm, Address dest)
    {
        if (shouldBlind(imm) && haveScratchRegisterForBlinding()) {
            RegisterID scratchRegister = scratchRegisterForBlinding();
            loadRotationBlindedConstant(rotationBlindConstant(imm), scratchRegister);
            storePtr(scratchRegister, dest);
        } else
            storePtr(imm.asTrustedImmPtr(), dest);
    }

#endif // !CPU(ADDRESS64)

#if CPU(REGISTER64)
    void loadRegWord(Address address, RegisterID dest)
    {
        load64(address, dest);
    }

    void loadRegWord(BaseIndex address, RegisterID dest)
    {
#if CPU(NEEDS_ALIGNED_ACCESS)
        ASSERT(address.scale == ScaleRegWord || address.scale == TimesOne);
#endif
        load64(address, dest);
    }

    void loadRegWord(const void* address, RegisterID dest)
    {
        load64(address, dest);
    }

    void storeRegWord(RegisterID src, Address address)
    {
        store64(src, address);
    }

    void storeRegWord(RegisterID src, BaseIndex address)
    {
        store64(src, address);
    }

    void storeRegWord(RegisterID src, void* address)
    {
        store64(src, address);
    }

    void storeRegWord(TrustedImm64 imm, Address address)
    {
        store64(imm, address);
    }

#elif CPU(REGISTER32)
    void loadRegWord(Address address, RegisterID dest)
    {
        load32(address, dest);
    }

    void loadRegWord(BaseIndex address, RegisterID dest)
    {
#if CPU(NEEDS_ALIGNED_ACCESS)
        ASSERT(address.scale == ScaleRegWord || address.scale == TimesOne);
#endif
        load32(address, dest);
    }

    void loadRegWord(const void* address, RegisterID dest)
    {
        load32(address, dest);
    }

    void storeRegWord(RegisterID src, Address address)
    {
        store32(src, address);
    }

    void storeRegWord(RegisterID src, BaseIndex address)
    {
        store32(src, address);
    }

    void storeRegWord(RegisterID src, void* address)
    {
        store32(src, address);
    }

    void storeRegWord(TrustedImm32 imm, Address address)
    {
        store32(imm, address);
    }
#else
#  error "Unknown register size"
#endif

#if USE(JSVALUE64)
    bool shouldBlindDouble(double value)
    {
        // Don't trust NaN or +/-Infinity
        if (!std::isfinite(value))
            return shouldConsiderBlinding();

        // Try to force normalisation, and check that there's no change
        // in the bit pattern
        if (bitwise_cast<uint64_t>(value * 1.0) != bitwise_cast<uint64_t>(value))
            return shouldConsiderBlinding();

        value = std::abs(value);
        // Only allow a limited set of fractional components
        double scaledValue = value * 8;
        if (scaledValue / 8 != value)
            return shouldConsiderBlinding();
        double frac = scaledValue - floor(scaledValue);
        if (frac != 0.0)
            return shouldConsiderBlinding();

        return value > 0xff;
    }
    
    bool shouldBlindPointerForSpecificArch(uintptr_t value)
    {
        if (sizeof(void*) == 4)
            return shouldBlindForSpecificArch(static_cast<uint32_t>(value));
        return shouldBlindForSpecificArch(static_cast<uint64_t>(value));
    }

#if CPU(X86_64)
    bool shouldBlind(ImmPtr imm)
    {
        static_assert(canBlind());
        
#if ENABLE(FORCED_JIT_BLINDING)
        UNUSED_PARAM(imm);
        // Debug always blind all constants, if only so we know
        // if we've broken blinding during patch development.
        return true;
#endif

        // First off we'll special case common, "safe" values to avoid hurting
        // performance too much
        uint64_t value = imm.asTrustedImmPtr().asIntptr();
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
            if (~value <= 0xff)
                return false;
        }
        }

        if (!shouldConsiderBlinding())
            return false;

        return shouldBlindPointerForSpecificArch(static_cast<uintptr_t>(value));
    }
#else
    static constexpr bool shouldBlind(ImmPtr)
    {
        static_assert(!canBlind());
        return false;
    }
#endif

    uint8_t generateRotationSeed(size_t widthInBits)
    {
        // Generate the seed in [1, widthInBits - 1]. We should not generate widthInBits or 0
        // since it leads to `<< widthInBits` or `>> widthInBits`, which cause undefined behaviors.
        return (random() % (widthInBits - 1)) + 1;
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
        uint8_t rotation = generateRotationSeed(sizeof(void*) * 8);
        uintptr_t value = imm.asTrustedImmPtr().asIntptr();
        value = (value << rotation) | (value >> (sizeof(void*) * 8 - rotation));
        return RotatedImmPtr(value, rotation);
    }
    
    void loadRotationBlindedConstant(RotatedImmPtr constant, RegisterID dest)
    {
        move(constant.value, dest);
        rotateRightPtr(constant.rotation, dest);
    }

#if CPU(X86_64)
    bool shouldBlind(Imm64 imm)
    {
        static_assert(canBlind());

#if ENABLE(FORCED_JIT_BLINDING)
        UNUSED_PARAM(imm);
        // Debug always blind all constants, if only so we know
        // if we've broken blinding during patch development.
        return true;        
#endif

        // First off we'll special case common, "safe" values to avoid hurting
        // performance too much
        uint64_t value = imm.asTrustedImm64().m_value;
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
            if (~value <= 0xff)
                return false;

            JSValue jsValue = JSValue::decode(value);
            if (jsValue.isInt32())
                return shouldBlind(Imm32(jsValue.asInt32()));
            if (jsValue.isDouble() && !shouldBlindDouble(jsValue.asDouble()))
                return false;

            if (!shouldBlindDouble(bitwise_cast<double>(value)))
                return false;
        }
        }

        if (!shouldConsiderBlinding())
            return false;

        return shouldBlindForSpecificArch(value);
    }
#else
    static constexpr bool shouldBlind(Imm64)
    {
        static_assert(!canBlind());
        return false;
    }
#endif
    
    struct RotatedImm64 {
        RotatedImm64(uint64_t v1, uint8_t v2)
            : value(v1)
            , rotation(v2)
        {
        }
        TrustedImm64 value;
        TrustedImm32 rotation;
    };
    
    RotatedImm64 rotationBlindConstant(Imm64 imm)
    {
        uint8_t rotation = generateRotationSeed(sizeof(int64_t) * 8);
        uint64_t value = imm.asTrustedImm64().m_value;
        value = (value << rotation) | (value >> (sizeof(int64_t) * 8 - rotation));
        return RotatedImm64(value, rotation);
    }
    
    void loadRotationBlindedConstant(RotatedImm64 constant, RegisterID dest)
    {
        move(constant.value, dest);
        rotateRight64(constant.rotation, dest);
    }

    void convertInt32ToDouble(Imm32 imm, FPRegisterID dest)
    {
        if (shouldBlind(imm) && haveScratchRegisterForBlinding()) {
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

    void move(Imm64 imm, RegisterID dest)
    {
        if (shouldBlind(imm))
            loadRotationBlindedConstant(rotationBlindConstant(imm), dest);
        else
            move(imm.asTrustedImm64(), dest);
    }

    void and64(Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = andBlindedConstant(imm);
            and64(key.value1, dest);
            and64(key.value2, dest);
        } else
            and64(imm.asTrustedImm32(), dest);
    }

    void and64(Imm32 imm, RegisterID src, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            move(src, dest);
            and64(imm, dest);
        } else
            and64(imm.asTrustedImm32(), src, dest);
    }

#endif // USE(JSVALUE64)

#if CPU(X86_64) || CPU(RISCV64)
    void moveFloat(Imm32 imm, FPRegisterID dest)
    {
        move(imm, scratchRegister());
        move32ToFloat(scratchRegister(), dest);
    }

    void moveDouble(Imm64 imm, FPRegisterID dest)
    {
        move(imm, scratchRegister());
        move64ToDouble(scratchRegister(), dest);
    }
#endif

#if CPU(ARM64)
    void moveFloat(Imm32 imm, FPRegisterID dest)
    {
        move(imm, getCachedMemoryTempRegisterIDAndInvalidate());
        move32ToFloat(getCachedMemoryTempRegisterIDAndInvalidate(), dest);
    }

    void moveDouble(Imm64 imm, FPRegisterID dest)
    {
        move(imm, getCachedMemoryTempRegisterIDAndInvalidate());
        move64ToDouble(getCachedMemoryTempRegisterIDAndInvalidate(), dest);
    }
#endif

#if !CPU(X86) && !CPU(X86_64) && !CPU(ARM64)
    // We should implement this the right way eventually, but for now, it's fine because it arises so
    // infrequently.
    void compareDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right, RegisterID dest)
    {
        move(TrustedImm32(0), dest);
        Jump falseCase = branchDouble(invert(cond), left, right);
        move(TrustedImm32(1), dest);
        falseCase.link(this);
    }

    void compareDoubleWithZero(DoubleCondition cond, FPRegisterID left, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }
#endif

    void lea32(Address address, RegisterID dest)
    {
        add32(TrustedImm32(address.offset), address.base, dest);
    }

#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
    void lea64(Address address, RegisterID dest)
    {
        add64(TrustedImm32(address.offset), address.base, dest);
    }
#endif // CPU(X86_64) || CPU(ARM64)

#if CPU(X86_64)
    bool shouldBlind(Imm32 imm)
    {
        static_assert(canBlind());

#if ENABLE(FORCED_JIT_BLINDING)
        UNUSED_PARAM(imm);
        // Debug always blind all constants, if only so we know
        // if we've broken blinding during patch development.
        return true;
#else // ENABLE(FORCED_JIT_BLINDING)

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
            if (~value <= 0xff)
                return false;
        }

        if (!shouldConsiderBlinding())
            return false;

        return shouldBlindForSpecificArch(value);
#endif // ENABLE(FORCED_JIT_BLINDING)
    }
#else
    static constexpr bool shouldBlind(Imm32)
    {
        static_assert(!canBlind());
        return false;
    }
#endif

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
        static const uint32_t maskTable[4] = { 0xfffffffc, 0xffffffff, 0xfffffffe, 0xffffffff };

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

    void add32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = additionBlindedConstant(imm);
            add32(key.value1, src, dest);
            add32(key.value2, dest);
        } else
            add32(imm.asTrustedImm32(), src, dest);
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

    void mul32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            if (src != dest || haveScratchRegisterForBlinding()) {
                if (src == dest) {
                    move(src, scratchRegisterForBlinding());
                    src = scratchRegisterForBlinding();
                }
                loadXorBlindedConstant(xorBlindConstant(imm), dest);
                mul32(src, dest);
                return;
            }
            // If we don't have a scratch register available for use, we'll just
            // place a random number of nops.
            uint32_t nopCount = random() & 3;
            while (nopCount--)
                nop();
        }
        mul32(imm.asTrustedImm32(), src, dest);
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
    
#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
    void poke(Imm64 value, int index = 0)
    {
        store64(value, addressForPoke(index));
    }

    void store64(Imm64 imm, Address dest)
    {
        if (shouldBlind(imm) && haveScratchRegisterForBlinding()) {
            RegisterID scratchRegister = scratchRegisterForBlinding();
            loadRotationBlindedConstant(rotationBlindConstant(imm), scratchRegister);
            store64(scratchRegister, dest);
        } else
            store64(imm.asTrustedImm64(), dest);
    }

#endif // CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
    
    void store32(Imm32 imm, Address dest)
    {
        if (shouldBlind(imm)) {
#if CPU(X86) || CPU(X86_64)
            BlindedImm32 blind = xorBlindConstant(imm);
            store32(blind.value1, dest);
            xor32(blind.value2, dest);
#else // CPU(X86) || CPU(X86_64)
            if (haveScratchRegisterForBlinding()) {
                loadXorBlindedConstant(xorBlindConstant(imm), scratchRegisterForBlinding());
                store32(scratchRegisterForBlinding(), dest);
            } else {
                // If we don't have a scratch register available for use, we'll just 
                // place a random number of nops.
                uint32_t nopCount = random() & 3;
                while (nopCount--)
                    nop();
                store32(imm.asTrustedImm32(), dest);
            }
#endif // CPU(X86) || CPU(X86_64)
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

    void sub32(RegisterID src, Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            BlindedImm32 key = additionBlindedConstant(imm);
            sub32(src, key.value1, dest);
            sub32(key.value2, dest);
        } else
            sub32(src, imm.asTrustedImm32(), dest);
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
            if (haveScratchRegisterForBlinding()) {
                loadXorBlindedConstant(xorBlindConstant(right), scratchRegisterForBlinding());
                return branch32(cond, left, scratchRegisterForBlinding());
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

#if CPU(X86_64)
    // Other 64-bit platforms don't need blinding, and have branch64(RelationalCondition, RegisterID, Imm64) directly defined in the right file.
    // We cannot put this in MacroAssemblerX86_64.h, because it uses shouldBlind(), loadRoationBlindedConstant, etc.. which are only defined here and not there.
    Jump branch64(RelationalCondition cond, RegisterID left, Imm64 right)
    {
        if (shouldBlind(right)) {
            if (haveScratchRegisterForBlinding()) {
                loadRotationBlindedConstant(rotationBlindConstant(right), scratchRegisterForBlinding());
                return branch64(cond, left, scratchRegisterForBlinding());
            }

            // If we don't have a scratch register available for use, we'll just
            // place a random number of nops.
            uint32_t nopCount = random() & 3;
            while (nopCount--)
                nop();
            return branch64(cond, left, right.asTrustedImm64());
        }
        return branch64(cond, left, right.asTrustedImm64());
    }
#endif // CPU(X86_64)

    void compare32(RelationalCondition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        if (shouldBlind(right)) {
            if (left != dest || haveScratchRegisterForBlinding()) {
                RegisterID blindedConstantReg = dest;
                if (left == dest)
                    blindedConstantReg = scratchRegisterForBlinding();
                loadXorBlindedConstant(xorBlindConstant(right), blindedConstantReg);
                compare32(cond, left, blindedConstantReg, dest);
                return;
            }
            // If we don't have a scratch register available for use, we'll just
            // place a random number of nops.
            uint32_t nopCount = random() & 3;
            while (nopCount--)
                nop();
            compare32(cond, left, right.asTrustedImm32(), dest);
            return;
        }

        compare32(cond, left, right.asTrustedImm32(), dest);
    }

    Jump branchAdd32(ResultCondition cond, RegisterID src, Imm32 imm, RegisterID dest)
    {
        if (shouldBlind(imm)) {
            if (src != dest || haveScratchRegisterForBlinding()) {
                if (src == dest) {
                    move(src, scratchRegisterForBlinding());
                    src = scratchRegisterForBlinding();
                }
                loadXorBlindedConstant(xorBlindConstant(imm), dest);
                return branchAdd32(cond, src, dest);
            }
            // If we don't have a scratch register available for use, we'll just
            // place a random number of nops.
            uint32_t nopCount = random() & 3;
            while (nopCount--)
                nop();
        }
        return branchAdd32(cond, src, imm.asTrustedImm32(), dest);            
    }
    
    Jump branchMul32(ResultCondition cond, RegisterID src, Imm32 imm, RegisterID dest)
    {
        if (src == dest)
            ASSERT(haveScratchRegisterForBlinding());

        if (shouldBlind(imm)) {
            if (src == dest) {
                move(src, scratchRegisterForBlinding());
                src = scratchRegisterForBlinding();
            }
            loadXorBlindedConstant(xorBlindConstant(imm), dest);
            return branchMul32(cond, src, dest);  
        }
        return branchMul32(cond, src, imm.asTrustedImm32(), dest);
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

    void mul32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (hasOneBitSet(imm.m_value)) {
            lshift32(src, TrustedImm32(getLSBSet(imm.m_value)), dest);
            return;
        }
        MacroAssemblerBase::mul32(imm, src, dest);
    }

    void jitAssert(const WTF::ScopedLambda<Jump(void)>&);

    // This function emits code to preserve the CPUState (e.g. registers),
    // call a user supplied probe function, and restore the CPUState before
    // continuing with other JIT generated code.
    //
    // The user supplied probe function will be called with a single pointer to
    // a Probe::State struct (defined below) which contains, among other things,
    // the preserved CPUState. This allows the user probe function to inspect
    // the CPUState at that point in the JIT generated code.
    //
    // If the user probe function alters the register values in the Probe::State,
    // the altered values will be loaded into the CPU registers when the probe
    // returns.
    //
    // The Probe::State is stack allocated and is only valid for the duration
    // of the call to the user probe function.
    //
    // The probe function may choose to move the stack pointer (in any direction).
    // To do this, the probe function needs to set the new sp value in the CPUState.
    //
    // The probe function may also choose to fill stack space with some values.
    // To do this, the probe function must first:
    // 1. Set the new sp value in the Probe::State's CPUState.
    // 2. Set the Probe::State's initializeStackFunction to a Probe::Function callback
    //    which will do the work of filling in the stack values after the probe
    //    trampoline has adjusted the machine stack pointer.
    // 3. Set the Probe::State's initializeStackArgs to any value that the client wants
    //    to pass to the initializeStackFunction callback.
    // 4. Return from the probe function.
    //
    // Upon returning from the probe function, the probe trampoline will adjust the
    // the stack pointer based on the sp value in CPUState. If initializeStackFunction
    // is not set, the probe trampoline will restore registers and return to its caller.
    //
    // If initializeStackFunction is set, the trampoline will move the Probe::State
    // beyond the range of the stack pointer i.e. it will place the new Probe::State at
    // an address lower than where CPUState.sp() points. This ensures that the
    // Probe::State will not be trashed by the initializeStackFunction when it writes to
    // the stack. Then, the trampoline will call back to the initializeStackFunction
    // Probe::Function to let it fill in the stack values as desired. The
    // initializeStackFunction Probe::Function will be passed the moved Probe::State at
    // the new location.
    //
    // initializeStackFunction may now write to the stack at addresses greater or
    // equal to CPUState.sp(), but not below that. initializeStackFunction is also
    // not allowed to change CPUState.sp(). If the initializeStackFunction does not
    // abide by these rules, then behavior is undefined, and bad things may happen.
    //
    // Note: this version of probe() should be implemented by the target specific
    // MacroAssembler.
    void probe(Probe::Function, void* arg, SavedFPWidth = SavedFPWidth::DontSaveVectors);

    // This leaks memory. Must not be used for production.
    JS_EXPORT_PRIVATE void probeDebug(Function<void(Probe::Context&)>);

    // Let's you print from your JIT generated code.
    // See comments in MacroAssemblerPrinter.h for examples of how to use this.
    template<typename... Arguments>
    void print(Arguments&&... args);

    void print(Printer::PrintRecordList*);
};

} // namespace JSC

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::MacroAssembler::RelationalCondition);
void printInternal(PrintStream&, JSC::MacroAssembler::ResultCondition);
void printInternal(PrintStream&, JSC::MacroAssembler::DoubleCondition);

} // namespace WTF

#else // ENABLE(ASSEMBLER)

namespace JSC {

// If there is no assembler for this platform, at least allow code to make references to
// some of the things it would otherwise define, albeit without giving that code any way
// of doing anything useful.
class MacroAssembler {
private:
    MacroAssembler() { }
    
public:
    
    enum RegisterID : int8_t { NoRegister, InvalidGPRReg = -1 };
    enum FPRegisterID : int8_t { NoFPRegister, InvalidFPRReg = -1 };
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#include "LinkBuffer.h"

namespace JSC {

using namespace WTF;
#if OS(DARWIN) && CPU(ARM64)
// We already rely on page size being CeilingOnPageSize elsewhere (e.g. MarkedBlock).
// Just use the constexpr CeilingOnPageSize for better efficiency.
static inline constexpr size_t executablePageSize() { return CeilingOnPageSize; }
#else
static inline size_t executablePageSize() { return WTF::pageSize(); }
#endif

#if ENABLE(LIBPAS_JIT_HEAP)
static constexpr size_t minimumPoolSizeForSegregatedHeap = 256 * MB;
#endif

#if defined(FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB) && FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB > 0
static constexpr size_t fixedExecutableMemoryPoolSize = FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB * MB;
#elif CPU(ARM64)
#if ENABLE(JUMP_ISLANDS)
static constexpr size_t fixedExecutableMemoryPoolSize = 512 * MB;
#else
static constexpr size_t fixedExecutableMemoryPoolSize = 128 * MB;
#endif
#elif CPU(ARM_THUMB2)
#if ENABLE(JUMP_ISLANDS)
static constexpr size_t fixedExecutableMemoryPoolSize = 32 * MB;
#else
static constexpr size_t fixedExecutableMemoryPoolSize = 16 * MB;
#endif
#elif CPU(X86_64)
static constexpr size_t fixedExecutableMemoryPoolSize = 1 * GB;
#else
static constexpr size_t fixedExecutableMemoryPoolSize = 32 * MB;
#endif

#if ENABLE(JUMP_ISLANDS)
#if CPU(ARM64)
static constexpr double islandRegionSizeFraction = 0.125;
static constexpr size_t islandSizeInBytes = 4;
#elif CPU(ARM_THUMB2)
static constexpr double islandRegionSizeFraction = 0.05;
static constexpr size_t islandSizeInBytes = 4;
#endif
#endif

// Quick sanity check, in case FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB was set.
#if !ENABLE(JUMP_ISLANDS)
static_assert(fixedExecutableMemoryPoolSize <= MacroAssembler::nearJumpRange, "Executable pool size is too large for near jump/call without JUMP_ISLANDS");
#endif

#if CPU(ARM)
static constexpr double executablePoolReservationFraction = 0.15;
#else
static constexpr double executablePoolReservationFraction = 0.25;
#endif

#if ENABLE(LIBPAS_JIT_HEAP)
// This size is derived from jit_config's medium table size.
static constexpr size_t minimumExecutablePoolReservationSize = 256 * KB;
static_assert(fixedExecutableMemoryPoolSize * executablePoolReservationFraction >= minimumExecutablePoolReservationSize);
static_assert(fixedExecutableMemoryPoolSize < 4 * GB, "ExecutableMemoryHandle assumes it is less than 4GB");
#endif

struct JITReservation {
    PageReservation pageReservation;
    void* base { nullptr };
    size_t size { 0 };
};


static bool isJITEnabled()
{
    bool jitEnabled = !g_jscConfig.jitDisabled;
#if HAVE(IOS_JIT_RESTRICTIONS)
    jitEnabled = jitEnabled && processHasEntitlement("dynamic-codesigning"_s);
#elif HAVE(MAC_JIT_RESTRICTIONS) && USE(APPLE_INTERNAL_SDK)
    jitEnabled = jitEnabled && processHasEntitlement("com.apple.security.cs.allow-jit"_s);
#endif
    return jitEnabled;
}

static ALWAYS_INLINE JITReservation initializeJITPageReservation()
{
    JITReservation reservation;
    if (!isJITEnabled())
        return reservation;

#if OS(DARWIN)
    // Call pageSize() to run its assertions to enforce invariants that executablePageSize() relies on.
    WTF::pageSize();
#endif
    reservation.size = fixedExecutableMemoryPoolSize;

    if (Options::jitMemoryReservationSize()) {
        reservation.size = Options::jitMemoryReservationSize();
#if ENABLE(LIBPAS_JIT_HEAP)
        if (reservation.size * executablePoolReservationFraction < minimumExecutablePoolReservationSize)
            reservation.size += minimumExecutablePoolReservationSize;
#endif
    }
    reservation.size = std::max(roundUpToMultipleOf(executablePageSize(), reservation.size), executablePageSize() * 2);

#if !ENABLE(JUMP_ISLANDS)
    RELEASE_ASSERT(reservation.size <= MacroAssembler::nearJumpRange, "Executable pool size is too large for near jump/call without JUMP_ISLANDS");
#endif

#if ENABLE(LIBPAS_JIT_HEAP)
    if (reservation.size < minimumPoolSizeForSegregatedHeap)
        jit_heap_runtime_config.max_segregated_object_size = 0;
#endif

    auto tryCreatePageReservation = [] (size_t reservationSize) {
#if OS(LINUX)
        // On Linux, if we use uncommitted reservation, mmap operation is recorded with small page size in perf command's output.
        // This makes the following JIT code logging broken and some of JIT code is not recorded correctly.
        // To avoid this problem, we use committed reservation if we need perf JITDump logging.
        if (Options::logJITCodeForPerf())
            return PageReservation::tryReserveAndCommitWithGuardPages(reservationSize, OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true, false);
#endif
        if (Options::useJITCage() && JSC_ALLOW_JIT_CAGE_SPECIFIC_RESERVATION)
            return PageReservation::tryReserve(reservationSize, OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true, Options::useJITCage());
        return PageReservation::tryReserveWithGuardPages(reservationSize, OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true, false);
    };

    reservation.pageReservation = tryCreatePageReservation(reservation.size);

    if (Options::verboseExecutablePoolAllocation())
        dataLog(getpid(), ": Got executable pool reservation at ", RawPointer(reservation.pageReservation.base()), "...", RawPointer(reservation.pageReservation.end()), ", while I'm at ", RawPointer(bitwise_cast<void*>(initializeJITPageReservation)), "\n");
    
    if (reservation.pageReservation) {
        ASSERT(reservation.pageReservation.size() == reservation.size);
        reservation.base = reservation.pageReservation.base();

        bool fastJITPermissionsIsSupported = false;
#if OS(DARWIN) && CPU(ARM64)
#if USE(INLINE_JIT_PERMISSIONS_API)
        fastJITPermissionsIsSupported = (se_memory_inline_jit_restrict_with_witness_supported != nullptr)
            && !!se_memory_inline_jit_restrict_with_witness_supported();
#elif USE(PTHREAD_JIT_PERMISSIONS_API)
        fastJITPermissionsIsSupported = !!pthread_jit_write_protect_supported_np();
#elif USE(APPLE_INTERNAL_SDK)
        fastJITPermissionsIsSupported = !!os_thread_self_restrict_rwx_is_supported();
#endif
#endif
        g_jscConfig.useFastJITPermissions = fastJITPermissionsIsSupported;

        if (g_jscConfig.useFastJITPermissions)
            threadSelfRestrictRWXToRX();

// #if ENABLE(SEPARATED_WX_HEAP)
//         if (!g_jscConfig.useFastJITPermissions) {
//             // First page of our JIT allocation is reserved.
//             ASSERT(reservation.size >= executablePageSize() * 2);
//             reservation.base = (void*)((uintptr_t)(reservation.base) + executablePageSize());
//             reservation.size -= executablePageSize();
//             initializeSeparatedWXHeaps(reservation.pageReservation.base(), executablePageSize(), reservation.base, reservation.size);
//         }
// #endif

        void* reservationEnd = reinterpret_cast<uint8_t*>(reservation.base) + reservation.size;
        g_jscConfig.startExecutableMemory = reservation.base;
        g_jscConfig.endExecutableMemory = reservationEnd;

#if !USE(SYSTEM_MALLOC) && ENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
        WebConfig::g_config[0] = bitwise_cast<uintptr_t>(reservation.base);
        WebConfig::g_config[1] = bitwise_cast<uintptr_t>(reservationEnd);
#endif
    }

    return reservation;
}

class FixedVMPoolExecutableAllocator final {
    WTF_MAKE_FAST_ALLOCATED;

#if ENABLE(JUMP_ISLANDS)
    class Islands;
    class RegionAllocator;
#endif

public:
    FixedVMPoolExecutableAllocator()
#if !ENABLE(JUMP_ISLANDS)
        : m_allocator(*this)
#endif
    {
        JITReservation reservation = initializeJITPageReservation();
        m_reservation = WTFMove(reservation.pageReservation);
        if (m_reservation) {
#if ENABLE(JUMP_ISLANDS)
            // Consider this scenario:
            //
            //                                    <------------- nearJumpRange ------------->
            //    <------------- nearJumpRange -------------->
            //    [  island 0 ] [ JIT region 1  ] [ island 1 ] [ JIT region 2  ] [ island 2 ] [ JIT region3  ]
            //
            //                         C1 ---jump---> I1 --------------jump---------> I2 ---jump---> C3
            //
            // In order to jump across a distance that spans multiple nearJumpRanges, we currently
            // use chaining near jumps. Hence, a near jump in a jump island also needs to be able
            // to reach its neighboring jump islands in order to form this chain.
            //
            // For example, let say we have code in JIT region 1 that needs to jump to code in JIT region 3 in
            // the illustration above. That jump will be implemented as:
            //   1. Code C1 in JIT region 1 near jumps to island I1 in island 1.
            //   2. Island I1 near jumps to island I2 in island 2.
            //   3. Island I2 near jumps to code C3 in JIT region 3.
            //
            // Each of these near jumps need to be within the range of MacroAssembler::nearJumpRange.
            //
            // As a result, the maximum size of each JIT region is:
            //     MacroAssembler::nearJumpRange - 2 * islandRegionSize
            //
            // This is why each RegionAllocator tracks a range of m_regionSize instead of
            // MacroAssembler::nearJumpRange.
            //
            // Note: the illustration above shows a jump chain in the forward direction. The jump island
            // scheme also allows for a jump chain in the backward direction e.g. from C3 to C1.
            
            const size_t islandRegionSize = roundUpToMultipleOf(executablePageSize(), static_cast<size_t>(MacroAssembler::nearJumpRange * islandRegionSizeFraction));
            m_regionSize = MacroAssembler::nearJumpRange - islandRegionSize;
            RELEASE_ASSERT(isPageAligned(executablePageSize(), islandRegionSize));
            RELEASE_ASSERT(isPageAligned(executablePageSize(), m_regionSize));
            const unsigned numAllocators = (reservation.size + m_regionSize - 1) / m_regionSize;
            m_allocators = FixedVector<RegionAllocator>::createWithSizeAndConstructorArguments(numAllocators, *this);

            uintptr_t start = bitwise_cast<uintptr_t>(memoryStart());
            uintptr_t reservationEnd = bitwise_cast<uintptr_t>(memoryEnd());
            for (size_t i = 0; i < numAllocators; ++i) {
                uintptr_t end = start + m_regionSize;
                uintptr_t islandBegin = end - islandRegionSize;
                // The island in the very last region is never actually used (everything goes backwards), but we
                // can't put code there in case they do need to use a backward jump island, so set up accordingly.
                if (i == numAllocators - 1)
                    islandBegin = end = std::min(islandBegin, reservationEnd);
                RELEASE_ASSERT(end <= reservationEnd);
                m_allocators[i].configure(start, islandBegin, end);
                m_bytesReserved += m_allocators[i].allocatorSize();
                start += m_regionSize;
            }
#else
            m_allocator.addFreshFreeSpace(reservation.base, reservation.size);
            m_bytesReserved += reservation.size;
#endif
        }
    }

    ~FixedVMPoolExecutableAllocator()
    {
        m_reservation.deallocate();
    }

    void* memoryStart() { return g_jscConfig.startExecutableMemory; }
    void* memoryEnd() { return g_jscConfig.endExecutableMemory; }
    bool isValid() { return !!m_reservation; }

    RefPtr<ExecutableMemoryHandle> allocate(size_t sizeInBytes)
    {
#if ENABLE(LIBPAS_JIT_HEAP)
        auto result = ExecutableMemoryHandle::createImpl(sizeInBytes);
        if (LIKELY(result))
            m_bytesAllocated.fetch_add(result->sizeInBytes(), std::memory_order_relaxed);
        return result;
#elif ENABLE(JUMP_ISLANDS)
        Locker locker { getLock() };

        unsigned start = 0;
        if (UNLIKELY(Options::useRandomizingExecutableIslandAllocation()))
            start = cryptographicallyRandomNumber<uint32_t>() % m_allocators.size();

        unsigned i = start;
        while (true) {
            RegionAllocator& allocator = m_allocators[i];
            if (RefPtr<ExecutableMemoryHandle> result = allocator.allocate(locker, sizeInBytes))
                return result;
            i = (i + 1) % m_allocators.size();
            if (i == start)
                break;
        }
        return nullptr;
#else
        return m_allocator.allocate(sizeInBytes);
#endif
    }

    Lock& getLock() WTF_RETURNS_LOCK(m_lock) { return m_lock; }

#if ENABLE(LIBPAS_JIT_HEAP)
    void shrinkBytesAllocated(size_t oldSizeInBytes, size_t newSizeInBytes)
    {
        m_bytesAllocated.fetch_add(newSizeInBytes - oldSizeInBytes, std::memory_order_relaxed);
    }
#endif

    // Non atomic
    size_t bytesAllocated()
    {
#if ENABLE(LIBPAS_JIT_HEAP)
        return m_bytesAllocated.load(std::memory_order_relaxed);
#else
        size_t result = 0;
        forEachAllocator([&] (Allocator& allocator) {
            result += allocator.bytesAllocated();
        });
        return result;
#endif
    }

    size_t bytesReserved() const
    {
        return m_bytesReserved;
    }

    size_t bytesAvailable()
    {
        size_t bytesReserved = this->bytesReserved();
#if ENABLE(LIBPAS_JIT_HEAP)
        size_t nonAvailableSize = static_cast<size_t>(bytesReserved * executablePoolReservationFraction);
        if (nonAvailableSize < minimumExecutablePoolReservationSize)
            return bytesReserved - minimumExecutablePoolReservationSize;
        return bytesReserved - nonAvailableSize;
#else
        return static_cast<size_t>(bytesReserved * (1 - executablePoolReservationFraction));
#endif
    }

#if !ENABLE(LIBPAS_JIT_HEAP)
    size_t bytesCommitted()
    {
        size_t result = 0;
        forEachAllocator([&] (Allocator& allocator) {
            result += allocator.bytesCommitted();
        });
        return result;
    }
#endif

    bool isInAllocatedMemory(const AbstractLocker& locker, void* address)
    {
#if ENABLE(JUMP_ISLANDS)
        if (RegionAllocator* allocator = findRegion(bitwise_cast<uintptr_t>(address)))
            return allocator->isInAllocatedMemory(locker, address);
        return false;
#else
        return m_allocator.isInAllocatedMemory(locker, address);
#endif
    }

#if ENABLE(META_ALLOCATOR_PROFILE)
    void dumpProfile()
    {
        forEachAllocator([&] (Allocator& allocator) {
            allocator.dumpProfile();
        });
    }
#endif

#if !ENABLE(LIBPAS_JIT_HEAP)
    MetaAllocator::Statistics currentStatistics()
    {
        Locker locker { getLock() };
        MetaAllocator::Statistics result { 0, 0, 0 };
        forEachAllocator([&] (Allocator& allocator) {
            auto allocatorStats = allocator.currentStatistics(locker);
            result.bytesAllocated += allocatorStats.bytesAllocated;
            result.bytesReserved += allocatorStats.bytesReserved;
            result.bytesCommitted += allocatorStats.bytesCommitted;
        });
        return result;
    }
#endif // !ENABLE(LIBPAS_JIT_HEAP)

#if ENABLE(LIBPAS_JIT_HEAP)
    void handleWillBeReleased(ExecutableMemoryHandle& handle, size_t sizeInBytes)
    {
        m_bytesAllocated.fetch_sub(sizeInBytes, std::memory_order_relaxed);
#if ENABLE(JUMP_ISLANDS)
        if (m_islandsForJumpSourceLocation.isEmpty())
            return;
        
        Locker locker { getLock() };
        handleWillBeReleased(locker, handle);
#else // ENABLE(JUMP_ISLANDS) -> so !ENABLE(JUMP_ISLANDS)
        UNUSED_PARAM(handle);
#endif // ENABLE(JUMP_ISLANDS) -> so end of !ENABLE(JUMP_ISLANDS)
    }
#endif // ENABLE(LIBPAS_JIT_HEAP)

#if ENABLE(JUMP_ISLANDS)
    void handleWillBeReleased(const LockHolder& locker, ExecutableMemoryHandle& handle)
    {
        if (m_islandsForJumpSourceLocation.isEmpty())
            return;

        Vector<Islands*, 16> toRemove;
        void* start = handle.start().untaggedPtr();
        void* end = handle.end().untaggedPtr();
        m_islandsForJumpSourceLocation.iterate([&] (Islands& islands, bool& visitLeft, bool& visitRight) {
            if (start <= islands.key() && islands.key() < end)
                toRemove.append(&islands);
            if (islands.key() > start)
                visitLeft = true;
            if (islands.key() < end)
                visitRight = true;
        });

        for (Islands* islands : toRemove)
            freeIslands(locker, islands);

        if (ASSERT_ENABLED) {
            m_islandsForJumpSourceLocation.iterate([&] (Islands& islands, bool& visitLeft, bool& visitRight) {
                if (start <= islands.key() && islands.key() < end) {
                    dataLogLn("did not remove everything!");
                    RELEASE_ASSERT_NOT_REACHED();
                }
                visitLeft = true;
                visitRight = true;
            });
        }
    }

    ALWAYS_INLINE void* makeIsland(uintptr_t jumpLocation, uintptr_t newTarget, bool concurrently)
    {
        Locker locker { getLock() };
        return islandForJumpLocation(locker, jumpLocation, newTarget, concurrently);
    }

private:
    RegionAllocator* findRegion(uintptr_t ptr)
    {
        RegionAllocator* result = nullptr;
        forEachAllocator([&] (RegionAllocator& allocator) {
            if (allocator.start() <= ptr && ptr < allocator.end()) {
                result = &allocator;
                return IterationStatus::Done;
            }
            return IterationStatus::Continue;
        });
        return result;
    }

    ALWAYS_INLINE void freeJumpIslands(const LockHolder&, Islands* islands)
    {
        for (CodeLocationLabel<ExecutableMemoryPtrTag> jumpIsland : islands->jumpIslands) {
            uintptr_t untaggedJumpIsland = bitwise_cast<uintptr_t>(jumpIsland.dataLocation());
            RegionAllocator* allocator = findRegion(untaggedJumpIsland);
            RELEASE_ASSERT(allocator);
            allocator->freeIsland(untaggedJumpIsland);
        }
        islands->jumpIslands.clear();
    }

    void freeIslands(const LockHolder& locker, Islands* islands)
    {
        freeJumpIslands(locker, islands);
        m_islandsForJumpSourceLocation.remove(islands);
        delete islands;
    }

    ALWAYS_INLINE void* islandForJumpLocation(const LockHolder& locker, uintptr_t jumpLocation, uintptr_t target, bool concurrently)
    {
        Islands* islands = m_islandsForJumpSourceLocation.findExact(bitwise_cast<void*>(jumpLocation));
        if (islands) {
            // FIXME: We could create some method of reusing already allocated islands here, but it's
            // unlikely to matter in practice.
            if (!concurrently)
                freeJumpIslands(locker, islands);
        } else {
            islands = new Islands;
            islands->jumpSourceLocation = CodeLocationLabel<ExecutableMemoryPtrTag>(tagCodePtr<ExecutableMemoryPtrTag>(bitwise_cast<void*>(jumpLocation)));
            m_islandsForJumpSourceLocation.insert(islands);
        }

        RegionAllocator* allocator = findRegion(jumpLocation > target ? jumpLocation - m_regionSize : jumpLocation);
        RELEASE_ASSERT(allocator);
        void* result = allocator->allocateIsland();
        void* currentIsland = result;
        jumpLocation = bitwise_cast<uintptr_t>(currentIsland);
        while (true) {
            islands->jumpIslands.append(CodeLocationLabel<ExecutableMemoryPtrTag>(tagCodePtr<ExecutableMemoryPtrTag>(currentIsland)));

            auto emitJumpTo = [&] (void* target) ALWAYS_INLINE_LAMBDA {
                RELEASE_ASSERT(Assembler::canEmitJump(bitwise_cast<void*>(jumpLocation), target));

                MacroAssembler jit;
                auto nearTailCall = jit.nearTailCall();
                LinkBuffer linkBuffer(jit, CodePtr<NoPtrTag>(currentIsland), islandSizeInBytes, LinkBuffer::Profile::JumpIsland, JITCompilationMustSucceed, false);
                RELEASE_ASSERT(linkBuffer.isValid());

                // We use this to appease the assertion that we're not finalizing on a compiler thread. In this situation, it's
                // ok to do this on a compiler thread, since the compiler thread is linking a jump to this code (and no other live
                // code can jump to these islands). It's ok because the CPU protocol for exposing code to other CPUs is:
                // - Self modifying code fence (what FINALIZE_CODE does below). This does various memory flushes + instruction sync barrier (isb).
                // - Any CPU that will run the code must run a crossModifyingCodeFence (isb) before running it. Since the code that
                // has a jump linked to this island hasn't finalized yet, they're guaranteed to finalize there code and run an isb.
                linkBuffer.setIsJumpIsland();

                linkBuffer.link(nearTailCall, CodeLocationLabel<NoPtrTag>(target));
                FINALIZE_CODE(linkBuffer, NoPtrTag, "Jump Island: %lu", jumpLocation);
            };

            if (Assembler::canEmitJump(bitwise_cast<void*>(jumpLocation), bitwise_cast<void*>(target))) {
                emitJumpTo(bitwise_cast<void*>(target));
                break;
            }

            uintptr_t nextIslandRegion;
            if (jumpLocation > target)
                nextIslandRegion = jumpLocation - m_regionSize;
            else
                nextIslandRegion = jumpLocation + m_regionSize;

            RegionAllocator* allocator = findRegion(nextIslandRegion);
            RELEASE_ASSERT(allocator);
            void* nextIsland = allocator->allocateIsland();
            emitJumpTo(nextIsland);
            jumpLocation = bitwise_cast<uintptr_t>(nextIsland);
            currentIsland = nextIsland;
        }

        return result;
    }
#endif // ENABLE(JUMP_ISLANDS)

private:
    class Allocator
#if !ENABLE(LIBPAS_JIT_HEAP)
        : public MetaAllocator
#endif
    {
#if !ENABLE(LIBPAS_JIT_HEAP)
        using Base = MetaAllocator;
#endif
    public:
        Allocator(FixedVMPoolExecutableAllocator& allocator)
#if !ENABLE(LIBPAS_JIT_HEAP)
            : Base(allocator.getLock(), jitAllocationGranule, executablePageSize()) // round up all allocations to 32 bytes
            ,
#else
            :
#endif
            m_fixedAllocator(allocator)
        {
        }

#if ENABLE(LIBPAS_JIT_HEAP)
        void addFreshFreeSpace(void* start, size_t sizeInBytes)
        {
            RELEASE_ASSERT(!m_start);
            RELEASE_ASSERT(!m_end);
            m_start = reinterpret_cast<uintptr_t>(start);
            m_end = m_start + sizeInBytes;
            jit_heap_add_fresh_memory(pas_range_create(m_start, m_end));
        }

        bool isInAllocatedMemory(const AbstractLocker&, void* address)
        {
            uintptr_t addressAsInt = reinterpret_cast<uintptr_t>(address);
            return addressAsInt >= m_start && addressAsInt < m_end;
        }
#endif // ENABLE(LIBPAS_JIT_HEAP)

#if !ENABLE(LIBPAS_JIT_HEAP)
        FreeSpacePtr allocateNewSpace(size_t&) override
        {
            // We're operating in a fixed pool, so new allocation is always prohibited.
            return nullptr;
        }

        void notifyNeedPage(void* page, size_t count) override
        {
            m_fixedAllocator.m_reservation.commit(page, executablePageSize() * count);
        }

        void notifyPageIsFree(void* page, size_t count) override
        {
            m_fixedAllocator.m_reservation.decommit(page, executablePageSize() * count);
        }
#endif // !ENABLE(LIBPAS_JIT_HEAP)

        FixedVMPoolExecutableAllocator& m_fixedAllocator;
#if ENABLE(LIBPAS_JIT_HEAP)
        uintptr_t m_start { 0 };
        uintptr_t m_end { 0 };
#endif // ENABLE(LIBPAS_JIT_HEAP)
    };

#if ENABLE(JUMP_ISLANDS)
    class RegionAllocator final : public Allocator {
        using Base = Allocator;
    public:
        RegionAllocator(FixedVMPoolExecutableAllocator& allocator)
            : Base(allocator)
        {
            RELEASE_ASSERT(!(executablePageSize() % islandSizeInBytes), "Current implementation relies on this");
        }

        void configure(uintptr_t start, uintptr_t islandBegin, uintptr_t end)
        {
            RELEASE_ASSERT(start < islandBegin);
            RELEASE_ASSERT(islandBegin <= end);
            m_start = bitwise_cast<void*>(start);
            m_islandBegin = bitwise_cast<void*>(islandBegin);
            m_end = bitwise_cast<void*>(end);
            RELEASE_ASSERT(!((this->islandBegin() - this->start()) % executablePageSize()));
            RELEASE_ASSERT(!((this->end() - this->islandBegin()) % executablePageSize()));
            addFreshFreeSpace(bitwise_cast<void*>(this->start()), allocatorSize());
        }

        //  ------------------------------------
        //  | jit allocations -->   <-- islands |
        //  -------------------------------------

        uintptr_t start() { return bitwise_cast<uintptr_t>(m_start); }
        uintptr_t islandBegin() { return bitwise_cast<uintptr_t>(m_islandBegin); }
        uintptr_t end() { return bitwise_cast<uintptr_t>(m_end); }

        size_t maxIslandsInThisRegion() { return (end() - islandBegin()) / islandSizeInBytes; }

        uintptr_t allocatorSize()
        {
            return islandBegin() - start();
        }

        size_t islandsPerPage()
        {
            size_t islandsPerPage = executablePageSize() / islandSizeInBytes;
            ASSERT(islandsPerPage * islandSizeInBytes == executablePageSize());
            ASSERT(isPowerOfTwo(islandsPerPage));
            return islandsPerPage;
        }

#if !ENABLE(LIBPAS_JIT_HEAP)
        void release(const LockHolder& locker, MetaAllocatorHandle& handle) final
        {
            AssemblyCommentRegistry::singleton().unregisterCodeRange(handle.start().untaggedPtr(), handle.end().untaggedPtr());
            m_fixedAllocator.handleWillBeReleased(locker, handle);
            Base::release(locker, handle);
        }
#endif

        ALWAYS_INLINE void* allocateIsland()
        {
            uintptr_t end = this->end();
            auto findResult = [&] () -> void* {
                size_t resultBit = islandBits.findClearBit(0);
                if (resultBit == islandBits.size())
                    return nullptr;
                islandBits[resultBit] = true;
                uintptr_t result = end - ((resultBit + 1) * islandSizeInBytes); 
                return bitwise_cast<void*>(result);
            };

            if (void* result = findResult())
                return result;

            const size_t oldSize = islandBits.size();
            const size_t maxIslandsInThisRegion = this->maxIslandsInThisRegion();

            RELEASE_ASSERT(oldSize <= maxIslandsInThisRegion);
            if (UNLIKELY(oldSize == maxIslandsInThisRegion))
                crashOnJumpIslandExhaustion();

            const size_t newSize = std::min(oldSize + islandsPerPage(), maxIslandsInThisRegion);
            islandBits.resize(newSize);

            uintptr_t islandsBegin = end - (newSize * islandSizeInBytes); // [islandsBegin, end)
            m_fixedAllocator.m_reservation.commit(bitwise_cast<void*>(islandsBegin), (newSize - oldSize) * islandSizeInBytes);

            void* result = findResult();
            RELEASE_ASSERT(result);
            return result;
        }

        NEVER_INLINE NO_RETURN_DUE_TO_CRASH void crashOnJumpIslandExhaustion()
        {
            CRASH();
        }

        std::optional<size_t> islandBit(uintptr_t island)
        {
            uintptr_t end = this->end();
            if (islandBegin() <= island && island < end)
                return ((end - island) / islandSizeInBytes) - 1;
            return std::nullopt;
        }

        void freeIsland(uintptr_t island)
        {
            RELEASE_ASSERT(islandBegin() <= island && island < end());
            size_t bit = islandBit(island).value();
            RELEASE_ASSERT(!!islandBits[bit]);
            islandBits[bit] = false;
        }

        bool isInAllocatedMemory(const AbstractLocker& locker, void* address)
        {
            if (Base::isInAllocatedMemory(locker, address))
                return true;
            if (std::optional<size_t> bit = islandBit(bitwise_cast<uintptr_t>(address))) {
                if (bit.value() < islandBits.size())
                    return !!islandBits[bit.value()];
            }
            return false;
        }

    private:
#define REGION_ALLOCATOR_CODEPTR(field) \
    WTF_FUNCPTR_PTRAUTH_STR("RegionAllocator." #field) field

        // Range: [start, end)
        void* REGION_ALLOCATOR_CODEPTR(m_start);
        void* REGION_ALLOCATOR_CODEPTR(m_islandBegin);
        void* REGION_ALLOCATOR_CODEPTR(m_end);
        FastBitVector islandBits;
    };
#endif // ENABLE(JUMP_ISLANDS)

    template <typename Function>
    void forEachAllocator(Function function)
    {
#if ENABLE(JUMP_ISLANDS)
        for (RegionAllocator& allocator : m_allocators) {
            using FunctionResultType = decltype(function(allocator));
            if constexpr (std::is_same<IterationStatus, FunctionResultType>::value) {
                if (function(allocator) == IterationStatus::Done)
                    break;
            } else {
                static_assert(std::is_same<void, FunctionResultType>::value);
                function(allocator);
            }
        }
#else
        function(m_allocator);
#endif // ENABLE(JUMP_ISLANDS)
    }

#if ENABLE(JUMP_ISLANDS)
    class Islands : public RedBlackTree<Islands, void*>::Node {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        void* key() { return jumpSourceLocation.dataLocation(); }
        CodeLocationLabel<ExecutableMemoryPtrTag> jumpSourceLocation;
        Vector<CodeLocationLabel<ExecutableMemoryPtrTag>> jumpIslands;
    };
#endif // ENABLE(JUMP_ISLANDS)

    Lock m_lock;
    PageReservation m_reservation;
#if ENABLE(JUMP_ISLANDS)
    size_t m_regionSize;
    FixedVector<RegionAllocator> m_allocators;
    RedBlackTree<Islands, void*> m_islandsForJumpSourceLocation;
#else
    Allocator m_allocator;
#endif // ENABLE(JUMP_ISLANDS)
    size_t m_bytesReserved { 0 };
#if ENABLE(LIBPAS_JIT_HEAP)
    std::atomic<size_t> m_bytesAllocated { 0 };
#endif
};

#if ENABLE(JUMP_ISLANDS)
void* ExecutableAllocator::getJumpIslandTo(void* from, void* newDestination)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        RELEASE_ASSERT_NOT_REACHED();

    return allocator->makeIsland(bitwise_cast<uintptr_t>(from), bitwise_cast<uintptr_t>(newDestination), false);
}

void* ExecutableAllocator::getJumpIslandToConcurrently(void* from, void* newDestination)
{
    FixedVMPoolExecutableAllocator* allocator = g_jscConfig.fixedVMPoolExecutableAllocator;
    if (!allocator)
        RELEASE_ASSERT_NOT_REACHED();

    return allocator->makeIsland(bitwise_cast<uintptr_t>(from), bitwise_cast<uintptr_t>(newDestination), true);
}
#endif

} // namespace JSC
