/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/DataLog.h>

namespace WTF {

enum class PACKeyType {
    ProcessIndependent,
    ProcessDependent
};

#define FOR_EACH_BASE_WTF_PTRTAG(v) \
    v(NoPtrTag) \
    v(CFunctionPtrTag) \

#define FOR_EACH_ADDITIONAL_WTF_PTRTAG(v) \
    v(FreeSpacePtrTag) \
    v(HandleMemoryPtrTag) \
    v(PlatformRegistersLRPtrTag) \
    v(PlatformRegistersPCPtrTag) \

#define FOR_EACH_WTF_PTRTAG(v) \
    FOR_EACH_BASE_WTF_PTRTAG(v) \
    FOR_EACH_ADDITIONAL_WTF_PTRTAG(v) \

enum PtrTag : uintptr_t {
    NoPtrTag = 0, // Note: We use the 0 tag for temporarily holding the return PC during JSC's arity fixup.
    CFunctionPtrTag,
};

enum class PtrTagCallerType : uint8_t { Native, JIT, None };
enum class PtrTagCalleeType : uint8_t { Native, JIT, };

template<PtrTag tag, typename PtrType>
ALWAYS_INLINE static PtrType tagNativeCodePtrImpl(PtrType ptr)
{
#if CPU(ARM64E)
    if constexpr (tag == NoPtrTag)
        return ptr;
    if constexpr (tag == CFunctionPtrTag)
        return ptrauth_sign_unauthenticated(ptr, ptrauth_key_function_pointer, 0);
    return ptrauth_sign_unauthenticated(ptr, ptrauth_key_process_dependent_code, tag);
#else
    return ptr;
#endif
}

template<PtrTag tag, typename PtrType>
ALWAYS_INLINE static PtrType untagNativeCodePtrImpl(PtrType ptr)
{
#if CPU(ARM64E)
    if constexpr (tag == NoPtrTag)
        return ptr;
    if constexpr (tag == CFunctionPtrTag)
        return __builtin_ptrauth_auth(ptr, ptrauth_key_function_pointer, 0);
    return __builtin_ptrauth_auth(ptr, ptrauth_key_process_dependent_code, tag);
#else
    return ptr;
#endif
}

template<PtrTag tag, typename PtrType>
ALWAYS_INLINE static bool isTaggedNativeCodePtrImpl(PtrType);

template<PtrTag passedTag>
struct PtrTagTraits {
    static constexpr PtrTag tag = passedTag;
    static constexpr PtrTagCallerType callerType = PtrTagCallerType::Native;
    static constexpr PtrTagCalleeType calleeType = PtrTagCalleeType::Native;
    static constexpr bool isSpecialized = false;

    template<typename PtrType>
    ALWAYS_INLINE static PtrType tagCodePtr(PtrType ptr)
    {
        return tagNativeCodePtrImpl<tag>(ptr);
    }

    template<typename PtrType>
    ALWAYS_INLINE static PtrType untagCodePtr(PtrType ptr)
    {
        return untagNativeCodePtrImpl<tag>(ptr);
    }

    template<typename PtrType>
    ALWAYS_INLINE static bool isTagged(PtrType ptr)
    {
        return isTaggedNativeCodePtrImpl<tag>(ptr);
    }
};

#if CPU(ARM64E)
#define WTF_PTRTAG_HASH(tag) ptrauth_string_discriminator(#tag)

#else // not CPU(ARM64E)

template<size_t N>
constexpr uintptr_t makePtrTagHash(const char (&str)[N])
{
    uintptr_t result = 134775813;
    for (size_t i = 0; i < N; ++i)
        result += ((result * str[i]) ^ (result >> 16));
    return result & 0xffff;
}

#define WTF_PTRTAG_HASH(tag) WTF::makePtrTagHash(#tag)
#endif // not CPU(ARM64E)

#define WTF_DECLARE_PTRTAG(tag) \
    constexpr PtrTag tag = static_cast<PtrTag>(WTF_PTRTAG_HASH(#tag)); \
    static_assert(tag != NoPtrTag && tag != CFunctionPtrTag);

static_assert(static_cast<uintptr_t>(NoPtrTag) == static_cast<uintptr_t>(0));
static_assert(static_cast<uintptr_t>(CFunctionPtrTag) == static_cast<uintptr_t>(1));

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable:4307)
#endif

FOR_EACH_ADDITIONAL_WTF_PTRTAG(WTF_DECLARE_PTRTAG)

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

struct PtrTagLookup {
    using TagForPtrFunc = const char* (*)(const void*);
    using PtrTagNameFunc = const char* (*)(PtrTag);

    void initialize(TagForPtrFunc tagForPtr, PtrTagNameFunc ptrTagName)
    {
        this->tagForPtr = tagForPtr;
        this->ptrTagName = ptrTagName;
    }

    TagForPtrFunc tagForPtr;
    PtrTagNameFunc ptrTagName;
    PtrTagLookup* next;
};

#if CPU(ARM64E)
#define ENABLE_PTRTAG_DEBUGGING ASSERT_ENABLED

#if ENABLE(PTRTAG_DEBUGGING)

WTF_EXPORT_PRIVATE void registerPtrTagLookup(PtrTagLookup*);
WTF_EXPORT_PRIVATE void reportBadTag(const void*, PtrTag expectedTag);

WTF_EXPORT_PRIVATE const char* ptrTagName(PtrTag);
WTF_EXPORT_PRIVATE const char* tagForPtr(const void*);

constexpr bool enablePtrTagDebugAssert = true;
#define REPORT_BAD_TAG(success, ptr, expectedTag) do { \
        if (UNLIKELY(!success)) \
            reportBadTag(reinterpret_cast<const void*>(ptr), expectedTag); \
    } while (false)
#else
constexpr bool enablePtrTagDebugAssert = false;
#define REPORT_BAD_TAG(success, ptr, expectedTag)
#endif

#define WTF_PTRTAG_ASSERT(action, ptr, expectedTag, assertion) \
    do { \
        if constexpr (action == PtrTagAction::ReleaseAssert \
            || (WTF::enablePtrTagDebugAssert && action == PtrTagAction::DebugAssert)) { \
            bool passed = (assertion); \
            REPORT_BAD_TAG(passed, ptr, expectedTag); \
            RELEASE_ASSERT(passed && #assertion); \
        } \
    } while (false)
#else

inline void registerPtrTagLookup(PtrTagLookup*) { }
inline void reportBadTag(const void*, PtrTag) { }

#define WTF_PTRTAG_ASSERT(action, ptr, expectedTag, assertion) \
    do { \
        if constexpr (action == PtrTagAction::ReleaseAssert) { \
            UNUSED_PARAM(ptr); \
            RELEASE_ASSERT(assertion); \
        } \
    } while (false)
#endif

enum class PtrTagAction {
    ReleaseAssert,
    DebugAssert,
    NoAssert,
};

constexpr PtrTag AnyPtrTag = static_cast<PtrTag>(-1); // Only used for assertion messages.

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline constexpr T removeCodePtrTag(PtrType ptr)
{
#if CPU(ARM64E)
    return bitwise_cast<T>(ptrauth_strip(ptr, ptrauth_key_process_dependent_code));
#else
    return bitwise_cast<T>(ptr);
#endif
}

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline constexpr PtrType removeCodePtrTag(PtrType ptr)
{
#if CPU(ARM64E)
    return ptrauth_strip(ptr, ptrauth_key_process_dependent_code);
#else
    return ptr;
#endif
}

template<PtrTagAction tagAction, PtrTag tag, typename PtrType>
inline PtrType tagCodePtrImpl(PtrType ptr)
{
    if (!ptr)
        return nullptr;
    WTF_PTRTAG_ASSERT(tagAction, ptr, NoPtrTag, removeCodePtrTag(ptr) == ptr);
    return PtrTagTraits<tag>::tagCodePtr(ptr);
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T tagCodePtr(PtrType ptr)
{
    return bitwise_cast<T>(tagCodePtrImpl<PtrTagAction::DebugAssert, tag>(ptr));
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCodePtr(PtrType ptr) { return tagCodePtrImpl<PtrTagAction::DebugAssert, tag>(ptr); }

template<PtrTagAction tagAction, PtrTag tag, typename PtrType>
inline PtrType untagCodePtrImpl(PtrType ptr)
{
    if (!ptr)
        return nullptr;
    PtrType result = PtrTagTraits<tag>::untagCodePtr(ptr);
    WTF_PTRTAG_ASSERT(tagAction, ptr, tag, removeCodePtrTag(ptr) == result);
    return result;
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCodePtr(PtrType ptr)
{
    return bitwise_cast<T>(untagCodePtrImpl<PtrTagAction::DebugAssert, tag>(ptr));
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCodePtr(PtrType ptr) { return untagCodePtrImpl<PtrTagAction::DebugAssert, tag>(ptr); }

template<PtrTagAction tagAction, PtrTag oldTag, PtrTag newTag, typename PtrType>
inline PtrType retagCodePtrImplHelper(PtrType ptr)
{
    if constexpr (oldTag == newTag || (oldTag == NoPtrTag && newTag == NoPtrTag))
        return ptr;
    if constexpr (newTag == NoPtrTag)
        return untagCodePtrImpl<tagAction, oldTag>(ptr);
    if constexpr (oldTag == NoPtrTag)
        return tagCodePtrImpl<tagAction, newTag>(ptr);
#if CPU(ARM64E)
    if constexpr (PtrTagTraits<oldTag>::isSpecialized || PtrTagTraits<newTag>::isSpecialized)
        return tagCodePtrImpl<tagAction, newTag>(untagCodePtrImpl<tagAction, oldTag>(ptr));
    if constexpr (oldTag == CFunctionPtrTag)
        return ptrauth_auth_and_resign(ptr, ptrauth_key_function_pointer, 0, ptrauth_key_process_dependent_code, newTag);
    if constexpr (newTag == CFunctionPtrTag)
        return ptrauth_auth_and_resign(ptr, ptrauth_key_process_dependent_code, oldTag, ptrauth_key_function_pointer, 0);
    return ptrauth_auth_and_resign(ptr, ptrauth_key_process_dependent_code, oldTag, ptrauth_key_process_dependent_code, newTag);
#else
    return tagCodePtrImpl<tagAction, newTag>(untagCodePtrImpl<tagAction, oldTag>(ptr));
#endif
}

template<PtrTagAction tagAction, PtrTag oldTag, PtrTag newTag, typename PtrType>
inline PtrType retagCodePtrImpl(PtrType ptr)
{
    if (!ptr)
        return nullptr;
    WTF_PTRTAG_ASSERT(tagAction, ptr, oldTag, ptr == (tagCodePtrImpl<PtrTagAction::NoAssert, oldTag>(removeCodePtrTag(ptr))));
    PtrType result = retagCodePtrImplHelper<tagAction, oldTag, newTag>(ptr);
    WTF_PTRTAG_ASSERT(tagAction, ptr, newTag, result == (tagCodePtrImpl<PtrTagAction::NoAssert, newTag>(removeCodePtrTag(ptr))));
    return result;
}

template<typename T, PtrTag oldTag, PtrTag newTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T retagCodePtr(PtrType ptr)
{
    return bitwise_cast<T>(retagCodePtrImpl<PtrTagAction::DebugAssert, oldTag, newTag>(ptr));
}

template<PtrTag oldTag, PtrTag newTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType retagCodePtr(PtrType ptr) { return retagCodePtrImpl<PtrTagAction::DebugAssert, oldTag, newTag>(ptr); }

template<typename PtrType>
void assertIsCFunctionPtr(PtrType value)
{
    void* ptr = bitwise_cast<void*>(value);
    WTF_PTRTAG_ASSERT(PtrTagAction::DebugAssert, ptr, CFunctionPtrTag, ptr == (tagCodePtrImpl<PtrTagAction::NoAssert, CFunctionPtrTag>(removeCodePtrTag(ptr))));
}

template<typename PtrType>
void assertIsNullOrCFunctionPtr(PtrType ptr)
{
    if (ptr)
        assertIsCFunctionPtr(ptr);
}

template<typename PtrType>
void assertIsNotTagged(PtrType value)
{
    void* ptr = bitwise_cast<void*>(value);
    WTF_PTRTAG_ASSERT(PtrTagAction::DebugAssert, ptr, NoPtrTag, ptr == removeCodePtrTag(ptr));
}

template<PtrTag tag, typename PtrType>
ALWAYS_INLINE static bool isTaggedNativeCodePtrImpl(PtrType ptr)
{
#if CPU(ARM64E)
    return ptr == tagNativeCodePtrImpl<tag>(removeCodePtrTag(ptr));
#else
    UNUSED_PARAM(ptr);
    return true;
#endif
}

template<PtrTag tag, typename PtrType>
bool isTaggedWith(PtrType value)
{
    void* ptr = bitwise_cast<void*>(value);
    if (tag == NoPtrTag)
        return ptr == removeCodePtrTag(ptr);
    return PtrTagTraits<tag>::isTagged(ptr);
}

template<PtrTag tag, typename PtrType>
void assertIsTaggedWith(PtrType value)
{
    UNUSED_PARAM(value);
    WTF_PTRTAG_ASSERT(PtrTagAction::DebugAssert, value, tag, PtrTagTraits<tag>::isTagged(value));
}

template<PtrTag tag, typename PtrType>
void assertIsNullOrTaggedWith(PtrType ptr)
{
    if (ptr)
        assertIsTaggedWith<tag>(ptr);
}

template<PtrTagAction tagAction, PtrTag tag, typename PtrType>
inline PtrType tagCFunctionPtrImpl(PtrType ptr)
{
    if (!ptr)
        return nullptr;
    WTF_PTRTAG_ASSERT(tagAction, ptr, CFunctionPtrTag, ptr == (tagCodePtrImpl<PtrTagAction::NoAssert, CFunctionPtrTag>(removeCodePtrTag(ptr))));
    return retagCodePtrImpl<tagAction, CFunctionPtrTag, tag>(ptr);
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T tagCFunctionPtr(PtrType ptr)
{
    return bitwise_cast<T>(tagCFunctionPtrImpl<PtrTagAction::DebugAssert, tag>(ptr));
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCFunctionPtr(PtrType ptr) { return tagCFunctionPtrImpl<PtrTagAction::DebugAssert, tag>(ptr); }

template<PtrTag newTag, typename FunctionType, class = typename std::enable_if<std::is_pointer<FunctionType>::value && std::is_function<typename std::remove_pointer<FunctionType>::type>::value>::type>
inline FunctionType tagCFunction(FunctionType func)
{
    return tagCFunctionPtrImpl<PtrTagAction::DebugAssert, newTag>(func);
}

template<typename ReturnType, PtrTag newTag, typename FunctionType, class = typename std::enable_if<std::is_pointer<FunctionType>::value && std::is_function<typename std::remove_pointer<FunctionType>::type>::value>::type>
inline ReturnType tagCFunction(FunctionType func)
{
    return bitwise_cast<ReturnType>(tagCFunction<newTag>(func));
}

template<PtrTagAction tagAction, PtrTag tag, typename PtrType>
inline PtrType untagCFunctionPtrImpl(PtrType ptr)
{
    if (!ptr)
        return nullptr;
    WTF_PTRTAG_ASSERT(tagAction, ptr, tag, ptr == (tagCodePtrImpl<PtrTagAction::NoAssert, tag>(removeCodePtrTag(ptr))));
    return retagCodePtrImpl<tagAction, tag, CFunctionPtrTag>(ptr);
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCFunctionPtr(PtrType ptr)
{
    return bitwise_cast<T>(untagCFunctionPtrImpl<PtrTagAction::DebugAssert, tag>(ptr));
}

template<typename T, PtrTag tag, PtrTagAction tagAction, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCFunctionPtr(PtrType ptr)
{
    return bitwise_cast<T>(untagCFunctionPtrImpl<tagAction, tag>(ptr));
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCFunctionPtr(PtrType ptr) { return untagCFunctionPtrImpl<PtrTagAction::DebugAssert, tag>(ptr); }

#if CPU(ARM64E)

inline const void* untagReturnPC(const void* pc, const void* sp)
{
    auto ptr = __builtin_ptrauth_auth(pc, ptrauth_key_return_address, sp);
    assertIsNotTagged(ptr);
    return ptr;
}

template <typename IntType, PACKeyType keyType = PACKeyType::ProcessDependent>
inline IntType untagInt(IntType ptrInt, PtrTag tag)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t));
    if constexpr (keyType == PACKeyType::ProcessDependent)
        return bitwise_cast<IntType>(ptrauth_auth_data(bitwise_cast<void*>(ptrInt), ptrauth_key_process_dependent_data, tag));
    return bitwise_cast<IntType>(ptrauth_auth_data(bitwise_cast<void*>(ptrInt), ptrauth_key_process_independent_data, tag));
}

template<typename T>
inline T* tagArrayPtr(std::nullptr_t ptr, size_t length)
{
    ASSERT(!length);
    length = length & ((1ull << 48) - 1); // See rdar://107561209, rdar://107724053.
    return ptrauth_sign_unauthenticated(static_cast<T*>(ptr), ptrauth_key_process_dependent_data, length);
}


template<typename T>
inline T* tagArrayPtr(T* ptr, size_t length)
{
    length = length & ((1ull << 48) - 1); // See rdar://107561209, rdar://107724053.
    return ptrauth_sign_unauthenticated(ptr, ptrauth_key_process_dependent_data, length);
}

template<typename T>
inline T* untagArrayPtr(T* ptr, size_t length)
{
    return ptrauth_auth_data(ptr, ptrauth_key_process_dependent_data, length);
}

template<typename T>
inline T* removeArrayPtrTag(T* ptr)
{
    return ptrauth_strip(ptr, ptrauth_key_process_dependent_data);
}

template<typename T>
inline T* retagArrayPtr(T* ptr, size_t oldLength, size_t newLength)
{
    newLength = newLength & ((1ull << 48) - 1); // See rdar://107561209, rdar://107724053.
    return ptrauth_auth_and_resign(ptr, ptrauth_key_process_dependent_data, oldLength, ptrauth_key_process_dependent_data, newLength);
}

template <PtrTag tag, typename IntType, PACKeyType keyType = PACKeyType::ProcessDependent>
inline IntType tagInt(IntType ptrInt)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t));
    if constexpr (keyType == PACKeyType::ProcessDependent)
        return bitwise_cast<IntType>(ptrauth_sign_unauthenticated(bitwise_cast<void*>(ptrInt), ptrauth_key_process_dependent_data, tag));
    return bitwise_cast<IntType>(ptrauth_sign_unauthenticated(bitwise_cast<void*>(ptrInt), ptrauth_key_process_independent_data, tag));
}

template <typename IntType, PACKeyType keyType = PACKeyType::ProcessDependent>
inline IntType tagInt(IntType ptrInt, PtrTag tag)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t));
    if constexpr (keyType == PACKeyType::ProcessDependent)
        return bitwise_cast<IntType>(ptrauth_sign_unauthenticated(bitwise_cast<void*>(ptrInt), ptrauth_key_process_dependent_data, tag));
    return bitwise_cast<IntType>(ptrauth_sign_unauthenticated(bitwise_cast<void*>(ptrInt), ptrauth_key_process_independent_data, tag));
}

inline bool usesPointerTagging() { return true; }

// vtbl function pointers need to sign with ptrauth_key_process_independent_code
// because they reside in library code shared by multiple processes.
// The second argument to __ptrauth() being 1 means to use the address of the pointer
// for diversification as well. __ptrauth() expects a literal int for this argument.
#define WTF_VTBL_FUNCPTR_PTRAUTH(discriminator) WTF_VTBL_FUNCPTR_PTRAUTH_STR(#discriminator)
#define WTF_VTBL_FUNCPTR_PTRAUTH_STR(discriminatorStr) \
    __ptrauth(ptrauth_key_process_independent_code, 1, ptrauth_string_discriminator(discriminatorStr))

#define WTF_FUNCPTR_PTRAUTH(discriminator) WTF_FUNCPTR_PTRAUTH_STR(#discriminator)
#define WTF_FUNCPTR_PTRAUTH_STR(discriminatorStr) \
    __ptrauth(ptrauth_key_process_dependent_code, 1, ptrauth_string_discriminator(discriminatorStr))

#else // not CPU(ARM64E)

inline const void* untagReturnPC(const void* pc, const void*)
{
    return pc;
}

template<typename T>
inline T* tagArrayPtr(std::nullptr_t, size_t size)
{
    ASSERT_UNUSED(size, !size);
    return nullptr;
}

template<typename T>
inline T* tagArrayPtr(T* ptr, size_t)
{
    return ptr;
}

template<typename T>
inline T* untagArrayPtr(T* ptr, size_t)
{
    return ptr;
}

template<typename T>
inline T* removeArrayPtrTag(T* ptr)
{
    return ptr;
}

template<typename T>
inline T* retagArrayPtr(T* ptr, size_t, size_t)
{
    return ptr;
}

template <PtrTag, typename IntType, PACKeyType>
inline IntType tagInt(IntType ptrInt)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t));
    return ptrInt;
}

template <typename IntType, PACKeyType>
inline IntType tagInt(IntType ptrInt, PtrTag)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t));
    return ptrInt;
}

template <typename IntType, PACKeyType>
inline IntType untagInt(IntType ptrInt, PtrTag)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t));
    return ptrInt;
}

inline bool usesPointerTagging() { return false; }

#define WTF_VTBL_FUNCPTR_PTRAUTH(discriminator)
#define WTF_VTBL_FUNCPTR_PTRAUTH_STR(discriminatorStr)
#define WTF_FUNCPTR_PTRAUTH(discriminator)
#define WTF_FUNCPTR_PTRAUTH_STR(discriminatorStr)

#endif // CPU(ARM64E)

template <PACKeyType keyType, PtrTag tag, typename IntType>
inline IntType tagInt(IntType ptrInt)
{
    return tagInt<tag, IntType, keyType>(ptrInt);
}

template <PACKeyType keyType, typename IntType>
inline IntType tagInt(IntType ptrInt, PtrTag tag)
{
    return tagInt<IntType, keyType>(ptrInt, tag);
}

template <PACKeyType keyType, typename IntType>
inline IntType untagInt(IntType ptrInt, PtrTag tag)
{
    return untagInt<IntType, keyType>(ptrInt, tag);
}

} // namespace WTF

using WTF::CFunctionPtrTag;
using WTF::NoPtrTag;
using WTF::PACKeyType;
using WTF::PlatformRegistersLRPtrTag;
using WTF::PlatformRegistersPCPtrTag;
using WTF::PtrTag;
using WTF::PtrTagCallerType;
using WTF::PtrTagCalleeType;

#if ENABLE(PTRTAG_DEBUGGING)
using WTF::reportBadTag;
#endif

using WTF::untagReturnPC;
using WTF::tagArrayPtr;
using WTF::untagArrayPtr;
using WTF::retagArrayPtr;
using WTF::removeArrayPtrTag;

using WTF::tagCodePtr;
using WTF::untagCodePtr;
using WTF::retagCodePtr;
using WTF::removeCodePtrTag;
using WTF::tagCFunction;
using WTF::tagCFunctionPtr;
using WTF::untagCFunctionPtr;
using WTF::tagInt;
using WTF::untagInt;

using WTF::assertIsCFunctionPtr;
using WTF::assertIsNullOrCFunctionPtr;
using WTF::assertIsNotTagged;
using WTF::isTaggedWith;
using WTF::assertIsTaggedWith;
using WTF::assertIsNullOrTaggedWith;
using WTF::usesPointerTagging;
