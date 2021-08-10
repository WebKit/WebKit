/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

#include "ExecutableMemoryHandle.h"
#include "JSCPtrTag.h"
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
#define ASSERT_NULL_OR_VALID_CODE_POINTER(ptr) \
    ASSERT(!ptr || reinterpret_cast<intptr_t>(ptr) & ~1)
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(reinterpret_cast<intptr_t>(ptr) & ~1)
#define ASSERT_VALID_CODE_OFFSET(offset) \
    ASSERT(!(offset & 1)) // Must be multiple of 2.
#else
#define ASSERT_NULL_OR_VALID_CODE_POINTER(ptr) // Anything goes!
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(ptr)
#define ASSERT_VALID_CODE_OFFSET(offset) // Anything goes!
#endif

namespace JSC {

template<PtrTag> class MacroAssemblerCodePtr;

enum OpcodeID : unsigned;

// CFunctionPtr can only be used to hold C/C++ functions.
class CFunctionPtr {
public:
    using Ptr = void(*)();

    CFunctionPtr() { }
    CFunctionPtr(std::nullptr_t) { }

    template<typename ReturnType, typename... Arguments>
    constexpr CFunctionPtr(ReturnType(&ptr)(Arguments...))
        : m_ptr(reinterpret_cast<Ptr>(&ptr))
    { }

    template<typename ReturnType, typename... Arguments>
    explicit CFunctionPtr(ReturnType(*ptr)(Arguments...))
        : m_ptr(reinterpret_cast<Ptr>(ptr))
    {
        assertIsCFunctionPtr(m_ptr);
    }

    // MSVC doesn't seem to treat functions with different calling conventions as
    // different types; these methods are already defined for fastcall, below.
#if CALLING_CONVENTION_IS_STDCALL && !OS(WINDOWS)
    template<typename ReturnType, typename... Arguments>
    constexpr CFunctionPtr(ReturnType(CDECL &ptr)(Arguments...))
        : m_ptr(reinterpret_cast<Ptr>(&ptr))
    { }

    template<typename ReturnType, typename... Arguments>
    explicit CFunctionPtr(ReturnType(CDECL *ptr)(Arguments...))
        : m_ptr(reinterpret_cast<Ptr>(ptr))
    {
        assertIsCFunctionPtr(m_ptr);
    }

#endif // CALLING_CONVENTION_IS_STDCALL && !OS(WINDOWS)

#if COMPILER_SUPPORTS(FASTCALL_CALLING_CONVENTION)
    template<typename ReturnType, typename... Arguments>
    constexpr CFunctionPtr(ReturnType(FASTCALL &ptr)(Arguments...))
        : m_ptr(reinterpret_cast<Ptr>(&ptr))
    { }

    template<typename ReturnType, typename... Arguments>
    explicit CFunctionPtr(ReturnType(FASTCALL *ptr)(Arguments...))
        : m_ptr(reinterpret_cast<Ptr>(ptr))
    {
        assertIsCFunctionPtr(m_ptr);
    }
#endif // COMPILER_SUPPORTS(FASTCALL_CALLING_CONVENTION)

    constexpr Ptr get() const { return m_ptr; }
    void* address() const { return reinterpret_cast<void*>(m_ptr); }

    explicit operator bool() const { return !!m_ptr; }
    bool operator!() const { return !m_ptr; }

    bool operator==(const CFunctionPtr& other) const { return m_ptr == other.m_ptr; }
    bool operator!=(const CFunctionPtr& other) const { return m_ptr != other.m_ptr; }

private:
    Ptr m_ptr { nullptr };
};


// FunctionPtr:
//
// FunctionPtr should be used to wrap pointers to C/C++ functions in JSC
// (particularly, the stub functions).
template<PtrTag tag = CFunctionPtrTag>
class FunctionPtr {
public:
    FunctionPtr() { }
    FunctionPtr(std::nullptr_t) { }

    template<typename ReturnType, typename... Arguments>
    FunctionPtr(ReturnType(*value)(Arguments...))
        : m_value(tagCFunctionPtr<void*, tag>(value))
    {
        assertIsNullOrCFunctionPtr(value);
        ASSERT_NULL_OR_VALID_CODE_POINTER(m_value);
    }

// MSVC doesn't seem to treat functions with different calling conventions as
// different types; these methods already defined for fastcall, below.
#if CALLING_CONVENTION_IS_STDCALL && !OS(WINDOWS)

    template<typename ReturnType, typename... Arguments>
    FunctionPtr(ReturnType(CDECL *value)(Arguments...))
        : m_value(tagCFunctionPtr<void*, tag>(value))
    {
        assertIsNullOrCFunctionPtr(value);
        ASSERT_NULL_OR_VALID_CODE_POINTER(m_value);
    }

#endif // CALLING_CONVENTION_IS_STDCALL && !OS(WINDOWS)

#if COMPILER_SUPPORTS(FASTCALL_CALLING_CONVENTION)

    template<typename ReturnType, typename... Arguments>
    FunctionPtr(ReturnType(FASTCALL *value)(Arguments...))
        : m_value(tagCFunctionPtr<void*, tag>(value))
    {
        assertIsNullOrCFunctionPtr(value);
        ASSERT_NULL_OR_VALID_CODE_POINTER(m_value);
    }

#endif // COMPILER_SUPPORTS(FASTCALL_CALLING_CONVENTION)

    template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_function<typename std::remove_pointer<PtrType>::type>::value>>
    explicit FunctionPtr(PtrType value)
        // Using a C-ctyle cast here to avoid compiler error on RVTC:
        // Error:  #694: reinterpret_cast cannot cast away const or other type qualifiers
        // (I guess on RVTC function pointers have a different constness to GCC/MSVC?)
        : m_value(tagCFunctionPtr<void*, tag>(value))
    {
        assertIsNullOrCFunctionPtr(value);
        ASSERT_NULL_OR_VALID_CODE_POINTER(m_value);
    }

    explicit FunctionPtr(MacroAssemblerCodePtr<tag>);

    template<PtrTag otherTag>
    FunctionPtr<otherTag> retagged() const
    {
        if (!m_value)
            return FunctionPtr<otherTag>();
        return FunctionPtr<otherTag>(*this);
    }

    void* executableAddress() const
    {
        return m_value;
    }

    template<PtrTag newTag>
    void* retaggedExecutableAddress() const
    {
        return retagCodePtr<tag, newTag>(m_value);
    }

    explicit operator bool() const { return !!m_value; }
    bool operator!() const { return !m_value; }

    bool operator==(const FunctionPtr& other) const { return m_value == other.m_value; }
    bool operator!=(const FunctionPtr& other) const { return m_value != other.m_value; }

private:
    template<PtrTag otherTag>
    explicit FunctionPtr(const FunctionPtr<otherTag>& other)
        : m_value(retagCodePtr<otherTag, tag>(other.executableAddress()))
    {
        ASSERT_NULL_OR_VALID_CODE_POINTER(m_value);
    }

    void* m_value { nullptr };

    template<PtrTag> friend class FunctionPtr;
};

static_assert(sizeof(FunctionPtr<CFunctionPtrTag>) == sizeof(void*), "");
#if COMPILER_SUPPORTS(BUILTIN_IS_TRIVIALLY_COPYABLE)
static_assert(__is_trivially_copyable(FunctionPtr<CFunctionPtrTag>), "");
#endif

// ReturnAddressPtr:
//
// ReturnAddressPtr should be used to wrap return addresses generated by processor
// 'call' instructions exectued in JIT code.  We use return addresses to look up
// exception and optimization information, and to repatch the call instruction
// that is the source of the return address.
class ReturnAddressPtr {
public:
    ReturnAddressPtr() { }

    explicit ReturnAddressPtr(const void* returnAddress)
    {
#if CPU(ARM64E)
        assertIsNotTagged(returnAddress);
        returnAddress = retagCodePtr<NoPtrTag, ReturnAddressPtrTag>(returnAddress);
#endif
        m_value = returnAddress;
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    static ReturnAddressPtr fromTaggedPC(const void* pc, const void* sp)
    {
        return ReturnAddressPtr(untagReturnPC(pc, sp));
    }

    const void* value() const
    {
        return m_value;
    }
    
    const void* untaggedValue() const
    {
        return untagCodePtr<ReturnAddressPtrTag>(m_value);
    }

    void dump(PrintStream& out) const
    {
        out.print(RawPointer(m_value));
    }

private:
    const void* m_value { nullptr };
};

// MacroAssemblerCodePtr:
//
// MacroAssemblerCodePtr should be used to wrap pointers to JIT generated code.
class MacroAssemblerCodePtrBase {
protected:
    static void dumpWithName(void* executableAddress, void* dataLocation, const char* name, PrintStream& out);
};

// FIXME: Make JSC MacroAssemblerCodePtr injerit from MetaAllocatorPtr.
// https://bugs.webkit.org/show_bug.cgi?id=185145
template<PtrTag tag>
class MacroAssemblerCodePtr : private MacroAssemblerCodePtrBase {
public:
    MacroAssemblerCodePtr() = default;
    MacroAssemblerCodePtr(std::nullptr_t) : m_value(nullptr) { }

    explicit MacroAssemblerCodePtr(const void* value)
#if CPU(ARM_THUMB2)
        // Decorate the pointer as a thumb code pointer.
        : m_value(reinterpret_cast<const char*>(value) + 1)
#else
        : m_value(value)
#endif
    {
        assertIsTaggedWith<tag>(value);
        ASSERT(value);
#if CPU(ARM_THUMB2)
        ASSERT(!(reinterpret_cast<uintptr_t>(value) & 1));
#endif
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    static MacroAssemblerCodePtr createFromExecutableAddress(const void* value)
    {
        ASSERT(value);
        ASSERT_VALID_CODE_POINTER(value);
        assertIsTaggedWith<tag>(value);
        MacroAssemblerCodePtr result;
        result.m_value = value;
        return result;
    }

    explicit MacroAssemblerCodePtr(ReturnAddressPtr ra)
        : m_value(retagCodePtr<ReturnAddressPtrTag, tag>(ra.value()))
    {
        ASSERT(ra.untaggedValue());
        ASSERT_VALID_CODE_POINTER(m_value);
    }

    template<PtrTag newTag>
    MacroAssemblerCodePtr<newTag> retagged() const
    {
        if (!m_value)
            return MacroAssemblerCodePtr<newTag>();
        return MacroAssemblerCodePtr<newTag>::createFromExecutableAddress(retaggedExecutableAddress<newTag>());
    }

    template<typename T = void*>
    T executableAddress() const
    {
        return bitwise_cast<T>(m_value);
    }

    template<typename T = void*>
    T untaggedExecutableAddress() const
    {
        return untagCodePtr<T, tag>(m_value);
    }

    template<PtrTag newTag, typename T = void*>
    T retaggedExecutableAddress() const
    {
        return retagCodePtr<T, tag, newTag>(m_value);
    }

#if CPU(ARM_THUMB2)
    // To use this pointer as a data address remove the decoration.
    template<typename T = void*>
    T dataLocation() const
    {
        ASSERT_VALID_CODE_POINTER(m_value);
        return bitwise_cast<T>(m_value ? bitwise_cast<char*>(m_value) - 1 : nullptr);
    }
#else
    template<typename T = void*>
    T dataLocation() const
    {
        ASSERT_VALID_CODE_POINTER(m_value);
        return untagCodePtr<T, tag>(m_value);
    }
#endif

    bool operator!() const
    {
        return !m_value;
    }
    explicit operator bool() const { return !(!*this); }
    
    bool operator==(const MacroAssemblerCodePtr& other) const
    {
        return m_value == other.m_value;
    }

    // Disallow any casting operations (except for booleans). Instead, the client
    // should be asking executableAddress() explicitly.
    template<typename T, typename = std::enable_if_t<!std::is_same<T, bool>::value>>
    operator T() = delete;

    void dumpWithName(const char* name, PrintStream& out) const
    {
        MacroAssemblerCodePtrBase::dumpWithName(executableAddress(), dataLocation(), name, out);
    }

    void dump(PrintStream& out) const { dumpWithName("CodePtr", out); }

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

    unsigned hash() const { return PtrHash<const void*>::hash(m_value); }

    static void initialize();

private:
    static const void* emptyValue() { return bitwise_cast<void*>(static_cast<intptr_t>(1)); }
    static const void* deletedValue() { return bitwise_cast<void*>(static_cast<intptr_t>(2)); }

    const void* m_value { nullptr };
};

template<PtrTag tag>
struct MacroAssemblerCodePtrHash {
    static unsigned hash(const MacroAssemblerCodePtr<tag>& ptr) { return ptr.hash(); }
    static bool equal(const MacroAssemblerCodePtr<tag>& a, const MacroAssemblerCodePtr<tag>& b)
    {
        return a == b;
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

// MacroAssemblerCodeRef:
//
// A reference to a section of JIT generated code.  A CodeRef consists of a
// pointer to the code, and a ref pointer to the pool from within which it
// was allocated.
class MacroAssemblerCodeRefBase {
protected:
    static bool tryToDisassemble(MacroAssemblerCodePtr<DisassemblyPtrTag>, size_t, const char* prefix, PrintStream& out);
    static bool tryToDisassemble(MacroAssemblerCodePtr<DisassemblyPtrTag>, size_t, const char* prefix);
    JS_EXPORT_PRIVATE static CString disassembly(MacroAssemblerCodePtr<DisassemblyPtrTag>, size_t);
};

template<PtrTag tag>
class MacroAssemblerCodeRef : private MacroAssemblerCodeRefBase {
private:
    // This is private because it's dangerous enough that we want uses of it
    // to be easy to find - hence the static create method below.
    explicit MacroAssemblerCodeRef(MacroAssemblerCodePtr<tag> codePtr)
        : m_codePtr(codePtr)
    {
        ASSERT(m_codePtr);
    }

public:
    MacroAssemblerCodeRef() = default;

    MacroAssemblerCodeRef(Ref<ExecutableMemoryHandle>&& executableMemory)
        : m_codePtr(executableMemory->start().retaggedPtr<tag>())
        , m_executableMemory(WTFMove(executableMemory))
    {
        ASSERT(m_executableMemory->start());
        ASSERT(m_codePtr);
    }

    template<PtrTag otherTag>
    MacroAssemblerCodeRef& operator=(const MacroAssemblerCodeRef<otherTag>& otherCodeRef)
    {
        m_codePtr = MacroAssemblerCodePtr<tag>::createFromExecutableAddress(otherCodeRef.code().template retaggedExecutableAddress<tag>());
        m_executableMemory = otherCodeRef.m_executableMemory;
        return *this;
    }
    
    // Use this only when you know that the codePtr refers to code that is
    // already being kept alive through some other means. Typically this means
    // that codePtr is immortal.
    static MacroAssemblerCodeRef createSelfManagedCodeRef(MacroAssemblerCodePtr<tag> codePtr)
    {
        return MacroAssemblerCodeRef(codePtr);
    }
    
    ExecutableMemoryHandle* executableMemory() const
    {
        return m_executableMemory.get();
    }
    
    MacroAssemblerCodePtr<tag> code() const
    {
        return m_codePtr;
    }

    template<PtrTag newTag>
    MacroAssemblerCodePtr<newTag> retaggedCode() const
    {
        return m_codePtr.template retagged<newTag>();
    }

    template<PtrTag newTag>
    MacroAssemblerCodeRef<newTag> retagged() const
    {
        return MacroAssemblerCodeRef<newTag>(*this);
    }

    size_t size() const
    {
        if (!m_executableMemory)
            return 0;
        return m_executableMemory->sizeInBytes();
    }

    bool tryToDisassemble(PrintStream& out, const char* prefix = "") const
    {
        return tryToDisassemble(retaggedCode<DisassemblyPtrTag>(), size(), prefix, out);
    }
    
    bool tryToDisassemble(const char* prefix = "") const
    {
        return tryToDisassemble(retaggedCode<DisassemblyPtrTag>(), size(), prefix);
    }
    
    CString disassembly() const
    {
        return MacroAssemblerCodeRefBase::disassembly(retaggedCode<DisassemblyPtrTag>(), size());
    }
    
    explicit operator bool() const { return !!m_codePtr; }
    
    void dump(PrintStream& out) const
    {
        m_codePtr.dumpWithName("CodeRef", out);
    }

private:
    template<PtrTag otherTag>
    MacroAssemblerCodeRef(const MacroAssemblerCodeRef<otherTag>& otherCodeRef)
    {
        *this = otherCodeRef;
    }

    MacroAssemblerCodePtr<tag> m_codePtr;
    RefPtr<ExecutableMemoryHandle> m_executableMemory;

    template<PtrTag> friend class MacroAssemblerCodeRef;
};

template<PtrTag tag>
inline FunctionPtr<tag>::FunctionPtr(MacroAssemblerCodePtr<tag> ptr)
    : m_value(ptr.executableAddress())
{
}

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<JSC::PtrTag tag> struct DefaultHash<JSC::MacroAssemblerCodePtr<tag>> : JSC::MacroAssemblerCodePtrHash<tag> { };

template<typename T> struct HashTraits;
template<JSC::PtrTag tag> struct HashTraits<JSC::MacroAssemblerCodePtr<tag>> : public CustomHashTraits<JSC::MacroAssemblerCodePtr<tag>> { };

} // namespace WTF
