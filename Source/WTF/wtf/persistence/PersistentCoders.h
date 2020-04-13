/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include <utility>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/SHA1.h>
#include <wtf/Seconds.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace WTF {
namespace Persistence {

template<typename T, typename U> struct Coder<std::pair<T, U>> {
    static void encode(Encoder& encoder, const std::pair<T, U>& pair)
    {
        encoder << pair.first << pair.second;
    }

    static Optional<std::pair<T, U>> decode(Decoder& decoder)
    {
        Optional<T> first;
        decoder >> first;
        if (!first)
            return WTF::nullopt;

        Optional<U> second;
        decoder >> second;
        if (!second)
            return WTF::nullopt;

        return {{ WTFMove(*first), WTFMove(*second) }};
    }
};

template<typename T> struct Coder<Optional<T>> {
    static void encode(Encoder& encoder, const Optional<T>& optional)
    {
        if (!optional) {
            encoder << false;
            return;
        }
        
        encoder << true;
        encoder << optional.value();
    }
    
    static Optional<Optional<T>> decode(Decoder& decoder)
    {
        Optional<bool> isEngaged;
        decoder >> isEngaged;
        if (!isEngaged)
            return WTF::nullopt;
        if (!*isEngaged)
            return Optional<Optional<T>> { Optional<T> { WTF::nullopt } };
        
        Optional<T> value;
        decoder >> value;
        if (!value)
            return WTF::nullopt;
        
        return Optional<Optional<T>> { Optional<T> { WTFMove(*value) } };
    }
};

template<typename KeyType, typename ValueType> struct Coder<WTF::KeyValuePair<KeyType, ValueType>> {
    static void encode(Encoder& encoder, const WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        encoder << pair.key << pair.value;
    }

    static Optional<WTF::KeyValuePair<KeyType, ValueType>> decode(Decoder& decoder)
    {
        Optional<KeyType> key;
        decoder >> key;
        if (!key)
            return WTF::nullopt;

        Optional<ValueType> value;
        decoder >>value;
        if (!value)
            return WTF::nullopt;

        return {{ WTFMove(*key), WTFMove(*value) }};
    }
};

template<bool fixedSizeElements, typename T, size_t inlineCapacity> struct VectorCoder;

template<typename T, size_t inlineCapacity> struct VectorCoder<false, T, inlineCapacity> {
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        for (size_t i = 0; i < vector.size(); ++i)
            encoder << vector[i];
    }

    static Optional<Vector<T, inlineCapacity>> decode(Decoder& decoder)
    {
        Optional<uint64_t> size;
        decoder >> size;
        if (!size)
            return WTF::nullopt;

        Vector<T, inlineCapacity> tmp;
        for (size_t i = 0; i < *size; ++i) {
            Optional<T> element;
            decoder >> element;
            if (!element)
                return WTF::nullopt;
            tmp.append(WTFMove(*element));
        }

        tmp.shrinkToFit();
        return tmp;
    }
};

template<typename T, size_t inlineCapacity> struct VectorCoder<true, T, inlineCapacity> {
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(vector.data()), vector.size() * sizeof(T));
    }
    
    static Optional<Vector<T, inlineCapacity>> decode(Decoder& decoder)
    {
        Optional<uint64_t> decodedSize;
        decoder >> decodedSize;
        if (!decodedSize)
            return WTF::nullopt;

        if (!isInBounds<size_t>(*decodedSize))
            return WTF::nullopt;

        auto size = static_cast<size_t>(*decodedSize);

        // Since we know the total size of the elements, we can allocate the vector in
        // one fell swoop. Before allocating we must however make sure that the decoder buffer
        // is big enough.
        if (!decoder.bufferIsLargeEnoughToContain<T>(size))
            return WTF::nullopt;

        Vector<T, inlineCapacity> temp;
        temp.grow(size);

        if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(temp.data()), size * sizeof(T)))
            return WTF::nullopt;

        return temp;
    }
};

template<typename T, size_t inlineCapacity> struct Coder<Vector<T, inlineCapacity>> : VectorCoder<std::is_arithmetic<T>::value, T, inlineCapacity> { };

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg> struct Coder<HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>> {
    typedef HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg> HashMapType;

    static void encode(Encoder& encoder, const HashMapType& hashMap)
    {
        encoder << static_cast<uint64_t>(hashMap.size());
        for (typename HashMapType::const_iterator it = hashMap.begin(), end = hashMap.end(); it != end; ++it)
            encoder << *it;
    }

    static Optional<HashMapType> decode(Decoder& decoder)
    {
        Optional<uint64_t> hashMapSize;
        decoder >> hashMapSize;
        if (!hashMapSize)
            return WTF::nullopt;

        HashMapType tempHashMap;
        tempHashMap.reserveInitialCapacity(static_cast<unsigned>(*hashMapSize));
        for (uint64_t i = 0; i < *hashMapSize; ++i) {
            Optional<KeyArg> key;
            decoder >> key;
            if (!key)
                return WTF::nullopt;
            Optional<MappedArg> value;
            decoder >> value;
            if (!value)
                return WTF::nullopt;

            if (!tempHashMap.add(WTFMove(*key), WTFMove(*value)).isNewEntry) {
                // The hash map already has the specified key, bail.
                return WTF::nullopt;
            }
        }

        return tempHashMap;
    }
};

template<typename KeyArg, typename HashArg, typename KeyTraitsArg> struct Coder<HashSet<KeyArg, HashArg, KeyTraitsArg>> {
    typedef HashSet<KeyArg, HashArg, KeyTraitsArg> HashSetType;

    static void encode(Encoder& encoder, const HashSetType& hashSet)
    {
        encoder << static_cast<uint64_t>(hashSet.size());
        for (typename HashSetType::const_iterator it = hashSet.begin(), end = hashSet.end(); it != end; ++it)
            encoder << *it;
    }

    static Optional<HashSetType> decode(Decoder& decoder)
    {
        Optional<uint64_t> hashSetSize;
        decoder >> hashSetSize;
        if (!hashSetSize)
            return WTF::nullopt;

        HashSetType tempHashSet;
        for (uint64_t i = 0; i < *hashSetSize; ++i) {
            Optional<KeyArg> key;
            decoder >> key;
            if (!key)
                return WTF::nullopt;

            if (!tempHashSet.add(WTFMove(*key)).isNewEntry) {
                // The hash map already has the specified key, bail.
                return WTF::nullopt;
            }
        }

        return tempHashSet;
    }
};

template<> struct Coder<Seconds> {
    static void encode(Encoder& encoder, const Seconds& seconds)
    {
        encoder << seconds.value();
    }

    static Optional<Seconds> decode(Decoder& decoder)
    {
        Optional<double> value;
        decoder >> value;
        if (!value)
            return WTF::nullopt;
        return Seconds(*value);
    }
};

template<> struct Coder<WallTime> {
    static void encode(Encoder& encoder, const WallTime& time)
    {
        encoder << time.secondsSinceEpoch().value();
    }

    static Optional<WallTime> decode(Decoder& decoder)
    {
        Optional<double> value;
        decoder >> value;
        if (!value)
            return WTF::nullopt;

        return WallTime::fromRawSeconds(*value);
    }
};

template<> struct Coder<AtomString> {
    WTF_EXPORT_PRIVATE static void encode(Encoder&, const AtomString&);
    WTF_EXPORT_PRIVATE static Optional<AtomString> decode(Decoder&);
};

template<> struct Coder<CString> {
    WTF_EXPORT_PRIVATE static void encode(Encoder&, const CString&);
    WTF_EXPORT_PRIVATE static Optional<CString> decode(Decoder&);
};

template<> struct Coder<String> {
    WTF_EXPORT_PRIVATE static void encode(Encoder&, const String&);
    WTF_EXPORT_PRIVATE static Optional<String> decode(Decoder&);
};

template<> struct Coder<SHA1::Digest> {
    WTF_EXPORT_PRIVATE static void encode(Encoder&, const SHA1::Digest&);
    WTF_EXPORT_PRIVATE static Optional<SHA1::Digest> decode(Decoder&);
};

}
}
