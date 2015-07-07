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

#ifndef SimpleArgumentCoder_h
#define SimpleArgumentCoder_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <utility>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace IPC {

// An argument coder works on POD types
template<typename T> struct SimpleArgumentCoder {
    static void encode(ArgumentEncoder& encoder, const T& t)
    {
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(&t), sizeof(T), alignof(T));
    }

    static bool decode(ArgumentDecoder& decoder, T& t)
    {
        return decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&t), sizeof(T), alignof(T));
    }
};

template<typename T> struct ArgumentCoder<WTF::Optional<T>> {
    static void encode(ArgumentEncoder& encoder, const WTF::Optional<T>& optional)
    {
        if (!optional) {
            encoder << false;
            return;
        }

        encoder << true;
        encoder << optional.value();
    }

    static bool decode(ArgumentDecoder& decoder, WTF::Optional<T>& optional)
    {
        bool isEngaged;
        if (!decoder.decode(isEngaged))
            return false;

        if (!isEngaged) {
            optional = Nullopt;
            return true;
        }

        T value;
        if (!decoder.decode(value))
            return false;

        optional = WTF::move(value);
        return true;
    }
};

template<typename T, typename U> struct ArgumentCoder<std::pair<T, U>> {
    static void encode(ArgumentEncoder& encoder, const std::pair<T, U>& pair)
    {
        encoder << pair.first << pair.second;
    }

    static bool decode(ArgumentDecoder& decoder, std::pair<T, U>& pair)
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

template<typename KeyType, typename ValueType> struct ArgumentCoder<WTF::KeyValuePair<KeyType, ValueType>> {
    static void encode(ArgumentEncoder& encoder, const WTF::KeyValuePair<KeyType, ValueType>& pair)
    {
        encoder << pair.key << pair.value;
    }

    static bool decode(ArgumentDecoder& decoder, WTF::KeyValuePair<KeyType, ValueType>& pair)
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
    static void encode(ArgumentEncoder& encoder, const Vector<T, inlineCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        for (size_t i = 0; i < vector.size(); ++i)
            encoder << vector[i];
    }

    static bool decode(ArgumentDecoder& decoder, Vector<T, inlineCapacity>& vector)
    {
        uint64_t size;
        if (!decoder.decode(size))
            return false;

        Vector<T, inlineCapacity> tmp;
        for (size_t i = 0; i < size; ++i) {
            T element;
            if (!decoder.decode(element))
                return false;
            
            tmp.append(WTF::move(element));
        }

        tmp.shrinkToFit();
        vector.swap(tmp);
        return true;
    }
};

template<typename T, size_t inlineCapacity> struct VectorArgumentCoder<true, T, inlineCapacity> {
    static void encode(ArgumentEncoder& encoder, const Vector<T, inlineCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(vector.data()), vector.size() * sizeof(T), alignof(T));
    }
    
    static bool decode(ArgumentDecoder& decoder, Vector<T, inlineCapacity>& vector)
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

    static void encode(ArgumentEncoder& encoder, const HashMapType& hashMap)
    {
        encoder << static_cast<uint64_t>(hashMap.size());
        for (typename HashMapType::const_iterator it = hashMap.begin(), end = hashMap.end(); it != end; ++it)
            encoder << *it;
    }

    static bool decode(ArgumentDecoder& decoder, HashMapType& hashMap)
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

    static void encode(ArgumentEncoder& encoder, const HashSetType& hashSet)
    {
        encoder << static_cast<uint64_t>(hashSet.size());
        for (typename HashSetType::const_iterator it = hashSet.begin(), end = hashSet.end(); it != end; ++it)
            encoder << *it;
    }

    static bool decode(ArgumentDecoder& decoder, HashSetType& hashSet)
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

template<> struct ArgumentCoder<AtomicString> {
    static void encode(ArgumentEncoder&, const AtomicString&);
    static bool decode(ArgumentDecoder&, AtomicString&);
};

template<> struct ArgumentCoder<CString> {
    static void encode(ArgumentEncoder&, const CString&);
    static bool decode(ArgumentDecoder&, CString&);
};

template<> struct ArgumentCoder<String> {
    static void encode(ArgumentEncoder&, const String&);
    static bool decode(ArgumentDecoder&, String&);
};

} // namespace IPC

#endif // ArgumentCoders_h
