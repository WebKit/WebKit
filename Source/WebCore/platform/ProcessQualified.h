/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "ProcessIdentifier.h"
#include <wtf/Hasher.h>
#include <wtf/Markable.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

template <typename T>
class ProcessQualified {
public:
    // This class is used when a single process retains objects representing things in multiple other processes.
    // (E.g. resources in the GPU process.)
    // Generally, objects are identified uniquely per process, but if multiple processes share them to a single
    // process, the single process should distinguish between them by augmenting the objects with the
    // ProcessIdentifier of the process which created them.

    ProcessQualified() = default;

    ProcessQualified(T&& object, ProcessIdentifier processIdentifier)
        : m_object(WTFMove(object))
        , m_processIdentifier(processIdentifier)
    {
    }

    ProcessQualified(const T& object, ProcessIdentifier processIdentifier)
        : m_object(object)
        , m_processIdentifier(processIdentifier)
    {
    }

    ProcessQualified(WTF::HashTableDeletedValueType)
        : m_processIdentifier(WTF::HashTableDeletedValue)
    {
    }

    operator bool() const
    {
        return static_cast<bool>(m_object);
    }

    const T& object() const
    {
        return m_object;
    }

    ProcessIdentifier processIdentifier() const
    {
        return m_processIdentifier;
    }

    bool isHashTableDeletedValue() const
    {
        return m_processIdentifier.isHashTableDeletedValue();
    }

    bool operator==(const ProcessQualified& other) const
    {
        return m_object == other.m_object
            && m_processIdentifier == other.m_processIdentifier;
    }

    bool operator!=(const ProcessQualified& other) const
    {
        return !(*this == other);
    }

    static ProcessQualified generateThreadSafe() { return { T::generateThreadSafe(), Process::identifier() }; }
    static ProcessQualified generate() { return { T::generate(), Process::identifier() }; }
    String toString() const { return makeString(m_processIdentifier.toUInt64(), '-', m_object.toUInt64()); }

    template<typename Encoder> void encode(Encoder& encoder) const { encoder << m_object << m_processIdentifier; }
    template<typename Decoder> static std::optional<ProcessQualified> decode(Decoder&);

    struct MarkableTraits {
        static bool isEmptyValue(const ProcessQualified& identifier) { return !identifier; }
        static constexpr ProcessQualified emptyValue() { return { }; }
    };

private:
    T m_object;
    ProcessIdentifier m_processIdentifier;
};

template <typename T>
template<typename Decoder> std::optional<ProcessQualified<T>> ProcessQualified<T>::decode(Decoder& decoder)
{
    std::optional<T> object;
    decoder >> object;
    if (!object)
        return std::nullopt;
    std::optional<ProcessIdentifier> processIdentifier;
    decoder >> processIdentifier;
    if (!processIdentifier)
        return std::nullopt;
    return { { *object, *processIdentifier } };
}

template <typename T>
inline TextStream& operator<<(TextStream& ts, const ProcessQualified<T>& processQualified)
{
    ts << "ProcessQualified(" << processQualified.object() << ", " << processQualified.processIdentifier() << ')';
    return ts;
}

template <typename T>
inline void add(Hasher& hasher, const ProcessQualified<T>& processQualified)
{
    add(hasher, processQualified.object());
    add(hasher, processQualified.processIdentifier());
}

} // namespace WebCore

namespace WTF {

template<typename T> struct DefaultHash;
template<typename T> struct DefaultHash<WebCore::ProcessQualified<T>> {
    static unsigned hash(const WebCore::ProcessQualified<T>& processQualified)
    {
        return computeHash(processQualified);
    }

    static bool equal(const WebCore::ProcessQualified<T>& a, const WebCore::ProcessQualified<T>& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = DefaultHash<T>::safeToCompareToEmptyOrDeleted;
};

template<typename T> struct HashTraits<WebCore::ProcessQualified<T>> : SimpleClassHashTraits<WebCore::ProcessQualified<T>> {
    static constexpr bool emptyValueIsZero = HashTraits<T>::emptyValueIsZero;
};

} // namespace WTF
