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

#ifndef UserMessageCoders_h
#define UserMessageCoders_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "DataReference.h"
#include "ImmutableArray.h"
#include "ImmutableDictionary.h"
#include "ShareableBitmap.h"
#include "WebCertificateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebData.h"
#include "WebGeometry.h"
#include "WebImage.h"
#include "WebNumber.h"
#include "WebRenderLayer.h"
#include "WebRenderObject.h"
#include "WebSerializedScriptValue.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLRequest.h"
#include "WebUserContentURLPattern.h"

namespace WebKit {

//   - Null -> Null
//   - Array -> Array
//   - Dictionary -> Dictionary
//   - SerializedScriptValue -> SerializedScriptValue
//   - String -> String
//   - UserContentURLPattern -> UserContentURLPattern
//   - WebCertificateInfo -> WebCertificateInfo
//   - WebData -> WebData
//   - WebDouble -> WebDouble
//   - WebImage -> WebImage
//   - WebRenderLayer -> WebRenderLayer
//   - WebRenderObject -> WebRenderObject
//   - WebUInt64 -> WebUInt64
//   - WebURL -> WebURL
//   - WebURLRequest -> WebURLRequest

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
            encoder->encodeVariableLengthByteArray(scriptValue->dataReference());
            return true;
        }
        case APIObject::TypeBoolean: {
            WebBoolean* booleanObject = static_cast<WebBoolean*>(m_root);
            encoder->encode(booleanObject->value());
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
        case APIObject::TypePoint: {
            WebPoint* pointObject = static_cast<WebPoint*>(m_root);
            encoder->encode(pointObject->point().x);
            encoder->encode(pointObject->point().y);
            return true;
        }
        case APIObject::TypeSize: {
            WebSize* sizeObject = static_cast<WebSize*>(m_root);
            encoder->encode(sizeObject->size().width);
            encoder->encode(sizeObject->size().height);
            return true;
        }
        case APIObject::TypeRect: {
            WebRect* rectObject = static_cast<WebRect*>(m_root);
            encoder->encode(rectObject->rect().origin.x);
            encoder->encode(rectObject->rect().origin.y);
            encoder->encode(rectObject->rect().size.width);
            encoder->encode(rectObject->rect().size.height);
            return true;
        }
        case APIObject::TypeRenderLayer: {
            WebRenderLayer* renderLayer = static_cast<WebRenderLayer*>(m_root);
            encoder->encode(Owner(renderLayer->renderer()));
            encoder->encode(renderLayer->isReflection());
            encoder->encode(renderLayer->isClipping());
            encoder->encode(renderLayer->isClipped());
            encoder->encode(static_cast<uint32_t>(renderLayer->compositingLayerType()));
            encoder->encode(renderLayer->absoluteBoundingBox());
            encoder->encode(Owner(renderLayer->negativeZOrderList()));
            encoder->encode(Owner(renderLayer->normalFlowList()));
            encoder->encode(Owner(renderLayer->positiveZOrderList()));
            return true;
        }
        case APIObject::TypeRenderObject: {
            WebRenderObject* renderObject = static_cast<WebRenderObject*>(m_root);
            encoder->encode(renderObject->name());
            encoder->encode(renderObject->elementTagName());
            encoder->encode(renderObject->elementID());
            encoder->encode(Owner(renderObject->elementClassNames()));
            encoder->encode(renderObject->absolutePosition());
            encoder->encode(renderObject->frameRect());
            encoder->encode(Owner(renderObject->children().get()));
            return true;
        }
        case APIObject::TypeURL: {
            WebURL* urlObject = static_cast<WebURL*>(m_root);
            encoder->encode(urlObject->string());
            return true;
        }
        case APIObject::TypeURLRequest: {
            WebURLRequest* urlRequestObject = static_cast<WebURLRequest*>(m_root);
            encoder->encode(urlRequestObject->resourceRequest());
            return true;
        }
        case APIObject::TypeUserContentURLPattern: {
            WebUserContentURLPattern* urlPattern = static_cast<WebUserContentURLPattern*>(m_root);
            encoder->encode(urlPattern->patternString());
            return true;
        }
        case APIObject::TypeImage: {
            WebImage* image = static_cast<WebImage*>(m_root);

            ShareableBitmap::Handle handle;
            ASSERT(!image->bitmap() || image->bitmap()->isBackedBySharedMemory());            
            if (!image->bitmap() || !image->bitmap()->isBackedBySharedMemory() || !image->bitmap()->createHandle(handle)) {
                // Initial false indicates no allocated bitmap or is not shareable.
                encoder->encode(false);
                return true;
            }

            // Initial true indicates a bitmap was allocated and is shareable.
            encoder->encode(true);

            encoder->encode(handle);
            return true;
        }
        case APIObject::TypeData: {
            WebData* data = static_cast<WebData*>(m_root);
            encoder->encodeVariableLengthByteArray(data->dataReference());
            return true;
        }
        case APIObject::TypeCertificateInfo: {
            WebCertificateInfo* certificateInfo = static_cast<WebCertificateInfo*>(m_root);
            encoder->encode(certificateInfo->platformCertificateInfo());
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
//   - SerializedScriptValue -> SerializedScriptValue
//   - String -> String
//   - UserContentURLPattern -> UserContentURLPattern
//   - WebCertificateInfo -> WebCertificateInfo
//   - WebData -> WebData
//   - WebDouble -> WebDouble
//   - WebImage -> WebImage
//   - WebUInt64 -> WebUInt64
//   - WebURL -> WebURL
//   - WebURLRequest -> WebURLRequest

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

                ImmutableDictionary::MapType::AddResult result = map.set(key, element.release());
                if (!result.isNewEntry)
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
            CoreIPC::DataReference dataReference;
            if (!decoder->decodeVariableLengthByteArray(dataReference))
                return false;
            
            Vector<uint8_t> vector = dataReference.vector();
            coder.m_root = WebSerializedScriptValue::adopt(vector);
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
        case APIObject::TypeBoolean: {
            bool value;
            if (!decoder->decode(value))
                return false;
            coder.m_root = WebBoolean::create(value);
            break;
        }
        case APIObject::TypeSize: {
            double width;
            double height;
            if (!decoder->decode(width))
                return false;
            if (!decoder->decode(height))
                return false;
            coder.m_root = WebSize::create(WKSizeMake(width, height));
            break;
        }
        case APIObject::TypePoint: {
            double x;
            double y;
            if (!decoder->decode(x))
                return false;
            if (!decoder->decode(y))
                return false;
            coder.m_root = WebPoint::create(WKPointMake(x, y));
            break;
        }
        case APIObject::TypeRect: {
            double x;
            double y;
            double width;
            double height;
            if (!decoder->decode(x))
                return false;
            if (!decoder->decode(y))
                return false;
            if (!decoder->decode(width))
                return false;
            if (!decoder->decode(height))
                return false;
            coder.m_root = WebRect::create(WKRectMake(x, y, width, height));
            break;
        }
        case APIObject::TypeRenderLayer: {
            RefPtr<APIObject> renderer;
            bool isReflection;
            bool isClipping;
            bool isClipped;
            uint32_t compositingLayerTypeAsUInt32;
            WebCore::IntRect absoluteBoundingBox;
            RefPtr<APIObject> negativeZOrderList;
            RefPtr<APIObject> normalFlowList;
            RefPtr<APIObject> positiveZOrderList;

            Owner rendererCoder(coder, renderer);
            if (!decoder->decode(rendererCoder))
                return false;
            if (renderer->type() != APIObject::TypeRenderObject)
                return false;
            if (!decoder->decodeBool(isReflection))
                return false;
            if (!decoder->decodeBool(isClipping))
                return false;
            if (!decoder->decodeBool(isClipped))
                return false;
            if (!decoder->decodeUInt32(compositingLayerTypeAsUInt32))
                return false;
            if (!decoder->decode(absoluteBoundingBox))
                return false;
            Owner negativeZOrderListCoder(coder, negativeZOrderList);
            if (!decoder->decode(negativeZOrderListCoder))
                return false;
            Owner normalFlowListCoder(coder, normalFlowList);
            if (!decoder->decode(normalFlowListCoder))
                return false;
            Owner positiveZOrderListCoder(coder, positiveZOrderList);
            if (!decoder->decode(positiveZOrderListCoder))
                return false;
            coder.m_root = WebRenderLayer::create(static_pointer_cast<WebRenderObject>(renderer), isReflection, isClipping, isClipped, static_cast<WebRenderLayer::CompositingLayerType>(compositingLayerTypeAsUInt32),
                absoluteBoundingBox, static_pointer_cast<MutableArray>(negativeZOrderList), static_pointer_cast<MutableArray>(normalFlowList),
                static_pointer_cast<MutableArray>(positiveZOrderList));
            break;
        }
        case APIObject::TypeRenderObject: {
            String name;
            String elementTagName;
            String elementID;
            RefPtr<APIObject> elementClassNames;
            WebCore::IntPoint absolutePosition;
            WebCore::IntRect frameRect;
            RefPtr<APIObject> children;
            
            if (!decoder->decode(name))
                return false;
            if (!decoder->decode(elementTagName))
                return false;
            if (!decoder->decode(elementID))
                return false;
            Owner classNamesCoder(coder, elementClassNames);
            if (!decoder->decode(classNamesCoder))
                return false;
            if (!decoder->decode(absolutePosition))
                return false;
            if (!decoder->decode(frameRect))
                return false;
            Owner messageCoder(coder, children);
            if (!decoder->decode(messageCoder))
                return false;
            if (children && children->type() != APIObject::TypeArray)
                return false;
            coder.m_root = WebRenderObject::create(name, elementTagName, elementID, WTF::static_pointer_cast<MutableArray>(elementClassNames), absolutePosition, frameRect, WTF::static_pointer_cast<MutableArray>(children));
            break;
        }
        case APIObject::TypeURL: {
            String string;
            if (!decoder->decode(string))
                return false;
            coder.m_root = WebURL::create(string);
            break;
        }
        case APIObject::TypeURLRequest: {
            WebCore::ResourceRequest request;
            if (!decoder->decode(request))
                return false;
            coder.m_root = WebURLRequest::create(request);
            break;
        }
        case APIObject::TypeUserContentURLPattern: {
            String string;
            if (!decoder->decode(string))
                return false;
            coder.m_root = WebUserContentURLPattern::create(string);
            break;
        }
        case APIObject::TypeImage: {
            bool didEncode = false;
            if (!decoder->decode(didEncode))
                return false;

            if (!didEncode)
                break;

            ShareableBitmap::Handle handle;
            if (!decoder->decode(handle))
                return false;

            coder.m_root = WebImage::create(ShareableBitmap::create(handle));
            return true;
        }
        case APIObject::TypeData: {
            CoreIPC::DataReference dataReference;
            if (!decoder->decodeVariableLengthByteArray(dataReference))
                return false;
            coder.m_root = WebData::create(dataReference.data(), dataReference.size());
            break;
        }
        case APIObject::TypeCertificateInfo: {
            PlatformCertificateInfo platformCertificateInfo;
            if (!decoder->decode(platformCertificateInfo))
                return false;
            coder.m_root = WebCertificateInfo::create(platformCertificateInfo);
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

#endif // UserMessageCoders_h
