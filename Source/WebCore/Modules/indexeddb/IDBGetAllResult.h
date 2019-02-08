/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyData.h"
#include "IDBValue.h"
#include "IndexedDB.h"

#include <wtf/Variant.h>

namespace WebCore {

class IDBGetAllResult {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IDBGetAllResult()
    {
    }

    IDBGetAllResult(IndexedDB::GetAllType type)
        : m_type(type)
    {
        switch (m_type) {
        case IndexedDB::GetAllType::Keys:
            m_results = Vector<IDBKeyData>();
            break;
        case IndexedDB::GetAllType::Values:
            m_results = Vector<IDBValue>();
            break;
        }
    }

    enum IsolatedCopyTag { IsolatedCopy };
    IDBGetAllResult(const IDBGetAllResult&, IsolatedCopyTag);
    IDBGetAllResult isolatedCopy() const;

    IndexedDB::GetAllType type() const { return m_type; }
    const Vector<IDBKeyData>& keys() const;
    const Vector<IDBValue>& values() const;

    void addKey(IDBKeyData&&);
    void addValue(IDBValue&&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBGetAllResult&);

    WEBCORE_EXPORT Vector<String> allBlobFilePaths() const;

private:
    static void isolatedCopy(const IDBGetAllResult& source, IDBGetAllResult& destination);

    IndexedDB::GetAllType m_type { IndexedDB::GetAllType::Keys };
    WTF::Variant<Vector<IDBKeyData>, Vector<IDBValue>, std::nullptr_t> m_results { nullptr };
};

template<class Encoder>
void IDBGetAllResult::encode(Encoder& encoder) const
{
    encoder << m_type << static_cast<uint64_t>(m_results.index());

    switch (m_results.index()) {
    case 0:
        encoder << WTF::get<Vector<IDBKeyData>>(m_results);
        break;
    case 1:
        encoder << WTF::get<Vector<IDBValue>>(m_results);
        break;
    case 2:
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<class Decoder>
bool IDBGetAllResult::decode(Decoder& decoder, IDBGetAllResult& result)
{
    if (!decoder.decode(result.m_type))
        return false;

    uint64_t index;
    if (!decoder.decode(index))
        return false;

    switch (index) {
    case 0: {
        result.m_results = Vector<IDBKeyData>();
        if (!decoder.decode(WTF::get<Vector<IDBKeyData>>(result.m_results)))
            return false;
        break;
    }
    case 1: {
        result.m_results = Vector<IDBValue>();
        Optional<Vector<IDBValue>> optional;
        decoder >> optional;
        if (!optional)
            return false;
        WTF::get<Vector<IDBValue>>(result.m_results) = WTFMove(*optional);
        break;
    }
    case 2:
        result.m_results = nullptr;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
