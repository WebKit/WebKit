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

#include "ProcessQualified.h"
#include <wtf/Forward.h>
#include <wtf/UUID.h>

namespace WebCore {

template <>
class ProcessQualified<UUID> {
public:
    static ProcessQualified generate() { return { UUID::createVersion4(), Process::identifier() }; }

    ProcessQualified()
        : m_object(UUID::emptyValue)
    {
    }

    ProcessQualified(UUID object, ProcessIdentifier processIdentifier)
        : m_object(WTFMove(object))
        , m_processIdentifier(processIdentifier)
    {
    }

    ProcessQualified(WTF::HashTableDeletedValueType)
        : m_object(WTF::HashTableDeletedValue)
        , m_processIdentifier(WTF::HashTableDeletedValue)
    {
    }

    operator bool() const { return !!m_object; }

    const UUID& object() const { return m_object; }
    ProcessIdentifier processIdentifier() const { return m_processIdentifier; }

    unsigned hash() const { return m_object.hash(); }
    bool isHashTableDeletedValue() const { return m_processIdentifier.isHashTableDeletedValue(); }

    bool operator==(const ProcessQualified& other) const { return m_object == other.m_object && m_processIdentifier == other.m_processIdentifier; }
    bool operator!=(const ProcessQualified& other) const { return !(*this == other); }

    String toString() const { return m_object.toString(); }

    template<typename Encoder> void encode(Encoder& encoder) const { encoder << m_object << m_processIdentifier; }
    template<typename Decoder> static std::optional<ProcessQualified> decode(Decoder&);

private:
    UUID m_object;
    ProcessIdentifier m_processIdentifier;
};

template<typename Decoder> std::optional<ProcessQualified<UUID>> ProcessQualified<UUID>::decode(Decoder& decoder)
{
    std::optional<UUID> object;
    decoder >> object;
    if (!object)
        return std::nullopt;
    std::optional<ProcessIdentifier> processIdentifier;
    decoder >> processIdentifier;
    if (!processIdentifier)
        return std::nullopt;
    return { { *object, *processIdentifier } };
}

template <>
inline TextStream& operator<<(TextStream& ts, const ProcessQualified<UUID>& processQualified)
{
    ts << "ProcessQualified(" << processQualified.processIdentifier().toUInt64() << '-' << processQualified.object().toString() << ')';
    return ts;
}

using ScriptExecutionContextIdentifier = ProcessQualified<UUID>;

}

namespace WTF {

template<>
inline uint32_t computeHash(const WebCore::ScriptExecutionContextIdentifier& identifier)
{
    return identifier.object().hash();
}

}
