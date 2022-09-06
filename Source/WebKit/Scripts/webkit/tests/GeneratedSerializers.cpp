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

namespace IPC {

#if ENABLE(TEST_FEATURE)

void ArgumentCoder<Namespace::Subnamespace::StructName>::encode(Encoder& encoder, const Namespace::Subnamespace::StructName& instance)
{
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

    return { Namespace::Subnamespace::StructName {
        WTFMove(*firstMemberName),
#if ENABLE(SECOND_MEMBER)
        WTFMove(*secondMemberName),
#endif
        WTFMove(*nullableTestMember)
    } };
}

#endif


void ArgumentCoder<Namespace::OtherClass>::encode(Encoder& encoder, const Namespace::OtherClass& instance)
{
    encoder << instance.isNull;
    if (instance.isNull)
        return;
    encoder << instance.a;
    encoder << instance.b;
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

    return { Namespace::OtherClass {
        WTFMove(*isNull),
        WTFMove(*a),
        WTFMove(*b)
    } };
}


void ArgumentCoder<Namespace::ReturnRefClass>::encode(Encoder& encoder, const Namespace::ReturnRefClass& instance)
{
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

    return { Namespace::ReturnRefClass::create(
        WTFMove(*functionCallmember1),
        WTFMove(*functionCallmember2),
        WTFMove(*uniqueMember)
    ) };
}


void ArgumentCoder<Namespace::EmptyConstructorStruct>::encode(Encoder& encoder, const Namespace::EmptyConstructorStruct& instance)
{
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
    encoder << instance.m_isNull;
    if (instance.m_isNull)
        return;
    encoder << instance.m_type;
    encoder << instance.m_value;
}

std::optional<Namespace::EmptyConstructorNullable> ArgumentCoder<Namespace::EmptyConstructorNullable>::decode(Decoder& decoder)
{
    std::optional<bool> m_isNull;
    decoder >> m_isNull;
    if (!m_isNull)
        return std::nullopt;
    if (*m_isNull)
        return { Namespace::EmptyConstructorNullable { } };

    std::optional<MemberType> m_type;
    decoder >> m_type;
    if (!m_type)
        return std::nullopt;

    std::optional<OtherMemberType> m_value;
    decoder >> m_value;
    if (!m_value)
        return std::nullopt;

    Namespace::EmptyConstructorNullable result;
    result.m_isNull = WTFMove(*m_isNull);
    result.m_type = WTFMove(*m_type);
    result.m_value = WTFMove(*m_value);
    return { WTFMove(result) };
}

} // namespace IPC
