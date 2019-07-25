/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
#include <wtf/MonotonicTime.h>
#include <wtf/SHA1.h>
#include <wtf/Unexpected.h>
#include <wtf/WallTime.h>

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

    static Optional<OptionSet<T>> decode(Decoder& decoder)
    {
        Optional<uint64_t> value;
        decoder >> value;
        if (!value)
            return WTF::nullopt;
        return OptionSet<T>::fromRaw(*value);
    }
};

template<typename T> struct ArgumentCoder<Optional<T>> {
    static void encode(Encoder& encoder, const Optional<T>& optional)
    {
        if (!optional) {
            encoder << false;
            return;
        }

        encoder << true;
        encoder << optional.value();
    }

    static bool decode(Decoder& decoder, Optional<T>& optional)
    {
        bool isEngaged;
        if (!decoder.decode(isEngaged))
            return false;

        if (!isEngaged) {
            optional = WTF::nullopt;
            return true;
        }

        T value;
        if (!decoder.decode(value))
            return false;

        optional = WTFMove(value);
        return true;
    }
    
    static Optional<Optional<T>> decode(Decoder& decoder)
    {
        Optional<bool> isEngaged;
        decoder >> isEngaged;
        if (!isEngaged)
            return WTF::nullopt;
        if (*isEngaged) {
            Optional<T> value;
            decoder >> value;
            if (!value)
                return WTF::nullopt;
            return Optional<Optional<T>>(WTFMove(*value));
        }
        return Optional<Optional<T>>(Optional<T>(WTF::nullopt));
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

template<size_t index, typename... Elements>
struct TupleEncoder {
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        encoder << std::get<sizeof...(Elements) - index>(tuple);
        TupleEncoder<index - 1, Elements...>::encode(encoder, tuple);
    }
};

template<typename... Elements>
struct TupleEncoder<0, Elements...> {
    static void encode(Encoder&, const std::tuple<Elements...>&)
    {
    }
};

template <typename T, typename... Elements, size_t... Indices>
auto tupleFromTupleAndObject(T&& object, std::tuple<Elements...>&& tuple, std::index_sequence<Indices...>)
{
    return std::make_tuple(WTFMove(object), WTFMove(std::get<Indices>(tuple))...);
}

template <typename T, typename... Elements>
auto tupleFromTupleAndObject(T&& object, std::tuple<Elements...>&& tuple)
{
    return tupleFromTupleAndObject(WTFMove(object), WTFMove(tuple), std::index_sequence_for<Elements...>());
}

template<typename Type, typename... Types>
struct TupleDecoderImpl {
    static Optional<std::tuple<Type, Types...>> decode(Decoder& decoder)
    {
        Optional<Type> optional;
        decoder >> optional;
        if (!optional)
            return WTF::nullopt;

        Optional<std::tuple<Types...>> subTuple = TupleDecoderImpl<Types...>::decode(decoder);
        if (!subTuple)
            return WTF::nullopt;

        return tupleFromTupleAndObject(WTFMove(*optional), WTFMove(*subTuple));
    }
};

template<typename Type>
struct TupleDecoderImpl<Type> {
    static Optional<std::tuple<Type>> decode(Decoder& decoder)
    {
        Optional<Type> optional;
        decoder >> optional;
        if (!optional)
            return WTF::nullopt;
        return std::make_tuple(WTFMove(*optional));
    }
};

template<size_t size, typename... Elements>
struct TupleDecoder {
    static Optional<std::tuple<Elements...>> decode(Decoder& decoder)
    {
        return TupleDecoderImpl<Elements...>::decode(decoder);
    }
};

template<>
struct TupleDecoder<0> {
    static Optional<std::tuple<>> decode(Decoder& decoder)
    {
        return std::make_tuple();
    }
};

template<typename... Elements> struct ArgumentCoder<std::tuple<Elements...>> {
    static void encode(Encoder& encoder, const std::tuple<Elements...>& tuple)
    {
        TupleEncoder<sizeof...(Elements), Elements...>::encode(encoder, tuple);
    }

    static Optional<std::tuple<Elements...>> decode(Decoder& decoder)
    {
        return TupleDecoder<sizeof...(Elements), Elements...>::decode(decoder);
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

template<bool fixedSizeElements, typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder;

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder<false, T, inlineCapacity, OverflowHandler, minCapacity> {
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        for (size_t i = 0; i < vector.size(); ++i)
            encoder << vector[i];
    }

    static bool decode(Decoder& decoder, Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        Optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> optional;
        decoder >> optional;
        if (!optional)
            return false;
        vector = WTFMove(*optional);
        return true;
    }

    static Optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        uint64_t size;
        if (!decoder.decode(size))
            return WTF::nullopt;

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> vector;
        for (size_t i = 0; i < size; ++i) {
            Optional<T> element;
            decoder >> element;
            if (!element)
                return WTF::nullopt;
            vector.append(WTFMove(*element));
        }
        vector.shrinkToFit();
        return vector;
    }
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct VectorArgumentCoder<true, T, inlineCapacity, OverflowHandler, minCapacity> {
    static void encode(Encoder& encoder, const Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
    {
        encoder << static_cast<uint64_t>(vector.size());
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(vector.data()), vector.size() * sizeof(T), alignof(T));
    }
    
    static bool decode(Decoder& decoder, Vector<T, inlineCapacity, OverflowHandler, minCapacity>& vector)
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

        Vector<T, inlineCapacity, OverflowHandler, minCapacity> temp;
        temp.grow(size);

        decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(temp.data()), size * sizeof(T), alignof(T));

        vector.swap(temp);
        return true;
    }
    
    static Optional<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> decode(Decoder& decoder)
    {
        uint64_t size;
        if (!decoder.decode(size))
            return WTF::nullopt;
        
        // Since we know the total size of the elements, we can allocate the vector in
        // one fell swoop. Before allocating we must however make sure that the decoder buffer
        // is big enough.
        if (!decoder.bufferIsLargeEnoughToContain<T>(size)) {
            decoder.markInvalid();
            return WTF::nullopt;
        }
        
        Vector<T, inlineCapacity, OverflowHandler, minCapacity> vector;
        vector.grow(size);
        
        decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(vector.data()), size * sizeof(T), alignof(T));
        
        return vector;
    }
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity> struct ArgumentCoder<Vector<T, inlineCapacity, OverflowHandler, minCapacity>> : VectorArgumentCoder<std::is_arithmetic<T>::value, T, inlineCapacity, OverflowHandler, minCapacity> { };

template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg> struct ArgumentCoder<HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>> {
    typedef HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg> HashMapType;

    static void encode(Encoder& encoder, const HashMapType& hashMap)
    {
        encoder << static_cast<uint32_t>(hashMap.size());
        for (typename HashMapType::const_iterator it = hashMap.begin(), end = hashMap.end(); it != end; ++it)
            encoder << *it;
    }

    static Optional<HashMapType> decode(Decoder& decoder)
    {
        uint32_t hashMapSize;
        if (!decoder.decode(hashMapSize))
            return WTF::nullopt;

        HashMapType hashMap;
        hashMap.reserveInitialCapacity(hashMapSize);
        for (uint32_t i = 0; i < hashMapSize; ++i) {
            Optional<KeyArg> key;
            decoder >> key;
            if (UNLIKELY(!key))
                return WTF::nullopt;

            Optional<MappedArg> value;
            decoder >> value;
            if (UNLIKELY(!value))
                return WTF::nullopt;

            if (UNLIKELY(!hashMap.add(WTFMove(*key), WTFMove(*value)).isNewEntry)) {
                // The hash map already has the specified key, bail.
                decoder.markInvalid();
                return WTF::nullopt;
            }
        }

        return hashMap;
    }

    static bool decode(Decoder& decoder, HashMapType& hashMap)
    {
        Optional<HashMapType> tempHashMap;
        decoder >> tempHashMap;
        if (!tempHashMap)
            return false;
        hashMap.swap(*tempHashMap);
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
        Optional<HashSetType> tempHashSet;
        decoder >> tempHashSet;
        if (!tempHashSet)
            return false;

        hashSet.swap(tempHashSet.value());
        return true;
    }

    static Optional<HashSetType> decode(Decoder& decoder)
    {
        uint64_t hashSetSize;
        if (!decoder.decode(hashSetSize))
            return WTF::nullopt;

        HashSetType hashSet;
        for (uint64_t i = 0; i < hashSetSize; ++i) {
            Optional<KeyArg> key;
            decoder >> key;
            if (!key)
                return WTF::nullopt;

            if (!hashSet.add(WTFMove(key.value())).isNewEntry) {
                // The hash set already has the specified key, bail.
                decoder.markInvalid();
                return WTF::nullopt;
            }
        }

        return hashSet;
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

template<typename ValueType, typename ErrorType> struct ArgumentCoder<Expected<ValueType, ErrorType>> {
    static void encode(Encoder& encoder, const Expected<ValueType, ErrorType>& expected)
    {
        if (!expected.has_value()) {
            encoder << false;
            encoder << expected.error();
            return;
        }
        encoder << true;
        encoder << expected.value();
    }

    static Optional<Expected<ValueType, ErrorType>> decode(Decoder& decoder)
    {
        Optional<bool> hasValue;
        decoder >> hasValue;
        if (!hasValue)
            return WTF::nullopt;
        
        if (*hasValue) {
            Optional<ValueType> value;
            decoder >> value;
            if (!value)
                return WTF::nullopt;
            
            Expected<ValueType, ErrorType> expected(WTFMove(*value));
            return expected;
        }
        Optional<ErrorType> error;
        decoder >> error;
        if (!error)
            return WTF::nullopt;
        return { makeUnexpected(WTFMove(*error)) };
    }
};

template<size_t index, typename... Types>
struct VariantCoder {
    static void encode(Encoder& encoder, const WTF::Variant<Types...>& variant, unsigned i)
    {
        if (i == index) {
            encoder << WTF::get<index>(variant);
            return;
        }
        VariantCoder<index - 1, Types...>::encode(encoder, variant, i);
    }
    
    static Optional<WTF::Variant<Types...>> decode(Decoder& decoder, unsigned i)
    {
        if (i == index) {
            Optional<typename WTF::variant_alternative<index, WTF::Variant<Types...>>::type> optional;
            decoder >> optional;
            if (!optional)
                return WTF::nullopt;
            return { WTFMove(*optional) };
        }
        return VariantCoder<index - 1, Types...>::decode(decoder, i);
    }
};

template<typename... Types>
struct VariantCoder<0, Types...> {
    static void encode(Encoder& encoder, const WTF::Variant<Types...>& variant, unsigned i)
    {
        ASSERT_UNUSED(i, !i);
        encoder << WTF::get<0>(variant);
    }
    
    static Optional<WTF::Variant<Types...>> decode(Decoder& decoder, unsigned i)
    {
        ASSERT_UNUSED(i, !i);
        Optional<typename WTF::variant_alternative<0, WTF::Variant<Types...>>::type> optional;
        decoder >> optional;
        if (!optional)
            return WTF::nullopt;
        return { WTFMove(*optional) };
    }
};

template<typename... Types> struct ArgumentCoder<WTF::Variant<Types...>> {
    static void encode(Encoder& encoder, const WTF::Variant<Types...>& variant)
    {
        unsigned i = variant.index();
        encoder << i;
        VariantCoder<sizeof...(Types) - 1, Types...>::encode(encoder, variant, i);
    }
    
    static Optional<WTF::Variant<Types...>> decode(Decoder& decoder)
    {
        Optional<unsigned> i;
        decoder >> i;
        if (!i)
            return WTF::nullopt;
        return VariantCoder<sizeof...(Types) - 1, Types...>::decode(decoder, *i);
    }
};
    
template<> struct ArgumentCoder<WallTime> {
    static void encode(Encoder&, const WallTime&);
    static bool decode(Decoder&, WallTime&);
    static Optional<WallTime> decode(Decoder&);
};

template<> struct ArgumentCoder<AtomString> {
    static void encode(Encoder&, const AtomString&);
    static bool decode(Decoder&, AtomString&);
};

template<> struct ArgumentCoder<CString> {
    static void encode(Encoder&, const CString&);
    static bool decode(Decoder&, CString&);
};

template<> struct ArgumentCoder<String> {
    static void encode(Encoder&, const String&);
    static bool decode(Decoder&, String&);
    static Optional<String> decode(Decoder&);
};

template<> struct ArgumentCoder<SHA1::Digest> {
    static void encode(Encoder&, const SHA1::Digest&);
    static bool decode(Decoder&, SHA1::Digest&);
};

} // namespace IPC
