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
#include "SerializedTypeInfo.h"

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
#include "GeneratedWebKitSecureCoding.h"
#include "HeaderWithoutCondition"
#include "LayerProperties.h"
#include "PlatformClass.h"
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
#include <WebCore/FloatBoxExtent.h>
#include <WebCore/InheritanceGrandchild.h>
#include <WebCore/InheritsFrom.h>
#include <WebCore/MoveOnlyBaseClass.h>
#include <WebCore/MoveOnlyDerivedClass.h>
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
        { "Namespace::ClassWithMemberPrecondition"_s, {
            {
                "RetainPtr<PKPaymentMethod>"_s,
                "m_pkPaymentMethod"_s
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
                "int"_s,
                "a"_s
            },
            {
                "float"_s,
                "b"_s
            },
        } },
        { "WebCore::InheritanceGrandchild"_s, {
            {
                "int"_s,
                "a"_s
            },
            {
                "float"_s,
                "b"_s
            },
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
            { "std::variant<"
                "WebCore::LinearTimingFunction"
                ", WebCore::CubicBezierTimingFunction"
#if CONDITION
                ", WebCore::StepsTimingFunction"
#endif
                ", WebCore::SpringTimingFunction"
            ">"_s, "subclasses"_s }
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
            { "std::variant<"
                "WebCore::MoveOnlyDerivedClass"
            ">"_s, "subclasses"_s }
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
        { "WebKit::TemplateTest"_s, {
            {
                "bool"_s,
                "value"_s
            },
        } },
        { "WebCore::ScrollingStateFrameHostingNode"_s, {
            {
                "WebCore::ScrollingNodeID"_s,
                "scrollingNodeID()"_s
            },
            {
                "Vector<Ref<WebCore::ScrollingStateNode>>"_s,
                "children()"_s
            },
            {
                "OptionalTuple<"
                    "std::optional<WebCore::PlatformLayerIdentifier>"
                ">"_s,
                "optionalTuple"_s
            },
        } },
        { "WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple"_s, {
            {
                "WebCore::ScrollingNodeID"_s,
                "scrollingNodeID()"_s
            },
            {
                "Vector<Ref<WebCore::ScrollingStateNode>>"_s,
                "children()"_s
            },
            {
                "OptionalTuple<"
                    "std::optional<WebCore::PlatformLayerIdentifier>"
                    ", bool"
                ">"_s,
                "optionalTuple"_s
            },
            {
                "int"_s,
                "memberAfterTuple"_s
            },
        } },
        { "RequestEncodedWithBody"_s, {
            {
                "WebCore::ResourceRequest"_s,
                "request"_s
            },
        } },
        { "RequestEncodedWithBodyRValue"_s, {
            {
                "WebCore::ResourceRequest"_s,
                "request"_s
            },
        } },
        { "webkit_secure_coding AVOutputContext"_s, {
            {
                "AVOutputContextSerializationKeyContextID"_s,
                "WebKit::CoreIPCString"_s
            },
            {
                "AVOutputContextSerializationKeyContextType"_s,
                "WebKit::CoreIPCString"_s
            },
        } },
        { "WebKit::CoreIPCAVOutputContext"_s, {
            {
                "WebKit::CoreIPCDictionary"_s,
                "m_propertyList"_s
            },
        } },
        { "webkit_secure_coding NSSomeFoundationType"_s, {
            {
                "StringKey"_s,
                "WebKit::CoreIPCString"_s
            },
            {
                "NumberKey"_s,
                "WebKit::CoreIPCNumber"_s
            },
            {
                "OptionalNumberKey"_s,
                "WebKit::CoreIPCNumber?"_s
            },
            {
                "ArrayKey"_s,
                "WebKit::CoreIPCArray"_s
            },
            {
                "OptionalArrayKey"_s,
                "WebKit::CoreIPCArray?"_s
            },
            {
                "DictionaryKey"_s,
                "WebKit::CoreIPCDictionary"_s
            },
            {
                "OptionalDictionaryKey"_s,
                "WebKit::CoreIPCDictionary?"_s
            },
        } },
        { "WebKit::CoreIPCNSSomeFoundationType"_s, {
            {
                "WebKit::CoreIPCDictionary"_s,
                "m_propertyList"_s
            },
        } },
        { "webkit_secure_coding DDScannerResult"_s, {
            {
                "StringKey"_s,
                "WebKit::CoreIPCString"_s
            },
            {
                "NumberKey"_s,
                "WebKit::CoreIPCNumber"_s
            },
            {
                "OptionalNumberKey"_s,
                "WebKit::CoreIPCNumber?"_s
            },
            {
                "ArrayKey"_s,
                "WebKit::CoreIPCArray<WebKit::CoreIPCDDScannerResult>"_s
            },
            {
                "OptionalArrayKey"_s,
                "WebKit::CoreIPCArray<WebKit::CoreIPCDDScannerResult>?"_s
            },
            {
                "DictionaryKey"_s,
                "WebKit::CoreIPCDictionary<WebKit::CoreIPCString, WebKit::CoreIPCNumber>"_s
            },
            {
                "OptionalDictionaryKey"_s,
                "WebKit::CoreIPCDictionary<WebKit::CoreIPCString, WebKit::CoreIPCDDScannerResult>?"_s
            },
        } },
        { "WebKit::CoreIPCDDScannerResult"_s, {
            {
                "WebKit::CoreIPCDictionary"_s,
                "m_propertyList"_s
            },
        } },
        { "WebKit::RValueWithFunctionCalls"_s, {
            {
                "SandboxExtensionHandle"_s,
                "callFunction()"_s
            },
        } },
        { "WebKit::RemoteVideoFrameReference"_s, {
            {
                "WebKit::RemoteVideoFrameIdentifier"_s,
                "identifier()"_s
            },
            {
                "uint64_t"_s,
                "version()"_s
            },
        } },
        { "WebKit::RemoteVideoFrameWriteReference"_s, {
            {
                "IPC::ObjectIdentifierReference<WebKit::RemoteVideoFrameIdentifier>"_s,
                "reference()"_s
            },
            {
                "uint64_t"_s,
                "pendingReads()"_s
            },
        } },
        { "Namespace::OuterClass"_s, {
            {
                "int"_s,
                "outerValue"_s
            },
        } },
        { "Namespace::OtherOuterClass"_s, {
            {
                "int"_s,
                "outerValue"_s
            },
        } },
        { "WebCore::SharedStringHash"_s, {
            { "uint32_t"_s, "alias"_s }
        } },
        { "WebCore::UsingWithSemicolon"_s, {
            { "uint32_t"_s, "alias"_s }
        } },
#if OS(WINDOWS)
        { "WTF::ProcessID"_s, {
            { "int"_s, "alias"_s }
        } },
#endif
#if !(OS(WINDOWS))
        { "WTF::ProcessID"_s, {
            { "pid_t"_s, "alias"_s }
        } },
#endif
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
#if !(ENABLE(OPTION_SET_SECOND_VALUE))
            static_cast<uint64_t>(EnumNamespace2::OptionSetEnumType::OptionSetSecondValueElse),
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
#if (ENABLE(OUTER_CONDITION)) && (ENABLE(INNER_CONDITION))
        { "EnumNamespace::InnerEnumType"_s, sizeof(EnumNamespace::InnerEnumType), false, {
            static_cast<uint64_t>(EnumNamespace::InnerEnumType::InnerValue),
#if ENABLE(INNER_INNER_CONDITION)
            static_cast<uint64_t>(EnumNamespace::InnerEnumType::InnerInnerValue),
#endif
#if !(ENABLE(INNER_INNER_CONDITION))
            static_cast<uint64_t>(EnumNamespace::InnerEnumType::OtherInnerInnerValue),
#endif
        } },
#endif
#if (ENABLE(OUTER_CONDITION)) && (!(ENABLE(INNER_CONDITION)))
        { "EnumNamespace::InnerBoolType"_s, sizeof(EnumNamespace::InnerBoolType), false, {
            0, 1
        } },
#endif
    };
}

} // namespace WebKit

#endif // ENABLE(IPC_TESTING_API)
