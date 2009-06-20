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

#ifndef AbstractMacroAssembler_h
#define AbstractMacroAssembler_h

#include <wtf/Platform.h>

#include <MacroAssemblerCodeRef.h>
#include <CodeLocation.h>
#include <wtf/Noncopyable.h>
#include <wtf/UnusedParam.h>

#if ENABLE(ASSEMBLER)

// FIXME: keep transitioning this out into MacroAssemblerX86_64.
#if PLATFORM(X86_64)
#define REPTACH_OFFSET_CALL_R11 3
#endif

namespace JSC {

template <class AssemblerType>
class AbstractMacroAssembler {
public:
    typedef MacroAssemblerCodePtr CodePtr;
    typedef MacroAssemblerCodeRef CodeRef;

    class Jump;
    class LinkBuffer;
    class RepatchBuffer;

    typedef typename AssemblerType::RegisterID RegisterID;
    typedef typename AssemblerType::FPRegisterID FPRegisterID;
    typedef typename AssemblerType::JmpSrc JmpSrc;
    typedef typename AssemblerType::JmpDst JmpDst;


    // Section 1: MacroAssembler operand types
    //
    // The following types are used as operands to MacroAssembler operations,
    // describing immediate  and memory operands to the instructions to be planted.


    enum Scale {
        TimesOne,
        TimesTwo,
        TimesFour,
        TimesEight,
    };

    // Address:
    //
    // Describes a simple base-offset address.
    struct Address {
        explicit Address(RegisterID base, int32_t offset = 0)
            : base(base)
            , offset(offset)
        {
        }

        RegisterID base;
        int32_t offset;
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
        }

        ImplicitAddress(Address address)
            : base(address.base)
            , offset(address.offset)
        {
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
    };

    // AbsoluteAddress:
    //
    // Describes an memory operand given by a pointer.  For regular load & store
    // operations an unwrapped void* will be used, rather than using this.
    struct AbsoluteAddress {
        explicit AbsoluteAddress(void* ptr)
            : m_ptr(ptr)
        {
        }

        void* m_ptr;
    };

    // ImmPtr:
    //
    // A pointer sized immediate operand to an instruction - this is wrapped
    // in a class requiring explicit construction in order to differentiate
    // from pointers used as absolute addresses to memory operations
    struct ImmPtr {
        explicit ImmPtr(void* value)
            : m_value(value)
        {
        }

        intptr_t asIntptr()
        {
            return reinterpret_cast<intptr_t>(m_value);
        }

        void* m_value;
    };

    // Imm32:
    //
    // A 32bit immediate operand to an instruction - this is wrapped in a
    // class requiring explicit construction in order to prevent RegisterIDs
    // (which are implemented as an enum) from accidentally being passed as
    // immediate values.
    struct Imm32 {
        explicit Imm32(int32_t value)
            : m_value(value)
#if PLATFORM_ARM_ARCH(7)
            , m_isPointer(false)
#endif
        {
        }

#if !PLATFORM(X86_64)
        explicit Imm32(ImmPtr ptr)
            : m_value(ptr.asIntptr())
#if PLATFORM_ARM_ARCH(7)
            , m_isPointer(true)
#endif
        {
        }
#endif

        int32_t m_value;
#if PLATFORM_ARM_ARCH(7)
        // We rely on being able to regenerate code to recover exception handling
        // information.  Since ARMv7 supports 16-bit immediates there is a danger
        // that if pointer values change the layout of the generated code will change.
        // To avoid this problem, always generate pointers (and thus Imm32s constructed
        // from ImmPtrs) with a code sequence that is able  to represent  any pointer
        // value - don't use a more compact form in these cases.
        bool m_isPointer;
#endif
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
        template<class TemplateAssemblerType>
        friend class AbstractMacroAssembler;
        friend class Jump;
        friend class MacroAssemblerCodeRef;
        friend class LinkBuffer;

    public:
        Label()
        {
        }

        Label(AbstractMacroAssembler<AssemblerType>* masm)
            : m_label(masm->m_assembler.label())
        {
        }
        
        bool isUsed() const { return m_label.isUsed(); }
        void used() { m_label.used(); }
    private:
        JmpDst m_label;
    };

    // DataLabelPtr:
    //
    // A DataLabelPtr is used to refer to a location in the code containing a pointer to be
    // patched after the code has been generated.
    class DataLabelPtr {
        template<class TemplateAssemblerType>
        friend class AbstractMacroAssembler;
        friend class LinkBuffer;
    public:
        DataLabelPtr()
        {
        }

        DataLabelPtr(AbstractMacroAssembler<AssemblerType>* masm)
            : m_label(masm->m_assembler.label())
        {
        }
        
    private:
        JmpDst m_label;
    };

    // DataLabel32:
    //
    // A DataLabelPtr is used to refer to a location in the code containing a pointer to be
    // patched after the code has been generated.
    class DataLabel32 {
        template<class TemplateAssemblerType>
        friend class AbstractMacroAssembler;
        friend class LinkBuffer;
    public:
        DataLabel32()
        {
        }

        DataLabel32(AbstractMacroAssembler<AssemblerType>* masm)
            : m_label(masm->m_assembler.label())
        {
        }

    private:
        JmpDst m_label;
    };

    // Call:
    //
    // A Call object is a reference to a call instruction that has been planted
    // into the code buffer - it is typically used to link the call, setting the
    // relative offset such that when executed it will call to the desired
    // destination.
    class Call {
        template<class TemplateAssemblerType>
        friend class AbstractMacroAssembler;
        friend class LinkBuffer;
    public:
        enum Flags {
            None = 0x0,
            Linkable = 0x1,
            Near = 0x2,
            LinkableNear = 0x3,
        };

        Call()
            : m_flags(None)
        {
        }
        
        Call(JmpSrc jmp, Flags flags)
            : m_jmp(jmp)
            , m_flags(flags)
        {
        }

        bool isFlagSet(Flags flag)
        {
            return m_flags & flag;
        }

        static Call fromTailJump(Jump jump)
        {
            return Call(jump.m_jmp, Linkable);
        }

    private:
        JmpSrc m_jmp;
        Flags m_flags;
    };

    // Jump:
    //
    // A jump object is a reference to a jump instruction that has been planted
    // into the code buffer - it is typically used to link the jump, setting the
    // relative offset such that when executed it will jump to the desired
    // destination.
    class Jump {
        template<class TemplateAssemblerType>
        friend class AbstractMacroAssembler;
        friend class Call;
        friend class LinkBuffer;
    public:
        Jump()
        {
        }
        
        Jump(JmpSrc jmp)    
            : m_jmp(jmp)
        {
        }
        
        void link(AbstractMacroAssembler<AssemblerType>* masm)
        {
            masm->m_assembler.linkJump(m_jmp, masm->m_assembler.label());
        }
        
        void linkTo(Label label, AbstractMacroAssembler<AssemblerType>* masm)
        {
            masm->m_assembler.linkJump(m_jmp, label.m_label);
        }

    private:
        JmpSrc m_jmp;
    };

    // JumpList:
    //
    // A JumpList is a set of Jump objects.
    // All jumps in the set will be linked to the same destination.
    class JumpList {
        friend class LinkBuffer;

    public:
        void link(AbstractMacroAssembler<AssemblerType>* masm)
        {
            size_t size = m_jumps.size();
            for (size_t i = 0; i < size; ++i)
                m_jumps[i].link(masm);
            m_jumps.clear();
        }
        
        void linkTo(Label label, AbstractMacroAssembler<AssemblerType>* masm)
        {
            size_t size = m_jumps.size();
            for (size_t i = 0; i < size; ++i)
                m_jumps[i].linkTo(label, masm);
            m_jumps.clear();
        }
        
        void append(Jump jump)
        {
            m_jumps.append(jump);
        }
        
        void append(JumpList& other)
        {
            m_jumps.append(other.m_jumps.begin(), other.m_jumps.size());
        }

        bool empty()
        {
            return !m_jumps.size();
        }

    private:
        Vector<Jump, 16> m_jumps;
    };


    // Section 3: LinkBuffer - utility to finalize code generation.

    static CodePtr trampolineAt(CodeRef ref, Label label)
    {
        return CodePtr(AssemblerType::getRelocatedAddress(ref.m_code.dataLocation(), label.m_label));
    }

    // LinkBuffer:
    //
    // This class assists in linking code generated by the macro assembler, once code generation
    // has been completed, and the code has been copied to is final location in memory.  At this
    // time pointers to labels within the code may be resolved, and relative offsets to external
    // addresses may be fixed.
    //
    // Specifically:
    //   * Jump objects may be linked to external targets,
    //   * The address of Jump objects may taken, such that it can later be relinked.
    //   * The return address of a Jump object representing a call may be acquired.
    //   * The address of a Label pointing into the code may be resolved.
    //   * The value referenced by a DataLabel may be fixed.
    //
    // FIXME: distinguish between Calls & Jumps (make a specific call to obtain the return
    // address of calls, as opposed to a point that can be used to later relink a Jump -
    // possibly wrap the later up in an object that can do just that).
    class LinkBuffer : public Noncopyable {
    public:
        // Note: Initialization sequence is significant, since executablePool is a PassRefPtr.
        //       First, executablePool is copied into m_executablePool, then the initialization of
        //       m_code uses m_executablePool, *not* executablePool, since this is no longer valid.
        LinkBuffer(AbstractMacroAssembler<AssemblerType>* masm, PassRefPtr<ExecutablePool> executablePool)
            : m_executablePool(executablePool)
            , m_code(masm->m_assembler.executableCopy(m_executablePool.get()))
            , m_size(masm->m_assembler.size())
#ifndef NDEBUG
            , m_completed(false)
#endif
        {
        }

        ~LinkBuffer()
        {
            ASSERT(m_completed);
        }

        // These methods are used to link or set values at code generation time.

        void link(Call call, FunctionPtr function)
        {
            ASSERT(call.isFlagSet(Call::Linkable));
#if PLATFORM(X86_64)
            if (!call.isFlagSet(Call::Near)) {
                char* callLocation = reinterpret_cast<char*>(AssemblerType::getRelocatedAddress(code(), call.m_jmp)) - REPTACH_OFFSET_CALL_R11;
                AssemblerType::patchPointerForCall(callLocation, function.value());
            } else
#endif
            AssemblerType::linkCall(code(), call.m_jmp, function.value());
        }
        
        void link(Jump jump, CodeLocationLabel label)
        {
            AssemblerType::linkJump(code(), jump.m_jmp, label.dataLocation());
        }

        void link(JumpList list, CodeLocationLabel label)
        {
            for (unsigned i = 0; i < list.m_jumps.size(); ++i)
                AssemblerType::linkJump(code(), list.m_jumps[i].m_jmp, label.dataLocation());
        }

        void patch(DataLabelPtr label, void* value)
        {
            AssemblerType::patchPointer(code(), label.m_label, value);
        }

        void patch(DataLabelPtr label, CodeLocationLabel value)
        {
            AssemblerType::patchPointer(code(), label.m_label, value.executableAddress());
        }

        // These methods are used to obtain handles to allow the code to be relinked / repatched later.

        CodeLocationCall locationOf(Call call)
        {
            ASSERT(call.isFlagSet(Call::Linkable));
            ASSERT(!call.isFlagSet(Call::Near));
            return CodeLocationCall(AssemblerType::getRelocatedAddress(code(), call.m_jmp));
        }

        CodeLocationNearCall locationOfNearCall(Call call)
        {
            ASSERT(call.isFlagSet(Call::Linkable));
            ASSERT(call.isFlagSet(Call::Near));
            return CodeLocationNearCall(AssemblerType::getRelocatedAddress(code(), call.m_jmp));
        }

        CodeLocationLabel locationOf(Label label)
        {
            return CodeLocationLabel(AssemblerType::getRelocatedAddress(code(), label.m_label));
        }

        CodeLocationDataLabelPtr locationOf(DataLabelPtr label)
        {
            return CodeLocationDataLabelPtr(AssemblerType::getRelocatedAddress(code(), label.m_label));
        }

        CodeLocationDataLabel32 locationOf(DataLabel32 label)
        {
            return CodeLocationDataLabel32(AssemblerType::getRelocatedAddress(code(), label.m_label));
        }

        // This method obtains the return address of the call, given as an offset from
        // the start of the code.
        unsigned returnAddressOffset(Call call)
        {
            return AssemblerType::getCallReturnOffset(call.m_jmp);
        }

        // Upon completion of all patching either 'finalizeCode()' or 'finalizeCodeAddendum()' should be called
        // once to complete generation of the code.  'finalizeCode()' is suited to situations
        // where the executable pool must also be retained, the lighter-weight 'finalizeCodeAddendum()' is
        // suited to adding to an existing allocation.
        CodeRef finalizeCode()
        {
            performFinalization();

            return CodeRef(m_code, m_executablePool, m_size);
        }
        CodeLocationLabel finalizeCodeAddendum()
        {
            performFinalization();

            return CodeLocationLabel(code());
        }

    private:
        // Keep this private! - the underlying code should only be obtained externally via 
        // finalizeCode() or finalizeCodeAddendum().
        void* code()
        {
            return m_code;
        }

        void performFinalization()
        {
#ifndef NDEBUG
            ASSERT(!m_completed);
            m_completed = true;
#endif

            ExecutableAllocator::makeExecutable(code(), m_size);
        }

        RefPtr<ExecutablePool> m_executablePool;
        void* m_code;
        size_t m_size;
#ifndef NDEBUG
        bool m_completed;
#endif
    };

    class RepatchBuffer {
    public:
        RepatchBuffer()
        {
        }

        void relink(CodeLocationJump jump, CodeLocationLabel destination)
        {
            AssemblerType::relinkJump(jump.dataLocation(), destination.dataLocation());
        }

        void relink(CodeLocationCall call, CodeLocationLabel destination)
        {
#if PLATFORM(X86_64)
            repatch(call.dataLabelPtrAtOffset(-REPTACH_OFFSET_CALL_R11), destination.executableAddress());
#else
            AssemblerType::relinkCall(call.dataLocation(), destination.executableAddress());
#endif
        }

        void relink(CodeLocationCall call, FunctionPtr destination)
        {
#if PLATFORM(X86_64)
            repatch(call.dataLabelPtrAtOffset(-REPTACH_OFFSET_CALL_R11), destination.executableAddress());
#else
            AssemblerType::relinkCall(call.dataLocation(), destination.executableAddress());
#endif
        }

        void relink(CodeLocationNearCall nearCall, CodePtr destination)
        {
            AssemblerType::relinkCall(nearCall.dataLocation(), destination.executableAddress());
        }

        void relink(CodeLocationNearCall nearCall, CodeLocationLabel destination)
        {
            AssemblerType::relinkCall(nearCall.dataLocation(), destination.executableAddress());
        }

        void relink(CodeLocationNearCall nearCall, FunctionPtr destination)
        {
            AssemblerType::relinkCall(nearCall.dataLocation(), destination.executableAddress());
        }

        void repatch(CodeLocationDataLabel32 dataLabel32, int32_t value)
        {
            AssemblerType::repatchInt32(dataLabel32.dataLocation(), value);
        }

        void repatch(CodeLocationDataLabelPtr dataLabelPtr, void* value)
        {
            AssemblerType::repatchPointer(dataLabelPtr.dataLocation(), value);
        }

        void relinkCallerToTrampoline(ReturnAddressPtr returnAddress, CodeLocationLabel label)
        {
            relink(CodeLocationCall(CodePtr(returnAddress)), label);
        }
        
        void relinkCallerToTrampoline(ReturnAddressPtr returnAddress, CodePtr newCalleeFunction)
        {
            relinkCallerToTrampoline(returnAddress, CodeLocationLabel(newCalleeFunction));
        }

        void relinkCallerToFunction(ReturnAddressPtr returnAddress, FunctionPtr function)
        {
            relink(CodeLocationCall(CodePtr(returnAddress)), function);
        }
        
        void relinkNearCallerToTrampoline(ReturnAddressPtr returnAddress, CodeLocationLabel label)
        {
            relink(CodeLocationNearCall(CodePtr(returnAddress)), label);
        }
        
        void relinkNearCallerToTrampoline(ReturnAddressPtr returnAddress, CodePtr newCalleeFunction)
        {
            relinkNearCallerToTrampoline(returnAddress, CodeLocationLabel(newCalleeFunction));
        }

        void repatchLoadPtrToLEA(CodeLocationInstruction instruction)
        {
            AssemblerType::repatchLoadPtrToLEA(instruction.dataLocation());
        }
    };


    // Section 4: Misc admin methods

    size_t size()
    {
        return m_assembler.size();
    }

    Label label()
    {
        return Label(this);
    }
    
    Label align()
    {
        m_assembler.align(16);
        return Label(this);
    }

    ptrdiff_t differenceBetween(Label from, Jump to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_jmp);
    }

    ptrdiff_t differenceBetween(Label from, Call to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_jmp);
    }

    ptrdiff_t differenceBetween(Label from, Label to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    ptrdiff_t differenceBetween(Label from, DataLabelPtr to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    ptrdiff_t differenceBetween(Label from, DataLabel32 to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    ptrdiff_t differenceBetween(DataLabelPtr from, Jump to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_jmp);
    }

    ptrdiff_t differenceBetween(DataLabelPtr from, DataLabelPtr to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    ptrdiff_t differenceBetween(DataLabelPtr from, Call to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_jmp);
    }

protected:
    AssemblerType m_assembler;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // AbstractMacroAssembler_h
