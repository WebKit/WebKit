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

#ifndef APIDictionary_h
#define APIDictionary_h

#include "APIObject.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace API {

class Array;

class Dictionary final : public ObjectImpl<Object::Type::Dictionary> {
public:
    typedef HashMap<WTF::String, RefPtr<Object>> MapType;

    static Ref<Dictionary> create();
    static Ref<Dictionary> create(MapType);

    virtual ~Dictionary();

    template<typename T>
    T* get(const WTF::String& key) const
    {
        RefPtr<Object> item = m_map.get(key);
        if (!item)
            return 0;

        if (item->type() != T::APIType)
            return 0;

        return static_cast<T*>(item.get());
    }

    Object* get(const WTF::String& key) const
    {
        return m_map.get(key);
    }

    Object* get(const WTF::String& key, bool& exists) const
    {
        auto it = m_map.find(key);
        exists = it != m_map.end();
        if (!exists)
            return nullptr;
        
        return it->value.get();
    }

    Ref<Array> keys() const;

    bool add(const WTF::String& key, RefPtr<Object>&&);
    bool set(const WTF::String& key, RefPtr<Object>&&);
    void remove(const WTF::String& key);

    size_t size() const { return m_map.size(); }

    const MapType& map() const { return m_map; }

protected:
    explicit Dictionary(MapType);

    MapType m_map;
};

} // namespace API

#endif // APIDictionary_h
