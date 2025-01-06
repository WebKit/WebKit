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

#pragma once

#include <wtf/ArgumentCoder.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>

namespace EnumNamespace {
#if ENABLE(BOOL_ENUM)
enum class BoolEnumType : bool;
#endif
#if ENABLE(UINT16_ENUM)
enum class EnumType : uint16_t;
#endif
#if (ENABLE(OUTER_CONDITION)) && (ENABLE(INNER_CONDITION))
enum class InnerEnumType : uint8_t;
#endif
#if (ENABLE(OUTER_CONDITION)) && (!(ENABLE(INNER_CONDITION)))
enum class InnerBoolType : bool;
#endif
}

namespace JSC {
enum class Incredible;
}

namespace Namespace {
class ReturnRefClass;
struct EmptyConstructorStruct;
class EmptyConstructorWithIf;
#if ENABLE(TEST_FEATURE)
class ConditionalCommonClass;
#endif
class CommonClass;
class AnotherCommonClass;
#if ENABLE(OUTER_CONDITION)
class OuterClass;
#endif
#if !(ENABLE(OUTER_CONDITION))
class OtherOuterClass;
#endif
}

namespace Namespace::Subnamespace {
#if ENABLE(TEST_FEATURE)
struct StructName;
#endif
}

namespace Testing {
enum class StorageSize : uint8_t;
}

namespace WTF {
class Seconds;
class CreateUsingClass;
}

namespace WebCore {
class InheritsFrom;
class InheritanceGrandchild;
class TimingFunction;
class MoveOnlyBaseClass;
class MoveOnlyDerivedClass;
class ScrollingStateFrameHostingNode;
class ScrollingStateFrameHostingNodeWithStuffAfterTuple;
#if USE(APPKIT)
class AppKitControlSystemImage;
#endif
template<typename> class RectEdges;
struct Amazing;
}

namespace WebKit {
class PlatformClass;
class CustomEncoded;
class LayerProperties;
#if USE(AVFOUNDATION)
class CoreIPCAVOutputContext;
#endif
class CoreIPCNSSomeFoundationType;
class CoreIPCclass NSSomeOtherFoundationType;
#if ENABLE(DATA_DETECTION)
class CoreIPCDDScannerResult;
#endif
class RValueWithFunctionCalls;
class Fabulous;
}

enum class EnumWithoutNamespace : uint8_t;
enum class OptionSetEnumFirstCondition : uint32_t;
enum class OptionSetEnumLastCondition : uint32_t;
enum class OptionSetEnumAllCondition : uint32_t;
class WithoutNamespace;
class WithoutNamespaceWithAttributes;
struct SoftLinkedMember;
struct RequestEncodedWithBody;
struct RequestEncodedWithBodyRValue;
#if USE(SKIA)
class SkFooBar;
#endif

#if USE(CFBAR)
typedef struct __CFBar * CFBarRef;
#endif
namespace IPC { template<typename> class ObjectIdentifierReference; };
namespace IPC { template<typename> class ObjectIdentifierWriteReference; };
namespace WebKit { using RemoteVideoFrameIdentifier = AtomicObjectIdentifier<RemoteVideoFrameIdentifierType>; };
namespace WebCore {
template<typename, typename> class ScrollSnapOffsetsInfo;
using FloatBoxExtent = ScrollSnapOffsetsInfo<float, double>;
}
namespace WebKit {
using RemoteVideoFrameReference = IPC::ObjectIdentifierReference<RemoteVideoFrameIdentifier>;
}
namespace WebKit {
using RemoteVideoFrameWriteReference = IPC::ObjectIdentifierWriteReference<RemoteVideoFrameIdentifier>;
}

namespace IPC {

class Decoder;
class Encoder;
class StreamConnectionEncoder;

#if ENABLE(TEST_FEATURE)
template<> struct ArgumentCoder<Namespace::Subnamespace::StructName> {
    static void encode(Encoder&, const Namespace::Subnamespace::StructName&);
    static void encode(OtherEncoder&, const Namespace::Subnamespace::StructName&);
    static std::optional<Namespace::Subnamespace::StructName> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<Namespace::ReturnRefClass> {
    static void encode(Encoder&, const Namespace::ReturnRefClass&);
    static std::optional<Ref<Namespace::ReturnRefClass>> decode(Decoder&);
};

template<> struct ArgumentCoder<Namespace::EmptyConstructorStruct> {
    static void encode(Encoder&, const Namespace::EmptyConstructorStruct&);
    static std::optional<Namespace::EmptyConstructorStruct> decode(Decoder&);
};

template<> struct ArgumentCoder<Namespace::EmptyConstructorWithIf> {
    static void encode(Encoder&, const Namespace::EmptyConstructorWithIf&);
    static std::optional<Namespace::EmptyConstructorWithIf> decode(Decoder&);
};

template<> struct ArgumentCoder<WithoutNamespace> {
    static void encode(Encoder&, const WithoutNamespace&);
    static std::optional<WithoutNamespace> decode(Decoder&);
};

template<> struct ArgumentCoder<WithoutNamespaceWithAttributes> {
    static void encode(Encoder&, const WithoutNamespaceWithAttributes&);
    static void encode(OtherEncoder&, const WithoutNamespaceWithAttributes&);
    static std::optional<WithoutNamespaceWithAttributes> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::InheritsFrom> {
    static void encode(Encoder&, const WebCore::InheritsFrom&);
    static std::optional<WebCore::InheritsFrom> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::InheritanceGrandchild> {
    static void encode(Encoder&, const WebCore::InheritanceGrandchild&);
    static std::optional<WebCore::InheritanceGrandchild> decode(Decoder&);
};

template<> struct ArgumentCoder<WTF::Seconds> {
    static void encode(Encoder&, const WTF::Seconds&);
    static std::optional<WTF::Seconds> decode(Decoder&);
};

template<> struct ArgumentCoder<WTF::CreateUsingClass> {
    static void encode(Encoder&, const WTF::CreateUsingClass&);
    static std::optional<WTF::CreateUsingClass> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FloatBoxExtent> {
    static void encode(Encoder&, const WebCore::FloatBoxExtent&);
    static std::optional<WebCore::FloatBoxExtent> decode(Decoder&);
};

template<> struct ArgumentCoder<SoftLinkedMember> {
    static void encode(Encoder&, const SoftLinkedMember&);
    static std::optional<SoftLinkedMember> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::TimingFunction> {
    static void encode(Encoder&, const WebCore::TimingFunction&);
    static std::optional<Ref<WebCore::TimingFunction>> decode(Decoder&);
};

#if ENABLE(TEST_FEATURE)
template<> struct ArgumentCoder<Namespace::ConditionalCommonClass> {
    static void encode(Encoder&, const Namespace::ConditionalCommonClass&);
    static std::optional<Namespace::ConditionalCommonClass> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<Namespace::CommonClass> {
    static void encode(Encoder&, const Namespace::CommonClass&);
    static std::optional<Namespace::CommonClass> decode(Decoder&);
};

template<> struct ArgumentCoder<Namespace::AnotherCommonClass> {
    static void encode(Encoder&, const Namespace::AnotherCommonClass&);
    static std::optional<Ref<Namespace::AnotherCommonClass>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::MoveOnlyBaseClass> {
    static void encode(Encoder&, WebCore::MoveOnlyBaseClass&&);
    static std::optional<WebCore::MoveOnlyBaseClass> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::MoveOnlyDerivedClass> {
    static void encode(Encoder&, WebCore::MoveOnlyDerivedClass&&);
    static std::optional<WebCore::MoveOnlyDerivedClass> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::PlatformClass> {
    static void encode(Encoder&, const WebKit::PlatformClass&);
    static std::optional<WebKit::PlatformClass> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::CustomEncoded> {
    static void encode(Encoder&, const WebKit::CustomEncoded&);
    static std::optional<WebKit::CustomEncoded> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::LayerProperties> {
    static void encode(Encoder&, const WebKit::LayerProperties&);
    static std::optional<WebKit::LayerProperties> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::TemplateTest<WebKit::Fabulous>> {
    static void encode(Encoder&, const WebKit::TemplateTest<WebKit::Fabulous>&);
    static std::optional<WebKit::TemplateTest<WebKit::Fabulous>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::TemplateTest<WebCore::Amazing>> {
    static void encode(Encoder&, const WebKit::TemplateTest<WebCore::Amazing>&);
    static std::optional<WebKit::TemplateTest<WebCore::Amazing>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::TemplateTest<JSC::Incredible>> {
    static void encode(Encoder&, const WebKit::TemplateTest<JSC::Incredible>&);
    static std::optional<WebKit::TemplateTest<JSC::Incredible>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::TemplateTest<Testing::StorageSize>> {
    static void encode(Encoder&, const WebKit::TemplateTest<Testing::StorageSize>&);
    static std::optional<WebKit::TemplateTest<Testing::StorageSize>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ScrollingStateFrameHostingNode> {
    static void encode(Encoder&, const WebCore::ScrollingStateFrameHostingNode&);
    static std::optional<Ref<WebCore::ScrollingStateFrameHostingNode>> decode(Decoder&);
};

template<> struct ArgumentCoder<WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple> {
    static void encode(Encoder&, const WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple&);
    static std::optional<Ref<WebCore::ScrollingStateFrameHostingNodeWithStuffAfterTuple>> decode(Decoder&);
};

template<> struct ArgumentCoder<RequestEncodedWithBody> {
    static void encode(Encoder&, const RequestEncodedWithBody&);
    static std::optional<RequestEncodedWithBody> decode(Decoder&);
};

template<> struct ArgumentCoder<RequestEncodedWithBodyRValue> {
    static void encode(Encoder&, RequestEncodedWithBodyRValue&&);
    static std::optional<RequestEncodedWithBodyRValue> decode(Decoder&);
};

#if USE(AVFOUNDATION)
template<> struct ArgumentCoder<WebKit::CoreIPCAVOutputContext> {
    static void encode(Encoder&, const WebKit::CoreIPCAVOutputContext&);
    static std::optional<WebKit::CoreIPCAVOutputContext> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<WebKit::CoreIPCNSSomeFoundationType> {
    static void encode(Encoder&, const WebKit::CoreIPCNSSomeFoundationType&);
    static std::optional<WebKit::CoreIPCNSSomeFoundationType> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::CoreIPCclass NSSomeOtherFoundationType> {
    static void encode(Encoder&, const WebKit::CoreIPCclass NSSomeOtherFoundationType&);
    static std::optional<WebKit::CoreIPCclass NSSomeOtherFoundationType> decode(Decoder&);
};

#if ENABLE(DATA_DETECTION)
template<> struct ArgumentCoder<WebKit::CoreIPCDDScannerResult> {
    static void encode(Encoder&, const WebKit::CoreIPCDDScannerResult&);
    static std::optional<WebKit::CoreIPCDDScannerResult> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<CFFooRef> {
    static void encode(Encoder&, CFFooRef);
};
template<> struct ArgumentCoder<RetainPtr<CFFooRef>> {
    static void encode(Encoder& encoder, const RetainPtr<CFFooRef>& retainPtr)
    {
        ArgumentCoder<CFFooRef>::encode(encoder, retainPtr.get());
    }
    static std::optional<RetainPtr<CFFooRef>> decode(Decoder&);
};

#if USE(CFBAR)
template<> struct ArgumentCoder<CFBarRef> {
    static void encode(Encoder&, CFBarRef);
    static void encode(StreamConnectionEncoder&, CFBarRef);
};
template<> struct ArgumentCoder<RetainPtr<CFBarRef>> {
    static void encode(Encoder& encoder, const RetainPtr<CFBarRef>& retainPtr)
    {
        ArgumentCoder<CFBarRef>::encode(encoder, retainPtr.get());
    }
    static void encode(StreamConnectionEncoder& encoder, const RetainPtr<CFBarRef>& retainPtr)
    {
        ArgumentCoder<CFBarRef>::encode(encoder, retainPtr.get());
    }
    static std::optional<RetainPtr<CFBarRef>> decode(Decoder&);
};
#endif

#if USE(CFSTRING)
template<> struct ArgumentCoder<CFStringRef> {
    static void encode(Encoder&, CFStringRef);
    static void encode(StreamConnectionEncoder&, CFStringRef);
};
template<> struct ArgumentCoder<RetainPtr<CFStringRef>> {
    static void encode(Encoder& encoder, const RetainPtr<CFStringRef>& retainPtr)
    {
        ArgumentCoder<CFStringRef>::encode(encoder, retainPtr.get());
    }
    static void encode(StreamConnectionEncoder& encoder, const RetainPtr<CFStringRef>& retainPtr)
    {
        ArgumentCoder<CFStringRef>::encode(encoder, retainPtr.get());
    }
    static std::optional<RetainPtr<CFStringRef>> decode(Decoder&);
};
#endif

#if USE(SKIA)
template<> struct ArgumentCoder<SkFooBar> {
    static void encode(Encoder&, const SkFooBar&);
    static void encode(OtherEncoder&, const SkFooBar&);
    static std::optional<SkFooBar> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<WebKit::RValueWithFunctionCalls> {
    static void encode(Encoder&, WebKit::RValueWithFunctionCalls&&);
    static std::optional<WebKit::RValueWithFunctionCalls> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::RemoteVideoFrameReference> {
    static void encode(Encoder&, const WebKit::RemoteVideoFrameReference&);
    static void encode(StreamConnectionEncoder&, const WebKit::RemoteVideoFrameReference&);
    static std::optional<WebKit::RemoteVideoFrameReference> decode(Decoder&);
};

template<> struct ArgumentCoder<WebKit::RemoteVideoFrameWriteReference> {
    static void encode(Encoder&, const WebKit::RemoteVideoFrameWriteReference&);
    static void encode(StreamConnectionEncoder&, const WebKit::RemoteVideoFrameWriteReference&);
    static std::optional<WebKit::RemoteVideoFrameWriteReference> decode(Decoder&);
};

#if ENABLE(OUTER_CONDITION)
template<> struct ArgumentCoder<Namespace::OuterClass> {
    static void encode(Encoder&, const Namespace::OuterClass&);
    static std::optional<Namespace::OuterClass> decode(Decoder&);
};
#endif

#if !(ENABLE(OUTER_CONDITION))
template<> struct ArgumentCoder<Namespace::OtherOuterClass> {
    static void encode(Encoder&, const Namespace::OtherOuterClass&);
    static std::optional<Namespace::OtherOuterClass> decode(Decoder&);
};
#endif

#if USE(APPKIT)
template<> struct ArgumentCoder<WebCore::AppKitControlSystemImage> {
    static void encode(Encoder&, const WebCore::AppKitControlSystemImage&);
    static void encode(StreamConnectionEncoder&, const WebCore::AppKitControlSystemImage&);
    static std::optional<Ref<WebCore::AppKitControlSystemImage>> decode(Decoder&);
};
#endif

template<> struct ArgumentCoder<WebCore::RectEdges<bool>> {
    static void encode(Encoder&, const WebCore::RectEdges<bool>&);
    static std::optional<WebCore::RectEdges<bool>> decode(Decoder&);
};

} // namespace IPC


namespace WTF {

template<> bool isValidEnum<EnumWithoutNamespace>(uint8_t);
#if ENABLE(UINT16_ENUM)
template<> bool isValidEnum<EnumNamespace::EnumType>(uint16_t);
#endif
template<> bool isValidOptionSet<OptionSetEnumFirstCondition>(OptionSet<OptionSetEnumFirstCondition>);
template<> bool isValidOptionSet<OptionSetEnumLastCondition>(OptionSet<OptionSetEnumLastCondition>);
template<> bool isValidOptionSet<OptionSetEnumAllCondition>(OptionSet<OptionSetEnumAllCondition>);
#if (ENABLE(OUTER_CONDITION)) && (ENABLE(INNER_CONDITION))
template<> bool isValidEnum<EnumNamespace::InnerEnumType>(uint8_t);
#endif

} // namespace WTF
