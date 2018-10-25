/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include <wtf/PointerPreparations.h>

namespace WTF {

#define FOR_EACH_BASE_WTF_PTRTAG(v) \
    v(NoPtrTag) \
    v(CFunctionPtrTag) \

#define FOR_EACH_ADDITIONAL_WTF_PTRTAG(v) \
    v(FreeSpacePtrTag) \
    v(HandleMemoryPtrTag) \

#define FOR_EACH_WTF_PTRTAG(v) \
    FOR_EACH_BASE_WTF_PTRTAG(v) \
    FOR_EACH_ADDITIONAL_WTF_PTRTAG(v) \

enum PtrTag : uintptr_t {
    NoPtrTag,
    CFunctionPtrTag,
};

#ifndef WTF_PTRTAG_HASH
template<size_t N>
constexpr uintptr_t makePtrTagHash(const char (&str)[N])
{
    uintptr_t result = 134775813;
    for (size_t i = 0; i < N; ++i)
        result += ((result * str[i]) ^ (result >> 16));
    return result & 0xffff;
}

#define WTF_PTRTAG_HASH(tag) WTF::makePtrTagHash(#tag)
#endif

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

#if !USE(POINTER_PROFILING)

inline const char* tagForPtr(const void*) { return "<no tag>"; }

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

#define CALL_WITH_PTRTAG(callInstructionString, targetRegisterString, tag) \
    callInstructionString " " targetRegisterString "\n"

#endif // !USE(POINTER_PROFILING)

} // namespace WTF

using WTF::CFunctionPtrTag;
using WTF::NoPtrTag;
using WTF::PtrTag;

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/PtrTagSupport.h>)
#include <WebKitAdditions/PtrTagSupport.h>
#endif

using WTF::tagForPtr;

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
