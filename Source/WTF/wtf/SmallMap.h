/*
 * Copyright (C) 2024 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <variant>
#include <wtf/HashMap.h>
#include <wtf/ScopedLambda.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// This is a map optimized for holding 0 or 1 items with no hashing or allocations in those cases.
template<typename Key, typename Value>
class SmallMap {
public:
    using Pair = KeyValuePair<Key, Value>;
    using Map = UncheckedKeyHashMap<Key, Value>;
    using Storage = std::variant<std::monostate, Pair, Map>;

    static_assert(sizeof(Pair) <= 4 * sizeof(uint64_t), "Don't use SmallMap with large types. It probably wastes memory.");

    Value& ensure(const Key& key, const auto& functor)
    {
        ASSERT(Map::isValidKey(key));
        if (std::holds_alternative<std::monostate>(m_storage)) {
            m_storage = Pair { key, functor() };
            return std::get<Pair>(m_storage).value;
        }
        if (auto* pair = std::get_if<Pair>(&m_storage)) {
            if (pair->key == key)
                return pair->value;
            Map map;
            map.add(WTFMove(pair->key), WTFMove(pair->value));
            m_storage = WTFMove(map);
            return std::get<Map>(m_storage).add(key, functor()).iterator->value;
        }
        return std::get<Map>(m_storage).ensure(key, functor).iterator->value;
    }

    void remove(const Key& key)
    {
        ASSERT(Map::isValidKey(key));
        if (auto* pair = std::get_if<Pair>(&m_storage)) {
            if (pair->key == key)
                m_storage = std::monostate { };
        } else if (auto* map = std::get_if<Map>(&m_storage))
            map->remove(key);
    }

    const Value* get(const Key& key) const
    {
        ASSERT(Map::isValidKey(key));
        if (auto* pair = std::get_if<Pair>(&m_storage)) {
            if (pair->key == key)
                return std::addressof(pair->value);
        } else if (auto* map = std::get_if<Map>(&m_storage)) {
            if (auto it = map->find(key); it != map->end())
                return std::addressof(it->value);
        }
        return nullptr;
    }

    void forEach(const auto& callback) const
    {
        switchOn(m_storage, [&] (const std::monostate&) {
        }, [&] (const Pair& pair) {
            callback(pair.key, pair.value);
        }, [&] (const Map& map) {
            for (auto& [key, value] : map)
                callback(key, value);
        });
    }

    size_t size() const
    {
        return switchOn(m_storage, [&] (const std::monostate&) {
            return 0u;
        }, [&] (const Pair&) {
            return 1u;
        }, [&] (const Map& map) {
            return map.size();
        });
    }

    const Storage& rawStorage() const { return m_storage; }

private:
    Storage m_storage;
};

} // namespace WTF

using WTF::SmallMap;
