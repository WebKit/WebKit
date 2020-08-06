/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGPU)

#include <wtf/Variant.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

namespace WHLSL {

namespace Metal {

struct MangledVariableName {
    explicit operator bool() const { return value != std::numeric_limits<unsigned>::max(); }
    unsigned value { std::numeric_limits<unsigned>::max() };
    static constexpr const char* prefix = "variable";
};

struct MangledTypeName {
    unsigned value;
    static constexpr const char* prefix = "type";
};

struct MangledStructureElementName {
    unsigned value;
    static constexpr const char* prefix = "structureElement";
};

struct MangledEnumerationMemberName {
    unsigned value;
    static constexpr const char* prefix = "enumerationMember";
};

struct MangledFunctionName {
    unsigned value;
    static constexpr const char* prefix = "function";

    String toString() const { return makeString(prefix, value); }
};

using MangledOrNativeTypeName = Variant<MangledTypeName, String>;

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

namespace WTF {

template<typename MangledNameType>
class MangledNameAdaptor {
public:
    MangledNameAdaptor(MangledNameType name)
        : m_name { name }
    {
    }

    unsigned length() { return strlen(MangledNameType::prefix) + lengthOfIntegerAsString(m_name.value); }
    bool is8Bit() { return true; }
    template<typename CharacterType> void writeTo(CharacterType* destination)
    {
        StringImpl::copyCharacters(destination, reinterpret_cast<const LChar*>(MangledNameType::prefix), strlen(MangledNameType::prefix));
        writeIntegerToBuffer(m_name.value, destination + strlen(MangledNameType::prefix));
    }

private:
    MangledNameType m_name;
};

template<> class StringTypeAdapter<WebCore::WHLSL::Metal::MangledVariableName, void> : public MangledNameAdaptor<WebCore::WHLSL::Metal::MangledVariableName> {
public:
    StringTypeAdapter(WebCore::WHLSL::Metal::MangledVariableName name)
        : MangledNameAdaptor(name)
    {
    }
};
template<> class StringTypeAdapter<WebCore::WHLSL::Metal::MangledTypeName, void> : public MangledNameAdaptor<WebCore::WHLSL::Metal::MangledTypeName> {
public:
    StringTypeAdapter(WebCore::WHLSL::Metal::MangledTypeName name)
        : MangledNameAdaptor(name)
    {
    }
};
template<> class StringTypeAdapter<WebCore::WHLSL::Metal::MangledStructureElementName, void> : public MangledNameAdaptor<WebCore::WHLSL::Metal::MangledStructureElementName> {
public:
    StringTypeAdapter(WebCore::WHLSL::Metal::MangledStructureElementName name)
        : MangledNameAdaptor(name)
    {
    }
};
template<> class StringTypeAdapter<WebCore::WHLSL::Metal::MangledEnumerationMemberName, void> : public MangledNameAdaptor<WebCore::WHLSL::Metal::MangledEnumerationMemberName> {
public:
    StringTypeAdapter(WebCore::WHLSL::Metal::MangledEnumerationMemberName name)
        : MangledNameAdaptor(name)
    {
    }
};
template<> class StringTypeAdapter<WebCore::WHLSL::Metal::MangledFunctionName, void> : public MangledNameAdaptor<WebCore::WHLSL::Metal::MangledFunctionName> {
public:
    StringTypeAdapter(WebCore::WHLSL::Metal::MangledFunctionName name)
        : MangledNameAdaptor(name)
    {
    }
};

template<> class StringTypeAdapter<WebCore::WHLSL::Metal::MangledOrNativeTypeName, void> {
public:
    StringTypeAdapter(const WebCore::WHLSL::Metal::MangledOrNativeTypeName& name)
        : m_name { name }
    {
    }

    unsigned length()
    {
        return WTF::switchOn(m_name,
            [&] (const WebCore::WHLSL::Metal::MangledTypeName& mangledTypeName) {
                return strlen(WebCore::WHLSL::Metal::MangledTypeName::prefix) + lengthOfIntegerAsString(mangledTypeName.value);
            },
            [&] (const String& string) {
                return string.length();
            }
        );
    }

    bool is8Bit()
    {
        return WTF::switchOn(m_name,
            [&] (const WebCore::WHLSL::Metal::MangledTypeName&) {
                return true;
            },
            [&] (const String& string) {
                return string.is8Bit();
            }
        );
    }

    template<typename CharacterType> void writeTo(CharacterType* destination)
    {
        WTF::switchOn(m_name,
            [&] (const WebCore::WHLSL::Metal::MangledTypeName& mangledTypeName) {
                StringImpl::copyCharacters(destination, reinterpret_cast<const LChar*>(WebCore::WHLSL::Metal::MangledTypeName::prefix), strlen(WebCore::WHLSL::Metal::MangledTypeName::prefix));
                writeIntegerToBuffer(mangledTypeName.value, destination + strlen(WebCore::WHLSL::Metal::MangledTypeName::prefix));
            },
            [&] (const String& string) {
                StringView { string }.getCharactersWithUpconvert(destination);
            }
        );
    }

private:
    const WebCore::WHLSL::Metal::MangledOrNativeTypeName& m_name;
};

}

#endif
