/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
#include "WebExtensionUtilities.h"
#include "JSWebExtensionWrapper.h"
#include <wtf/text/MakeString.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

Ref<JSON::Array> filterObjects(const JSON::Array& array, WTF::Function<bool(const JSON::Value&)>&& lambda)
{
    auto result = JSON::Array::create();

    for (Ref value : array) {
        if (!value)
            continue;

        if (lambda(value))
            result->pushValue(WTF::move(value));
    }

    return result;
}

Vector<String> makeStringVector(const JSON::Array& array)
{
    Vector<String> vector;
    size_t count = array.length();
    vector.reserveInitialCapacity(count);

    for (Ref value : array) {
        if (auto string = value->asString(); !string.isNull())
            vector.append(WTF::move(string));
    }

    vector.shrinkToFit();
    return vector;
}

double largestDisplayScale()
{
    auto largestDisplayScale = 1.0;

    for (double scale : availableScreenScales()) {
        if (scale > largestDisplayScale)
            largestDisplayScale = scale;
    }

    return largestDisplayScale;
}

RefPtr<JSON::Object> jsonWithLowercaseKeys(RefPtr<JSON::Object> json)
{
    if (!json)
        return json;

    Ref newObject = JSON::Object::create();
    for (auto& key : json->keys())
        newObject->setValue(key.convertToASCIILowercase(), *json->getValue(key));

    return newObject;
}

RefPtr<JSON::Object> mergeJSON(RefPtr<JSON::Object> jsonA, RefPtr<JSON::Object> jsonB)
{
    if (!jsonA || !jsonB)
        return jsonA ?: jsonB;

    RefPtr mergedObject = jsonA.copyRef();
    for (auto& key : jsonB->keys()) {
        if (!mergedObject->getValue(key))
            mergedObject->setValue(key, *jsonB->getValue(key));
    }

    return mergedObject;
}

void serializeToMultipleJSONStrings(Ref<JSON::Object> jsonObject, Function<void(String&&)>&& chunkCallback)
{
    // StringBuilder is limited to INT_MAX characters and JSON chars may expand up to 6x to account for escaping (\uNNNN).
    // We can assume memoryCost() ≈ total bytes of string storage, so a threshold cap of INT_MAX / 6
    // will ensure we don't hit the overflow when creating the JSON string.
    static constexpr size_t memoryCostThreshold =
        static_cast<size_t>(std::numeric_limits<int32_t>::max()) / 6;

    if (jsonObject->memoryCost() <= memoryCostThreshold) {
        chunkCallback(jsonObject->toJSONString());
        return;
    }

    size_t currentMemoryCost = 0;
    Ref<JSON::Object> currentJSON = JSON::Object::create();

    for (auto& key : jsonObject->keys()) {
        RefPtr value = jsonObject->getValue(key);
        if (!value)
            continue;

        size_t entryMemoryCost = key.sizeInBytes() + value->memoryCost();

        // If we've hit the threshold, then create a new JSON string to avoid an overflow.
        if (currentMemoryCost + entryMemoryCost > memoryCostThreshold) {
            chunkCallback(currentJSON->toJSONString());
            currentJSON = JSON::Object::create();
            currentMemoryCost = 0;
        }

        currentJSON->setValue(key, *value);
        currentMemoryCost += entryMemoryCost;
    }

    if (currentJSON->size())
        chunkCallback(currentJSON->toJSONString());
}

static inline String lowercaseFirst(const String& input)
{
    return !input.isEmpty() ? makeString(input.left(1).convertToASCIILowercase(), input.substring(1, input.length())) : input;
}

static inline String uppercaseFirst(const String& input)
{
    return !input.isEmpty() ? makeString(input.left(1).convertToASCIIUppercase(), input.substring(1, input.length())) : input;
}

String toErrorString(const String& callingAPIName, const String& sourceKey, const String& underlyingErrorString)
{
    ASSERT(!underlyingErrorString.isEmpty());

    String formattedUnderlyingErrorString = underlyingErrorString.trim([](char16_t character) -> bool {
        return character == '.';
    });

    String source = sourceKey;

    if (!callingAPIName.isEmpty() && !sourceKey.isEmpty() && formattedUnderlyingErrorString.contains("value is invalid"_s)) [[unlikely]] {
        ASSERT_NOT_REACHED_WITH_MESSAGE("Overly nested error string, use a `nullString()` sourceKey for this call instead.");
        source = nullString();
    }

    if (!callingAPIName.isEmpty() && !source.isEmpty())
        return makeString("Invalid call to "_s, callingAPIName, ". The '"_s, source, "' value is invalid, because "_s, lowercaseFirst(formattedUnderlyingErrorString), "."_s);

    if (callingAPIName.isEmpty() && !source.isEmpty())
        return makeString("The '"_s, source, "' value is invalid, because "_s, lowercaseFirst(formattedUnderlyingErrorString), "."_s);

    if (!callingAPIName.isEmpty())
        return makeString("Invalid call to "_s, callingAPIName, ". "_s, uppercaseFirst(formattedUnderlyingErrorString), "."_s);

    return formattedUnderlyingErrorString;
}

JSObjectRef toJSError(JSContextRef context, const String& callingAPIName, const String& sourceKey, const String& underlyingErrorString)
{
    return toJSError(context, toErrorString(callingAPIName, sourceKey, underlyingErrorString));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
