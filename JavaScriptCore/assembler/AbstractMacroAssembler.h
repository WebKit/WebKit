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

#if ENABLE(ASSEMBLER)

namespace JSC {

template <class AssemblerType>
class AbstractMacroAssembler {
public:
    class Jump;
    class PatchBuffer;
    class CodeLocationInstruction;
    class CodeLocationLabel;
    class CodeLocationJump;
    class CodeLocationCall;
    class CodeLocationDataLabel32;
    class CodeLocationDataLabelPtr;

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
        {
        }

#if !PLATFORM(X86_64)
        explicit Imm32(ImmPtr ptr)
            : m_value(ptr.asIntptr())
        {
        }
#endif

        int32_t m_value;
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
        friend class Jump;
        template<class AssemblerType_T>
        friend class AbstractMacroAssembler;
        friend class PatchBuffer;
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
        template<class AssemblerType_T>
        friend class AbstractMacroAssembler;
        friend class PatchBuffer;
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
        template<class AssemblerType_T>
        friend class AbstractMacroAssembler;
        friend class PatchBuffer;
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
        friend class PatchBuffer;
        template<class AssemblerType_T>
        friend class AbstractMacroAssembler;
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
        friend class PatchBuffer;
        template<class AssemblerType_T>
        friend class AbstractMacroAssembler;
        friend class Call;
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
        friend class PatchBuffer;

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


    // Section 3: MacroAssembler JIT instruction stream handles.
    //
    // The MacroAssembler supported facilities to modify a JIT generated
    // instruction stream after it has been generated (relinking calls and
    // jumps, and repatching data values).  The following types are used
    // to store handles into the underlying instruction stream, the type
    // providing semantic information as to what it is that is in the
    // instruction stream at this point, and thus what operations may be
    // performed on it.


    // CodeLocationCommon:
    //
    // Base type for other CodeLocation* types.  A postion in the JIT genertaed
    // instruction stream, without any semantic information.
    class CodeLocationCommon {
    public:
        CodeLocationCommon()
            : m_location(0)
        {
        }

        // In order to avoid the need to store multiple handles into the
        // instructions stream, where the code generation is deterministic
        // and the labels will always be a fixed distance apart, these
        // methods may be used to recover a handle that has nopw been
        // retained, based on a known fixed relative offset from one that has.
        CodeLocationInstruction instructionAtOffset(int offset);
        CodeLocationLabel labelAtOffset(int offset);
        CodeLocationJump jumpAtOffset(int offset);
        CodeLocationCall callAtOffset(int offset);
        CodeLocationDataLabelPtr dataLabelPtrAtOffset(int offset);
        CodeLocationDataLabel32 dataLabel32AtOffset(int offset);

        operator bool() { return m_location; }
        void reset() { m_location = 0; }

    protected:
        explicit CodeLocationCommon(void* location)
            : m_location(location)
        {
        }

        void* m_location;
    };

    // CodeLocationInstruction:
    //
    // An arbitrary instruction in the JIT code.
    class CodeLocationInstruction : public CodeLocationCommon {
        friend class CodeLocationCommon;
    public:
        CodeLocationInstruction()
        {
        }

        void patchLoadToLEA() {
            AssemblerType::patchLoadToLEA(reinterpret_cast<intptr_t>(this->m_location));
        }

    private:
        explicit CodeLocationInstruction(void* location)
            : CodeLocationCommon(location)
        {
        }
    };

    // CodeLocationLabel:
    //
    // A point in the JIT code maked with a label.
    class CodeLocationLabel : public CodeLocationCommon {
        friend class CodeLocationCommon;
        friend class CodeLocationJump;
        friend class PatchBuffer;
    public:
        CodeLocationLabel()
        {
        }

        void* addressForSwitch() { return this->m_location; }
        void* addressForExceptionHandler() { return this->m_location; }
        void* addressForJSR() { return this->m_location; }

    private:
        explicit CodeLocationLabel(void* location)
            : CodeLocationCommon(location)
        {
        }

        void* getJumpDestination() { return this->m_location; }
    };

    // CodeLocationJump:
    //
    // A point in the JIT code at which there is a jump instruction.
    class CodeLocationJump : public CodeLocationCommon {
        friend class CodeLocationCommon;
        friend class PatchBuffer;
    public:
        CodeLocationJump()
        {
        }

        void relink(CodeLocationLabel destination)
        {
            AssemblerType::patchJump(reinterpret_cast<intptr_t>(this->m_location), destination.m_location);
        }

    private:
        explicit CodeLocationJump(void* location)
            : CodeLocationCommon(location)
        {
        }
    };

    // CodeLocationCall:
    //
    // A point in the JIT code at which there is a call instruction.
    class CodeLocationCall : public CodeLocationCommon {
        friend class CodeLocationCommon;
        friend class PatchBuffer;
    public:
        CodeLocationCall()
        {
        }

        template<typename FunctionSig>
        void relink(FunctionSig* function)
        {
            AssemblerType::patchMacroAssemblerCall(reinterpret_cast<intptr_t>(this->m_location), reinterpret_cast<void*>(function));
        }

        // This methods returns the value that will be set as the return address
        // within a function that has been called from this call instruction.
        void* calleeReturnAddressValue()
        {
            return this->m_location;
        }

    private:
        explicit CodeLocationCall(void* location)
            : CodeLocationCommon(location)
        {
        }
    };

    // CodeLocationNearCall:
    //
    // A point in the JIT code at which there is a call instruction with near linkage.
    class CodeLocationNearCall : public CodeLocationCommon {
        friend class CodeLocationCommon;
        friend class PatchBuffer;
    public:
        CodeLocationNearCall()
        {
        }

        template<typename FunctionSig>
        void relink(FunctionSig* function)
        {
            AssemblerType::patchCall(reinterpret_cast<intptr_t>(this->m_location), reinterpret_cast<void*>(function));
        }

        // This methods returns the value that will be set as the return address
        // within a function that has been called from this call instruction.
        void* calleeReturnAddressValue()
        {
            return this->m_location;
        }

    private:
        explicit CodeLocationNearCall(void* location)
            : CodeLocationCommon(location)
        {
        }
    };

    // CodeLocationDataLabel32:
    //
    // A point in the JIT code at which there is an int32_t immediate that may be repatched.
    class CodeLocationDataLabel32 : public CodeLocationCommon {
        friend class CodeLocationCommon;
        friend class PatchBuffer;
    public:
        CodeLocationDataLabel32()
        {
        }

        void repatch(int32_t value)
        {
            AssemblerType::patchImmediate(reinterpret_cast<intptr_t>(this->m_location), value);
        }

    private:
        explicit CodeLocationDataLabel32(void* location)
            : CodeLocationCommon(location)
        {
        }
    };

    // CodeLocationDataLabelPtr:
    //
    // A point in the JIT code at which there is a void* immediate that may be repatched.
    class CodeLocationDataLabelPtr : public CodeLocationCommon {
        friend class CodeLocationCommon;
        friend class PatchBuffer;
    public:
        CodeLocationDataLabelPtr()
        {
        }

        void repatch(void* value)
        {
            AssemblerType::patchPointer(reinterpret_cast<intptr_t>(this->m_location), reinterpret_cast<intptr_t>(value));
        }

    private:
        explicit CodeLocationDataLabelPtr(void* location)
            : CodeLocationCommon(location)
        {
        }
    };

    // ProcessorReturnAddress:
    //
    // This class can be used to relink a call identified by its return address.
    class ProcessorReturnAddress {
    public:
        ProcessorReturnAddress(void* location)
            : m_location(location)
        {
        }

        template<typename FunctionSig>
        void relinkCallerToFunction(FunctionSig* newCalleeFunction)
        {
            AssemblerType::patchMacroAssemblerCall(reinterpret_cast<intptr_t>(this->m_location), reinterpret_cast<void*>(newCalleeFunction));
        }
        
        template<typename FunctionSig>
        void relinkNearCallerToFunction(FunctionSig* newCalleeFunction)
        {
            AssemblerType::patchCall(reinterpret_cast<intptr_t>(this->m_location), reinterpret_cast<void*>(newCalleeFunction));
        }
        
        operator void*()
        {
            return m_location;
        }

    private:
        void* m_location;
    };


    // Section 4: The patch buffer - utility to finalize code generation.


    // PatchBuffer:
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
    class PatchBuffer {
    public:
        PatchBuffer(void* code)
            : m_code(code)
        {
        }

        CodeLocationLabel entry()
        {
            return CodeLocationLabel(m_code);
        }

        void* trampolineAt(Label label)
        {
            return AssemblerType::getRelocatedAddress(m_code, label.m_label);
        }
        
        // These methods are used to link or set values at code generation time.

        template<typename FunctionSig>
        void link(Call call, FunctionSig* function)
        {
            ASSERT(call.isFlagSet(Call::Linkable));
#if PLATFORM(X86_64)
            if (call.isFlagSet(Call::Near)) {
                AssemblerType::linkCall(m_code, call.m_jmp, reinterpret_cast<void*>(function));
            } else {
                intptr_t callLocation = reinterpret_cast<intptr_t>(AssemblerType::getRelocatedAddress(m_code, call.m_jmp));
                AssemblerType::patchMacroAssemblerCall(callLocation, reinterpret_cast<void*>(function));
            }
#else
            AssemblerType::linkCall(m_code, call.m_jmp, reinterpret_cast<void*>(function));
#endif
        }
        
        template<typename FunctionSig>
        void linkTailRecursive(Jump jump, FunctionSig* function)
        {
            AssemblerType::linkJump(m_code, jump.m_jmp, reinterpret_cast<void*>(function));
        }

        template<typename FunctionSig>
        void linkTailRecursive(JumpList list, FunctionSig* function)
        {
            for (unsigned i = 0; i < list.m_jumps.size(); ++i) {
                AssemblerType::linkJump(m_code, list.m_jumps[i].m_jmp, reinterpret_cast<void*>(function));
            }
        }

        void link(Jump jump, CodeLocationLabel label)
        {
            AssemblerType::linkJump(m_code, jump.m_jmp, label.m_location);
        }

        void link(JumpList list, CodeLocationLabel label)
        {
            for (unsigned i = 0; i < list.m_jumps.size(); ++i)
                AssemblerType::linkJump(m_code, list.m_jumps[i].m_jmp, label.m_location);
        }

        void patch(DataLabelPtr label, void* value)
        {
            AssemblerType::patchAddress(m_code, label.m_label, value);
        }

        // These methods are used to obtain handles to allow the code to be relinked / repatched later.

        CodeLocationCall locationOf(Call call)
        {
            ASSERT(call.isFlagSet(Call::Linkable));
            ASSERT(!call.isFlagSet(Call::Near));
            return CodeLocationCall(AssemblerType::getRelocatedAddress(m_code, call.m_jmp));
        }

        CodeLocationNearCall locationOfNearCall(Call call)
        {
            ASSERT(call.isFlagSet(Call::Linkable));
            ASSERT(call.isFlagSet(Call::Near));
            return CodeLocationNearCall(AssemblerType::getRelocatedAddress(m_code, call.m_jmp));
        }

        CodeLocationLabel locationOf(Label label)
        {
            return CodeLocationLabel(AssemblerType::getRelocatedAddress(m_code, label.m_label));
        }

        CodeLocationDataLabelPtr locationOf(DataLabelPtr label)
        {
            return CodeLocationDataLabelPtr(AssemblerType::getRelocatedAddress(m_code, label.m_label));
        }

        CodeLocationDataLabel32 locationOf(DataLabel32 label)
        {
            return CodeLocationDataLabel32(AssemblerType::getRelocatedAddress(m_code, label.m_label));
        }

        // This method obtains the return address of the call, given as an offset from
        // the start of the code.
        unsigned returnAddressOffset(Call call)
        {
            return AssemblerType::getCallReturnOffset(call.m_jmp);
        }

    private:
        void* m_code;
    };


    // Section 5: Misc admin methods

    size_t size()
    {
        return m_assembler.size();
    }

    void* copyCode(ExecutablePool* allocator)
    {
        return m_assembler.executableCopy(allocator);
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

    ptrdiff_t differenceBetween(DataLabelPtr from, Call to)
    {
        return AssemblerType::getDifferenceBetweenLabels(from.m_label, to.m_jmp);
    }

protected:
    AssemblerType m_assembler;
};


template <class AssemblerType>
typename AbstractMacroAssembler<AssemblerType>::CodeLocationInstruction AbstractMacroAssembler<AssemblerType>::CodeLocationCommon::instructionAtOffset(int offset)
{
    return typename AbstractMacroAssembler::CodeLocationInstruction(reinterpret_cast<char*>(m_location) + offset);
}

template <class AssemblerType>
typename AbstractMacroAssembler<AssemblerType>::CodeLocationLabel AbstractMacroAssembler<AssemblerType>::CodeLocationCommon::labelAtOffset(int offset)
{
    return typename AbstractMacroAssembler::CodeLocationLabel(reinterpret_cast<char*>(m_location) + offset);
}

template <class AssemblerType>
typename AbstractMacroAssembler<AssemblerType>::CodeLocationJump AbstractMacroAssembler<AssemblerType>::CodeLocationCommon::jumpAtOffset(int offset)
{
    return typename AbstractMacroAssembler::CodeLocationJump(reinterpret_cast<char*>(m_location) + offset);
}

template <class AssemblerType>
typename AbstractMacroAssembler<AssemblerType>::CodeLocationCall AbstractMacroAssembler<AssemblerType>::CodeLocationCommon::callAtOffset(int offset)
{
    return typename AbstractMacroAssembler::CodeLocationCall(reinterpret_cast<char*>(m_location) + offset);
}

template <class AssemblerType>
typename AbstractMacroAssembler<AssemblerType>::CodeLocationDataLabelPtr AbstractMacroAssembler<AssemblerType>::CodeLocationCommon::dataLabelPtrAtOffset(int offset)
{
    return typename AbstractMacroAssembler::CodeLocationDataLabelPtr(reinterpret_cast<char*>(m_location) + offset);
}

template <class AssemblerType>
typename AbstractMacroAssembler<AssemblerType>::CodeLocationDataLabel32 AbstractMacroAssembler<AssemblerType>::CodeLocationCommon::dataLabel32AtOffset(int offset)
{
    return typename AbstractMacroAssembler::CodeLocationDataLabel32(reinterpret_cast<char*>(m_location) + offset);
}


} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // AbstractMacroAssembler_h
