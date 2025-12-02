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

#include "CommonHeader.h"
#if ENABLE(TEST_FEATURE)
#include "CommonHeader.h"
#endif
#include "CustomEncoded.h"
#if ENABLE(TEST_FEATURE)
#include "FirstMemberType.h"
#endif
#include "FooWrapper.h"
#include "FormDataReference.h"
#include "HeaderWithoutCondition"
#include "LayerProperties.h"
#include "RValueWithFunctionCalls.h"
#include "RemoteVideoFrameIdentifier.h"
#if ENABLE(TEST_FEATURE)
#include "SecondMemberType.h"
#endif
#if ENABLE(TEST_FEATURE)
#include "StructHeader.h"
#endif
#include "TemplateTest.h"
#include <Namespace/EmptyConstructorStruct.h>
#include <Namespace/EmptyConstructorWithIf.h>
#if !(ENABLE(OUTER_CONDITION))
#include <Namespace/OtherOuterClass.h>
#endif
#if ENABLE(OUTER_CONDITION)
#include <Namespace/OuterClass.h>
#endif
#include <Namespace/ReturnRefClass.h>
#if USE(APPKIT)
#include <WebCore/AppKitControlSystemImage.h>
#endif
#include <WebCore/FloatBoxExtent.h>
#include <WebCore/InheritanceGrandchild.h>
#include <WebCore/InheritsFrom.h>
#include <WebCore/MoveOnlyBaseClass.h>
#include <WebCore/MoveOnlyDerivedClass.h>
#include <WebCore/OpaqueTypeObject.h>
#if USE(APPKIT)
#include <WebCore/ScrollbarTrackCornerSystemImageMac.h>
#endif
#include <WebCore/ScrollingStateFrameHostingNode.h>
#include <WebCore/ScrollingStateFrameHostingNodeWithStuffAfterTuple.h>
#include <WebCore/TimingFunction.h>
#if USE(AVFOUNDATION)
#include <pal/cocoa/AVFoundationSoftLink.h>
#endif
#if ENABLE(DATA_DETECTION)
#include <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif
#include <wtf/CreateUsingClass.h>
#include <wtf/Seconds.h>

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


template<> struct ArgumentCoder<Namespace::OtherClass> {
    static void encode(Encoder&, const Namespace::OtherClass&);
    static std::optional<Namespace::OtherClass> decode(Decoder&);
};

template<> struct ArgumentCoder<Namespace::ClassWithMemberPrecondition> {
    static void encode(Encoder&, const Namespace::ClassWithMemberPrecondition&);
    static std::optional<Namespace::ClassWithMemberPrecondition> decode(Decoder&);
};

#if ENABLE(TEST_FEATURE)
void ArgumentCoder<Namespace::Subnamespace::StructName>::encode(Encoder& encoder, const Namespace::Subnamespace::StructName& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.firstMemberName)>, FirstMemberType>);
#if ENABLE(SECOND_MEMBER)
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.secondMemberName)>, SecondMemberType>);
#endif
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.nullableTestMember)>, RetainPtr<CFTypeRef>>);
    struct ShouldBeSameSizeAsStructName : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::Subnamespace::StructName>, false> {
        FirstMemberType firstMemberName;
#if ENABLE(SECOND_MEMBER)
        SecondMemberType secondMemberName;
#endif
        RetainPtr<CFTypeRef> nullableTestMember;
    };
    static_assert(sizeof(ShouldBeSameSizeAsStructName) == sizeof(Namespace::Subnamespace::StructName));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::Subnamespace::StructName, firstMemberName)
#if ENABLE(SECOND_MEMBER)
        , offsetof(Namespace::Subnamespace::StructName, secondMemberName)
#endif
        , offsetof(Namespace::Subnamespace::StructName, nullableTestMember)
    >::value);

    encoder << instance.firstMemberName;
#if ENABLE(SECOND_MEMBER)
    encoder << instance.secondMemberName;
#endif
    encoder << instance.nullableTestMember;
}

void ArgumentCoder<Namespace::Subnamespace::StructName>::encode(OtherEncoder& encoder, const Namespace::Subnamespace::StructName& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.firstMemberName)>, FirstMemberType>);
#if ENABLE(SECOND_MEMBER)
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.secondMemberName)>, SecondMemberType>);
#endif
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.nullableTestMember)>, RetainPtr<CFTypeRef>>);
    struct ShouldBeSameSizeAsStructName : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::Subnamespace::StructName>, false> {
        FirstMemberType firstMemberName;
#if ENABLE(SECOND_MEMBER)
        SecondMemberType secondMemberName;
#endif
        RetainPtr<CFTypeRef> nullableTestMember;
    };
    static_assert(sizeof(ShouldBeSameSizeAsStructName) == sizeof(Namespace::Subnamespace::StructName));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::Subnamespace::StructName, firstMemberName)
#if ENABLE(SECOND_MEMBER)
        , offsetof(Namespace::Subnamespace::StructName, secondMemberName)
#endif
        , offsetof(Namespace::Subnamespace::StructName, nullableTestMember)
    >::value);

    encoder << instance.firstMemberName;
#if ENABLE(SECOND_MEMBER)
    encoder << instance.secondMemberName;
#endif
    encoder << instance.nullableTestMember;
}

std::optional<Namespace::Subnamespace::StructName> ArgumentCoder<Namespace::Subnamespace::StructName>::decode(Decoder& decoder)
{
    bool addedDecodingFailureIndex = false;
    auto firstMemberName = decoder.decode<FirstMemberType>();
    if (!firstMemberName && !addedDecodingFailureIndex) [[unlikely]] {
        decoder.addIndexOfDecodingFailure(0);
        addedDecodingFailureIndex = true;
    }
#if ENABLE(SECOND_MEMBER)
    auto secondMemberName = decoder.decode<SecondMemberType>();
    if (!secondMemberName && !addedDecodingFailureIndex) [[unlikely]] {
        decoder.addIndexOfDecodingFailure(1);
        addedDecodingFailureIndex = true;
    }
#endif
    auto nullableTestMember = decoder.decode<RetainPtr<CFTypeRef>>();
    if (!nullableTestMember && !addedDecodingFailureIndex) [[unlikely]] {
        decoder.addIndexOfDecodingFailure(2);
        addedDecodingFailureIndex = true;
    }
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::Subnamespace::StructName {
            WTFMove(*firstMemberName),
#if ENABLE(SECOND_MEMBER)
            WTFMove(*secondMemberName),
#endif
            WTFMove(*nullableTestMember)
        }
    };
}

#endif

void ArgumentCoder<Namespace::OtherClass>::encode(Encoder& encoder, const Namespace::OtherClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.b)>, bool>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.dataDetectorResults)>, RetainPtr<NSArray>>);
    struct ShouldBeSameSizeAsOtherClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::OtherClass>, false> {
        int a;
        bool b : 1;
        RetainPtr<NSArray> dataDetectorResults;
    };
    static_assert(sizeof(ShouldBeSameSizeAsOtherClass) == sizeof(Namespace::OtherClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::OtherClass, a)
        , offsetof(Namespace::OtherClass, dataDetectorResults)
    >::value);

    encoder << instance.a;
    encoder << instance.b;
    ASSERT(!isInWebProcess());
    encoder << instance.dataDetectorResults;
}

std::optional<Namespace::OtherClass> ArgumentCoder<Namespace::OtherClass>::decode(Decoder& decoder)
{
    auto a = decoder.decode<int>();
    auto b = decoder.decode<bool>();
    auto dataDetectorResults = decoder.decodeWithAllowedClasses<NSArray>({ NSArray.class, PAL::getDDScannerResultClassSingleton() });
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::OtherClass {
            WTFMove(*a),
            WTFMove(*b),
            WTFMove(*dataDetectorResults)
        }
    };
}

void ArgumentCoder<Namespace::ClassWithMemberPrecondition>::encode(Encoder& encoder, const Namespace::ClassWithMemberPrecondition& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_pkPaymentMethod)>, RetainPtr<PKPaymentMethod>>);
    struct ShouldBeSameSizeAsClassWithMemberPrecondition : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::ClassWithMemberPrecondition>, false> {
        RetainPtr<PKPaymentMethod> m_pkPaymentMethod;
    };
    static_assert(sizeof(ShouldBeSameSizeAsClassWithMemberPrecondition) == sizeof(Namespace::ClassWithMemberPrecondition));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::ClassWithMemberPrecondition, m_pkPaymentMethod)
    >::value);

    encoder << instance.m_pkPaymentMethod;
}

std::optional<Namespace::ClassWithMemberPrecondition> ArgumentCoder<Namespace::ClassWithMemberPrecondition>::decode(Decoder& decoder)
{
    if (!(PAL::isPassKitCoreFrameworkAvailable()))
        return std::nullopt;
    auto m_pkPaymentMethod = decoder.decodeWithAllowedClasses<PKPaymentMethod>({ PAL::getPKPaymentMethodClassSingleton() });
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::ClassWithMemberPrecondition {
            WTFMove(*m_pkPaymentMethod)
        }
    };
}

void ArgumentCoder<Namespace::ReturnRefClass>::encode(Encoder& encoder, const Namespace::ReturnRefClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.functionCall().member1)>, double>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.functionCall().member2)>, double>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.uniqueMember)>, std::unique_ptr<int>>);

    encoder << instance.functionCall().member1;
    encoder << instance.functionCall().member2;
    encoder << instance.uniqueMember;
}

std::optional<Ref<Namespace::ReturnRefClass>> ArgumentCoder<Namespace::ReturnRefClass>::decode(Decoder& decoder)
{
    auto functionCallmember1 = decoder.decode<double>();
    auto functionCallmember2 = decoder.decode<double>();
    auto uniqueMember = decoder.decode<std::unique_ptr<int>>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::ReturnRefClass::create(
            WTFMove(*functionCallmember1),
            WTFMove(*functionCallmember2),
            WTFMove(*uniqueMember)
        )
    };
}

void ArgumentCoder<Namespace::EmptyConstructorStruct>::encode(Encoder& encoder, const Namespace::EmptyConstructorStruct& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_int)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_double)>, double>);
    struct ShouldBeSameSizeAsEmptyConstructorStruct : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::EmptyConstructorStruct>, false> {
        int m_int;
        double m_double;
    };
    static_assert(sizeof(ShouldBeSameSizeAsEmptyConstructorStruct) == sizeof(Namespace::EmptyConstructorStruct));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::EmptyConstructorStruct, m_int)
        , offsetof(Namespace::EmptyConstructorStruct, m_double)
    >::value);

    encoder << instance.m_int;
    encoder << instance.m_double;
}

std::optional<Namespace::EmptyConstructorStruct> ArgumentCoder<Namespace::EmptyConstructorStruct>::decode(Decoder& decoder)
{
    auto m_int = decoder.decode<int>();
    auto m_double = decoder.decode<double>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    Namespace::EmptyConstructorStruct result;
    result.m_int = WTFMove(*m_int);
    result.m_double = WTFMove(*m_double);
    return { WTFMove(result) };
}

void ArgumentCoder<Namespace::EmptyConstructorWithIf>::encode(Encoder& encoder, const Namespace::EmptyConstructorWithIf& instance)
{
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_type)>, MemberType>);
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_value)>, OtherMemberType>);
#endif
    struct ShouldBeSameSizeAsEmptyConstructorWithIf : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::EmptyConstructorWithIf>, false> {
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
        MemberType m_type;
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
        OtherMemberType m_value;
#endif
    };
    static_assert(sizeof(ShouldBeSameSizeAsEmptyConstructorWithIf) == sizeof(Namespace::EmptyConstructorWithIf));
    static_assert(MembersInCorrectOrder < 0
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
        , offsetof(Namespace::EmptyConstructorWithIf, m_type)
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
        , offsetof(Namespace::EmptyConstructorWithIf, m_value)
#endif
    >::value);

#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    encoder << instance.m_type;
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    encoder << instance.m_value;
#endif
}

std::optional<Namespace::EmptyConstructorWithIf> ArgumentCoder<Namespace::EmptyConstructorWithIf>::decode(Decoder& decoder)
{
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    auto m_type = decoder.decode<MemberType>();
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    auto m_value = decoder.decode<OtherMemberType>();
#endif
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    Namespace::EmptyConstructorWithIf result;
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    result.m_type = WTFMove(*m_type);
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    result.m_value = WTFMove(*m_value);
#endif
    return { WTFMove(result) };
}

void ArgumentCoder<WithoutNamespace>::encode(Encoder& encoder, const WithoutNamespace& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    struct ShouldBeSameSizeAsWithoutNamespace : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WithoutNamespace>, false> {
        int a;
    };
    static_assert(sizeof(ShouldBeSameSizeAsWithoutNamespace) == sizeof(WithoutNamespace));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WithoutNamespace, a)
    >::value);

    encoder << instance.a;
}

std::optional<WithoutNamespace> ArgumentCoder<WithoutNamespace>::decode(Decoder& decoder)
{
    auto a = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WithoutNamespace {
            WTFMove(*a)
        }
    };
}

void ArgumentCoder<WithoutNamespaceWithAttributes>::encode(Encoder& encoder, const WithoutNamespaceWithAttributes& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    struct ShouldBeSameSizeAsWithoutNamespaceWithAttributes : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WithoutNamespaceWithAttributes>, false> {
        int a;
    };
    static_assert(sizeof(ShouldBeSameSizeAsWithoutNamespaceWithAttributes) == sizeof(WithoutNamespaceWithAttributes));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WithoutNamespaceWithAttributes, a)
    >::value);

    encoder << instance.a;
}

void ArgumentCoder<WithoutNamespaceWithAttributes>::encode(OtherEncoder& encoder, const WithoutNamespaceWithAttributes& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    struct ShouldBeSameSizeAsWithoutNamespaceWithAttributes : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WithoutNamespaceWithAttributes>, false> {
        int a;
    };
    static_assert(sizeof(ShouldBeSameSizeAsWithoutNamespaceWithAttributes) == sizeof(WithoutNamespaceWithAttributes));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WithoutNamespaceWithAttributes, a)
    >::value);

    encoder << instance.a;
}

std::optional<WithoutNamespaceWithAttributes> ArgumentCoder<WithoutNamespaceWithAttributes>::decode(Decoder& decoder)
{
    auto a = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WithoutNamespaceWithAttributes {
            WTFMove(*a)
        }
    };
}

void ArgumentCoder<WebCore::InheritsFrom>::encode(Encoder& encoder, const WebCore::InheritsFrom& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WithoutNamespace, a)
    >::value);

    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.b)>, float>);
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebCore::InheritsFrom, b)
    >::value);

    encoder << instance.a;
    encoder << instance.b;
}

std::optional<WebCore::InheritsFrom> ArgumentCoder<WebCore::InheritsFrom>::decode(Decoder& decoder)
{
    auto a = decoder.decode<int>();
    auto b = decoder.decode<float>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::InheritsFrom {
            WithoutNamespace {
                WTFMove(*a)
            },
            WTFMove(*b)
        }
    };
}

void ArgumentCoder<WebCore::InheritanceGrandchild>::encode(Encoder& encoder, const WebCore::InheritanceGrandchild& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WithoutNamespace, a)
    >::value);

    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.b)>, float>);
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebCore::InheritsFrom, b)
    >::value);

    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.c)>, double>);
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebCore::InheritanceGrandchild, c)
    >::value);

    encoder << instance.a;
    encoder << instance.b;
    encoder << instance.c;
}

std::optional<WebCore::InheritanceGrandchild> ArgumentCoder<WebCore::InheritanceGrandchild>::decode(Decoder& decoder)
{
    auto a = decoder.decode<int>();
    auto b = decoder.decode<float>();
    auto c = decoder.decode<double>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::InheritanceGrandchild {
            WebCore::InheritsFrom {
                WithoutNamespace {
                    WTFMove(*a)
                },
                WTFMove(*b)
            },
            WTFMove(*c)
        }
    };
}

void ArgumentCoder<WTF::Seconds>::encode(Encoder& encoder, const WTF::Seconds& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value())>, double>);

    encoder << instance.value();
}

std::optional<WTF::Seconds> ArgumentCoder<WTF::Seconds>::decode(Decoder& decoder)
{
    auto value = decoder.decode<double>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WTF::Seconds {
            WTFMove(*value)
        }
    };
}

void ArgumentCoder<WTF::CreateUsingClass>::encode(Encoder& encoder, const WTF::CreateUsingClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, double>);
    struct ShouldBeSameSizeAsCreateUsingClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WTF::CreateUsingClass>, false> {
        double value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsCreateUsingClass) == sizeof(WTF::CreateUsingClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WTF::CreateUsingClass, value)
    >::value);

    encoder << instance.value;
}

std::optional<WTF::CreateUsingClass> ArgumentCoder<WTF::CreateUsingClass>::decode(Decoder& decoder)
{
    auto value = decoder.decode<double>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WTF::CreateUsingClass::fromDouble(
            WTFMove(*value)
        )
    };
}

void ArgumentCoder<WebCore::FloatBoxExtent>::encode(Encoder& encoder, const WebCore::FloatBoxExtent& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.top())>, float>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.right())>, float>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.bottom())>, float>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.left())>, float>);

    encoder << instance.top();
    encoder << instance.right();
    encoder << instance.bottom();
    encoder << instance.left();
}

std::optional<WebCore::FloatBoxExtent> ArgumentCoder<WebCore::FloatBoxExtent>::decode(Decoder& decoder)
{
    auto top = decoder.decode<float>();
    auto right = decoder.decode<float>();
    auto bottom = decoder.decode<float>();
    auto left = decoder.decode<float>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::FloatBoxExtent {
            WTFMove(*top),
            WTFMove(*right),
            WTFMove(*bottom),
            WTFMove(*left)
        }
    };
}

void ArgumentCoder<SoftLinkedMember>::encode(Encoder& encoder, const SoftLinkedMember& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.firstMember)>, RetainPtr<DDActionContext>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.secondMember)>, RetainPtr<DDActionContext>>);
    struct ShouldBeSameSizeAsSoftLinkedMember : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<SoftLinkedMember>, false> {
        RetainPtr<DDActionContext> firstMember;
        RetainPtr<DDActionContext> secondMember;
    };
    static_assert(sizeof(ShouldBeSameSizeAsSoftLinkedMember) == sizeof(SoftLinkedMember));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(SoftLinkedMember, firstMember)
        , offsetof(SoftLinkedMember, secondMember)
    >::value);

    encoder << instance.firstMember;
    encoder << instance.secondMember;
}

std::optional<SoftLinkedMember> ArgumentCoder<SoftLinkedMember>::decode(Decoder& decoder)
{
    auto firstMember = decoder.decodeWithAllowedClasses<DDActionContext>({ PAL::getDDActionContextClassSingleton() });
    auto secondMember = decoder.decodeWithAllowedClasses<DDActionContext>({ PAL::getDDActionContextClassSingleton() });
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        SoftLinkedMember {
            WTFMove(*firstMember),
            WTFMove(*secondMember)
        }
    };
}

enum class WebCore_TimingFunction_Subclass : IPC::EncodedVariantIndex {
    LinearTimingFunction
    , CubicBezierTimingFunction
#if CONDITION
    , StepsTimingFunction
#endif
    , SpringTimingFunction
};

IGNORE_WARNINGS_BEGIN("missing-noreturn")
void ArgumentCoder<WebCore::TimingFunction>::encode(Encoder& encoder, const WebCore::TimingFunction& instance)
{
    if (auto* subclass = dynamicDowncast<WebCore::LinearTimingFunction>(instance)) {
        encoder << WebCore_TimingFunction_Subclass::LinearTimingFunction;
        encoder << *subclass;
        return;
    }
    if (auto* subclass = dynamicDowncast<WebCore::CubicBezierTimingFunction>(instance)) {
        encoder << WebCore_TimingFunction_Subclass::CubicBezierTimingFunction;
        encoder << *subclass;
        return;
    }
#if CONDITION
    if (auto* subclass = dynamicDowncast<WebCore::StepsTimingFunction>(instance)) {
        encoder << WebCore_TimingFunction_Subclass::StepsTimingFunction;
        encoder << *subclass;
        return;
    }
#endif
    if (auto* subclass = dynamicDowncast<WebCore::SpringTimingFunction>(instance)) {
        encoder << WebCore_TimingFunction_Subclass::SpringTimingFunction;
        encoder << *subclass;
        return;
    }
    ASSERT_NOT_REACHED();
}
IGNORE_WARNINGS_END

std::optional<Ref<WebCore::TimingFunction>> ArgumentCoder<WebCore::TimingFunction>::decode(Decoder& decoder)
{
    auto type = decoder.decode<WebCore_TimingFunction_Subclass>();
    UNUSED_PARAM(type);
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;

    if (type == WebCore_TimingFunction_Subclass::LinearTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::LinearTimingFunction>>();
        if (!decoder.isValid()) [[unlikely]]
            return std::nullopt;
        return WTFMove(*result);
    }
    if (type == WebCore_TimingFunction_Subclass::CubicBezierTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::CubicBezierTimingFunction>>();
        if (!decoder.isValid()) [[unlikely]]
            return std::nullopt;
        return WTFMove(*result);
    }
#if CONDITION
    if (type == WebCore_TimingFunction_Subclass::StepsTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::StepsTimingFunction>>();
        if (!decoder.isValid()) [[unlikely]]
            return std::nullopt;
        return WTFMove(*result);
    }
#endif
    if (type == WebCore_TimingFunction_Subclass::SpringTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::SpringTimingFunction>>();
        if (!decoder.isValid()) [[unlikely]]
            return std::nullopt;
        return WTFMove(*result);
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

#if ENABLE(TEST_FEATURE)
void ArgumentCoder<Namespace::ConditionalCommonClass>::encode(Encoder& encoder, const Namespace::ConditionalCommonClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, int>);
    struct ShouldBeSameSizeAsConditionalCommonClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::ConditionalCommonClass>, false> {
        int value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsConditionalCommonClass) == sizeof(Namespace::ConditionalCommonClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::ConditionalCommonClass, value)
    >::value);

    encoder << instance.value;
}

std::optional<Namespace::ConditionalCommonClass> ArgumentCoder<Namespace::ConditionalCommonClass>::decode(Decoder& decoder)
{
    auto value = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::ConditionalCommonClass {
            WTFMove(*value)
        }
    };
}

#endif

void ArgumentCoder<Namespace::CommonClass>::encode(Encoder& encoder, const Namespace::CommonClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.nonRefMemberWithSubclasses)>, WebCore::TimingFunction>);
    struct ShouldBeSameSizeAsCommonClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::CommonClass>, false> {
        int value;
        WebCore::TimingFunction nonRefMemberWithSubclasses;
    };
    static_assert(sizeof(ShouldBeSameSizeAsCommonClass) == sizeof(Namespace::CommonClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::CommonClass, value)
        , offsetof(Namespace::CommonClass, nonRefMemberWithSubclasses)
    >::value);

    encoder << instance.value;
    encoder << instance.nonRefMemberWithSubclasses;
}

std::optional<Namespace::CommonClass> ArgumentCoder<Namespace::CommonClass>::decode(Decoder& decoder)
{
    auto value = decoder.decode<int>();
    auto nonRefMemberWithSubclasses = decoder.decode<Ref<WebCore::TimingFunction>>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::CommonClass {
            WTFMove(*value),
            WTFMove(*nonRefMemberWithSubclasses)
        }
    };
}

void ArgumentCoder<Namespace::AnotherCommonClass>::encode(Encoder& encoder, const Namespace::AnotherCommonClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.notSerialized)>, double>);
    struct ShouldBeSameSizeAsAnotherCommonClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::AnotherCommonClass>, true> {
        int value;
        double notSerialized;
    };
    static_assert(sizeof(ShouldBeSameSizeAsAnotherCommonClass) == sizeof(Namespace::AnotherCommonClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::AnotherCommonClass, value)
        , offsetof(Namespace::AnotherCommonClass, notSerialized)
    >::value);

    encoder << instance.value;
}

std::optional<Ref<Namespace::AnotherCommonClass>> ArgumentCoder<Namespace::AnotherCommonClass>::decode(Decoder& decoder)
{
    auto value = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::AnotherCommonClass::create(
            WTFMove(*value)
        )
    };
}

enum class WebCore_MoveOnlyBaseClass_Subclass : IPC::EncodedVariantIndex {
    MoveOnlyDerivedClass
};

IGNORE_WARNINGS_BEGIN("missing-noreturn")
void ArgumentCoder<WebCore::MoveOnlyBaseClass>::encode(Encoder& encoder, WebCore::MoveOnlyBaseClass&& instance)
{
    if (auto* subclass = dynamicDowncast<WebCore::MoveOnlyDerivedClass>(instance)) {
        encoder << WebCore_MoveOnlyBaseClass_Subclass::MoveOnlyDerivedClass;
        encoder << WTFMove(*subclass);
        return;
    }
    ASSERT_NOT_REACHED();
}
IGNORE_WARNINGS_END

std::optional<WebCore::MoveOnlyBaseClass> ArgumentCoder<WebCore::MoveOnlyBaseClass>::decode(Decoder& decoder)
{
    auto type = decoder.decode<WebCore_MoveOnlyBaseClass_Subclass>();
    UNUSED_PARAM(type);
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;

    if (type == WebCore_MoveOnlyBaseClass_Subclass::MoveOnlyDerivedClass) {
        auto result = decoder.decode<Ref<WebCore::MoveOnlyDerivedClass>>();
        if (!decoder.isValid()) [[unlikely]]
            return std::nullopt;
        return WTFMove(*result);
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

void ArgumentCoder<WebCore::MoveOnlyDerivedClass>::encode(Encoder& encoder, WebCore::MoveOnlyDerivedClass&& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.firstMember)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.secondMember)>, int>);
    struct ShouldBeSameSizeAsMoveOnlyDerivedClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebCore::MoveOnlyDerivedClass>, false> {
        int firstMember;
        int secondMember;
    };
    static_assert(sizeof(ShouldBeSameSizeAsMoveOnlyDerivedClass) == sizeof(WebCore::MoveOnlyDerivedClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebCore::MoveOnlyDerivedClass, firstMember)
        , offsetof(WebCore::MoveOnlyDerivedClass, secondMember)
    >::value);

    encoder << WTFMove(instance.firstMember);
    encoder << WTFMove(instance.secondMember);
}

std::optional<WebCore::MoveOnlyDerivedClass> ArgumentCoder<WebCore::MoveOnlyDerivedClass>::decode(Decoder& decoder)
{
    auto firstMember = decoder.decode<int>();
    auto secondMember = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::MoveOnlyDerivedClass {
            WTFMove(*firstMember),
            WTFMove(*secondMember)
        }
    };
}

std::optional<WebKit::CustomEncoded> ArgumentCoder<WebKit::CustomEncoded>::decode(Decoder& decoder)
{
    auto value = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::CustomEncoded {
            WTFMove(*value)
        }
    };
}

void ArgumentCoder<WebKit::LayerProperties>::encode(Encoder& encoder, const WebKit::LayerProperties& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.changedProperties)>, OptionSet<WebKit::LayerChange>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.nonSerializedMember)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.name)>, String>);
#if ENABLE(FEATURE)
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.featureEnabledMember)>, std::unique_ptr<WebCore::TransformationMatrix>>);
#endif
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.bitFieldMember)>, bool>);
    struct ShouldBeSameSizeAsLayerProperties : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::LayerProperties>, false> {
        OptionSet<WebKit::LayerChange> changedProperties;
        int nonSerializedMember;
        String name;
#if ENABLE(FEATURE)
        std::unique_ptr<WebCore::TransformationMatrix> featureEnabledMember;
#endif
        bool bitFieldMember : 1;
    };
    static_assert(sizeof(ShouldBeSameSizeAsLayerProperties) == sizeof(WebKit::LayerProperties));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::LayerProperties, changedProperties)
        , offsetof(WebKit::LayerProperties, nonSerializedMember)
        , offsetof(WebKit::LayerProperties, name)
#if ENABLE(FEATURE)
        , offsetof(WebKit::LayerProperties, featureEnabledMember)
#endif
    >::value);
    static_assert(static_cast<uint64_t>(WebKit::LayerChange::NameChanged) == 1);
    static_assert(BitsInIncreasingOrder<
        static_cast<uint64_t>(WebKit::LayerChange::NameChanged)
#if ENABLE(FEATURE)
        , static_cast<uint64_t>(WebKit::LayerChange::TransformChanged)
#endif
        , static_cast<uint64_t>(WebKit::LayerChange::FeatureEnabledMember)
    >::value);

    encoder << instance.changedProperties;
    if (instance.changedProperties & WebKit::LayerChange::NameChanged)
        encoder << instance.name;
#if ENABLE(FEATURE)
    if (instance.changedProperties & WebKit::LayerChange::TransformChanged)
        encoder << instance.featureEnabledMember;
#endif
    if (instance.changedProperties & WebKit::LayerChange::FeatureEnabledMember)
        encoder << instance.bitFieldMember;
}

std::optional<WebKit::LayerProperties> ArgumentCoder<WebKit::LayerProperties>::decode(Decoder& decoder)
{
    WebKit::LayerProperties result;
    auto changedProperties = decoder.decode<OptionSet<WebKit::LayerChange>>();
    if (!changedProperties)
        return std::nullopt;
    result.changedProperties = *changedProperties;
    if (*changedProperties & WebKit::LayerChange::NameChanged) {
        if (auto deserialized = decoder.decode<String>())
            result.name = WTFMove(*deserialized);
        else
            return std::nullopt;
    }
#if ENABLE(FEATURE)
    if (*changedProperties & WebKit::LayerChange::TransformChanged) {
        if (auto deserialized = decoder.decode<std::unique_ptr<WebCore::TransformationMatrix>>())
            result.featureEnabledMember = WTFMove(*deserialized);
        else
            return std::nullopt;
    }
#endif
    if (*changedProperties & WebKit::LayerChange::FeatureEnabledMember) {
        if (auto deserialized = decoder.decode<bool>())
            result.bitFieldMember = WTFMove(*deserialized);
        else
            return std::nullopt;
    }
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return { WTFMove(result) };
}

void ArgumentCoder<WebKit::TemplateTest<WebKit::Fabulous>>::encode(Encoder& encoder, const WebKit::TemplateTest<WebKit::Fabulous>& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, bool>);
    struct ShouldBeSameSizeAsTemplateTest : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::TemplateTest>, false> {
        bool value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsTemplateTest) == sizeof(WebKit::TemplateTest));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::TemplateTest, value)
    >::value);

    encoder << instance.value;
}

std::optional<WebKit::TemplateTest<WebKit::Fabulous>> ArgumentCoder<WebKit::TemplateTest<WebKit::Fabulous>>::decode(Decoder& decoder)
{
    auto value = decoder.decode<bool>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::TemplateTest<WebKit::Fabulous> {
            WTFMove(*value)
        }
    };
}

void ArgumentCoder<WebKit::TemplateTest<WebCore::Amazing>>::encode(Encoder& encoder, const WebKit::TemplateTest<WebCore::Amazing>& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, bool>);
    struct ShouldBeSameSizeAsTemplateTest : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::TemplateTest>, false> {
        bool value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsTemplateTest) == sizeof(WebKit::TemplateTest));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::TemplateTest, value)
    >::value);

    encoder << instance.value;
}

std::optional<WebKit::TemplateTest<WebCore::Amazing>> ArgumentCoder<WebKit::TemplateTest<WebCore::Amazing>>::decode(Decoder& decoder)
{
    auto value = decoder.decode<bool>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::TemplateTest<WebCore::Amazing> {
            WTFMove(*value)
        }
    };
}

void ArgumentCoder<WebKit::TemplateTest<JSC::Incredible>>::encode(Encoder& encoder, const WebKit::TemplateTest<JSC::Incredible>& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, bool>);
    struct ShouldBeSameSizeAsTemplateTest : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::TemplateTest>, false> {
        bool value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsTemplateTest) == sizeof(WebKit::TemplateTest));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::TemplateTest, value)
    >::value);

    encoder << instance.value;
}

std::optional<WebKit::TemplateTest<JSC::Incredible>> ArgumentCoder<WebKit::TemplateTest<JSC::Incredible>>::decode(Decoder& decoder)
{
    auto value = decoder.decode<bool>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::TemplateTest<JSC::Incredible> {
            WTFMove(*value)
        }
    };
}

void ArgumentCoder<WebKit::TemplateTest<Testing::StorageSize>>::encode(Encoder& encoder, const WebKit::TemplateTest<Testing::StorageSize>& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.value)>, bool>);
    struct ShouldBeSameSizeAsTemplateTest : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebKit::TemplateTest>, false> {
        bool value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsTemplateTest) == sizeof(WebKit::TemplateTest));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebKit::TemplateTest, value)
    >::value);

    encoder << instance.value;
}

std::optional<WebKit::TemplateTest<Testing::StorageSize>> ArgumentCoder<WebKit::TemplateTest<Testing::StorageSize>>::decode(Decoder& decoder)
{
    auto value = decoder.decode<bool>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::TemplateTest<Testing::StorageSize> {
            WTFMove(*value)
        }
    };
}

void ArgumentCoder<WebCore::ScrollingStateFrameHostingNode>::encode(Encoder& encoder, const WebCore::ScrollingStateFrameHostingNode& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.scrollingNodeID())>, WebCore::ScrollingNodeID>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.children())>, Vector<Ref<WebCore::ScrollingStateNode>>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.changedProperties())>, OptionSet<WebCore::ScrollingStateNodeProperty>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.layer().layerIDForEncoding())>, std::optional<WebCore::PlatformLayerIdentifier>>);
    static_assert(static_cast<uint64_t>(WebCore::ScrollingStateNode::Property::Layer) == 1);
    static_assert(BitsInIncreasingOrder<
        static_cast<uint64_t>(WebCore::ScrollingStateNode::Property::Layer)
    >::value);

    encoder << instance.scrollingNodeID();
    encoder << instance.children();
    encoder << instance.changedProperties();
    if (instance.changedProperties() & WebCore::ScrollingStateNode::Property::Layer)
        encoder << instance.layer().layerIDForEncoding();
}

std::optional<Ref<WebCore::ScrollingStateFrameHostingNode>> ArgumentCoder<WebCore::ScrollingStateFrameHostingNode>::decode(Decoder& decoder)
{
    auto scrollingNodeID = decoder.decode<WebCore::ScrollingNodeID>();
    auto children = decoder.decode<Vector<Ref<WebCore::ScrollingStateNode>>>();
    auto changedProperties = decoder.decode<OptionSet<WebCore::ScrollingStateNodeProperty>>();
    if (!changedProperties)
        return std::nullopt;

    std::optional<WebCore::PlatformLayerIdentifier> layerlayerIDForEncoding { };
    if (*changedProperties & WebCore::ScrollingStateNode::Property::Layer) {
        if (auto deserialized = decoder.decode<std::optional<WebCore::PlatformLayerIdentifier>>())
            layerlayerIDForEncoding = WTFMove(*deserialized);
        else
            return std::nullopt;
    }
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::ScrollingStateFrameHostingNode::create(
            WTFMove(*scrollingNodeID),
            WTFMove(*children),
            WTFMove(*changedProperties),
            WTFMove(layerlayerIDForEncoding)
        )
    };
}

void ArgumentCoder<WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple>::encode(Encoder& encoder, const WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.scrollingNodeID())>, WebCore::ScrollingNodeID>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.children())>, Vector<Ref<WebCore::ScrollingStateNode>>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.changedProperties())>, OptionSet<WebCore::ScrollingStateNodeProperty>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.layer().layerIDForEncoding())>, std::optional<WebCore::PlatformLayerIdentifier>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.otherMember)>, bool>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.memberAfterTuple)>, int>);
    static_assert(static_cast<uint64_t>(WebCore::ScrollingStateNode::Property::Layer) == 1);
    static_assert(BitsInIncreasingOrder<
        static_cast<uint64_t>(WebCore::ScrollingStateNode::Property::Layer)
        , static_cast<uint64_t>(WebCore::ScrollingStateNode::Property::Other)
    >::value);

    encoder << instance.scrollingNodeID();
    encoder << instance.children();
    encoder << instance.changedProperties();
    if (instance.changedProperties() & WebCore::ScrollingStateNode::Property::Layer)
        encoder << instance.layer().layerIDForEncoding();
    if (instance.changedProperties() & WebCore::ScrollingStateNode::Property::Other)
        encoder << instance.otherMember;
    encoder << instance.memberAfterTuple;
}

std::optional<Ref<WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple>> ArgumentCoder<WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple>::decode(Decoder& decoder)
{
    auto scrollingNodeID = decoder.decode<WebCore::ScrollingNodeID>();
    auto children = decoder.decode<Vector<Ref<WebCore::ScrollingStateNode>>>();
    auto changedProperties = decoder.decode<OptionSet<WebCore::ScrollingStateNodeProperty>>();
    if (!changedProperties)
        return std::nullopt;

    std::optional<WebCore::PlatformLayerIdentifier> layerlayerIDForEncoding { };
    if (*changedProperties & WebCore::ScrollingStateNode::Property::Layer) {
        if (auto deserialized = decoder.decode<std::optional<WebCore::PlatformLayerIdentifier>>())
            layerlayerIDForEncoding = WTFMove(*deserialized);
        else
            return std::nullopt;
    }

    bool otherMember { };
    if (*changedProperties & WebCore::ScrollingStateNode::Property::Other) {
        if (auto deserialized = decoder.decode<bool>())
            otherMember = WTFMove(*deserialized);
        else
            return std::nullopt;
    }
    auto memberAfterTuple = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple::create(
            WTFMove(*scrollingNodeID),
            WTFMove(*children),
            WTFMove(*changedProperties),
            WTFMove(layerlayerIDForEncoding),
            WTFMove(otherMember),
            WTFMove(*memberAfterTuple)
        )
    };
}

void ArgumentCoder<RequestEncodedWithBody>::encode(Encoder& encoder, const RequestEncodedWithBody& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.request)>, WebCore::ResourceRequest>);
    struct ShouldBeSameSizeAsRequestEncodedWithBody : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<RequestEncodedWithBody>, false> {
        WebCore::ResourceRequest request;
    };
    static_assert(sizeof(ShouldBeSameSizeAsRequestEncodedWithBody) == sizeof(RequestEncodedWithBody));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(RequestEncodedWithBody, request)
    >::value);

    encoder << instance.request;
    encoder << IPC::FormDataReference { instance.request.httpBody() };
}

std::optional<RequestEncodedWithBody> ArgumentCoder<RequestEncodedWithBody>::decode(Decoder& decoder)
{
    auto request = decoder.decode<WebCore::ResourceRequest>();
    if (request) {
        if (auto requestBody = decoder.decode<IPC::FormDataReference>())
            request->setHTTPBody(requestBody->takeData());
    }
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        RequestEncodedWithBody {
            WTFMove(*request)
        }
    };
}

void ArgumentCoder<RequestEncodedWithBodyRValue>::encode(Encoder& encoder, RequestEncodedWithBodyRValue&& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.request)>, WebCore::ResourceRequest>);
    struct ShouldBeSameSizeAsRequestEncodedWithBodyRValue : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<RequestEncodedWithBodyRValue>, false> {
        WebCore::ResourceRequest request;
    };
    static_assert(sizeof(ShouldBeSameSizeAsRequestEncodedWithBodyRValue) == sizeof(RequestEncodedWithBodyRValue));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(RequestEncodedWithBodyRValue, request)
    >::value);

    RefPtr requestBody = instance.request.httpBody();
    encoder << WTFMove(instance.request);
    encoder << IPC::FormDataReference { WTFMove(requestBody) };
}

std::optional<RequestEncodedWithBodyRValue> ArgumentCoder<RequestEncodedWithBodyRValue>::decode(Decoder& decoder)
{
    auto request = decoder.decode<WebCore::ResourceRequest>();
    if (request) {
        if (auto requestBody = decoder.decode<IPC::FormDataReference>())
            request->setHTTPBody(requestBody->takeData());
    }
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        RequestEncodedWithBodyRValue {
            WTFMove(*request)
        }
    };
}

void ArgumentCoder<CFFooRef>::encode(Encoder& encoder, CFFooRef instance)
{
    encoder << WebKit::FooWrapper { instance };
}

std::optional<RetainPtr<CFFooRef>> ArgumentCoder<RetainPtr<CFFooRef>>::decode(Decoder& decoder)
{
    auto result = decoder.decode<WebKit::FooWrapper>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return result->toCF();
}

#if USE(CFBAR)
void ArgumentCoder<CFBarRef>::encode(Encoder& encoder, CFBarRef instance)
{
    encoder << WebKit::BarWrapper::createFromCF(instance);
}

void ArgumentCoder<CFBarRef>::encode(StreamConnectionEncoder& encoder, CFBarRef instance)
{
    encoder << WebKit::BarWrapper::createFromCF(instance);
}

std::optional<RetainPtr<CFBarRef>> ArgumentCoder<RetainPtr<CFBarRef>>::decode(Decoder& decoder)
{
    auto result = decoder.decode<WebKit::BarWrapper>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return createCFBar(*result);
}

#endif

#if USE(SKIA)
void ArgumentCoder<SkFooBar>::encode(Encoder& encoder, const SkFooBar& passedInstance)
{
    auto instance = CoreIPCSkFooBar(passedInstance);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.foo())>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.bar())>, double>);

    encoder << instance.foo();
    encoder << instance.bar();
}

void ArgumentCoder<SkFooBar>::encode(OtherEncoder& encoder, const SkFooBar& passedInstance)
{
    auto instance = CoreIPCSkFooBar(passedInstance);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.foo())>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.bar())>, double>);

    encoder << instance.foo();
    encoder << instance.bar();
}

std::optional<SkFooBar> ArgumentCoder<SkFooBar>::decode(Decoder& decoder)
{
    auto foo = decoder.decode<int>();
    auto bar = decoder.decode<double>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        CoreIPCSkFooBar {
            WTFMove(*foo),
            WTFMove(*bar)
        }
    };
}

#endif

void ArgumentCoder<WebKit::RValueWithFunctionCalls>::encode(Encoder& encoder, WebKit::RValueWithFunctionCalls&& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.callFunction())>, SandboxExtensionHandle>);

    encoder << instance.callFunction();
}

std::optional<WebKit::RValueWithFunctionCalls> ArgumentCoder<WebKit::RValueWithFunctionCalls>::decode(Decoder& decoder)
{
    auto callFunction = decoder.decode<SandboxExtensionHandle>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::RValueWithFunctionCalls {
            WTFMove(*callFunction)
        }
    };
}

void ArgumentCoder<WebKit::RemoteVideoFrameReference>::encode(Encoder& encoder, const WebKit::RemoteVideoFrameReference& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.identifier())>, WebKit::RemoteVideoFrameIdentifier>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.version())>, uint64_t>);

    encoder << instance.identifier();
    encoder << instance.version();
}

void ArgumentCoder<WebKit::RemoteVideoFrameReference>::encode(StreamConnectionEncoder& encoder, const WebKit::RemoteVideoFrameReference& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.identifier())>, WebKit::RemoteVideoFrameIdentifier>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.version())>, uint64_t>);

    encoder << instance.identifier();
    encoder << instance.version();
}

std::optional<WebKit::RemoteVideoFrameReference> ArgumentCoder<WebKit::RemoteVideoFrameReference>::decode(Decoder& decoder)
{
    auto identifier = decoder.decode<WebKit::RemoteVideoFrameIdentifier>();
    auto version = decoder.decode<uint64_t>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::RemoteVideoFrameReference {
            WTFMove(*identifier),
            WTFMove(*version)
        }
    };
}

void ArgumentCoder<WebKit::RemoteVideoFrameWriteReference>::encode(Encoder& encoder, const WebKit::RemoteVideoFrameWriteReference& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.reference())>, IPC::ObjectIdentifierReference<WebKit::RemoteVideoFrameIdentifier>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.pendingReads())>, uint64_t>);

    encoder << instance.reference();
    encoder << instance.pendingReads();
}

void ArgumentCoder<WebKit::RemoteVideoFrameWriteReference>::encode(StreamConnectionEncoder& encoder, const WebKit::RemoteVideoFrameWriteReference& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.reference())>, IPC::ObjectIdentifierReference<WebKit::RemoteVideoFrameIdentifier>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.pendingReads())>, uint64_t>);

    encoder << instance.reference();
    encoder << instance.pendingReads();
}

std::optional<WebKit::RemoteVideoFrameWriteReference> ArgumentCoder<WebKit::RemoteVideoFrameWriteReference>::decode(Decoder& decoder)
{
    auto reference = decoder.decode<IPC::ObjectIdentifierReference<WebKit::RemoteVideoFrameIdentifier>>();
    auto pendingReads = decoder.decode<uint64_t>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebKit::RemoteVideoFrameWriteReference {
            WTFMove(*reference),
            WTFMove(*pendingReads)
        }
    };
}

#if ENABLE(OUTER_CONDITION)
void ArgumentCoder<Namespace::OuterClass>::encode(Encoder& encoder, const Namespace::OuterClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.outerValue)>, int>);
    struct ShouldBeSameSizeAsOuterClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::OuterClass>, false> {
        int outerValue;
    };
    static_assert(sizeof(ShouldBeSameSizeAsOuterClass) == sizeof(Namespace::OuterClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::OuterClass, outerValue)
    >::value);

    encoder << instance.outerValue;
}

std::optional<Namespace::OuterClass> ArgumentCoder<Namespace::OuterClass>::decode(Decoder& decoder)
{
    auto outerValue = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::OuterClass {
            WTFMove(*outerValue)
        }
    };
}

#endif

#if !(ENABLE(OUTER_CONDITION))
void ArgumentCoder<Namespace::OtherOuterClass>::encode(Encoder& encoder, const Namespace::OtherOuterClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.outerValue)>, int>);
    struct ShouldBeSameSizeAsOtherOuterClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::OtherOuterClass>, false> {
        int outerValue;
    };
    static_assert(sizeof(ShouldBeSameSizeAsOtherOuterClass) == sizeof(Namespace::OtherOuterClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::OtherOuterClass, outerValue)
    >::value);

    encoder << instance.outerValue;
}

std::optional<Namespace::OtherOuterClass> ArgumentCoder<Namespace::OtherOuterClass>::decode(Decoder& decoder)
{
    auto outerValue = decoder.decode<int>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        Namespace::OtherOuterClass {
            WTFMove(*outerValue)
        }
    };
}

#endif

#if USE(APPKIT)
void ArgumentCoder<WebCore::AppKitControlSystemImage>::encode(Encoder& encoder, const WebCore::AppKitControlSystemImage& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_controlType)>, WebCore::AppKitControlSystemImageType>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_tintColor)>, WebCore::Color>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_useDarkAppearance)>, bool>);
    struct ShouldBeSameSizeAsAppKitControlSystemImage : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebCore::AppKitControlSystemImage>, true> {
        WebCore::AppKitControlSystemImageType m_controlType;
        WebCore::Color m_tintColor;
        bool m_useDarkAppearance;
    };
    static_assert(sizeof(ShouldBeSameSizeAsAppKitControlSystemImage) == sizeof(WebCore::AppKitControlSystemImage));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebCore::AppKitControlSystemImage, m_controlType)
        , offsetof(WebCore::AppKitControlSystemImage, m_tintColor)
        , offsetof(WebCore::AppKitControlSystemImage, m_useDarkAppearance)
    >::value);

    encoder << instance.m_tintColor;
    encoder << instance.m_useDarkAppearance;
}

void ArgumentCoder<WebCore::AppKitControlSystemImage>::encode(StreamConnectionEncoder& encoder, const WebCore::AppKitControlSystemImage& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_controlType)>, WebCore::AppKitControlSystemImageType>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_tintColor)>, WebCore::Color>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_useDarkAppearance)>, bool>);
    struct ShouldBeSameSizeAsAppKitControlSystemImage : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebCore::AppKitControlSystemImage>, true> {
        WebCore::AppKitControlSystemImageType m_controlType;
        WebCore::Color m_tintColor;
        bool m_useDarkAppearance;
    };
    static_assert(sizeof(ShouldBeSameSizeAsAppKitControlSystemImage) == sizeof(WebCore::AppKitControlSystemImage));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebCore::AppKitControlSystemImage, m_controlType)
        , offsetof(WebCore::AppKitControlSystemImage, m_tintColor)
        , offsetof(WebCore::AppKitControlSystemImage, m_useDarkAppearance)
    >::value);

    encoder << instance.m_tintColor;
    encoder << instance.m_useDarkAppearance;
}

std::optional<Ref<WebCore::AppKitControlSystemImage>> ArgumentCoder<WebCore::AppKitControlSystemImage>::decode(Decoder& decoder)
{
    auto m_tintColor = decoder.decode<WebCore::Color>();
    auto m_useDarkAppearance = decoder.decode<bool>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::ScrollbarTrackCornerSystemImageMac::create(
            WTFMove(*m_tintColor),
            WTFMove(*m_useDarkAppearance)
        )
    };
}

#endif

void ArgumentCoder<WebCore::RectEdges<bool>>::encode(Encoder& encoder, const WebCore::RectEdges<bool>& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.top())>, bool>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.right())>, bool>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.bottom())>, bool>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.left())>, bool>);

    encoder << instance.top();
    encoder << instance.right();
    encoder << instance.bottom();
    encoder << instance.left();
}

std::optional<WebCore::RectEdges<bool>> ArgumentCoder<WebCore::RectEdges<bool>>::decode(Decoder& decoder)
{
    auto top = decoder.decode<bool>();
    auto right = decoder.decode<bool>();
    auto bottom = decoder.decode<bool>();
    auto left = decoder.decode<bool>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::RectEdges<bool> {
            WTFMove(*top),
            WTFMove(*right),
            WTFMove(*bottom),
            WTFMove(*left)
        }
    };
}

void ArgumentCoder<WebCore::OpaqueTypeObject>::encode(Encoder& encoder, const WebCore::OpaqueTypeObject& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.member)>, NotDispatchableFromWebContent>);
    struct ShouldBeSameSizeAsOpaqueTypeObject : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<WebCore::OpaqueTypeObject>, false> {
        NotDispatchableFromWebContent member;
    };
    static_assert(sizeof(ShouldBeSameSizeAsOpaqueTypeObject) == sizeof(WebCore::OpaqueTypeObject));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(WebCore::OpaqueTypeObject, member)
    >::value);

    ASSERT(!isInWebProcess());
    encoder << instance.member;
}

std::optional<WebCore::OpaqueTypeObject> ArgumentCoder<WebCore::OpaqueTypeObject>::decode(Decoder& decoder)
{
    auto member = decoder.decode<NotDispatchableFromWebContent>();
    if (!decoder.isValid()) [[unlikely]]
        return std::nullopt;
    return {
        WebCore::OpaqueTypeObject {
            WTFMove(*member)
        }
    };
}

} // namespace IPC

namespace WTF {

template<> bool isValidEnum<IPC::WebCore_TimingFunction_Subclass>(IPC::EncodedVariantIndex value)
{
IGNORE_WARNINGS_BEGIN("switch-unreachable")
    switch (static_cast<IPC::WebCore_TimingFunction_Subclass>(value)) {
    case IPC::WebCore_TimingFunction_Subclass::LinearTimingFunction:
    case IPC::WebCore_TimingFunction_Subclass::CubicBezierTimingFunction:
#if CONDITION
    case IPC::WebCore_TimingFunction_Subclass::StepsTimingFunction:
#endif
    case IPC::WebCore_TimingFunction_Subclass::SpringTimingFunction:
        return true;
    }
IGNORE_WARNINGS_END
    return false;
}

template<> bool isValidEnum<IPC::WebCore_MoveOnlyBaseClass_Subclass>(IPC::EncodedVariantIndex value)
{
IGNORE_WARNINGS_BEGIN("switch-unreachable")
    switch (static_cast<IPC::WebCore_MoveOnlyBaseClass_Subclass>(value)) {
    case IPC::WebCore_MoveOnlyBaseClass_Subclass::MoveOnlyDerivedClass:
        return true;
    }
IGNORE_WARNINGS_END
    return false;
}

#if ENABLE(BOOL_ENUM)
template<> bool isValidEnum<EnumNamespace::BoolEnumType>(bool value)
{
    switch (static_cast<uint8_t>(value)) {
    case 0:
    case 1:
        return true;
    default:
        return false;
    }
}
#endif

template<> bool isValidEnum<EnumWithoutNamespace>(uint8_t value)
{
    switch (static_cast<EnumWithoutNamespace>(value)) {
    case EnumWithoutNamespace::Value1:
    case EnumWithoutNamespace::Value2:
    case EnumWithoutNamespace::Value3:
        return true;
    default:
        return false;
    }
}

#if ENABLE(UINT16_ENUM)
template<> bool isValidEnum<EnumNamespace::EnumType>(uint16_t value)
{
    switch (static_cast<EnumNamespace::EnumType>(value)) {
    case EnumNamespace::EnumType::FirstValue:
#if ENABLE(ENUM_VALUE_CONDITION)
    case EnumNamespace::EnumType::SecondValue:
#endif
        return true;
    default:
        return false;
    }
}
#endif

template<> bool isValidOptionSet<EnumNamespace2::OptionSetEnumType>(OptionSet<EnumNamespace2::OptionSetEnumType> value)
{
    constexpr uint8_t allValidBitsValue = 0
        | static_cast<uint8_t>(EnumNamespace2::OptionSetEnumType::OptionSetFirstValue)
#if ENABLE(OPTION_SET_SECOND_VALUE)
        | static_cast<uint8_t>(EnumNamespace2::OptionSetEnumType::OptionSetSecondValue)
#endif
#if !(ENABLE(OPTION_SET_SECOND_VALUE))
        | static_cast<uint8_t>(EnumNamespace2::OptionSetEnumType::OptionSetSecondValueElse)
#endif
        | static_cast<uint8_t>(EnumNamespace2::OptionSetEnumType::OptionSetThirdValue)
        | 0;
    return (value.toRaw() | allValidBitsValue) == allValidBitsValue;
}

template<> bool isValidOptionSet<OptionSetEnumFirstCondition>(OptionSet<OptionSetEnumFirstCondition> value)
{
    constexpr uint32_t allValidBitsValue = 0
#if ENABLE(OPTION_SET_FIRST_VALUE)
        | static_cast<uint32_t>(OptionSetEnumFirstCondition::OptionSetFirstValue)
#endif
        | static_cast<uint32_t>(OptionSetEnumFirstCondition::OptionSetSecondValue)
        | static_cast<uint32_t>(OptionSetEnumFirstCondition::OptionSetThirdValue)
        | 0;
    return (value.toRaw() | allValidBitsValue) == allValidBitsValue;
}

template<> bool isValidOptionSet<OptionSetEnumLastCondition>(OptionSet<OptionSetEnumLastCondition> value)
{
    constexpr uint32_t allValidBitsValue = 0
        | static_cast<uint32_t>(OptionSetEnumLastCondition::OptionSetFirstValue)
        | static_cast<uint32_t>(OptionSetEnumLastCondition::OptionSetSecondValue)
#if ENABLE(OPTION_SET_THIRD_VALUE)
        | static_cast<uint32_t>(OptionSetEnumLastCondition::OptionSetThirdValue)
#endif
        | 0;
    return (value.toRaw() | allValidBitsValue) == allValidBitsValue;
}

template<> bool isValidOptionSet<OptionSetEnumAllCondition>(OptionSet<OptionSetEnumAllCondition> value)
{
    constexpr uint32_t allValidBitsValue = 0
#if ENABLE(OPTION_SET_FIRST_VALUE)
        | static_cast<uint32_t>(OptionSetEnumAllCondition::OptionSetFirstValue)
#endif
#if ENABLE(OPTION_SET_SECOND_VALUE)
        | static_cast<uint32_t>(OptionSetEnumAllCondition::OptionSetSecondValue)
#endif
#if ENABLE(OPTION_SET_THIRD_VALUE)
        | static_cast<uint32_t>(OptionSetEnumAllCondition::OptionSetThirdValue)
#endif
        | 0;
    return (value.toRaw() | allValidBitsValue) == allValidBitsValue;
}

#if (ENABLE(OUTER_CONDITION)) && (ENABLE(INNER_CONDITION))
template<> bool isValidEnum<EnumNamespace::InnerEnumType>(uint8_t value)
{
    switch (static_cast<EnumNamespace::InnerEnumType>(value)) {
    case EnumNamespace::InnerEnumType::InnerValue:
#if ENABLE(INNER_INNER_CONDITION)
    case EnumNamespace::InnerEnumType::InnerInnerValue:
#endif
#if !(ENABLE(INNER_INNER_CONDITION))
    case EnumNamespace::InnerEnumType::OtherInnerInnerValue:
#endif
        return true;
    default:
        return false;
    }
}
#endif

#if (ENABLE(OUTER_CONDITION)) && (!(ENABLE(INNER_CONDITION)))
template<> bool isValidEnum<EnumNamespace::InnerBoolType>(bool value)
{
    switch (static_cast<uint8_t>(value)) {
    case 0:
    case 1:
        return true;
    default:
        return false;
    }
}
#endif

} // namespace WTF

IGNORE_WARNINGS_END
