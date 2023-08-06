/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeneratedSerializers.h"

#include "PlatformClass.h"

template<size_t...> struct MembersInCorrectOrder;
template<size_t onlyOffset> struct MembersInCorrectOrder<onlyOffset> {
    static constexpr bool value = true;
};
template<size_t firstOffset, size_t secondOffset, size_t... remainingOffsets> struct MembersInCorrectOrder<firstOffset, secondOffset, remainingOffsets...> {
    static constexpr bool value = firstOffset > secondOffset ? false : MembersInCorrectOrder<secondOffset, remainingOffsets...>::value;
};

template<uint64_t...> struct BitsInIncreasingOrder;
template<uint64_t onlyBit> struct BitsInIncreasingOrder<onlyBit> {
    static constexpr bool value = true;
};
template<uint64_t firstBit, uint64_t secondBit, uint64_t... remainingBits> struct BitsInIncreasingOrder<firstBit, secondBit, remainingBits...> {
    static constexpr bool value = firstBit == secondBit >> 1 && BitsInIncreasingOrder<secondBit, remainingBits...>::value;
};

template<bool, bool> struct VirtualTableAndRefCountOverhead;
template<> struct VirtualTableAndRefCountOverhead<true, true> {
    virtual ~VirtualTableAndRefCountOverhead() { }
    unsigned refCount;
#if ASSERT_ENABLED
    bool m_isOwnedByMainThread;
    bool m_areThreadingChecksEnabled;
#endif
#if CHECK_REF_COUNTED_LIFECYCLE
    bool m_deletionHasBegun;
    bool m_adoptionIsRequired;
#endif
};
template<> struct VirtualTableAndRefCountOverhead<false, true> {
    unsigned refCount;
#if ASSERT_ENABLED
    bool m_isOwnedByMainThread;
    bool m_areThreadingChecksEnabled;
#endif
#if CHECK_REF_COUNTED_LIFECYCLE
    bool m_deletionHasBegun;
    bool m_adoptionIsRequired;
#endif
};
template<> struct VirtualTableAndRefCountOverhead<true, false> {
    virtual ~VirtualTableAndRefCountOverhead() { }
};
template<> struct VirtualTableAndRefCountOverhead<false, false> { };

#if COMPILER(GCC)
IGNORE_WARNINGS_BEGIN("invalid-offsetof")
#endif

namespace IPC {

void ArgumentCoder<WebKit::PlatformClass>::encode(Encoder& encoder, const WebKit::PlatformClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, int>);
    struct ShouldBeSameSizeAsPlatformClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::PlatformClass>, false> {
        int value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsPlatformClass) == sizeof(WebKit::PlatformClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::PlatformClass, value)
    >::value);
    encoder << instance.value;
}

std::optional<WebKit::PlatformClass> ArgumentCoder<WebKit::PlatformClass>::decode(Decoder& decoder)
{
    auto value = decoder.decode<int>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return {
        WebKit::PlatformClass {
            WTFMove(*value)
        }
    };
}

} // namespace IPC

namespace WTF {

} // namespace WTF

#if COMPILER(GCC)
IGNORE_WARNINGS_END
#endif
