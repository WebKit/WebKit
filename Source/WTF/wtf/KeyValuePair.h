/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <type_traits>

namespace WTF {

template<typename KeyTypeArg, typename ValueTypeArg>
struct KeyValuePair {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    using KeyType = KeyTypeArg;
    using ValueType = ValueTypeArg;

    KeyValuePair()
    {
    }

    template<typename K, typename V>
    KeyValuePair(K&& key, V&& value)
        : key(std::forward<K>(key))
        , value(std::forward<V>(value))
    {
    }

    template <typename K, typename V>
    KeyValuePair(KeyValuePair<K, V>&& other)
        : key(std::forward<K>(other.key))
        , value(std::forward<V>(other.value))
    {
    }

    KeyType key;
    ValueType value { };
};

template<typename K, typename V>
inline KeyValuePair<typename std::decay<K>::type, typename std::decay<V>::type> makeKeyValuePair(K&& key, V&& value)
{
    return KeyValuePair<typename std::decay<K>::type, typename std::decay<V>::type> { std::forward<K>(key), std::forward<V>(value) };
}

}

using WTF::KeyValuePair;
