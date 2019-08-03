/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
    NoPtrTag,
    CFunctionPtrTag,
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
    static_assert(tag != NoPtrTag && tag != CFunctionPtrTag, "");

static_assert(static_cast<uintptr_t>(NoPtrTag) == static_cast<uintptr_t>(0), "");
static_assert(static_cast<uintptr_t>(CFunctionPtrTag) == static_cast<uintptr_t>(1), "");

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable:4307)
#endif

FOR_EACH_ADDITIONAL_WTF_PTRTAG(WTF_DECLARE_PTRTAG)

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

struct PtrTagLookup {
    const char* (*tagForPtr)(const void*);
    const char* (*ptrTagName)(PtrTag);
    PtrTagLookup* next { nullptr };
};

#if CPU(ARM64E)

enum class PtrTagAction {
    ReleaseAssert,
    DebugAssert,
    NoAssert,
};

constexpr PtrTag AnyPtrTag = static_cast<PtrTag>(-1); // Only used for assertion messages.

WTF_EXPORT_PRIVATE void registerPtrTagLookup(PtrTagLookup*);
WTF_EXPORT_PRIVATE void reportBadTag(const void*, PtrTag expectedTag);

#if ASSERT_DISABLED
constexpr bool enablePtrTagDebugAssert = false;
#else
constexpr bool enablePtrTagDebugAssert = true;
#endif

#define WTF_PTRTAG_ASSERT(action, ptr, expectedTag, assertion) \
    do { \
        if (action == PtrTagAction::ReleaseAssert \
            || (WTF::enablePtrTagDebugAssert && action == PtrTagAction::DebugAssert)) { \
            bool passed = (assertion); \
            if (UNLIKELY(!passed)) { \
                reportBadTag(reinterpret_cast<const void*>(ptr), expectedTag); \
            } \
            RELEASE_ASSERT(passed && #assertion); \
        } \
    } while (false)


template<typename T>
inline T* tagArrayPtr(std::nullptr_t ptr, size_t length)
{
    ASSERT(!length);
    return ptrauth_sign_unauthenticated(static_cast<T*>(ptr), ptrauth_key_process_dependent_data, length);
}


template<typename T>
inline T* tagArrayPtr(T* ptr, size_t length)
{
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
    return ptrauth_auth_and_resign(ptr, ptrauth_key_process_dependent_data, oldLength, ptrauth_key_process_dependent_data, newLength);
}

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline constexpr T removeCodePtrTag(PtrType ptr)
{
    return bitwise_cast<T>(ptrauth_strip(ptr, ptrauth_key_process_dependent_code));
}

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline constexpr PtrType removeCodePtrTag(PtrType ptr)
{
    return ptrauth_strip(ptr, ptrauth_key_process_dependent_code);
}

template<PtrTagAction tagAction, typename PtrType>
inline PtrType tagCodePtrImpl(PtrType ptr, PtrTag tag)
{
    if (!ptr)
        return nullptr;
    WTF_PTRTAG_ASSERT(tagAction, ptr, NoPtrTag, removeCodePtrTag(ptr) == ptr);
    if (tag == NoPtrTag)
        return ptr;
    if (tag == CFunctionPtrTag)
        return ptrauth_sign_unauthenticated(ptr, ptrauth_key_function_pointer, 0);
    return ptrauth_sign_unauthenticated(ptr, ptrauth_key_process_dependent_code, tag);
}

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline T tagCodePtr(PtrType ptr, PtrTag tag)
{
    return bitwise_cast<T>(tagCodePtrImpl<PtrTagAction::DebugAssert>(ptr, tag));
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T tagCodePtr(PtrType ptr)
{
    return bitwise_cast<T>(tagCodePtrImpl<PtrTagAction::DebugAssert>(ptr, tag));
}

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCodePtr(PtrType ptr, PtrTag tag)
{
    return tagCodePtrImpl<PtrTagAction::DebugAssert>(ptr, tag);
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCodePtr(PtrType ptr) { return tagCodePtr(ptr, tag); }

template<typename PtrType>
inline PtrType untagCodePtrImplHelper(PtrType ptr, PtrTag tag)
{
    if (tag == NoPtrTag)
        return ptr;
    if (tag == CFunctionPtrTag)
        return __builtin_ptrauth_auth(ptr, ptrauth_key_function_pointer, 0);
    return __builtin_ptrauth_auth(ptr, ptrauth_key_process_dependent_code, tag);
}

template<PtrTagAction tagAction, typename PtrType>
inline PtrType untagCodePtrImpl(PtrType ptr, PtrTag tag)
{
    if (!ptr)
        return nullptr;
    PtrType result = untagCodePtrImplHelper(ptr, tag);
    WTF_PTRTAG_ASSERT(tagAction, ptr, tag, removeCodePtrTag(ptr) == result);
    return result;
}

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline T untagCodePtr(PtrType ptr, PtrTag tag)
{
    return bitwise_cast<T>(untagCodePtrImpl<PtrTagAction::ReleaseAssert>(ptr, tag));
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCodePtr(PtrType ptr)
{
    return bitwise_cast<T>(untagCodePtrImpl<PtrTagAction::ReleaseAssert>(ptr, tag));
}

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCodePtr(PtrType ptr, PtrTag tag)
{
    return untagCodePtrImpl<PtrTagAction::ReleaseAssert>(ptr, tag);
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCodePtr(PtrType ptr) { return untagCodePtr(ptr, tag); }

template<PtrTagAction tagAction, typename PtrType>
inline PtrType retagCodePtrImplHelper(PtrType ptr, PtrTag oldTag, PtrTag newTag)
{
    if (oldTag == newTag || (oldTag == NoPtrTag && newTag == NoPtrTag))
        return ptr;
    if (newTag == NoPtrTag)
        return untagCodePtrImpl<tagAction>(ptr, oldTag);
    if (oldTag == NoPtrTag)
        return tagCodePtrImpl<tagAction>(ptr, newTag);
    if (oldTag == CFunctionPtrTag)
        return ptrauth_auth_and_resign(ptr, ptrauth_key_function_pointer, 0, ptrauth_key_process_dependent_code, newTag);
    if (newTag == CFunctionPtrTag)
        return ptrauth_auth_and_resign(ptr, ptrauth_key_process_dependent_code, oldTag, ptrauth_key_function_pointer, 0);
    return ptrauth_auth_and_resign(ptr, ptrauth_key_process_dependent_code, oldTag, ptrauth_key_process_dependent_code, newTag);
}

template<PtrTagAction tagAction, typename PtrType>
inline PtrType retagCodePtrImpl(PtrType ptr, PtrTag oldTag, PtrTag newTag)
{
    if (!ptr)
        return nullptr;
    PtrTagAction untagAction = (tagAction == PtrTagAction::NoAssert) ? PtrTagAction::NoAssert : PtrTagAction::ReleaseAssert;
    WTF_PTRTAG_ASSERT(untagAction, ptr, oldTag, removeCodePtrTag(ptr) == untagCodePtrImpl<PtrTagAction::NoAssert>(ptr, oldTag));
    PtrType result = retagCodePtrImplHelper<tagAction>(ptr, oldTag, newTag);
    WTF_PTRTAG_ASSERT(tagAction, ptr, newTag, result == tagCodePtrImpl<PtrTagAction::NoAssert>(removeCodePtrTag(ptr), newTag));
    return result;
}

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline T retagCodePtr(PtrType ptr, PtrTag oldTag, PtrTag newTag)
{
    return bitwise_cast<T>(retagCodePtrImpl<PtrTagAction::DebugAssert>(ptr, oldTag, newTag));
}

template<typename T, PtrTag oldTag, PtrTag newTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T retagCodePtr(PtrType ptr)
{
    return bitwise_cast<T>(retagCodePtrImpl<PtrTagAction::DebugAssert>(ptr, oldTag, newTag));
}

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType retagCodePtr(PtrType ptr, PtrTag oldTag, PtrTag newTag)
{
    return retagCodePtrImpl<PtrTagAction::DebugAssert>(ptr, oldTag, newTag);
}

template<PtrTag oldTag, PtrTag newTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType retagCodePtr(PtrType ptr) { return retagCodePtr(ptr, oldTag, newTag); }

template<PtrTagAction tagAction, typename PtrType>
inline PtrType tagCFunctionPtrImpl(PtrType ptr, PtrTag tag)
{
    if (!ptr)
        return nullptr;
    WTF_PTRTAG_ASSERT(tagAction, ptr, CFunctionPtrTag, removeCodePtrTag(ptr) == untagCodePtrImpl<PtrTagAction::NoAssert>(ptr, CFunctionPtrTag));
    return retagCodePtrImpl<tagAction>(ptr, CFunctionPtrTag, tag);
}

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline T tagCFunctionPtr(PtrType ptr, PtrTag tag)
{
    return bitwise_cast<T>(tagCFunctionPtrImpl<PtrTagAction::DebugAssert>(ptr, tag));
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T tagCFunctionPtr(PtrType ptr)
{
    return bitwise_cast<T>(tagCFunctionPtrImpl<PtrTagAction::DebugAssert>(ptr, tag));
}

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCFunctionPtr(PtrType ptr, PtrTag tag)
{
    return tagCFunctionPtrImpl<PtrTagAction::DebugAssert>(ptr, tag);
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCFunctionPtr(PtrType ptr) { return tagCFunctionPtr(ptr, tag); }

template<PtrTagAction tagAction, typename PtrType>
inline PtrType untagCFunctionPtrImpl(PtrType ptr, PtrTag tag)
{
    if (!ptr)
        return nullptr;
    WTF_PTRTAG_ASSERT(tagAction, ptr, tag, removeCodePtrTag(ptr) == untagCodePtrImpl<PtrTagAction::NoAssert>(ptr, tag));
    return retagCodePtrImpl<tagAction>(ptr, tag, CFunctionPtrTag);
}

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline T untagCFunctionPtr(PtrType ptr, PtrTag tag)
{
    return bitwise_cast<T>(untagCFunctionPtrImpl<PtrTagAction::DebugAssert>(ptr, tag));
}

template<typename T, PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCFunctionPtr(PtrType ptr)
{
    return bitwise_cast<T>(untagCFunctionPtrImpl<PtrTagAction::DebugAssert>(ptr, tag));
}

template<typename T, PtrTag tag, PtrTagAction tagAction, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCFunctionPtr(PtrType ptr)
{
    return bitwise_cast<T>(untagCFunctionPtrImpl<tagAction>(ptr, tag));
}

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCFunctionPtr(PtrType ptr, PtrTag tag)
{
    return untagCFunctionPtrImpl<PtrTagAction::DebugAssert>(ptr, tag);
}

template<PtrTag tag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCFunctionPtr(PtrType ptr) { return untagCFunctionPtr(ptr, tag); }

template <typename IntType>
inline IntType tagInt(IntType ptrInt, PtrTag tag)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t), "");
    return bitwise_cast<IntType>(ptrauth_sign_unauthenticated(bitwise_cast<void*>(ptrInt), ptrauth_key_process_dependent_data, tag));
}

template<typename PtrType>
void assertIsCFunctionPtr(PtrType value)
{
    void* ptr = bitwise_cast<void*>(value);
    WTF_PTRTAG_ASSERT(PtrTagAction::ReleaseAssert, ptr, CFunctionPtrTag, untagCodePtrImpl<PtrTagAction::NoAssert>(ptr, CFunctionPtrTag) == removeCodePtrTag(ptr));
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
    WTF_PTRTAG_ASSERT(PtrTagAction::ReleaseAssert, ptr, NoPtrTag, ptr == removeCodePtrTag(ptr));
}

template<typename PtrType>
void assertIsTagged(PtrType value)
{
    void* ptr = bitwise_cast<void*>(value);
    WTF_PTRTAG_ASSERT(PtrTagAction::ReleaseAssert, ptr, AnyPtrTag, ptr != removeCodePtrTag(ptr));
}

template<typename PtrType>
void assertIsNullOrTagged(PtrType ptr)
{
    if (ptr)
        assertIsTagged(ptr);
}

template<typename PtrType>
bool isTaggedWith(PtrType value, PtrTag tag)
{
    void* ptr = bitwise_cast<void*>(value);
    if (tag == NoPtrTag)
        return ptr == removeCodePtrTag(ptr);
    return untagCodePtrImpl<PtrTagAction::NoAssert>(ptr, tag) == removeCodePtrTag(ptr);
}

template<typename PtrType>
void assertIsTaggedWith(PtrType value, PtrTag tag)
{
    WTF_PTRTAG_ASSERT(PtrTagAction::ReleaseAssert, value, tag, isTaggedWith(value, tag));
}

template<typename PtrType>
void assertIsNullOrTaggedWith(PtrType ptr, PtrTag tag)
{
    if (ptr)
        assertIsTaggedWith(ptr, tag);
}

inline bool usesPointerTagging() { return true; }

// vtbl function pointers need to sign with ptrauth_key_process_independent_code
// because they reside in library code shared by multiple processes.
// The second argument to __ptrauth() being 1 means to use the address of the pointer
// for diversification as well. __ptrauth() expects a literal int for this argument.
#define WTF_VTBL_FUNCPTR_PTRAUTH(discriminator) WTF_VTBL_FUNCPTR_PTRAUTH_STR(#discriminator)
#define WTF_VTBL_FUNCPTR_PTRAUTH_STR(discriminatorStr) \
    __ptrauth(ptrauth_key_process_independent_code, 1, ptrauth_string_discriminator(discriminatorStr))

#else // not CPU(ARM64E)

inline void registerPtrTagLookup(PtrTagLookup*) { }
inline void reportBadTag(const void*, PtrTag) { }

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


template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
constexpr T tagCodePtr(PtrType ptr, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename T, PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T tagCodePtr(PtrType ptr) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
constexpr PtrType tagCodePtr(PtrType ptr, PtrTag) { return ptr; }

template<PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCodePtr(PtrType ptr) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
constexpr T untagCodePtr(PtrType ptr, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename T, PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCodePtr(PtrType ptr)  { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
constexpr PtrType untagCodePtr(PtrType ptr, PtrTag) { return ptr; }

template<PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCodePtr(PtrType ptr) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
constexpr T retagCodePtr(PtrType ptr, PtrTag, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename T, PtrTag, PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T retagCodePtr(PtrType ptr) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
constexpr PtrType retagCodePtr(PtrType ptr, PtrTag, PtrTag) { return ptr; }

template<PtrTag, PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType retagCodePtr(PtrType ptr) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
constexpr T removeCodePtrTag(PtrType ptr) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
constexpr PtrType removeCodePtrTag(PtrType ptr) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline T tagCFunctionPtr(PtrType ptr, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename T, PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T tagCFunctionPtr(PtrType ptr) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCFunctionPtr(PtrType ptr, PtrTag) { return ptr; }

template<PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType tagCFunctionPtr(PtrType ptr) { return ptr; }

template<typename T, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value && !std::is_same<T, PtrType>::value>>
inline T untagCFunctionPtr(PtrType ptr, PtrTag) { return bitwise_cast<T>(ptr); }

template<typename T, PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline T untagCFunctionPtr(PtrType ptr) { return bitwise_cast<T>(ptr); }

template<typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCFunctionPtr(PtrType ptr, PtrTag) { return ptr; }

template<PtrTag, typename PtrType, typename = std::enable_if_t<std::is_pointer<PtrType>::value>>
inline PtrType untagCFunctionPtr(PtrType ptr) { return ptr; }

template <typename IntType>
inline IntType tagInt(IntType ptrInt, PtrTag)
{
    static_assert(sizeof(IntType) == sizeof(uintptr_t), "");
    return ptrInt;
}

template<typename PtrType> void assertIsCFunctionPtr(PtrType) { }
template<typename PtrType> void assertIsNullOrCFunctionPtr(PtrType) { }

template<typename PtrType> void assertIsNotTagged(PtrType) { }
template<typename PtrType> void assertIsTagged(PtrType) { }
template<typename PtrType> void assertIsNullOrTagged(PtrType) { }

template<typename PtrType> bool isTaggedWith(PtrType, PtrTag) { return false; }

template<typename PtrType> void assertIsTaggedWith(PtrType, PtrTag) { }
template<typename PtrType> void assertIsNullOrTaggedWith(PtrType, PtrTag) { }

inline bool usesPointerTagging() { return false; }

#define WTF_VTBL_FUNCPTR_PTRAUTH(discriminator)
#define WTF_VTBL_FUNCPTR_PTRAUTH_STR(discriminatorStr)

#endif // CPU(ARM64E)

} // namespace WTF

using WTF::CFunctionPtrTag;
using WTF::NoPtrTag;
using WTF::PlatformRegistersLRPtrTag;
using WTF::PlatformRegistersPCPtrTag;
using WTF::PtrTag;

using WTF::reportBadTag;

using WTF::tagArrayPtr;
using WTF::untagArrayPtr;
using WTF::retagArrayPtr;
using WTF::removeArrayPtrTag;

using WTF::tagCodePtr;
using WTF::untagCodePtr;
using WTF::retagCodePtr;
using WTF::removeCodePtrTag;
using WTF::tagCFunctionPtr;
using WTF::untagCFunctionPtr;
using WTF::tagInt;

using WTF::assertIsCFunctionPtr;
using WTF::assertIsNullOrCFunctionPtr;
using WTF::assertIsNotTagged;
using WTF::assertIsTagged;
using WTF::assertIsNullOrTagged;
using WTF::isTaggedWith;
using WTF::assertIsTaggedWith;
using WTF::assertIsNullOrTaggedWith;
using WTF::usesPointerTagging;
