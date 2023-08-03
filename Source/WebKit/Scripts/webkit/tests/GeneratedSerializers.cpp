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

#include "CommonHeader.h"
#if ENABLE(TEST_FEATURE)
#include "CommonHeader.h"
#endif
#include "CustomEncoded.h"
#if ENABLE(TEST_FEATURE)
#include "FirstMemberType.h"
#endif
#include "HeaderWithoutCondition"
#include "LayerProperties.h"
#if ENABLE(TEST_FEATURE)
#include "SecondMemberType.h"
#endif
#if ENABLE(TEST_FEATURE)
#include "StructHeader.h"
#endif
#include <Namespace/EmptyConstructorStruct.h>
#include <Namespace/EmptyConstructorWithIf.h>
#include <Namespace/ReturnRefClass.h>
#include <WebCore/FloatBoxExtent.h>
#include <WebCore/InheritanceGrandchild.h>
#include <WebCore/InheritsFrom.h>
#include <WebCore/MoveOnlyBaseClass.h>
#include <WebCore/MoveOnlyDerivedClass.h>
#include <WebCore/TimingFunction.h>
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


template<> struct ArgumentCoder<Namespace::OtherClass> {
    static void encode(Encoder&, const Namespace::OtherClass&);
    static std::optional<Namespace::OtherClass> decode(Decoder&);
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
    auto firstMemberName = decoder.decode<FirstMemberType>();
#if ENABLE(SECOND_MEMBER)
    auto secondMemberName = decoder.decode<SecondMemberType>();
#endif
    auto nullableTestMember = decoder.decode<RetainPtr<CFTypeRef>>();
    if (UNLIKELY(!decoder.isValid()))
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
    encoder << instance.dataDetectorResults;
}

std::optional<Namespace::OtherClass> ArgumentCoder<Namespace::OtherClass>::decode(Decoder& decoder)
{
    auto a = decoder.decode<int>();
    auto b = decoder.decode<bool>();
    auto dataDetectorResults = IPC::decode<NSArray>(decoder, @[ NSArray.class, PAL::getDDScannerResultClass() ]);
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return {
        Namespace::OtherClass {
            WTFMove(*a),
            WTFMove(*b),
            WTFMove(*dataDetectorResults)
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    auto firstMember = IPC::decode<DDActionContext>(decoder, @[ PAL::getDDActionContextClass() ]);
    auto secondMember = IPC::decode<DDActionContext>(decoder, @[ PAL::getDDActionContextClass() ]);
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return {
        SoftLinkedMember {
            WTFMove(*firstMember),
            WTFMove(*secondMember)
        }
    };
}

enum class WebCore_TimingFunction_Subclass : IPC::EncodedVariantIndex {
    LinearTimingFunction,
    CubicBezierTimingFunction,
    StepsTimingFunction,
    SpringTimingFunction
};

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
    if (auto* subclass = dynamicDowncast<WebCore::StepsTimingFunction>(instance)) {
        encoder << WebCore_TimingFunction_Subclass::StepsTimingFunction;
        encoder << *subclass;
        return;
    }
    if (auto* subclass = dynamicDowncast<WebCore::SpringTimingFunction>(instance)) {
        encoder << WebCore_TimingFunction_Subclass::SpringTimingFunction;
        encoder << *subclass;
        return;
    }
    ASSERT_NOT_REACHED();
}

std::optional<Ref<WebCore::TimingFunction>> ArgumentCoder<WebCore::TimingFunction>::decode(Decoder& decoder)
{
    auto type = decoder.decode<WebCore_TimingFunction_Subclass>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;

    if (type == WebCore_TimingFunction_Subclass::LinearTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::LinearTimingFunction>>();
        if (UNLIKELY(!decoder.isValid()))
            return std::nullopt;
        return WTFMove(*result);
    }
    if (type == WebCore_TimingFunction_Subclass::CubicBezierTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::CubicBezierTimingFunction>>();
        if (UNLIKELY(!decoder.isValid()))
            return std::nullopt;
        return WTFMove(*result);
    }
    if (type == WebCore_TimingFunction_Subclass::StepsTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::StepsTimingFunction>>();
        if (UNLIKELY(!decoder.isValid()))
            return std::nullopt;
        return WTFMove(*result);
    }
    if (type == WebCore_TimingFunction_Subclass::SpringTimingFunction) {
        auto result = decoder.decode<Ref<WebCore::SpringTimingFunction>>();
        if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    struct ShouldBeSameSizeAsCommonClass : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<Namespace::CommonClass>, false> {
        int value;
    };
    static_assert(sizeof(ShouldBeSameSizeAsCommonClass) == sizeof(Namespace::CommonClass));
    static_assert(MembersInCorrectOrder < 0
        , offsetof(Namespace::CommonClass, value)
    >::value);
    encoder << instance.value;
}

std::optional<Namespace::CommonClass> ArgumentCoder<Namespace::CommonClass>::decode(Decoder& decoder)
{
    auto value = decoder.decode<int>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return {
        Namespace::CommonClass {
            WTFMove(*value)
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
    if (UNLIKELY(!decoder.isValid()))
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

void ArgumentCoder<WebCore::MoveOnlyBaseClass>::encode(Encoder& encoder, WebCore::MoveOnlyBaseClass&& instance)
{
    if (auto* subclass = dynamicDowncast<WebCore::MoveOnlyDerivedClass>(instance)) {
        encoder << WebCore_MoveOnlyBaseClass_Subclass::MoveOnlyDerivedClass;
        encoder << WTFMove(*subclass);
        return;
    }
    ASSERT_NOT_REACHED();
}

std::optional<WebCore::MoveOnlyBaseClass> ArgumentCoder<WebCore::MoveOnlyBaseClass>::decode(Decoder& decoder)
{
    auto type = decoder.decode<WebCore_MoveOnlyBaseClass_Subclass>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;

    if (type == WebCore_MoveOnlyBaseClass_Subclass::MoveOnlyDerivedClass) {
        auto result = decoder.decode<Ref<WebCore::MoveOnlyDerivedClass>>();
        if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    if (UNLIKELY(!decoder.isValid()))
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
    auto bits = decoder.decode<OptionSet<WebKit::LayerChange>>();
    if (!bits)
        return std::nullopt;
    result.changedProperties = *bits;

    if (*bits & WebKit::LayerChange::NameChanged) {
        if (auto deserialized = decoder.decode<String>())
            result.name = WTFMove(*deserialized);
        else
            return std::nullopt;
    }

#if ENABLE(FEATURE)
    if (*bits & WebKit::LayerChange::TransformChanged) {
        if (auto deserialized = decoder.decode<std::unique_ptr<WebCore::TransformationMatrix>>())
            result.featureEnabledMember = WTFMove(*deserialized);
        else
            return std::nullopt;
    }
#endif

    if (*bits & WebKit::LayerChange::FeatureEnabledMember) {
        if (auto deserialized = decoder.decode<bool>())
            result.bitFieldMember = WTFMove(*deserialized);
        else
            return std::nullopt;
    }

    return { WTFMove(result) };
}

} // namespace IPC

namespace WTF {

template<> bool isValidEnum<IPC::WebCore_TimingFunction_Subclass, void>(IPC::EncodedVariantIndex value)
{
    switch (static_cast<IPC::WebCore_TimingFunction_Subclass>(value)) {
    case IPC::WebCore_TimingFunction_Subclass::LinearTimingFunction:
    case IPC::WebCore_TimingFunction_Subclass::CubicBezierTimingFunction:
    case IPC::WebCore_TimingFunction_Subclass::StepsTimingFunction:
    case IPC::WebCore_TimingFunction_Subclass::SpringTimingFunction:
        return true;
    default:
        return false;
    }
}

template<> bool isValidEnum<IPC::WebCore_MoveOnlyBaseClass_Subclass, void>(IPC::EncodedVariantIndex value)
{
    switch (static_cast<IPC::WebCore_MoveOnlyBaseClass_Subclass>(value)) {
    case IPC::WebCore_MoveOnlyBaseClass_Subclass::MoveOnlyDerivedClass:
        return true;
    default:
        return false;
    }
}

template<> bool isValidEnum<EnumWithoutNamespace, void>(uint8_t value)
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
template<> bool isValidEnum<EnumNamespace::EnumType, void>(uint16_t value)
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

} // namespace WTF

#if COMPILER(GCC)
IGNORE_WARNINGS_END
#endif
