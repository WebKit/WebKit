/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ElementTargetingTypes.h"

#include "SharedBuffer.h"
#include <wtf/JSONValues.h>

namespace WebCore {

Ref<SharedBuffer> serializeTargetedElementSelectors(const TargetedElementSelectors& selectors)
{
    Ref rootObject = JSON::Object::create();
    rootObject->setInteger("version"_s, 1);

    Ref selectorsArray = JSON::Array::create();
    for (auto selectorSet : selectors) {
        Ref selectorSetArray = JSON::Array::create();
        for (auto selector : selectorSet)
            selectorSetArray->pushString(selector);
        selectorsArray->pushArray(WTF::move(selectorSetArray));
    }
    rootObject->setArray("selectors"_s, WTF::move(selectorsArray));

    auto jsonString = rootObject->toJSONString();
    return SharedBuffer::create(jsonString.utf8().span());
}

std::optional<TargetedElementSelectors> deserializeTargetedElementSelectors(std::span<const uint8_t> data)
{
    auto jsonString = String::fromUTF8(data);
    RefPtr parsedValue = JSON::Value::parseJSON(jsonString);
    if (!parsedValue)
        return { };

    RefPtr rootObject = parsedValue->asObject();
    if (!rootObject)
        return { };

    auto version = rootObject->getInteger("version"_s);
    if (!version || *version > 1)
        return { };

    RefPtr selectorsArray = rootObject->getArray("selectors"_s);
    if (!selectorsArray)
        return { };

    TargetedElementSelectors result;
    for (size_t i = 0; i < selectorsArray->length(); ++i) {
        RefPtr selectorSetArray = selectorsArray->get(i)->asArray();
        if (!selectorSetArray)
            return { };

        HashSet<String> selectorSet;
        for (size_t j = 0; j < selectorSetArray->length(); ++j) {
            auto selectorString = selectorSetArray->get(j)->asString();
            if (selectorString.isNull())
                return { };
            selectorSet.add(selectorString);
        }
        result.append(WTF::move(selectorSet));
    }

    return result;
}

} // namespace WebCore
