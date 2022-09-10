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

#pragma once

#include <wtf/ArgumentCoder.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>

namespace EnumNamespace { enum class EnumType : uint16_t; }
namespace EnumNamespace2 { enum class OptionSetEnumType : uint8_t; }
#if ENABLE(TEST_FEATURE)
namespace Namespace::Subnamespace { struct StructName; }
#endif
namespace Namespace { class OtherClass; }
namespace Namespace { class ReturnRefClass; }
namespace Namespace { struct EmptyConstructorStruct; }
namespace Namespace { class EmptyConstructorNullable; }
class WithoutNamespace;
class WithoutNamespaceWithAttributes;

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

template<> struct ArgumentCoder<Namespace::OtherClass> {
    static void encode(Encoder&, const Namespace::OtherClass&);
    static std::optional<Namespace::OtherClass> decode(Decoder&);
};

template<> struct ArgumentCoder<Namespace::ReturnRefClass> {
    static void encode(Encoder&, const Namespace::ReturnRefClass&);
    static std::optional<Ref<Namespace::ReturnRefClass>> decode(Decoder&);
};

template<> struct ArgumentCoder<Namespace::EmptyConstructorStruct> {
    static void encode(Encoder&, const Namespace::EmptyConstructorStruct&);
    static std::optional<Namespace::EmptyConstructorStruct> decode(Decoder&);
};

template<> struct ArgumentCoder<Namespace::EmptyConstructorNullable> {
    static void encode(Encoder&, const Namespace::EmptyConstructorNullable&);
    static std::optional<Namespace::EmptyConstructorNullable> decode(Decoder&);
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

} // namespace IPC


namespace WTF {

template<> bool isValidEnum<EnumNamespace::EnumType>(uint16_t);
template<> bool isValidOptionSet<EnumNamespace2::OptionSetEnumType>(OptionSet<EnumNamespace2::OptionSetEnumType>);

} // namespace WTF
