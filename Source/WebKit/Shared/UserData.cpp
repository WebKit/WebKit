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
#include "APIDictionary.h"
#include "APIError.h"
#include "APIFrameHandle.h"
#include "APIGeometry.h"
#include "APINumber.h"
#include "APIPageGroupHandle.h"
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
#include "WebCertificateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebImage.h"
#include "WebRenderLayer.h"
#include "WebRenderObject.h"
#include <wtf/CheckedArithmetic.h>

#if PLATFORM(COCOA)
#include "ObjCObjectGraph.h"
#endif

namespace WebKit {

UserData::UserData()
{
}

UserData::UserData(RefPtr<API::Object>&& object)
    : m_object(WTFMove(object))
{
}

UserData::~UserData()
{
}

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

            if (shouldTransform(*keyValuePair.value, transformer))
                return true;
        }
    }

    return transformer.shouldTransformObject(object);
}

static RefPtr<API::Object> transformGraph(API::Object& object, const UserData::Transformer& transformer)
{
    if (object.type() == API::Object::Type::Array) {
        auto& array = static_cast<API::Array&>(object);

        Vector<RefPtr<API::Object>> elements;
        elements.reserveInitialCapacity(array.elements().size());
        for (const auto& element : array.elements()) {
            if (!element)
                elements.uncheckedAppend(nullptr);
            else
                elements.uncheckedAppend(transformGraph(*element, transformer));
        }

        return API::Array::create(WTFMove(elements));
    }

    if (object.type() == API::Object::Type::Dictionary) {
        auto& dictionary = static_cast<API::Dictionary&>(object);

        API::Dictionary::MapType map;
        for (const auto& keyValuePair : dictionary.map()) {
            if (!keyValuePair.value)
                map.add(keyValuePair.key, nullptr);
            else
                map.add(keyValuePair.key, transformGraph(*keyValuePair.value, transformer));
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

    case API::Object::Type::CertificateInfo: {
        const auto& certificateInfo = static_cast<const WebCertificateInfo&>(object);
        encoder << certificateInfo.certificateInfo();
        break;
    }

    case API::Object::Type::Data:
        static_cast<const API::Data&>(object).encode(encoder);
        break;
    
    case API::Object::Type::Dictionary: {
        auto& dictionary = static_cast<const API::Dictionary&>(object);
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

    case API::Object::Type::FrameHandle:
        static_cast<const API::FrameHandle&>(object).encode(encoder);
        break;

    case API::Object::Type::Image: {
        auto& image = static_cast<const WebImage&>(object);

        ShareableBitmap::Handle handle;
        ASSERT(image.bitmap().isBackedBySharedMemory());
        if (!image.bitmap().isBackedBySharedMemory() || !image.bitmap().createHandle(handle)) {
            // Initial false indicates no allocated bitmap or is not shareable.
            encoder << false;
            break;
        }

        // Initial true indicates a bitmap was allocated and is shareable.
        encoder << true;
        encoder << handle;
        break;
    }

    case API::Object::Type::PageGroupHandle:
        static_cast<const API::PageGroupHandle&>(object).encode(encoder);
        break;

    case API::Object::Type::PageHandle:
        static_cast<const API::PageHandle&>(object).encode(encoder);
        break;

    case API::Object::Type::Point:
        static_cast<const API::Point&>(object).encode(encoder);
        break;

    case API::Object::Type::Rect:
        static_cast<const API::Rect&>(object).encode(encoder);
        break;

    case API::Object::Type::RenderLayer: {
        auto& renderLayer = static_cast<const WebRenderLayer&>(object);

        encode(encoder, renderLayer.renderer());
        encoder << renderLayer.isReflection();
        encoder << renderLayer.isClipping();
        encoder << renderLayer.isClipped();
        encoder << static_cast<uint32_t>(renderLayer.compositingLayerType());
        encoder << renderLayer.absoluteBoundingBox();
        encoder << renderLayer.backingStoreMemoryEstimate();
        encode(encoder, renderLayer.negativeZOrderList());
        encode(encoder, renderLayer.normalFlowList());
        encode(encoder, renderLayer.positiveZOrderList());
        encode(encoder, renderLayer.frameContentsLayer());
        break;
    }

    case API::Object::Type::RenderObject: {
        auto& renderObject = static_cast<const WebRenderObject&>(object);

        encoder << renderObject.name();
        encoder << renderObject.elementTagName();
        encoder << renderObject.elementID();
        encode(encoder, renderObject.elementClassNames());
        encoder << renderObject.absolutePosition();
        encoder << renderObject.frameRect();
        encoder << renderObject.textSnippet();
        encoder << renderObject.textLength();
        encode(encoder, renderObject.children());
        break;
    }

    case API::Object::Type::SerializedScriptValue: {
        auto& serializedScriptValue = static_cast<const API::SerializedScriptValue&>(object);
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

    case API::Object::Type::URL:
        static_cast<const API::URL&>(object).encode(encoder);
        break;

    case API::Object::Type::URLRequest:
        static_cast<const API::URLRequest&>(object).encode(encoder);
        break;

    case API::Object::Type::URLResponse:
        static_cast<const API::URLResponse&>(object).encode(encoder);
        break;

    case API::Object::Type::UInt64:
        static_cast<const API::UInt64&>(object).encode(encoder);
        break;

    case API::Object::Type::Int64:
        static_cast<const API::Int64&>(object).encode(encoder);
        break;

    case API::Object::Type::UserContentURLPattern: {
        auto& urlPattern = static_cast<const API::UserContentURLPattern&>(object);
        encoder << urlPattern.patternString();
        break;
    }

#if PLATFORM(COCOA)
    case API::Object::Type::ObjCObjectGraph:
        static_cast<const ObjCObjectGraph&>(object).encode(encoder);
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
        uint64_t decodedSize;
        if (!decoder.decode(decodedSize))
            return false;

        if (!isInBounds<size_t>(decodedSize))
            return false;

        auto size = static_cast<size_t>(decodedSize);

        Vector<RefPtr<API::Object>> elements;
        for (size_t i = 0; i < size; ++i) {
            RefPtr<API::Object> element;
            if (!decode(decoder, element))
                return false;

            elements.append(WTFMove(element));
        }

        result = API::Array::create(WTFMove(elements));
        break;
    }

    case API::Object::Type::Boolean:
        if (!API::Boolean::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::CertificateInfo: {
        Optional<WebCore::CertificateInfo> certificateInfo;
        decoder >> certificateInfo;
        if (!certificateInfo)
            return false;
        result = WebCertificateInfo::create(*certificateInfo);
        break;
    }

    case API::Object::Type::Data:
        if (!API::Data::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Dictionary: {
        uint64_t decodedSize;
        if (!decoder.decode(decodedSize))
            return false;

        if (!isInBounds<size_t>(decodedSize))
            return false;

        auto size = static_cast<size_t>(decodedSize);

        API::Dictionary::MapType map;
        for (size_t i = 0; i < size; ++i) {
            String key;
            if (!decoder.decode(key))
                return false;

            RefPtr<API::Object> value;
            if (!decode(decoder, value))
                return false;

            if (!map.add(WTFMove(key), WTFMove(value)).isNewEntry)
                return false;
        }

        result = API::Dictionary::create(WTFMove(map));
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

    case API::Object::Type::FrameHandle:
        if (!API::FrameHandle::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Image: {
        bool didEncode = false;
        if (!decoder.decode(didEncode))
            return false;

        if (!didEncode)
            break;

        ShareableBitmap::Handle handle;
        if (!decoder.decode(handle))
            return false;

        auto bitmap = ShareableBitmap::create(handle);
        if (!bitmap)
            return false;

        result = WebImage::create(bitmap.releaseNonNull());
        break;
    }

    case API::Object::Type::Null:
        result = nullptr;
        break;

    case API::Object::Type::PageGroupHandle:
        if (!API::PageGroupHandle::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::PageHandle:
        if (!API::PageHandle::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Point:
        if (!API::Point::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::Rect:
        if (!API::Rect::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::RenderLayer: {
        RefPtr<API::Object> renderer;
        bool isReflection;
        bool isClipping;
        bool isClipped;
        uint32_t compositingLayerTypeAsUInt32;
        WebCore::IntRect absoluteBoundingBox;
        double backingStoreMemoryEstimate;
        RefPtr<API::Object> negativeZOrderList;
        RefPtr<API::Object> normalFlowList;
        RefPtr<API::Object> positiveZOrderList;
        RefPtr<API::Object> frameContentsLayer;

        if (!decode(decoder, renderer))
            return false;
        if (renderer->type() != API::Object::Type::RenderObject)
            return false;
        if (!decoder.decode(isReflection))
            return false;
        if (!decoder.decode(isClipping))
            return false;
        if (!decoder.decode(isClipped))
            return false;
        if (!decoder.decode(compositingLayerTypeAsUInt32))
            return false;
        if (!decoder.decode(absoluteBoundingBox))
            return false;
        if (!decoder.decode(backingStoreMemoryEstimate))
            return false;
        if (!decode(decoder, negativeZOrderList))
            return false;
        if (!decode(decoder, normalFlowList))
            return false;
        if (!decode(decoder, positiveZOrderList))
            return false;
        if (!decode(decoder, frameContentsLayer))
            return false;

        result = WebRenderLayer::create(static_pointer_cast<WebRenderObject>(renderer), isReflection, isClipping, isClipped, static_cast<WebRenderLayer::CompositingLayerType>(compositingLayerTypeAsUInt32), absoluteBoundingBox, backingStoreMemoryEstimate, static_pointer_cast<API::Array>(negativeZOrderList), static_pointer_cast<API::Array>(normalFlowList), static_pointer_cast<API::Array>(positiveZOrderList), static_pointer_cast<WebRenderLayer>(frameContentsLayer));
        break;
    }

    case API::Object::Type::RenderObject: {
        String name;
        String textSnippet;
        String elementTagName;
        String elementID;
        unsigned textLength;
        RefPtr<API::Object> elementClassNames;
        WebCore::IntPoint absolutePosition;
        WebCore::IntRect frameRect;
        RefPtr<API::Object> children;

        if (!decoder.decode(name))
            return false;
        if (!decoder.decode(elementTagName))
            return false;
        if (!decoder.decode(elementID))
            return false;
        if (!decode(decoder, elementClassNames))
            return false;
        if (!decoder.decode(absolutePosition))
            return false;
        if (!decoder.decode(frameRect))
            return false;
        if (!decoder.decode(textSnippet))
            return false;
        if (!decoder.decode(textLength))
            return false;
        if (!decode(decoder, children))
            return false;
        if (children && children->type() != API::Object::Type::Array)
            return false;
        result = WebRenderObject::create(name, elementTagName, elementID, static_pointer_cast<API::Array>(elementClassNames), absolutePosition, frameRect, textSnippet, textLength, static_pointer_cast<API::Array>(children));
        break;
    }

    case API::Object::Type::SerializedScriptValue: {
        IPC::DataReference dataReference;
        if (!decoder.decode(dataReference))
            return false;

        result = API::SerializedScriptValue::adopt(dataReference.vector());
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

    case API::Object::Type::Int64:
        if (!API::Int64::decode(decoder, result))
            return false;
        break;

    case API::Object::Type::UserContentURLPattern: {
        String string;
        if (!decoder.decode(string))
            return false;
        result = API::UserContentURLPattern::create(string);
        break;
    }

#if PLATFORM(COCOA)
    case API::Object::Type::ObjCObjectGraph:
        if (!ObjCObjectGraph::decode(decoder, result))
            return false;
        break;
#endif

    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

} // namespace WebKit
