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

#ifndef ImmutableDictionary_h
#define ImmutableDictionary_h

#include "APIObject.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace API {
class Array;
}

namespace WebKit {

// ImmutableDictionary - An immutable dictionary type suitable for vending to an API.

class ImmutableDictionary : public API::ObjectImpl<API::Object::Type::Dictionary> {
public:
    typedef HashMap<String, RefPtr<API::Object>> MapType;

    static RefPtr<ImmutableDictionary> create();
    static RefPtr<ImmutableDictionary> create(MapType);

    virtual ~ImmutableDictionary();

    virtual bool isMutable() { return false; }

    template<typename T>
    T* get(const String& key) const
    {
        RefPtr<API::Object> item = m_map.get(key);
        if (!item)
            return 0;

        if (item->type() != T::APIType)
            return 0;

        return static_cast<T*>(item.get());
    }

    API::Object* get(const String& key) const
    {
        return m_map.get(key);
    }

    API::Object* get(const String& key, bool& exists) const
    {
        auto it = m_map.find(key);
        exists = it != m_map.end();
        return it->value.get();
    }

    PassRefPtr<API::Array> keys() const;

    size_t size() const { return m_map.size(); }

    const MapType& map() const { return m_map; }

protected:
    explicit ImmutableDictionary(MapType);

    MapType m_map;
};

} // namespace WebKit

#endif // ImmutableDictionary_h
