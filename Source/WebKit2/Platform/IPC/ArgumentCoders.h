/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include "Decoder.h"
#include "Encoder.h"
#include <utility>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>
#include <wtf/SHA1.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace IPC {

// An argument coder works on POD types
template<typename T> struct SimpleArgumentCoder {
    static void encode(Encoder& encoder, const T& t)
    {
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(&t), sizeof(T), alignof(T));
    }

    static bool decode(Decoder& decoder, T& t)
    {
        return decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&t), sizeof(T), alignof(T));
    }
};

template<typename T> struct ArgumentCoder<OptionSet<T>> {
    static void encode(Encoder& encoder, const OptionSet<T>& optionSet)
    {
        encoder << (static_cast<uint64_t>(optionSet.toRaw()));
    }

    static bool decode(Decoder& decoder, OptionSet<T>& optionSet)
    {
        uint64_t value;
        if (!decoder.decode(value))
            return false;

        optionSet = OptionSet<T>::fromRaw(value);
        return true;
    }
};

template<typename T> struct ArgumentCoder<std::optional<T>> {
    static void encode(Encoder& encoder, const std::optional<T>& optional)
    {
        if (!optional) {
            encoder << false;
            return;
        }

        encoder << true;
        encoder << optional.value();
    }

    static bool decode(Decoder& decoder, std::optional<T>& optional)
    {
        bool isEngaged;
        if (!decoder.decode(isEngaged))
            return false;

        if (!isEngaged) {
            optional = std::nullopt;
            return true;
        }

        T value;
        if (!decoder.decode(value))
            return false;

        optional = WTFMove(value);
        return true;
    }
};

template<typename T, typename U> struct ArgumentCoder<std::pair<T, U>> {
    static void encode(Encoder& encoder, const std::pair<T, U>& pair)
    {
        encoder << pair.first << pair.second;
    }

    static bool decode(Decoder& decoder, std::pair<T, U>& pair)
    {
        T first;
        if (!decoder.decode(first))
            return false;

        U second;
        if (!decoder.decode(second))
            return false;

        pair.first = first;
        pair.second = second;
        return true;
    }
};

template<size_t index, typename... Elements>
struct TupleCoder {
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        encoder << std::get<sizeof...(Elements) - index>(tuple);
        TupleCoder<index - 1, Elements...>::encode(encoder, tuple);
    }

    static bool decode(Decoder& decoder, std::tuple<Elements...>& tuple)
    {
        if (!decoder.decode(std::get<sizeof...(Elements) - index>(tuple)))
            return false;
        return TupleCoder<index - 1, Elements...>::decode(decoder, tuple);
    }
};

template<typename... Elements>
struct TupleCoder<0, Elements...> {
    static void encode(Encoder&, const std::tuple<Elements...>&)
    {
    }

    static bool decode(Decoder&, std::tuple<Elements...>&)
    {
        return true;
    }
};

template<typename... Elements> struct ArgumentCoder<std::tuple<Elements...>> {
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        TupleCoder<sizeof...(Elements), Elements...>::encode(encoder, tuple);
    }

    static bool decode(Decoder& decoder, std::tuple<Elements...>& tuple)
    {
        return TupleCoder<sizeof...(Elements), Elements...>::decode(decoder, tuple);
    }
};


template<typename Rep, typename Period> struct ArgumentCoder<std::chrono::duration<Rep, Period>> {
    static void encode(Encoder& encoder, const std::chrono::duration<Rep, Period>& duration)
    {
        static_assert(std::is_integral<Rep>::value && std::is_signed<Rep>::value && sizeof(Rep) <= sizeof(int64_t), "Serialization of this Rep type is not supported yet. Only signed integer type which can be fit in an int64_t is currently supported.");
        encoder << static_cast<int64_t>(duration.count());
    }

    static bool decode(Decoder& decoder, std::chrono::duration<Rep, Period>& result)
    {
        int64_t count;
        if (!decoder.decode(count))
            return false;
        result = std::chrono::duration<Rep, Period>(static_cast<Rep>(count));
        return true;
    }
};

template<typename KeyType, typename ValueType> struct ArgumentCoder<WTF::KeyValuePair<KeyType, ValueType>> {
    static void encode(Encoder& encoder, const WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        encoder << pair.key << pair.value;
    }

    static bool decode(Decoder& decoder, WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        KeyType key;
        if (!decoder.decode(key))
            return false;

        ValueType value;
        if (!decoder.decode(value))
            return false;

        pair.key = key;
        pair.value = value;
        return true;
    }
};

template<bool fixedSizeElements, typename T, size_t inlineCapacity> struct VectorArgumentCoder;

template<typename T, size_t inlineCapacity> struct VectorArgumentCoder<false, T, inlineCapacity> {
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        for (size_t i = 0; i < vector.size(); ++i)
            encoder << vector[i];
    }

    static bool decode(Decoder& decoder, Vector<T, inlineCapacity>& vector)
    {
        uint64_t size;
        if (!decoder.decode(size))
            return false;

        Vector<T, inlineCapacity> tmp;
        for (size_t i = 0; i < size; ++i) {
            T element;
            if (!decoder.decode(element))
                return false;
            
            tmp.append(WTFMove(element));
        }

        tmp.shrinkToFit();
        vector.swap(tmp);
        return true;
    }
};

template<typename T, size_t inlineCapacity> struct VectorArgumentCoder<true, T, inlineCapacity> {
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(vector.data()), vector.size() * sizeof(T), alignof(T));
    }
    
    static bool decode(Decoder& decoder, Vector<T, inlineCapacity>& vector)
    {
        uint64_t size;
        if (!decoder.decode(size))
            return false;

        // Since we know the total size of the elements, we can allocate the vector in
        // one fell swoop. Before allocating we must however make sure that the decoder buffer
        // is big enough.
        if (!decoder.bufferIsLargeEnoughToContain<T>(size)) {
            decoder.markInvalid();
            return false;
        }

        Vector<T, inlineCapacity> temp;
        temp.resize(size);

        decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(temp.data()), size * sizeof(T), alignof(T));

        vector.swap(temp);
        return true;
    }
};

template<typename T, size_t inlineCapacity> struct ArgumentCoder<Vector<T, inlineCapacity>> : VectorArgumentCoder<std::is_arithmetic<T>::value, T, inlineCapacity> { };

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg> struct ArgumentCoder<HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>> {
    typedef HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg> HashMapType;

    static void encode(Encoder& encoder, const HashMapType& hashMap)
    {
        encoder << static_cast<uint64_t>(hashMap.size());
        for (typename HashMapType::const_iterator it = hashMap.begin(), end = hashMap.end(); it != end; ++it)
            encoder << *it;
    }

    static bool decode(Decoder& decoder, HashMapType& hashMap)
    {
        uint64_t hashMapSize;
        if (!decoder.decode(hashMapSize))
            return false;

        HashMapType tempHashMap;
        for (uint64_t i = 0; i < hashMapSize; ++i) {
            KeyArg key;
            MappedArg value;
            if (!decoder.decode(key))
                return false;
            if (!decoder.decode(value))
                return false;

            if (!tempHashMap.add(key, value).isNewEntry) {
                // The hash map already has the specified key, bail.
                decoder.markInvalid();
                return false;
            }
        }

        hashMap.swap(tempHashMap);
        return true;
    }
};

template<typename KeyArg, typename HashArg, typename KeyTraitsArg> struct ArgumentCoder<HashSet<KeyArg, HashArg, KeyTraitsArg>> {
    typedef HashSet<KeyArg, HashArg, KeyTraitsArg> HashSetType;

    static void encode(Encoder& encoder, const HashSetType& hashSet)
    {
        encoder << static_cast<uint64_t>(hashSet.size());
        for (typename HashSetType::const_iterator it = hashSet.begin(), end = hashSet.end(); it != end; ++it)
            encoder << *it;
    }

    static bool decode(Decoder& decoder, HashSetType& hashSet)
    {
        uint64_t hashSetSize;
        if (!decoder.decode(hashSetSize))
            return false;

        HashSetType tempHashSet;
        for (uint64_t i = 0; i < hashSetSize; ++i) {
            KeyArg key;
            if (!decoder.decode(key))
                return false;

            if (!tempHashSet.add(key).isNewEntry) {
                // The hash map already has the specified key, bail.
                decoder.markInvalid();
                return false;
            }
        }

        hashSet.swap(tempHashSet);
        return true;
    }
};

template<typename KeyArg, typename HashArg, typename KeyTraitsArg> struct ArgumentCoder<HashCountedSet<KeyArg, HashArg, KeyTraitsArg>> {
    typedef HashCountedSet<KeyArg, HashArg, KeyTraitsArg> HashCountedSetType;
    
    static void encode(Encoder& encoder, const HashCountedSetType& hashCountedSet)
    {
        encoder << static_cast<uint64_t>(hashCountedSet.size());
        
        for (auto entry : hashCountedSet) {
            encoder << entry.key;
            encoder << entry.value;
        }
    }
    
    static bool decode(Decoder& decoder, HashCountedSetType& hashCountedSet)
    {
        uint64_t hashCountedSetSize;
        if (!decoder.decode(hashCountedSetSize))
            return false;
        
        HashCountedSetType tempHashCountedSet;
        for (uint64_t i = 0; i < hashCountedSetSize; ++i) {
            KeyArg key;
            if (!decoder.decode(key))
                return false;
            
            unsigned count;
            if (!decoder.decode(count))
                return false;
            
            if (!tempHashCountedSet.add(key, count).isNewEntry) {
                // The hash counted set already has the specified key, bail.
                decoder.markInvalid();
                return false;
            }
        }
        
        hashCountedSet.swap(tempHashCountedSet);
        return true;
    }
};

template<> struct ArgumentCoder<std::chrono::system_clock::time_point> {
    static void encode(Encoder&, const std::chrono::system_clock::time_point&);
    static bool decode(Decoder&, std::chrono::system_clock::time_point&);
};

template<> struct ArgumentCoder<AtomicString> {
    static void encode(Encoder&, const AtomicString&);
    static bool decode(Decoder&, AtomicString&);
};

template<> struct ArgumentCoder<CString> {
    static void encode(Encoder&, const CString&);
    static bool decode(Decoder&, CString&);
};

template<> struct ArgumentCoder<String> {
    static void encode(Encoder&, const String&);
    static bool decode(Decoder&, String&);
};

template<> struct ArgumentCoder<SHA1::Digest> {
    static void encode(Encoder&, const SHA1::Digest&);
    static bool decode(Decoder&, SHA1::Digest&);
};

} // namespace IPC
