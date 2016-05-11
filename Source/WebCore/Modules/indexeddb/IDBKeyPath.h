/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;

enum class IDBKeyPathParseError {
    None,
    Start,
    Identifier,
    Dot,
};

void IDBParseKeyPath(const String&, Vector<String>&, IDBKeyPathParseError&);

class IDBKeyPath {
public:
    IDBKeyPath() { }
    WEBCORE_EXPORT IDBKeyPath(const String&);
    WEBCORE_EXPORT IDBKeyPath(const Vector<String>& array);

    enum class Type { Null, String, Array };
    Type type() const { return m_type; }

    const Vector<String>& array() const
    {
        ASSERT(m_type == Type::Array);
        return m_array;
    }

    const String& string() const
    {
        ASSERT(m_type == Type::String);
        return m_string;
    }

    bool isNull() const { return m_type == Type::Null; }
    bool isValid() const;
    bool operator==(const IDBKeyPath& other) const;

    IDBKeyPath isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBKeyPath&);
    
    WEBCORE_EXPORT void encode(KeyedEncoder&) const;
    WEBCORE_EXPORT static bool decode(KeyedDecoder&, IDBKeyPath&);

private:
    Type m_type { Type::Null };
    String m_string;
    Vector<String> m_array;
};

template<class Encoder> void IDBKeyPath::encode(Encoder& encoder) const
{
    encoder.encodeEnum(m_type);

    switch (m_type) {
    case Type::Null:
        break;
    case Type::String:
        encoder << m_string;
        break;
    case Type::Array:
        encoder << m_array;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

template<class Decoder> bool IDBKeyPath::decode(Decoder& decoder, IDBKeyPath& keyPath)
{
    Type type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case Type::Null:
        keyPath = IDBKeyPath();
        return true;

    case Type::String: {
        String string;
        if (!decoder.decode(string))
            return false;

        keyPath = IDBKeyPath(string);
        return true;
    }
    case Type::Array: {
        Vector<String> array;
        if (!decoder.decode(array))
            return false;

        keyPath = IDBKeyPath(array);
        return true;
    }
    default:
        return true;
    }
}

} // namespace WebCore

#endif
