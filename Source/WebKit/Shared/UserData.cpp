/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
#include "APIDictionary.h"
#include "APIError.h"
#include "APIFrameHandle.h"
#include "APIGeometry.h"
#include "APINumber.h"
#include "APIPageHandle.h"
#include "APISerializedScriptValue.h"
#include "APIString.h"
#include "APIURL.h"
#include "APIURLRequest.h"
#include "APIURLResponse.h"
#include "APIUserContentURLPattern.h"
#include "ArgumentCoders.h"
#include "Encoder.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebImage.h"
#include <wtf/CheckedArithmetic.h>

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
#endif

namespace WebKit {

UserData::UserData() = default;

UserData::UserData(RefPtr<API::Object>&& object)
    : m_object(WTFMove(object))
{
}

UserData::~UserData() = default;

static bool shouldTransform(const API::Object& object, const UserData::Transformer& transformer)
{
    if (object.type() == API::Object::Type::Array) {
        const auto& array = static_cast<const API::Array&>(object);

        for (const auto& element : array.elements()) {
            if (!element)
                continue;

            if (shouldTransform(*element, transformer))
                return true;
        }
    }

    if (object.type() == API::Object::Type::Dictionary) {
        const auto& dictionary = static_cast<const API::Dictionary&>(object);

        for (const auto& keyValuePair : dictionary.map()) {
            if (!keyValuePair.value)
                continue;

            if (shouldTransform(Ref { *keyValuePair.value }, transformer))
                return true;
        }
    }

    return transformer.shouldTransformObject(object);
}

static RefPtr<API::Object> transformGraph(API::Object& object, const UserData::Transformer& transformer)
{
    if (object.type() == API::Object::Type::Array) {
        auto& array = static_cast<API::Array&>(object);

        auto elements = array.elements().map([&](auto& element) -> RefPtr<API::Object> {
            if (!element)
                return nullptr;
            return transformGraph(*element, transformer);
        });
        return API::Array::create(WTFMove(elements));
    }

    if (object.type() == API::Object::Type::Dictionary) {
        auto& dictionary = static_cast<API::Dictionary&>(object);

        API::Dictionary::MapType map;
        for (const auto& keyValuePair : dictionary.map()) {
            RefPtr value = keyValuePair.value;
            if (!value)
                map.add(keyValuePair.key, nullptr);
            else
                map.add(keyValuePair.key, transformGraph(*value, transformer));
        }
        return API::Dictionary::create(WTFMove(map));
    }

    return transformer.transformObject(object);
}

RefPtr<API::Object> UserData::transform(API::Object* object, const Transformer& transformer)
{
    if (!object)
        return nullptr;

    if (!shouldTransform(*object, transformer))
        return object;

    return transformGraph(*object, transformer);
}

void UserData::encode(IPC::Encoder& encoder) const
{
    encode(encoder, m_object.get());
}

bool UserData::decode(IPC::Decoder& decoder, UserData& userData)
{
    return decode(decoder, userData.m_object);
}

void UserData::encode(IPC::Encoder& encoder, const API::Object* object)
{
    if (!object) {
        encoder << API::Object::Type::Null;
        return;
    }

    encode(encoder, *object);
}

void UserData::encode(IPC::Encoder& encoder, const API::Object& object)
{
    API::Object::Type type = object.type();
    encoder << type;

    switch (object.type()) {
    case API::Object::Type::Array:
        encoder << static_cast<const API::Array&>(object);
        break;

    case API::Object::Type::Boolean:
        encoder << static_cast<const API::Boolean&>(object);
        break;

    case API::Object::Type::Data:
        encoder << static_cast<const API::Data&>(object);
        break;
    
    case API::Object::Type::Dictionary:
        encoder << static_cast<const API::Dictionary&>(object);
        break;

    case API::Object::Type::Double:
        encoder << static_cast<const API::Double&>(object);
        break;

    case API::Object::Type::Error:
        encoder << static_cast<const API::Error&>(object);
        break;

    case API::Object::Type::FrameHandle:
        encoder << static_cast<const API::FrameHandle&>(object);
        break;

    case API::Object::Type::Image:
        encoder << static_cast<const WebImage&>(object);
        break;

    case API::Object::Type::PageHandle:
        encoder << static_cast<const API::PageHandle&>(object);
        break;

    case API::Object::Type::Point:
        encoder << static_cast<const API::Point&>(object);
        break;

    case API::Object::Type::Rect:
        encoder << static_cast<const API::Rect&>(object);
        break;

    case API::Object::Type::SerializedScriptValue:
        encoder << static_cast<const API::SerializedScriptValue&>(object);
        break;

    case API::Object::Type::Size:
        encoder << static_cast<const API::Size&>(object);
        break;

    case API::Object::Type::String:
        encoder << static_cast<const API::String&>(object);
        break;

    case API::Object::Type::URL:
        encoder << static_cast<const API::URL&>(object);
        break;

    case API::Object::Type::URLRequest:
        encoder << static_cast<const API::URLRequest&>(object);
        break;

    case API::Object::Type::URLResponse:
        encoder << static_cast<const API::URLResponse&>(object);
        break;

    case API::Object::Type::UInt64:
        encoder << static_cast<const API::UInt64&>(object);
        break;

    case API::Object::Type::Int64:
        encoder << static_cast<const API::Int64&>(object);
        break;

    case API::Object::Type::UserContentURLPattern:
        encoder << static_cast<const API::UserContentURLPattern&>(object);
        break;

#if PLATFORM(COCOA)
    case API::Object::Type::ObjCObjectGraph:
        encoder << static_cast<const ObjCObjectGraph&>(object);
        break;
#endif

    default:
        ASSERT_NOT_REACHED();
    }
}

bool UserData::decode(IPC::Decoder& decoder, RefPtr<API::Object>& result)
{
    API::Object::Type type;
    if (!decoder.decode(type))
        return false;

    switch (type) {
    case API::Object::Type::Array: {
        auto array = decoder.decode<Ref<API::Array>>();
        if (!array)
            return false;
        result = WTFMove(*array);
        break;
    }

    case API::Object::Type::Boolean: {
        auto data = decoder.decode<Ref<API::Boolean>>();
        if (!data)
            return false;
        result = WTFMove(*data);
        break;
    }

    case API::Object::Type::Data: {
        auto data = decoder.decode<Ref<API::Data>>();
        if (!data)
            return false;
        result = WTFMove(*data);
        break;
    }

    case API::Object::Type::Dictionary: {
        auto dictionary = decoder.decode<Ref<API::Dictionary>>();
        if (!dictionary)
            return false;
        result = WTFMove(*dictionary);
        break;
    }

    case API::Object::Type::Double: {
        auto data = decoder.decode<Ref<API::Double>>();
        if (!data)
            return false;
        result = WTFMove(*data);
        break;
    }

    case API::Object::Type::Error: {
        std::optional<Ref<API::Error>> error;
        decoder >> error;
        if (!error)
            return false;
        result = WTFMove(*error);
        break;
    }

    case API::Object::Type::FrameHandle: {
        std::optional<Ref<API::FrameHandle>> frameHandle;
        decoder >> frameHandle;
        if (!frameHandle)
            return false;
        result = WTFMove(*frameHandle);
        break;
    }

    case API::Object::Type::Image: {
        std::optional<Ref<WebKit::WebImage>> image;
        decoder >> image;
        if (!image)
            return false;
        result = WTFMove(*image);
        break;
    }

    case API::Object::Type::Null:
        result = nullptr;
        break;

    case API::Object::Type::PageHandle: {
        std::optional<Ref<API::PageHandle>> pageHandle;
        decoder >> pageHandle;
        if (!pageHandle)
            return false;
        result = WTFMove(*pageHandle);
        break;
    }

    case API::Object::Type::Point: {
        std::optional<Ref<API::Point>> point;
        decoder >> point;
        if (!point)
            return false;
        result = WTFMove(*point);
        break;
    }

    case API::Object::Type::Rect: {
        std::optional<Ref<API::Rect>> rect;
        decoder >> rect;
        if (!rect)
            return false;
        result = WTFMove(*rect);
        break;
    }

    case API::Object::Type::SerializedScriptValue: {
        std::optional<Ref<API::SerializedScriptValue>> value;
        decoder >> value;
        if (!value)
            return false;
        result = WTFMove(*value);
        break;
    }

    case API::Object::Type::Size: {
        std::optional<Ref<API::Size>> size;
        decoder >> size;
        if (!size)
            return false;
        result = WTFMove(*size);
        break;
    }

    case API::Object::Type::String: {
        std::optional<Ref<API::String>> string;
        decoder >> string;
        if (!string)
            return false;
        result = WTFMove(*string);
        break;
    }

    case API::Object::Type::URL: {
        std::optional<Ref<API::URL>> url;
        decoder >> url;
        if (!url)
            return false;
        result = WTFMove(*url);
        break;
    }

    case API::Object::Type::URLRequest: {
        std::optional<Ref<API::URLRequest>> urlRequest;
        decoder >> urlRequest;
        if (!urlRequest)
            return false;
        result = WTFMove(*urlRequest);
        break;
    }

    case API::Object::Type::URLResponse: {
        std::optional<Ref<API::URLResponse>> urlResponse;
        decoder >> urlResponse;
        if (!urlResponse)
            return false;
        result = WTFMove(*urlResponse);
        break;
    }

    case API::Object::Type::UInt64: {
        auto data = decoder.decode<Ref<API::UInt64>>();
        if (!data)
            return false;
        result = WTFMove(*data);
        break;
    }

    case API::Object::Type::Int64: {
        auto data = decoder.decode<Ref<API::Int64>>();
        if (!data)
            return false;
        result = WTFMove(*data);
        break;
    }

    case API::Object::Type::UserContentURLPattern: {
        auto data = decoder.decode<Ref<API::UserContentURLPattern>>();
        if (!data)
            return false;
        result = WTFMove(*data);
        break;
    }

#if PLATFORM(COCOA)
    case API::Object::Type::ObjCObjectGraph: {
        auto graph = decoder.decode<Ref<WebKit::ObjCObjectGraph>>();
        if (!graph)
            return false;
        result = WTFMove(*graph);
        break;
    }
#endif

    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

} // namespace WebKit
