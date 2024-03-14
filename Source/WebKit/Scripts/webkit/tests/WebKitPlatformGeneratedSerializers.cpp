/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "GeneratedWebKitSecureCoding.h"

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
template<> struct VirtualTableAndRefCountOverhead<true, true> : public RefCounted<VirtualTableAndRefCountOverhead<true, true>> {
    virtual ~VirtualTableAndRefCountOverhead() { }
};
template<> struct VirtualTableAndRefCountOverhead<false, true> : public RefCounted<VirtualTableAndRefCountOverhead<false, true>> { };
template<> struct VirtualTableAndRefCountOverhead<true, false> {
    virtual ~VirtualTableAndRefCountOverhead() { }
};
template<> struct VirtualTableAndRefCountOverhead<false, false> { };

IGNORE_WARNINGS_BEGIN("invalid-offsetof")

namespace IPC {

#if USE(PASSKIT)
template<> void encodeObjectDirectly<PKPaymentMethod>(IPC::Encoder& encoder, PKPaymentMethod *instance)
{
    encoder << (instance ? std::optional(WebKit::CoreIPCPKPaymentMethod(instance)) : std::nullopt);
}

template<> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses<PKPaymentMethod>(IPC::Decoder& decoder)
{
    auto result = decoder.decode<std::optional<WebKit::CoreIPCPKPaymentMethod>>();
    if (!result)
        return std::nullopt;
    return *result ? (*result)->toID() : nullptr;
}
#endif // USE(PASSKIT)

template<> void encodeObjectDirectly<NSNull>(IPC::Encoder& encoder, NSNull *instance)
{
    encoder << (instance ? std::optional(WebKit::CoreIPCNull(instance)) : std::nullopt);
}

template<> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses<NSNull>(IPC::Decoder& decoder)
{
    auto result = decoder.decode<std::optional<WebKit::CoreIPCNull>>();
    if (!result)
        return std::nullopt;
    return *result ? (*result)->toID() : nullptr;
}

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

#if USE(AVFOUNDATION)
void ArgumentCoder<WebKit::CoreIPCAVOutputContext>::encode(Encoder& encoder, const WebKit::CoreIPCAVOutputContext& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_AVOutputContextSerializationKeyContextID)>, RetainPtr<NSString>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_AVOutputContextSerializationKeyContextType)>, RetainPtr<NSString>>);
    struct ShouldBeSameSizeAsAVOutputContext : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::CoreIPCAVOutputContext>, false> {
        RetainPtr<NSString> AVOutputContextSerializationKeyContextID;
        RetainPtr<NSString> AVOutputContextSerializationKeyContextType;
    };
    static_assert(sizeof(ShouldBeSameSizeAsAVOutputContext) == sizeof(WebKit::CoreIPCAVOutputContext));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::CoreIPCAVOutputContext, m_AVOutputContextSerializationKeyContextID)
        , offsetof(WebKit::CoreIPCAVOutputContext, m_AVOutputContextSerializationKeyContextType)
    >::value);

    encoder << instance.m_AVOutputContextSerializationKeyContextID;
    encoder << instance.m_AVOutputContextSerializationKeyContextType;
}

std::optional<WebKit::CoreIPCAVOutputContext> ArgumentCoder<WebKit::CoreIPCAVOutputContext>::decode(Decoder& decoder)
{
    auto AVOutputContextSerializationKeyContextID = decoder.decode<RetainPtr<NSString>>();
    if (!AVOutputContextSerializationKeyContextID)
        return std::nullopt;

    auto AVOutputContextSerializationKeyContextType = decoder.decode<RetainPtr<NSString>>();
    if (!AVOutputContextSerializationKeyContextType)
        return std::nullopt;

    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return {
        WebKit::CoreIPCAVOutputContext {
            WTFMove(*AVOutputContextSerializationKeyContextID),
            WTFMove(*AVOutputContextSerializationKeyContextType)
        }
    };
}

#endif

void ArgumentCoder<WebKit::CoreIPCNSSomeFoundationType>::encode(Encoder& encoder, const WebKit::CoreIPCNSSomeFoundationType& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_StringKey)>, RetainPtr<NSString>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_NumberKey)>, RetainPtr<NSNumber>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_OptionalNumberKey)>, RetainPtr<NSNumber>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_ArrayKey)>, RetainPtr<NSArray>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_OptionalArrayKey)>, RetainPtr<NSArray>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_DictionaryKey)>, RetainPtr<NSDictionary>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_OptionalDictionaryKey)>, RetainPtr<NSDictionary>>);
    struct ShouldBeSameSizeAsNSSomeFoundationType : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::CoreIPCNSSomeFoundationType>, false> {
        RetainPtr<NSString> StringKey;
        RetainPtr<NSNumber> NumberKey;
        RetainPtr<NSNumber> OptionalNumberKey;
        RetainPtr<NSArray> ArrayKey;
        RetainPtr<NSArray> OptionalArrayKey;
        RetainPtr<NSDictionary> DictionaryKey;
        RetainPtr<NSDictionary> OptionalDictionaryKey;
    };
    static_assert(sizeof(ShouldBeSameSizeAsNSSomeFoundationType) == sizeof(WebKit::CoreIPCNSSomeFoundationType));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::CoreIPCNSSomeFoundationType, m_StringKey)
        , offsetof(WebKit::CoreIPCNSSomeFoundationType, m_NumberKey)
        , offsetof(WebKit::CoreIPCNSSomeFoundationType, m_OptionalNumberKey)
        , offsetof(WebKit::CoreIPCNSSomeFoundationType, m_ArrayKey)
        , offsetof(WebKit::CoreIPCNSSomeFoundationType, m_OptionalArrayKey)
        , offsetof(WebKit::CoreIPCNSSomeFoundationType, m_DictionaryKey)
        , offsetof(WebKit::CoreIPCNSSomeFoundationType, m_OptionalDictionaryKey)
    >::value);

    encoder << instance.m_StringKey;
    encoder << instance.m_NumberKey;
    encoder << instance.m_OptionalNumberKey;
    encoder << instance.m_ArrayKey;
    encoder << instance.m_OptionalArrayKey;
    encoder << instance.m_DictionaryKey;
    encoder << instance.m_OptionalDictionaryKey;
}

std::optional<WebKit::CoreIPCNSSomeFoundationType> ArgumentCoder<WebKit::CoreIPCNSSomeFoundationType>::decode(Decoder& decoder)
{
    auto StringKey = decoder.decode<RetainPtr<NSString>>();
    if (!StringKey)
        return std::nullopt;

    auto NumberKey = decoder.decode<RetainPtr<NSNumber>>();
    if (!NumberKey)
        return std::nullopt;

    auto OptionalNumberKey = decoder.decode<RetainPtr<NSNumber>>();
    if (!OptionalNumberKey)
        return std::nullopt;

    auto ArrayKey = decoder.decode<RetainPtr<NSArray>>();
    if (!ArrayKey)
        return std::nullopt;

    auto OptionalArrayKey = decoder.decode<RetainPtr<NSArray>>();
    if (!OptionalArrayKey)
        return std::nullopt;

    auto DictionaryKey = decoder.decode<RetainPtr<NSDictionary>>();
    if (!DictionaryKey)
        return std::nullopt;

    auto OptionalDictionaryKey = decoder.decode<RetainPtr<NSDictionary>>();
    if (!OptionalDictionaryKey)
        return std::nullopt;

    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return {
        WebKit::CoreIPCNSSomeFoundationType {
            WTFMove(*StringKey),
            WTFMove(*NumberKey),
            WTFMove(*OptionalNumberKey),
            WTFMove(*ArrayKey),
            WTFMove(*OptionalArrayKey),
            WTFMove(*DictionaryKey),
            WTFMove(*OptionalDictionaryKey)
        }
    };
}

#if ENABLE(DATA_DETECTION)
void ArgumentCoder<WebKit::CoreIPCDDScannerResult>::encode(Encoder& encoder, const WebKit::CoreIPCDDScannerResult& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_StringKey)>, RetainPtr<NSString>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_NumberKey)>, RetainPtr<NSNumber>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_OptionalNumberKey)>, RetainPtr<NSNumber>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_ArrayKey)>, Vector<RetainPtr<DDScannerResult>>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_OptionalArrayKey)>, std::optional<Vector<RetainPtr<DDScannerResult>>>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_DictionaryKey)>, Vector<std::pair<String, RetainPtr<Number>>>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_OptionalDictionaryKey)>, std::optional<Vector<std::pair<String, RetainPtr<DDScannerResult>>>>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_DataArrayKey)>, Vector<RetainPtr<NSData>>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_SecTrustArrayKey)>, Vector<RetainPtr<SecTrustRef>>>);
    struct ShouldBeSameSizeAsDDScannerResult : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::CoreIPCDDScannerResult>, false> {
        RetainPtr<NSString> StringKey;
        RetainPtr<NSNumber> NumberKey;
        RetainPtr<NSNumber> OptionalNumberKey;
        Vector<RetainPtr<DDScannerResult>> ArrayKey;
        std::optional<Vector<RetainPtr<DDScannerResult>>> OptionalArrayKey;
        Vector<std::pair<String, RetainPtr<Number>>> DictionaryKey;
        std::optional<Vector<std::pair<String, RetainPtr<DDScannerResult>>>> OptionalDictionaryKey;
        Vector<RetainPtr<NSData>> DataArrayKey;
        Vector<RetainPtr<SecTrustRef>> SecTrustArrayKey;
    };
    static_assert(sizeof(ShouldBeSameSizeAsDDScannerResult) == sizeof(WebKit::CoreIPCDDScannerResult));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::CoreIPCDDScannerResult, m_StringKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_NumberKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_OptionalNumberKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_ArrayKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_OptionalArrayKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_DictionaryKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_OptionalDictionaryKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_DataArrayKey)
        , offsetof(WebKit::CoreIPCDDScannerResult, m_SecTrustArrayKey)
    >::value);

    encoder << instance.m_StringKey;
    encoder << instance.m_NumberKey;
    encoder << instance.m_OptionalNumberKey;
    encoder << instance.m_ArrayKey;
    encoder << instance.m_OptionalArrayKey;
    encoder << instance.m_DictionaryKey;
    encoder << instance.m_OptionalDictionaryKey;
    encoder << instance.m_DataArrayKey;
    encoder << instance.m_SecTrustArrayKey;
}

std::optional<WebKit::CoreIPCDDScannerResult> ArgumentCoder<WebKit::CoreIPCDDScannerResult>::decode(Decoder& decoder)
{
    auto StringKey = decoder.decode<RetainPtr<NSString>>();
    if (!StringKey)
        return std::nullopt;

    auto NumberKey = decoder.decode<RetainPtr<NSNumber>>();
    if (!NumberKey)
        return std::nullopt;

    auto OptionalNumberKey = decoder.decode<RetainPtr<NSNumber>>();
    if (!OptionalNumberKey)
        return std::nullopt;

    auto ArrayKey = decoder.decode<Vector<RetainPtr<DDScannerResult>>>();
    if (!ArrayKey)
        return std::nullopt;

    auto OptionalArrayKey = decoder.decode<std::optional<Vector<RetainPtr<DDScannerResult>>>>();
    if (!OptionalArrayKey)
        return std::nullopt;

    auto DictionaryKey = decoder.decode<Vector<std::pair<String, RetainPtr<Number>>>>();
    if (!DictionaryKey)
        return std::nullopt;

    auto OptionalDictionaryKey = decoder.decode<std::optional<Vector<std::pair<String, RetainPtr<DDScannerResult>>>>>();
    if (!OptionalDictionaryKey)
        return std::nullopt;

    auto DataArrayKey = decoder.decode<Vector<RetainPtr<NSData>>>();
    if (!DataArrayKey)
        return std::nullopt;

    auto SecTrustArrayKey = decoder.decode<Vector<RetainPtr<SecTrustRef>>>();
    if (!SecTrustArrayKey)
        return std::nullopt;

    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return {
        WebKit::CoreIPCDDScannerResult {
            WTFMove(*StringKey),
            WTFMove(*NumberKey),
            WTFMove(*OptionalNumberKey),
            WTFMove(*ArrayKey),
            WTFMove(*OptionalArrayKey),
            WTFMove(*DictionaryKey),
            WTFMove(*OptionalDictionaryKey),
            WTFMove(*DataArrayKey),
            WTFMove(*SecTrustArrayKey)
        }
    };
}

#endif

#if USE(CFSTRING)
void ArgumentCoder<CFStringRef>::encode(Encoder& encoder, CFStringRef instance)
{
    encoder << WTF::String { instance };
}

void ArgumentCoder<CFStringRef>::encode(StreamConnectionEncoder& encoder, CFStringRef instance)
{
    encoder << WTF::String { instance };
}

std::optional<RetainPtr<CFStringRef>> ArgumentCoder<RetainPtr<CFStringRef>>::decode(Decoder& decoder)
{
    auto result = decoder.decode<WTF::String>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return result->createCFString();
}

#endif

} // namespace IPC

namespace WTF {

} // namespace WTF

IGNORE_WARNINGS_END
