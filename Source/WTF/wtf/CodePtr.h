/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/FunctionPtr.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/PtrTag.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

class PrintStream;
template<PtrTag tag, typename, FunctionAttributes> class FunctionPtr;

// ASSERT_VALID_CODE_POINTER checks that ptr is a non-null pointer, and that it is a valid
// instruction address on the platform (for example, check any alignment requirements).
#if CPU(ARM_THUMB2) && ENABLE(JIT)
// ARM instructions must be 16-bit aligned. Thumb2 code pointers to be loaded into
// into the processor are decorated with the bottom bit set, while traditional ARM has
// the lower bit clear. Since we don't know what kind of pointer, we check for both
// decorated and undecorated null.
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(reinterpret_cast<intptr_t>(ptr) & ~1)
#else
#define ASSERT_VALID_CODE_POINTER(ptr) \
    ASSERT(ptr)
#endif

struct CodePtrBase {
public:
    // We need to declare this in this non-template base. Otherwise, every use of
    // AlreadyTaggedValueTag will require a specialized template qualification.
    enum AlreadyTaggedValueTag { AlreadyTaggedValue };

    friend bool operator==(CodePtrBase, CodePtrBase) = default;

protected:
    WTF_EXPORT_PRIVATE static void dumpWithName(void* executableAddress, void* dataLocation, ASCIILiteral name, PrintStream& out);
};

template<PtrTag tag, FunctionAttributes attr = FunctionAttributes::None>
class CodePtr : public CodePtrBase {
    using Base = CodePtrBase;
public:
    using UntypedFunc = void(*)();

    CodePtr() = default;
    CodePtr(std::nullptr_t) { }

    explicit CodePtr(void* value)
#if CPU(ARM_THUMB2)
        // Decorate the pointer as a thumb code pointer.
        : m_value(reinterpret_cast<char*>(value) + 1)
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

    template<typename Out, typename... In>
    constexpr CodePtr(Out(*ptr)(In...))
        : m_value(encodeFunc(ptr))
    { }

#if OS(WINDOWS)
    template<typename Out, typename... In>
    constexpr CodePtr(Out(SYSV_ABI *ptr)(In...))
        : m_value(encodeFunc(ptr))
    { }
#endif

// MSVC doesn't seem to treat functions with different calling conventions as
// different types; these methods already defined for fastcall, below.
#if CALLING_CONVENTION_IS_STDCALL && !OS(WINDOWS)
    template<typename Out, typename... In>
    constexpr CodePtr(Out(CDECL *ptr)(In...))
        : m_value(encodeFunc(ptr))
    { }
#endif

#if COMPILER_SUPPORTS(FASTCALL_CALLING_CONVENTION)
    template<typename Out, typename... In>
    constexpr CodePtr(Out(FASTCALL *ptr)(In...))
        : m_value(encodeFunc(ptr))
    { }
#endif

#if ENABLE(JIT)
    // These 2 constructors ignores the FunctionAttributes of the FunctionPtr parameter when
    // creating the FunctionPtr. In general, this can be a problem on x86 ports (especially for
    // Windows). However, these constructors are only used in Repatch.cpp for ENABLE(JIT).
    // Since we no longer support ENABLE(JIT) for x86, this is not really an issue.

    template<typename Out, typename... In, FunctionAttributes otherAttr>
    CodePtr(const FunctionPtr<tag, Out(In...), otherAttr>& other)
        : CodePtr(AlreadyTaggedValue, other.taggedPtr())
    { }

    template<PtrTag otherTag, typename Out, typename... In, FunctionAttributes otherAttr>
    CodePtr(const FunctionPtr<otherTag, Out(In...), otherAttr>& other)
        : CodePtr(AlreadyTaggedValue, other.template retaggedPtr<tag>())
    { }
#endif

    static CodePtr fromTaggedPtr(void* ptr)
    {
        ASSERT(ptr);
        ASSERT_VALID_CODE_POINTER(ptr);
        assertIsTaggedWith<tag>(ptr);
        return CodePtr(AlreadyTaggedValue, ptr);
    }

    static CodePtr fromUntaggedPtr(void* ptr)
    {
        ASSERT(ptr);
        ASSERT_VALID_CODE_POINTER(ptr);
        assertIsNotTagged(ptr);
        return CodePtr(AlreadyTaggedValue, tagCodePtr<tag>(ptr));
    }

    template<PtrTag newTag>
    CodePtr<newTag, attr> retagged() const
    {
        if (!m_value)
            return CodePtr<newTag, attr>();
        return CodePtr<newTag, attr>(AlreadyTaggedValue, retaggedPtr<newTag>());
    }

    constexpr UntypedFunc untypedFunc() const { return decodeFunc(m_value); }

    template<typename T = void*>
    T taggedPtr() const
    {
        return std::bit_cast<T>(m_value);
    }

    template<typename T = void*>
    T untaggedPtr() const
    {
        return untagCodePtr<T, tag>(m_value);
    }

    template<PtrTag newTag, typename T = void*>
    T retaggedPtr() const
    {
        return retagCodePtr<T, tag, newTag>(m_value);
    }

#if CPU(ARM_THUMB2)
    // To use this pointer as a data address remove the decoration.
    template<typename T = void*>
    T dataLocation() const
    {
        ASSERT_VALID_CODE_POINTER(m_value);
        return std::bit_cast<T>(m_value ? std::bit_cast<char*>(m_value) - 1 : nullptr);
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
    
    friend bool operator==(CodePtr, CodePtr) = default;

    // Disallow any casting operations (except for booleans). Instead, the client
    // should be asking taggedPtr() explicitly.
    template<typename T, typename = std::enable_if_t<!std::is_same<T, bool>::value>>
    operator T() = delete;

    CodePtr operator+(size_t sizeInBytes) const { return CodePtr::fromUntaggedPtr(untaggedPtr<uint8_t*>() + sizeInBytes); }
    CodePtr operator-(size_t sizeInBytes) const { return CodePtr::fromUntaggedPtr(untaggedPtr<uint8_t*>() - sizeInBytes); }

    CodePtr& operator+=(size_t sizeInBytes) { return *this = *this + sizeInBytes; }
    CodePtr& operator-=(size_t sizeInBytes) { return *this = *this - sizeInBytes; }

    void dumpWithName(ASCIILiteral name, PrintStream& out) const
    {
        if (m_value)
            CodePtrBase::dumpWithName(taggedPtr(), dataLocation(), name, out);
        else
            CodePtrBase::dumpWithName(nullptr, nullptr, name, out);
    }

    void dump(PrintStream& out) const { dumpWithName("CodePtr"_s, out); }

    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    
    CodePtr(EmptyValueTag)
        : m_value(emptyValue())
    { }
    
    CodePtr(DeletedValueTag)
        : m_value(deletedValue())
    { }
    
    bool isEmptyValue() const { return m_value == emptyValue(); }
    bool isDeletedValue() const { return m_value == deletedValue(); }

    unsigned hash() const { return PtrHash<void*>::hash(m_value); }

    static void initialize();
    static constexpr PtrTag getTag() { return tag; }
    void validate() const { assertIsTaggedWith<tag>(m_value); }

private:
    CodePtr(AlreadyTaggedValueTag, void* ptr)
        : m_value(ptr)
    {
        assertIsTaggedWith<tag>(ptr);
    }

    static void* emptyValue() { return std::bit_cast<void*>(static_cast<intptr_t>(1)); }
    static void* deletedValue() { return std::bit_cast<void*>(static_cast<intptr_t>(2)); }

    template<typename FuncPtr>
    ALWAYS_INLINE static constexpr void* encodeFunc(FuncPtr ptr)
    {
        // Note: we cannot do the assert before this check because it will disqualify
        // this function for use in an constexpr context for some use cases.
        if constexpr (tag == CFunctionPtrTag)
            return reinterpret_cast<void*>(ptr);
        assertIsNullOrCFunctionPtr(ptr);
        return retagCodePtr<void*, CFunctionPtrTag, tag>(ptr);
    }

    ALWAYS_INLINE static constexpr UntypedFunc decodeFunc(void* ptr)
    {
        if constexpr (tag == CFunctionPtrTag)
            return reinterpret_cast<UntypedFunc>(ptr);
        auto result = retagCodePtr<UntypedFunc, tag, CFunctionPtrTag>(ptr);
        assertIsNullOrCFunctionPtr(result);
        return result;
    }

    void* m_value { nullptr };

    template<PtrTag, FunctionAttributes> friend class CodePtr;
};

template<PtrTag tag, FunctionAttributes attr>
struct CodePtrHash {
    static unsigned hash(const CodePtr<tag, attr>& ptr) { return ptr.hash(); }
    static bool equal(const CodePtr<tag, attr>& a, const CodePtr<tag, attr>& b)
    {
        return a == b;
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T> struct DefaultHash;
template<PtrTag tag, FunctionAttributes attr>
struct DefaultHash<CodePtr<tag, attr>> : CodePtrHash<tag, attr> { };

template<typename T> struct HashTraits;
template<PtrTag tag, FunctionAttributes attr>
struct HashTraits<CodePtr<tag, attr>> : public CustomHashTraits<CodePtr<tag, attr>> { };

} // namespace WTF

using WTF::CodePtr;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
