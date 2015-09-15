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

#ifndef IDBKeyPath_h
#define IDBKeyPath_h

#if ENABLE(INDEXED_DATABASE)

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;

enum IDBKeyPathParseError {
    IDBKeyPathParseErrorNone,
    IDBKeyPathParseErrorStart,
    IDBKeyPathParseErrorIdentifier,
    IDBKeyPathParseErrorDot,
};

void IDBParseKeyPath(const String&, Vector<String>&, IDBKeyPathParseError&);

class IDBKeyPath {
public:
    IDBKeyPath() : m_type(NullType) { }
    WEBCORE_EXPORT explicit IDBKeyPath(const String&);
    WEBCORE_EXPORT explicit IDBKeyPath(const Vector<String>& array);

    enum Type {
        NullType = 0,
        StringType,
        ArrayType
    };

    Type type() const { return m_type; }

    const Vector<String>& array() const
    {
        ASSERT(m_type == ArrayType);
        return m_array;
    }

    const String& string() const
    {
        ASSERT(m_type == StringType);
        return m_string;
    }

    bool isNull() const { return m_type == NullType; }
    bool isValid() const;
    bool operator==(const IDBKeyPath& other) const;

    IDBKeyPath isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBKeyPath&);
    
    WEBCORE_EXPORT void encode(KeyedEncoder&) const;
    WEBCORE_EXPORT static bool decode(KeyedDecoder&, IDBKeyPath&);

private:
    Type m_type;
    String m_string;
    Vector<String> m_array;
};

template<class Encoder>
void IDBKeyPath::encode(Encoder& encoder) const
{
    encoder.encodeEnum(m_type);

    switch (m_type) {
    case IDBKeyPath::NullType:
        break;
    case IDBKeyPath::StringType:
        encoder << m_string;
        break;
    case IDBKeyPath::ArrayType:
        encoder << m_array;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

template<class Decoder>
bool IDBKeyPath::decode(Decoder& decoder, IDBKeyPath& keyPath)
{
    IDBKeyPath::Type type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case IDBKeyPath::NullType:
        keyPath = IDBKeyPath();
        return true;

    case IDBKeyPath::StringType: {
        String string;
        if (!decoder.decode(string))
            return false;

        keyPath = IDBKeyPath(string);
        return true;
    }
    case IDBKeyPath::ArrayType: {
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

#endif // IDBKeyPath_h
