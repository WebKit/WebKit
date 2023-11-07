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

#if ENABLE(BOOL_ENUM)
namespace EnumNamespace { enum class BoolEnumType : bool; }
#endif
enum class EnumWithoutNamespace : uint8_t;
#if ENABLE(UINT16_ENUM)
namespace EnumNamespace { enum class EnumType : uint16_t; }
#endif
enum class OptionSetEnumFirstCondition : uint32_t;
enum class OptionSetEnumLastCondition : uint32_t;
enum class OptionSetEnumAllCondition : uint32_t;
#if ENABLE(TEST_FEATURE)
namespace Namespace::Subnamespace { struct StructName; }
#endif
namespace Namespace { class ReturnRefClass; }
namespace Namespace { struct EmptyConstructorStruct; }
namespace Namespace { class EmptyConstructorWithIf; }
class WithoutNamespace;
class WithoutNamespaceWithAttributes;
namespace WebCore { class InheritsFrom; }
namespace WebCore { class InheritanceGrandchild; }
namespace WTF { class Seconds; }
namespace WTF { class CreateUsingClass; }
namespace WebCore {
template<typename, typename> class ScrollSnapOffsetsInfo;
using FloatBoxExtent = ScrollSnapOffsetsInfo<float, double>;
}
struct SoftLinkedMember;
namespace WebCore { class TimingFunction; }
#if ENABLE(TEST_FEATURE)
namespace Namespace { class ConditionalCommonClass; }
#endif
namespace Namespace { class CommonClass; }
namespace Namespace { class AnotherCommonClass; }
namespace WebCore { class MoveOnlyBaseClass; }
namespace WebCore { class MoveOnlyDerivedClass; }
namespace WebKit { class PlatformClass; }
namespace WebKit { class CustomEncoded; }
namespace WebKit { class LayerProperties; }
namespace WebKit { class Fabulous; }
namespace WebCore { struct Amazing; }
namespace JSC { enum class Incredible; }
namespace Testing { enum class StorageSize : uint8_t; }
namespace WebCore { class ScrollingStateFrameHostingNode; }
namespace WebCore { class ScrollingStateFrameHostingNodeWithStuffAfterTuple; }
#if USE(CFBAR)
#endif
namespace WebKit { class RValueWithFunctionCalls; }
#if USE(CFBAR)
typedef struct __CFBar * CFBarRef;
#endif

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

template<> struct ArgumentCoder<WebKit::RValueWithFunctionCalls> {
    static void encode(Encoder&, WebKit::RValueWithFunctionCalls&&);
    static std::optional<WebKit::RValueWithFunctionCalls> decode(Decoder&);
};

} // namespace IPC


namespace WTF {

template<> bool isValidEnum<EnumWithoutNamespace, void>(uint8_t);
#if ENABLE(UINT16_ENUM)
template<> bool isValidEnum<EnumNamespace::EnumType, void>(uint16_t);
#endif
template<> bool isValidOptionSet<OptionSetEnumFirstCondition>(OptionSet<OptionSetEnumFirstCondition>);
template<> bool isValidOptionSet<OptionSetEnumLastCondition>(OptionSet<OptionSetEnumLastCondition>);
template<> bool isValidOptionSet<OptionSetEnumAllCondition>(OptionSet<OptionSetEnumAllCondition>);

} // namespace WTF
