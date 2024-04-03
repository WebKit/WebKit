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
#include "WebCoreArgumentCoders.h"
#include "WebImage.h"
#include <WebCore/ShareableBitmap.h>
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

} // namespace WebKit
