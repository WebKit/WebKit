/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "ImmutableArray.h"
#include "ImmutableDictionary.h"
#include "WebCoreArgumentCoders.h"
#include "WebNumber.h"
#include "WebSerializedScriptValue.h"
#include "WebString.h"

namespace WebKit {

//   - Null -> Null
//   - Array -> Array
//   - Dictionary -> Dictionary
//   - String -> String
//   - SerializedScriptValue -> SerializedScriptValue
//   - WebDouble -> WebDouble
//   - WebUInt64 -> WebUInt64

template<typename Owner>
class UserMessageEncoder {
public:
    bool baseEncode(CoreIPC::ArgumentEncoder* encoder, APIObject::Type& type) const 
    {
        if (!m_root) {
            encoder->encodeUInt32(APIObject::TypeNull);
            return true;
        }

        type = m_root->type();
        encoder->encodeUInt32(type);

        switch (type) {
        case APIObject::TypeArray: {
            ImmutableArray* array = static_cast<ImmutableArray*>(m_root);
            encoder->encode(static_cast<uint64_t>(array->size()));
            for (size_t i = 0; i < array->size(); ++i)
                encoder->encode(Owner(array->at(i)));
            return true;
        }
        case APIObject::TypeDictionary: {
            ImmutableDictionary* dictionary = static_cast<ImmutableDictionary*>(m_root);
            const ImmutableDictionary::MapType& map = dictionary->map();
            encoder->encode(static_cast<uint64_t>(map.size()));

            ImmutableDictionary::MapType::const_iterator it = map.begin();
            ImmutableDictionary::MapType::const_iterator end = map.end();
            for (; it != end; ++it) {
                encoder->encode(it->first);
                encoder->encode(Owner(it->second.get()));
            }
            return true;
        }
        case APIObject::TypeString: {
            WebString* string = static_cast<WebString*>(m_root);
            encoder->encode(string->string());
            return true;
        }
        case APIObject::TypeSerializedScriptValue: {
            WebSerializedScriptValue* scriptValue = static_cast<WebSerializedScriptValue*>(m_root);
            encoder->encodeBytes(scriptValue->data().data(), scriptValue->data().size());
            return true;
        }
        case APIObject::TypeDouble: {
            WebDouble* doubleObject = static_cast<WebDouble*>(m_root);
            encoder->encode(doubleObject->value());
            return true;
        }
        case APIObject::TypeUInt64: {
            WebUInt64* uint64Object = static_cast<WebUInt64*>(m_root);
            encoder->encode(uint64Object->value());
            return true;
        }
        default:
            break;
        }

        return false;
    }

protected:
    UserMessageEncoder(APIObject* root) 
        : m_root(root)
    {
    }

    APIObject* m_root;
};


// Handles
//   - Null -> Null
//   - Array -> Array
//   - Dictionary -> Dictionary
//   - String -> String
//   - SerializedScriptValue -> SerializedScriptValue
//   - WebDouble -> WebDouble
//   - WebUInt64 -> WebUInt64

template<typename Owner>
class UserMessageDecoder {
public:
    static bool baseDecode(CoreIPC::ArgumentDecoder* decoder, Owner& coder, APIObject::Type& type)
    {
        uint32_t typeAsUInt32;
        if (!decoder->decode(typeAsUInt32))
            return false;

        type = static_cast<APIObject::Type>(typeAsUInt32);

        switch (type) {
        case APIObject::TypeArray: {
            uint64_t size;
            if (!decoder->decode(size))
                return false;

            Vector<RefPtr<APIObject> > vector;
            for (size_t i = 0; i < size; ++i) {
                RefPtr<APIObject> element;
                Owner messageCoder(coder, element);
                if (!decoder->decode(messageCoder))
                    return false;
                vector.append(element.release());
            }

            coder.m_root = ImmutableArray::adopt(vector);
            break;
        }
        case APIObject::TypeDictionary: {
            uint64_t size;
            if (!decoder->decode(size))
                return false;

            ImmutableDictionary::MapType map;
            for (size_t i = 0; i < size; ++i) {
                String key;
                if (!decoder->decode(key))
                    return false;

                RefPtr<APIObject> element;
                Owner messageCoder(coder, element);
                if (!decoder->decode(messageCoder))
                    return false;

                std::pair<ImmutableDictionary::MapType::iterator, bool> result = map.set(key, element.release());
                if (!result.second)
                    return false;
            }

            coder.m_root = ImmutableDictionary::adopt(map);
            break;
        }
        case APIObject::TypeString: {
            String string;
            if (!decoder->decode(string))
                return false;
            coder.m_root = WebString::create(string);
            break;
        }
        case APIObject::TypeSerializedScriptValue: {
            Vector<uint8_t> buffer;
            if (!decoder->decodeBytes(buffer))
                return false;
            coder.m_root = WebSerializedScriptValue::adopt(buffer);
            break;
        }
        case APIObject::TypeDouble: {
            double value;
            if (!decoder->decode(value))
                return false;
            coder.m_root = WebDouble::create(value);
            break;
        }
        case APIObject::TypeUInt64: {
            uint64_t value;
            if (!decoder->decode(value))
                return false;
            coder.m_root = WebUInt64::create(value);
            break;
        }
        default:
            break;
        }

        return true;
    }

protected:
    UserMessageDecoder(RefPtr<APIObject>& root)
        : m_root(root)
    {
    }

    RefPtr<APIObject>& m_root;
};

} // namespace WebKit
