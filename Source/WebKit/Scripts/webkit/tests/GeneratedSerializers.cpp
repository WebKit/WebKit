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

#if ENABLE(TEST_FEATURE)
#include "FirstMemberType.h"
#endif
#include "HeaderWithoutCondition"
#if ENABLE(TEST_FEATURE)
#include "SecondMemberType.h"
#endif
#if ENABLE(TEST_FEATURE)
#include "StructHeader.h"
#endif
#include <Namespace/EmptyConstructorNullable.h>
#include <Namespace/EmptyConstructorStruct.h>
#include <Namespace/ReturnRefClass.h>
#include <WebCore/FloatBoxExtent.h>
#include <WebCore/InheritanceGrandchild.h>
#include <WebCore/InheritsFrom.h>
#include <wtf/CreateUsingClass.h>
#include <wtf/Seconds.h>

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
    encoder << instance.firstMemberName;
#if ENABLE(SECOND_MEMBER)
    encoder << instance.secondMemberName;
#endif
    encoder << !!instance.nullableTestMember;
    if (!!instance.nullableTestMember)
        encoder << instance.nullableTestMember;
}

void ArgumentCoder<Namespace::Subnamespace::StructName>::encode(OtherEncoder& encoder, const Namespace::Subnamespace::StructName& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.firstMemberName)>, FirstMemberType>);
#if ENABLE(SECOND_MEMBER)
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.secondMemberName)>, SecondMemberType>);
#endif
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.nullableTestMember)>, RetainPtr<CFTypeRef>>);
    encoder << instance.firstMemberName;
#if ENABLE(SECOND_MEMBER)
    encoder << instance.secondMemberName;
#endif
    encoder << !!instance.nullableTestMember;
    if (!!instance.nullableTestMember)
        encoder << instance.nullableTestMember;
}

std::optional<Namespace::Subnamespace::StructName> ArgumentCoder<Namespace::Subnamespace::StructName>::decode(Decoder& decoder)
{
    std::optional<FirstMemberType> firstMemberName;
    decoder >> firstMemberName;
    if (!firstMemberName)
        return std::nullopt;

#if ENABLE(SECOND_MEMBER)
    std::optional<SecondMemberType> secondMemberName;
    decoder >> secondMemberName;
    if (!secondMemberName)
        return std::nullopt;
#endif

    std::optional<RetainPtr<CFTypeRef>> nullableTestMember;
    std::optional<bool> hasnullableTestMember;
    decoder >> hasnullableTestMember;
    if (!hasnullableTestMember)
        return std::nullopt;
    if (*hasnullableTestMember) {
        decoder >> nullableTestMember;
        if (!nullableTestMember)
            return std::nullopt;
    } else
        nullableTestMember = std::optional<RetainPtr<CFTypeRef>> { RetainPtr<CFTypeRef> { } };

    return {
        Namespace::Subnamespace::StructName {
            WTFMove(*firstMemberName)
#if ENABLE(SECOND_MEMBER)
            , WTFMove(*secondMemberName)
#endif
            , WTFMove(*nullableTestMember)
        }
    };
}

#endif


void ArgumentCoder<Namespace::OtherClass>::encode(Encoder& encoder, const Namespace::OtherClass& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.isNull)>, bool>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.b)>, bool>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.dataDetectorResults)>, RetainPtr<NSArray>>);
    encoder << instance.isNull;
    if (instance.isNull)
        return;
    encoder << instance.a;
    encoder << instance.b;
    encoder << instance.dataDetectorResults;
}

std::optional<Namespace::OtherClass> ArgumentCoder<Namespace::OtherClass>::decode(Decoder& decoder)
{
    std::optional<bool> isNull;
    decoder >> isNull;
    if (!isNull)
        return std::nullopt;
    if (*isNull)
        return { Namespace::OtherClass { } };

    std::optional<int> a;
    decoder >> a;
    if (!a)
        return std::nullopt;

    std::optional<bool> b;
    decoder >> b;
    if (!b)
        return std::nullopt;

    std::optional<RetainPtr<NSArray>> dataDetectorResults;
    dataDetectorResults = IPC::decode<NSArray>(decoder, @[ NSArray.class, PAL::getDDScannerResultClass() ]);
    if (!dataDetectorResults)
        return std::nullopt;

    return {
        Namespace::OtherClass {
            WTFMove(*isNull)
            , WTFMove(*a)
            , WTFMove(*b)
            , WTFMove(*dataDetectorResults)
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
    encoder << !!instance.uniqueMember;
    if (!!instance.uniqueMember)
        encoder << *instance.uniqueMember;
}

std::optional<Ref<Namespace::ReturnRefClass>> ArgumentCoder<Namespace::ReturnRefClass>::decode(Decoder& decoder)
{
    std::optional<double> functionCallmember1;
    decoder >> functionCallmember1;
    if (!functionCallmember1)
        return std::nullopt;

    std::optional<double> functionCallmember2;
    decoder >> functionCallmember2;
    if (!functionCallmember2)
        return std::nullopt;

    std::optional<std::unique_ptr<int>> uniqueMember;
    std::optional<bool> hasuniqueMember;
    decoder >> hasuniqueMember;
    if (!hasuniqueMember)
        return std::nullopt;
    if (*hasuniqueMember) {
        std::optional<int> contents;
        decoder >> contents;
        if (!contents)
            return std::nullopt;
        uniqueMember= makeUnique<int>(WTFMove(*contents));
    } else
        uniqueMember = std::optional<std::unique_ptr<int>> { std::unique_ptr<int> { } };

    return {
        Namespace::ReturnRefClass::create(
            WTFMove(*functionCallmember1)
            , WTFMove(*functionCallmember2)
            , WTFMove(*uniqueMember)
        )
    };
}


void ArgumentCoder<Namespace::EmptyConstructorStruct>::encode(Encoder& encoder, const Namespace::EmptyConstructorStruct& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_int)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_double)>, double>);
    encoder << instance.m_int;
    encoder << instance.m_double;
}

std::optional<Namespace::EmptyConstructorStruct> ArgumentCoder<Namespace::EmptyConstructorStruct>::decode(Decoder& decoder)
{
    std::optional<int> m_int;
    decoder >> m_int;
    if (!m_int)
        return std::nullopt;

    std::optional<double> m_double;
    decoder >> m_double;
    if (!m_double)
        return std::nullopt;

    Namespace::EmptyConstructorStruct result;
    result.m_int = WTFMove(*m_int);
    result.m_double = WTFMove(*m_double);
    return { WTFMove(result) };
}


void ArgumentCoder<Namespace::EmptyConstructorNullable>::encode(Encoder& encoder, const Namespace::EmptyConstructorNullable& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_isNull)>, bool>);
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_type)>, MemberType>);
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_value)>, OtherMemberType>);
#endif
    encoder << instance.m_isNull;
    if (instance.m_isNull)
        return;
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    encoder << instance.m_type;
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    encoder << instance.m_value;
#endif
}

std::optional<Namespace::EmptyConstructorNullable> ArgumentCoder<Namespace::EmptyConstructorNullable>::decode(Decoder& decoder)
{
    std::optional<bool> m_isNull;
    decoder >> m_isNull;
    if (!m_isNull)
        return std::nullopt;
    if (*m_isNull)
        return { Namespace::EmptyConstructorNullable { } };

#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    std::optional<MemberType> m_type;
    decoder >> m_type;
    if (!m_type)
        return std::nullopt;
#endif

#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
    std::optional<OtherMemberType> m_value;
    decoder >> m_value;
    if (!m_value)
        return std::nullopt;
#endif

    Namespace::EmptyConstructorNullable result;
    result.m_isNull = WTFMove(*m_isNull);
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
    encoder << instance.a;
}

std::optional<WithoutNamespace> ArgumentCoder<WithoutNamespace>::decode(Decoder& decoder)
{
    std::optional<int> a;
    decoder >> a;
    if (!a)
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
    encoder << instance.a;
}

void ArgumentCoder<WithoutNamespaceWithAttributes>::encode(OtherEncoder& encoder, const WithoutNamespaceWithAttributes& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    encoder << instance.a;
}

std::optional<WithoutNamespaceWithAttributes> ArgumentCoder<WithoutNamespaceWithAttributes>::decode(Decoder& decoder)
{
    std::optional<int> a;
    decoder >> a;
    if (!a)
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
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.b)>, float>);
    encoder << instance.a;
    encoder << instance.b;
}

std::optional<WebCore::InheritsFrom> ArgumentCoder<WebCore::InheritsFrom>::decode(Decoder& decoder)
{
    std::optional<int> a;
    decoder >> a;
    if (!a)
        return std::nullopt;

    std::optional<float> b;
    decoder >> b;
    if (!b)
        return std::nullopt;

    return {
        WebCore::InheritsFrom {
            WithoutNamespace {
                WTFMove(*a)
            }
            
            , WTFMove(*b)
        }
    };
}


void ArgumentCoder<WebCore::InheritanceGrandchild>::encode(Encoder& encoder, const WebCore::InheritanceGrandchild& instance)
{
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.a)>, int>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.b)>, float>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.c)>, double>);
    encoder << instance.a;
    encoder << instance.b;
    encoder << instance.c;
}

std::optional<WebCore::InheritanceGrandchild> ArgumentCoder<WebCore::InheritanceGrandchild>::decode(Decoder& decoder)
{
    std::optional<int> a;
    decoder >> a;
    if (!a)
        return std::nullopt;

    std::optional<float> b;
    decoder >> b;
    if (!b)
        return std::nullopt;

    std::optional<double> c;
    decoder >> c;
    if (!c)
        return std::nullopt;

    return {
        WebCore::InheritanceGrandchild {
            WebCore::InheritsFrom {
                WithoutNamespace {
                    WTFMove(*a)
                }
                
                , WTFMove(*b)
            }
            
            , WTFMove(*c)
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
    std::optional<double> value;
    decoder >> value;
    if (!value)
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
    encoder << instance.value;
}

std::optional<WTF::CreateUsingClass> ArgumentCoder<WTF::CreateUsingClass>::decode(Decoder& decoder)
{
    std::optional<double> value;
    decoder >> value;
    if (!value)
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
    std::optional<float> top;
    decoder >> top;
    if (!top)
        return std::nullopt;

    std::optional<float> right;
    decoder >> right;
    if (!right)
        return std::nullopt;

    std::optional<float> bottom;
    decoder >> bottom;
    if (!bottom)
        return std::nullopt;

    std::optional<float> left;
    decoder >> left;
    if (!left)
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

} // namespace IPC

namespace WTF {

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
    constexpr uint8_t allValidBitsValue =
        static_cast<uint8_t>(EnumNamespace2::OptionSetEnumType::OptionSetFirstValue)
#if ENABLE(OPTION_SET_SECOND_VALUE)
        | static_cast<uint8_t>(EnumNamespace2::OptionSetEnumType::OptionSetSecondValue)
#endif
        | static_cast<uint8_t>(EnumNamespace2::OptionSetEnumType::OptionSetThirdValue);
    return (value.toRaw() | allValidBitsValue) == allValidBitsValue;
}

} // namespace WTF
