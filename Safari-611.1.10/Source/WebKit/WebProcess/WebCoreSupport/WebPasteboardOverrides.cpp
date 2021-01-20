/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebPasteboardOverrides.h"

#include <wtf/NeverDestroyed.h>

namespace WebKit {

WebPasteboardOverrides& WebPasteboardOverrides::sharedPasteboardOverrides()
{
    static NeverDestroyed<WebPasteboardOverrides> sharedOverrides;
    return sharedOverrides;
}

WebPasteboardOverrides::WebPasteboardOverrides()
{
}

void WebPasteboardOverrides::addOverride(const String& pasteboardName, const String& type, const Vector<uint8_t>& data)
{
    auto addResult = m_overridesMap.add(pasteboardName, nullptr);

    if (addResult.isNewEntry) {
        std::unique_ptr<HashMap<String, Vector<uint8_t>>> typeMap = makeUnique<HashMap<String, Vector<uint8_t>>>();
        addResult.iterator->value = WTFMove(typeMap);
    }

    addResult.iterator->value->set(type, data);
}

void WebPasteboardOverrides::removeOverride(const String& pasteboardName, const String& type)
{
    auto it = m_overridesMap.find(pasteboardName);
    if (it == m_overridesMap.end())
        return;

    ASSERT(it->value);

    it->value->remove(type);

    // If this was the last override for this pasteboard, remove its record completely.
    if (it->value->isEmpty())
        m_overridesMap.remove(it);
}

Vector<String> WebPasteboardOverrides::overriddenTypes(const String& pasteboardName)
{
    Vector<String> result;

    auto it = m_overridesMap.find(pasteboardName);
    if (it == m_overridesMap.end())
        return result;

    ASSERT(it->value);

    for (String& type : it->value->keys())
        result.append(type);

    return result;
}

bool WebPasteboardOverrides::getDataForOverride(const String& pasteboardName, const String& type, Vector<uint8_t>& data) const
{
    auto pasteboardIterator = m_overridesMap.find(pasteboardName);
    if (pasteboardIterator == m_overridesMap.end())
        return false;

    auto typeIterator = pasteboardIterator->value->find(type);
    if (typeIterator == pasteboardIterator->value->end())
        return false;

    data = typeIterator->value;
    return true;
}

bool WebPasteboardOverrides::getDataForOverride(const String& pasteboardName, const String& type, Vector<char>& data) const
{
    Vector<uint8_t> foundBuffer;
    if (!getDataForOverride(pasteboardName, type, foundBuffer))
        return false;

    data.resize(foundBuffer.size());
    memcpy(data.data(), foundBuffer.data(), foundBuffer.size());

    return true;
}

} // namespace WebKit
