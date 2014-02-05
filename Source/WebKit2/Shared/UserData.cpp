/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "UserData.h"

#include "APIArray.h"
#include "APIData.h"
#include "APIError.h"
#include "APIFrameHandle.h"
#include "APIGeometry.h"
#include "APINumber.h"
#include "APIString.h"
#include "APIURL.h"
#include "APIURLRequest.h"
#include "APIURLResponse.h"
#include "ArgumentCoders.h"
#include "ArgumentEncoder.h"
#include "MutableDictionary.h"
#include "WebSerializedScriptValue.h"

namespace WebKit {

UserData::UserData(API::Object* object)
    : m_object(object)
{
}

UserData::~UserData()
{
}

RefPtr<API::Object> UserData::transform(API::Object* object, const std::function<RefPtr<API::Object> (const API::Object&)> transformer)
{
    if (!object)
        return nullptr;

    if (object->type() == API::Object::Type::Array) {
        auto& array = static_cast<API::Array&>(*object);

        Vector<RefPtr<API::Object>> elements;
        elements.reserveInitialCapacity(array.elements().size());
        for (const auto& element : array.elements())
            elements.uncheckedAppend(transform(element.get(), transformer));

        return API::Array::create(std::move(elements));
    }

    if (object->type() == API::Object::Type::Dictionary) {
        auto& dictionary = static_cast<ImmutableDictionary&>(*object);

        ImmutableDictionary::MapType map;
        for (const auto& keyValuePair : dictionary.map())
            map.add(keyValuePair.key, transform(keyValuePair.value.get(), transformer));

        return ImmutableDictionary::create(std::move(map));
    }

    if (auto transformedObject = transformer(*object))
        return transformedObject;

    return object;
}

void UserData::encode(IPC::ArgumentEncoder& encoder) const
{
    encode(encoder, m_object.get());
}

bool UserData::decode(IPC::ArgumentDecoder& decoder, UserData& userData)
{
    return decode(decoder, userData.m_object);
}

void UserData::encode(IPC::ArgumentEncoder& encoder, const API::Object* object) const
{
    if (!object) {
        encoder.encodeEnum(API::Object::Type::Null);
        return;
    }

    encode(encoder, *object);
}

void UserData::encode(IPC::ArgumentEncoder& encoder, const API::Object& object) const
{
    API::Object::Type type = object.type();
    encoder.encodeEnum(type);

    switch (object.type()) {
    case API::Object::Type::Array: {
        auto& array = static_cast<const API::Array&>(object);
        encoder << static_cast<uint64_t>(array.size());
        for (size_t i = 0; i < array.size(); ++i)
            encode(encoder, array.at(i));
        break;
    }

    case API::Object::Type::Boolean:
        static_cast<const API::Boolean&>(object).encode(encoder);
        break;

    case API::Object::Type::Data:
        static_cast<const API::Data&>(object).encode(encoder);
        break;
    
    case API::Object::Type::Dictionary: {
        auto& dictionary = static_cast<const ImmutableDictionary&>(object);
        auto& map = dictionary.map();

        encoder << static_cast<uint64_t>(map.size());
        for (const auto& keyValuePair : map) {
            encoder << keyValuePair.key;
            encode(encoder, keyValuePair.value.get());
        }
        break;
    }

    case API::Object::Type::Double:
        static_cast<const API::Double&>(object).encode(encoder);
        break;

    case API::Object::Type::Error:
        static_cast<const API::Error&>(object).encode(encoder);
        break;

    case API::Object::Type::FrameHandle: {
        auto& frameHandle = static_cast<const API::FrameHandle&>(object);
        encoder << frameHandle.frameID();
        break;
    }

    case API::Object::Type::Point:
        static_cast<const API::Point&>(object).encode(encoder);
        break;

    case API::Object::Type::Rect:
        static_cast<const API::Rect&>(object).encode(encoder);
        break;

    case API::Object::Type::SerializedScriptValue: {
        auto& serializedScriptValue = static_cast<const WebSerializedScriptValue&>(object);
        encoder << serializedScriptValue.dataReference();
        break;
    }

    case API::Object::Type::Size:
        static_cast<const API::Size&>(object).encode(encoder);
        break;

    case API::Object::Type::String: {
        auto& string = static_cast<const API::String&>(object);
        encoder << string.string();
        break;
    }

    case API::Object::Type::URL: {
        static_cast<const API::URL&>(object).encode(encoder);
        break;
    }

    case API::Object::Type::URLRequest:
        static_cast<const API::URLRequest&>(object).encode(encoder);
        break;

    case API::Object::Type::URLResponse:
        static_cast<const API::URLResponse&>(object).encode(encoder);
        break;

    case API::Object::Type::UInt64:
        static_cast<const API::UInt64&>(object).encode(encoder);
        break;

    default:
        ASSERT_NOT_REACHED();
    }
}

bool UserData::decode(IPC::ArgumentDecoder& decoder, RefPtr<API::Object>& result)
{
    API::Object::Type type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case API::Object::Type::Array: {
        uint64_t size;
        if (!decoder.decode(size))
            return false;

        Vector<RefPtr<API::Object>> elements;
        for (size_t i = 0; i < size; ++i) {
            RefPtr<API::Object> element;
            if (!decode(decoder, element))
                return false;

            elements.append(std::move(element));
        }

        result = API::Array::create(std::move(elements));
        break;
    }

    case API::Object::Type::Boolean:
        if (!API::Boolean::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Data:
        if (!API::Data::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Dictionary: {
        uint64_t size;
        if (!decoder.decode(size))
            return false;

        ImmutableDictionary::MapType map;
        for (size_t i = 0; i < size; ++i) {
            String key;
            if (!decoder.decode(key))
                return false;

            RefPtr<API::Object> value;
            if (!decode(decoder, value))
                return false;

            if (!map.add(std::move(key), std::move(value)).isNewEntry)
                return false;
        }

        result = ImmutableDictionary::create(std::move(map));
        break;
    }

    case API::Object::Type::Double:
        if (!API::Double::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Error:
        if (!API::Error::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::FrameHandle: {
        uint64_t frameID;
        if (!decoder.decode(frameID))
            return false;

        result = API::FrameHandle::create(frameID);
        break;
    }

    case API::Object::Type::Null:
        result = nullptr;
        break;
        
    case API::Object::Type::Point:
        if (!API::Point::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Rect:
        if (!API::Rect::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::SerializedScriptValue: {
        IPC::DataReference dataReference;
        if (!decoder.decode(dataReference))
            return false;

        auto vector = dataReference.vector();
        result = WebSerializedScriptValue::adopt(vector);
        break;
    }

    case API::Object::Type::Size:
        if (!API::Size::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::String: {
        String string;
        if (!decoder.decode(string))
            return false;

        result = API::String::create(string);
        break;
    }

    case API::Object::Type::URL:
        if (!API::URL::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::URLRequest:
        if (!API::URLRequest::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::URLResponse:
        if (!API::URLResponse::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::UInt64:
        if (!API::UInt64::decode(decoder, result))
            return false;
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    return true;
}

} // namespace WebKit
