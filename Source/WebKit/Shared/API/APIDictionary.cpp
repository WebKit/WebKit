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

#include "config.h"
#include "APIDictionary.h"

#include "APIArray.h"
#include "APIString.h"

namespace API {

Ref<Dictionary> Dictionary::create()
{
    return create({ });
}

Ref<Dictionary> Dictionary::create(MapType map)
{
    return adoptRef(*new Dictionary(WTFMove(map)));
}

Dictionary::Dictionary(MapType map)
    : m_map(WTFMove(map))
{
}

Dictionary::~Dictionary()
{
}

Ref<Array> Dictionary::keys() const
{
    if (m_map.isEmpty())
        return API::Array::create();

    Vector<RefPtr<API::Object>> keys;
    keys.reserveInitialCapacity(m_map.size());

    for (const auto& key : m_map.keys())
        keys.uncheckedAppend(API::String::create(key));

    return API::Array::create(WTFMove(keys));
}

bool Dictionary::add(const WTF::String& key, RefPtr<API::Object>&& item)
{
    MapType::AddResult result = m_map.add(key, WTFMove(item));
    return result.isNewEntry;
}

bool Dictionary::set(const WTF::String& key, RefPtr<API::Object>&& item)
{
    MapType::AddResult result = m_map.set(key, WTFMove(item));
    return result.isNewEntry;
}

void Dictionary::remove(const WTF::String& key)
{
    m_map.remove(key);
}

} // namespace API
