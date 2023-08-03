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
#include "SerializedTypeInfo.h"

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
#include "PlatformClass.h"
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

#if ENABLE(IPC_TESTING_API)

namespace WebKit {

Vector<SerializedTypeInfo> allSerializedTypes()
{
    return {
        { "Namespace::Subnamespace::StructName"_s, {
            {
                "FirstMemberType"_s,
                "firstMemberName"_s
            },
#if ENABLE(SECOND_MEMBER)
            {
                "SecondMemberType"_s,
                "secondMemberName"_s
            },
#endif
            {
                "RetainPtr<CFTypeRef>"_s,
                "nullableTestMember"_s
            },
        } },
        { "Namespace::OtherClass"_s, {
            {
                "int"_s,
                "a"_s
            },
            {
                "bool"_s,
                "b"_s
            },
            {
                "RetainPtr<NSArray>"_s,
                "dataDetectorResults"_s
            },
        } },
        { "Namespace::ReturnRefClass"_s, {
            {
                "double"_s,
                "functionCall().member1"_s
            },
            {
                "double"_s,
                "functionCall().member2"_s
            },
            {
                "std::unique_ptr<int>"_s,
                "uniqueMember"_s
            },
        } },
        { "Namespace::EmptyConstructorStruct"_s, {
            {
                "int"_s,
                "m_int"_s
            },
            {
                "double"_s,
                "m_double"_s
            },
        } },
        { "Namespace::EmptyConstructorWithIf"_s, {
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
            {
                "MemberType"_s,
                "m_type"_s
            },
#endif
#if CONDITION_AROUND_M_TYPE_AND_M_VALUE
            {
                "OtherMemberType"_s,
                "m_value"_s
            },
#endif
        } },
        { "WithoutNamespace"_s, {
            {
                "int"_s,
                "a"_s
            },
        } },
        { "WithoutNamespaceWithAttributes"_s, {
            {
                "int"_s,
                "a"_s
            },
        } },
        { "WebCore::InheritsFrom"_s, {
            {
                "float"_s,
                "b"_s
            },
        } },
        { "WebCore::InheritanceGrandchild"_s, {
            {
                "double"_s,
                "c"_s
            },
        } },
        { "Seconds"_s, {
            {
                "double"_s,
                "value()"_s
            },
        } },
        { "CreateUsingClass"_s, {
            {
                "double"_s,
                "value"_s
            },
        } },
        { "WebCore::FloatBoxExtent"_s, {
            {
                "float"_s,
                "top()"_s
            },
            {
                "float"_s,
                "right()"_s
            },
            {
                "float"_s,
                "bottom()"_s
            },
            {
                "float"_s,
                "left()"_s
            },
        } },
        { "SoftLinkedMember"_s, {
            {
                "RetainPtr<DDActionContext>"_s,
                "firstMember"_s
            },
            {
                "RetainPtr<DDActionContext>"_s,
                "secondMember"_s
            },
        } },
        { "WebCore::TimingFunction"_s, {
            { "std::variant<WebCore::LinearTimingFunction, WebCore::CubicBezierTimingFunction, WebCore::StepsTimingFunction, WebCore::SpringTimingFunction>"_s, "subclasses"_s }
        } },
        { "Namespace::ConditionalCommonClass"_s, {
            {
                "int"_s,
                "value"_s
            },
        } },
        { "Namespace::CommonClass"_s, {
            {
                "int"_s,
                "value"_s
            },
        } },
        { "Namespace::AnotherCommonClass"_s, {
            {
                "int"_s,
                "value"_s
            },
        } },
        { "WebCore::MoveOnlyBaseClass"_s, {
            { "std::variant<WebCore::MoveOnlyDerivedClass>"_s, "subclasses"_s }
        } },
        { "WebCore::MoveOnlyDerivedClass"_s, {
            {
                "int"_s,
                "firstMember"_s
            },
            {
                "int"_s,
                "secondMember"_s
            },
        } },
        { "WebKit::PlatformClass"_s, {
            {
                "int"_s,
                "value"_s
            },
        } },
        { "WebKit::CustomEncoded"_s, {
            {
                "int"_s,
                "value"_s
            },
        } },
        { "WebKit::LayerProperties"_s, {
            {
                "OptionalTuple<"
                    "String"
#if ENABLE(FEATURE)
                    ", std::unique_ptr<WebCore::TransformationMatrix>"
#endif
                    ", bool"
                ">"_s,
                "optionalTuple"_s
            },
        } },
        { "WebCore::SharedStringHash"_s, {
            { "uint32_t"_s, "alias"_s }
        } },
    };
}

Vector<SerializedEnumInfo> allSerializedEnums()
{
    return {
#if ENABLE(BOOL_ENUM)
        { "EnumNamespace::BoolEnumType"_s, sizeof(EnumNamespace::BoolEnumType), false, {
            0, 1
        } },
#endif
        { "EnumWithoutNamespace"_s, sizeof(EnumWithoutNamespace), false, {
            static_cast<uint64_t>(EnumWithoutNamespace::Value1),
            static_cast<uint64_t>(EnumWithoutNamespace::Value2),
            static_cast<uint64_t>(EnumWithoutNamespace::Value3),
        } },
#if ENABLE(UINT16_ENUM)
        { "EnumNamespace::EnumType"_s, sizeof(EnumNamespace::EnumType), false, {
            static_cast<uint64_t>(EnumNamespace::EnumType::FirstValue),
#if ENABLE(ENUM_VALUE_CONDITION)
            static_cast<uint64_t>(EnumNamespace::EnumType::SecondValue),
#endif
        } },
#endif
        { "EnumNamespace2::OptionSetEnumType"_s, sizeof(EnumNamespace2::OptionSetEnumType), true, {
            static_cast<uint64_t>(EnumNamespace2::OptionSetEnumType::OptionSetFirstValue),
#if ENABLE(OPTION_SET_SECOND_VALUE)
            static_cast<uint64_t>(EnumNamespace2::OptionSetEnumType::OptionSetSecondValue),
#endif
            static_cast<uint64_t>(EnumNamespace2::OptionSetEnumType::OptionSetThirdValue),
        } },
        { "OptionSetEnumFirstCondition"_s, sizeof(OptionSetEnumFirstCondition), true, {
#if ENABLE(OPTION_SET_FIRST_VALUE)
            static_cast<uint64_t>(OptionSetEnumFirstCondition::OptionSetFirstValue),
#endif
            static_cast<uint64_t>(OptionSetEnumFirstCondition::OptionSetSecondValue),
            static_cast<uint64_t>(OptionSetEnumFirstCondition::OptionSetThirdValue),
        } },
        { "OptionSetEnumLastCondition"_s, sizeof(OptionSetEnumLastCondition), true, {
            static_cast<uint64_t>(OptionSetEnumLastCondition::OptionSetFirstValue),
            static_cast<uint64_t>(OptionSetEnumLastCondition::OptionSetSecondValue),
#if ENABLE(OPTION_SET_THIRD_VALUE)
            static_cast<uint64_t>(OptionSetEnumLastCondition::OptionSetThirdValue),
#endif
        } },
        { "OptionSetEnumAllCondition"_s, sizeof(OptionSetEnumAllCondition), true, {
#if ENABLE(OPTION_SET_FIRST_VALUE)
            static_cast<uint64_t>(OptionSetEnumAllCondition::OptionSetFirstValue),
#endif
#if ENABLE(OPTION_SET_SECOND_VALUE)
            static_cast<uint64_t>(OptionSetEnumAllCondition::OptionSetSecondValue),
#endif
#if ENABLE(OPTION_SET_THIRD_VALUE)
            static_cast<uint64_t>(OptionSetEnumAllCondition::OptionSetThirdValue),
#endif
        } },
    };
}

} // namespace WebKit

#endif // ENABLE(IPC_TESTING_API)
