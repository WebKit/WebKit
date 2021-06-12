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

#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "IDBValue.h"
#include "IndexedDB.h"

#include <wtf/IsoMalloc.h>
#include <wtf/Variant.h>

namespace WebCore {

class IDBGetAllResult {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(IDBGetAllResult, WEBCORE_EXPORT);
public:
    IDBGetAllResult()
    {
    }

    IDBGetAllResult(IndexedDB::GetAllType type, const std::optional<IDBKeyPath>& keyPath)
        : m_type(type)
        , m_keyPath(keyPath)
    {
    }

    enum IsolatedCopyTag { IsolatedCopy };
    IDBGetAllResult(const IDBGetAllResult&, IsolatedCopyTag);
    IDBGetAllResult isolatedCopy() const;

    IndexedDB::GetAllType type() const { return m_type; }
    const std::optional<IDBKeyPath>& keyPath() const { return m_keyPath; }
    const Vector<IDBKeyData>& keys() const;
    const Vector<IDBValue>& values() const;

    void addKey(IDBKeyData&&);
    void addValue(IDBValue&&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, IDBGetAllResult&);

    WEBCORE_EXPORT Vector<String> allBlobFilePaths() const;

private:
    static void isolatedCopy(const IDBGetAllResult& source, IDBGetAllResult& destination);

    IndexedDB::GetAllType m_type { IndexedDB::GetAllType::Keys };
    Vector<IDBKeyData> m_keys;
    Vector<IDBValue> m_values;
    std::optional<IDBKeyPath> m_keyPath;
};

template<class Encoder>
void IDBGetAllResult::encode(Encoder& encoder) const
{
    encoder << m_type << m_keys << m_values << m_keyPath;
}

template<class Decoder>
bool IDBGetAllResult::decode(Decoder& decoder, IDBGetAllResult& result)
{
    if (!decoder.decode(result.m_type))
        return false;

    if (!decoder.decode(result.m_keys))
        return false;

    if (!decoder.decode(result.m_values))
        return false;
    
    if (!decoder.decode(result.m_keyPath))
        return false;

    return true;
}

} // namespace WebCore
