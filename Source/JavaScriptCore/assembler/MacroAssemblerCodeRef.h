/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#include "ExecutableAllocator.h"
#include "JSCPoison.h"
#include <wtf/DataLog.h>
#include <wtf/PrintStream.h>
#include <wtf/RefPtr.h>
#include <wtf/text/CString.h>

// ASSERT_VALID_CODE_POINTER checks that ptr is a non-null pointer, and that it is a valid
// instruction address on the platform (for example, check any alignment requirements).
#if CPU(ARM_THUMB2) && ENABLE(JIT)
// ARM instructions must be 16-bit aligned. Thumb2 code pointers to be loaded into
// into the processor are decorated with the bottom bit set, while traditional ARM has
// the lower bit clear. Since we don't know what kind of pointer, we check for both
// decorated and undecorated null.
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(reinterpret_cast<intptr_t>(ptr) & ~1)
#define ASSERT_VALID_CODE_OFFSET(offset) \
    ASSERT(!(offset & 1)) // Must be multiple of 2.
#else
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(ptr)
#define ASSERT_VALID_CODE_OFFSET(offset) // Anything goes!
#endif

namespace JSC {

class MacroAssemblerCodePtr;

enum OpcodeID : unsigned;

// FunctionPtr:
//
// FunctionPtr should be used to wrap pointers to C/C++ functions in JSC
// (particularly, the stub functions).
class FunctionPtr {
public:
    FunctionPtr() { }

    template<typename returnType>
    FunctionPtr(returnType(*value)())
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1>
    FunctionPtr(returnType(*value)(argType1))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2>
    FunctionPtr(returnType(*value)(argType1, argType2))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3>
    FunctionPtr(returnType(*value)(argType1, argType2, argType3))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3, typename argType4>
    FunctionPtr(returnType(*value)(argType1, argType2, argType3, argType4))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3, typename argType4, typename argType5>
    FunctionPtr(returnType(*value)(argType1, argType2, argType3, argType4, argType5))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3, typename argType4, typename argType5, typename argType6>
    FunctionPtr(returnType(*value)(argType1, argType2, argType3, argType4, argType5, argType6))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }
// MSVC doesn't seem to treat functions with different calling conventions as
// different types; these methods already defined for fastcall, below.
#if CALLING_CONVENTION_IS_STDCALL && !OS(WINDOWS)

    template<typename returnType>
    FunctionPtr(returnType (CDECL *value)())
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1>
    FunctionPtr(returnType (CDECL *value)(argType1))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2>
    FunctionPtr(returnType (CDECL *value)(argType1, argType2))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3>
    FunctionPtr(returnType (CDECL *value)(argType1, argType2, argType3))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3, typename argType4>
    FunctionPtr(returnType (CDECL *value)(argType1, argType2, argType3, argType4))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }
#endif

#if COMPILER_SUPPORTS(FASTCALL_CALLING_CONVENTION)

    template<typename returnType>
    FunctionPtr(returnType (FASTCALL *value)())
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1>
    FunctionPtr(returnType (FASTCALL *value)(argType1))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2>
    FunctionPtr(returnType (FASTCALL *value)(argType1, argType2))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3>
    FunctionPtr(returnType (FASTCALL *value)(argType1, argType2, argType3))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<typename returnType, typename argType1, typename argType2, typename argType3, typename argType4>
    FunctionPtr(returnType (FASTCALL *value)(argType1, argType2, argType3, argType4))
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }
#endif

    template<typename FunctionType>
    explicit FunctionPtr(FunctionType* value)
        // Using a C-ctyle cast here to avoid compiler error on RVTC:
        // Error:  #694: reinterpret_cast cannot cast away const or other type qualifiers
        // (I guess on RVTC function pointers have a different constness to GCC/MSVC?)
        : m_value((void*)value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    explicit FunctionPtr(MacroAssemblerCodePtr);

    void* value() const
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        return m_value;
    }
    void* executableAddress() const
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        return m_value;
    }

private:
    void* m_value { nullptr };
};

// ReturnAddressPtr:
//
// ReturnAddressPtr should be used to wrap return addresses generated by processor
// 'call' instructions exectued in JIT code.  We use return addresses to look up
// exception and optimization information, and to repatch the call instruction
// that is the source of the return address.
class ReturnAddressPtr {
public:
    ReturnAddressPtr() { }

    explicit ReturnAddressPtr(void* value)
        : m_value(value)
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    explicit ReturnAddressPtr(FunctionPtr function)
        : m_value(function.value())
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    void* value() const
    {
        PoisonedMasmPtr::assertIsNotPoisoned(m_value);
        return m_value;
    }
    
    void dump(PrintStream& out) const
    {
        out.print(RawPointer(m_value));
    }

private:
    void* m_value { nullptr };
};

// MacroAssemblerCodePtr:
//
// MacroAssemblerCodePtr should be used to wrap pointers to JIT generated code.
class MacroAssemblerCodePtr {
public:
    MacroAssemblerCodePtr() { }

    explicit MacroAssemblerCodePtr(void* value)
#if CPU(ARM_THUMB2)
        // Decorate the pointer as a thumb code pointer.
        : m_value(reinterpret_cast<char*>(value) + 1)
#else
        : m_value(value)
#endif
    {
        m_value.assertIsPoisoned();
        ASSERT(value);
        ASSERT_VALID_CODE_POINTER(m_value);
    }
    
    static MacroAssemblerCodePtr createFromExecutableAddress(void* value)
    {
        ASSERT(value);
        ASSERT_VALID_CODE_POINTER(value);
        MacroAssemblerCodePtr result;
        result.m_value = PoisonedMasmPtr(value);
        result.m_value.assertIsPoisoned();
        return result;
    }

    static MacroAssemblerCodePtr createLLIntCodePtr(OpcodeID codeId);

    explicit MacroAssemblerCodePtr(ReturnAddressPtr ra)
        : m_value(ra.value())
    {
        ASSERT(ra.value());
        m_value.assertIsPoisoned();
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    PoisonedMasmPtr poisonedPtr() const { return m_value; }

    template<typename T = void*>
    T executableAddress() const
    {
        m_value.assertIsPoisoned();
        return m_value.unpoisoned<T>();
    }
#if CPU(ARM_THUMB2)
    // To use this pointer as a data address remove the decoration.
    template<typename T = void*>
    T dataLocation() const
    {
        m_value.assertIsPoisoned();
        ASSERT_VALID_CODE_POINTER(m_value);
        return bitwise_cast<T>(m_value ? m_value.unpoisoned<char*>() - 1 : nullptr);
    }
#else
    template<typename T = void*>
    T dataLocation() const
    {
        m_value.assertIsPoisoned();
        ASSERT_VALID_CODE_POINTER(m_value);
        return m_value.unpoisoned<T>();
    }
#endif

    bool operator!() const
    {
#if ENABLE(POISON_ASSERTS)
        if (!isEmptyValue() && !isDeletedValue())
            m_value.assertIsPoisoned();
#endif
        return !m_value;
    }
    explicit operator bool() const { return !(!*this); }
    
    bool operator==(const MacroAssemblerCodePtr& other) const
    {
#if ENABLE(POISON_ASSERTS)
        if (!isEmptyValue() && !isDeletedValue())
            m_value.assertIsPoisoned();
        if (!other.isEmptyValue() && !other.isDeletedValue())
            other.m_value.assertIsPoisoned();
#endif
        return m_value == other.m_value;
    }

    // Disallow any casting operations (except for booleans). Instead, the client
    // should be asking for poisonedPtr() or executableAddress() explicitly.
    template<typename T, typename = std::enable_if_t<!std::is_same<T, bool>::value>>
    operator T() = delete;

    void dumpWithName(const char* name, PrintStream& out) const;
    
    void dump(PrintStream& out) const;
    
    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    
    MacroAssemblerCodePtr(EmptyValueTag)
        : m_value(emptyValue())
    { }
    
    MacroAssemblerCodePtr(DeletedValueTag)
        : m_value(deletedValue())
    { }
    
    bool isEmptyValue() const { return m_value == emptyValue(); }
    bool isDeletedValue() const { return m_value == deletedValue(); }

    unsigned hash() const { return IntHash<uintptr_t>::hash(m_value.bits()); }

    static void initialize();

private:
    static PoisonedMasmPtr emptyValue() { return PoisonedMasmPtr(AlreadyPoisoned, 1); }
    static PoisonedMasmPtr deletedValue() { return PoisonedMasmPtr(AlreadyPoisoned, 2); }

    PoisonedMasmPtr m_value;
};

struct MacroAssemblerCodePtrHash {
    static unsigned hash(const MacroAssemblerCodePtr& ptr) { return ptr.hash(); }
    static bool equal(const MacroAssemblerCodePtr& a, const MacroAssemblerCodePtr& b)
    {
        return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

// MacroAssemblerCodeRef:
//
// A reference to a section of JIT generated code.  A CodeRef consists of a
// pointer to the code, and a ref pointer to the pool from within which it
// was allocated.
class MacroAssemblerCodeRef {
private:
    // This is private because it's dangerous enough that we want uses of it
    // to be easy to find - hence the static create method below.
    explicit MacroAssemblerCodeRef(MacroAssemblerCodePtr codePtr)
        : m_codePtr(codePtr)
    {
        ASSERT(m_codePtr);
    }

public:
    MacroAssemblerCodeRef()
    {
    }

    MacroAssemblerCodeRef(Ref<ExecutableMemoryHandle>&& executableMemory)
        : m_codePtr(executableMemory->start())
        , m_executableMemory(WTFMove(executableMemory))
    {
        ASSERT(m_executableMemory->isManaged());
        ASSERT(m_executableMemory->start());
        ASSERT(m_codePtr);
    }
    
    // Use this only when you know that the codePtr refers to code that is
    // already being kept alive through some other means. Typically this means
    // that codePtr is immortal.
    static MacroAssemblerCodeRef createSelfManagedCodeRef(MacroAssemblerCodePtr codePtr)
    {
        return MacroAssemblerCodeRef(codePtr);
    }
    
    // Helper for creating self-managed code refs from LLInt.
    static MacroAssemblerCodeRef createLLIntCodeRef(OpcodeID codeId);

    ExecutableMemoryHandle* executableMemory() const
    {
        return m_executableMemory.get();
    }
    
    MacroAssemblerCodePtr code() const
    {
        return m_codePtr;
    }
    
    size_t size() const
    {
        if (!m_executableMemory)
            return 0;
        return m_executableMemory->sizeInBytes();
    }

    bool tryToDisassemble(PrintStream& out, const char* prefix = "") const;
    
    bool tryToDisassemble(const char* prefix = "") const;
    
    JS_EXPORT_PRIVATE CString disassembly() const;
    
    explicit operator bool() const { return !!m_codePtr; }
    
    void dump(PrintStream& out) const;

private:
    MacroAssemblerCodePtr m_codePtr;
    RefPtr<ExecutableMemoryHandle> m_executableMemory;
};

inline FunctionPtr::FunctionPtr(MacroAssemblerCodePtr ptr)
    : m_value(ptr.executableAddress())
{
    PoisonedMasmPtr::assertIsNotPoisoned(m_value);
    ASSERT_VALID_CODE_POINTER(m_value);
}

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::MacroAssemblerCodePtr> {
    typedef JSC::MacroAssemblerCodePtrHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::MacroAssemblerCodePtr> : public CustomHashTraits<JSC::MacroAssemblerCodePtr> { };

} // namespace WTF
