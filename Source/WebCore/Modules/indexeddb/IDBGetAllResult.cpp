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

#include "config.h"
#include "IDBGetAllResult.h"

#if ENABLE(INDEXED_DATABASE)

#include <wtf/HashSet.h>

namespace WebCore {

template<typename T> void isolatedCopyOfVariant(const WTF::Variant<Vector<IDBKeyData>, Vector<IDBValue>, std::nullptr_t>& source, WTF::Variant<Vector<IDBKeyData>, Vector<IDBValue>, std::nullptr_t>& target)
{
    target = Vector<T>();
    auto& sourceVector = WTF::get<Vector<T>>(source);
    auto& targetVector = WTF::get<Vector<T>>(target);
    targetVector.reserveInitialCapacity(sourceVector.size());
    for (auto& element : sourceVector)
        targetVector.uncheckedAppend(element.isolatedCopy());
}

IDBGetAllResult IDBGetAllResult::isolatedCopy() const
{
    IDBGetAllResult result;
    result.m_type = m_type;

    if (WTF::holds_alternative<std::nullptr_t>(m_results))
        return result;

    switch (m_type) {
    case IndexedDB::GetAllType::Keys:
        isolatedCopyOfVariant<IDBKeyData>(m_results, result.m_results);
        break;
    case IndexedDB::GetAllType::Values:
        isolatedCopyOfVariant<IDBValue>(m_results, result.m_results);
        break;
    }

    return result;
}

void IDBGetAllResult::addKey(IDBKeyData&& key)
{
    ASSERT(m_type == IndexedDB::GetAllType::Keys);
    ASSERT(WTF::holds_alternative<Vector<IDBKeyData>>(m_results));
    WTF::get<Vector<IDBKeyData>>(m_results).append(WTFMove(key));
}

void IDBGetAllResult::addValue(IDBValue&& value)
{
    ASSERT(m_type == IndexedDB::GetAllType::Values);
    ASSERT(WTF::holds_alternative<Vector<IDBValue>>(m_results));
    WTF::get<Vector<IDBValue>>(m_results).append(WTFMove(value));
}

const Vector<IDBKeyData>& IDBGetAllResult::keys() const
{
    ASSERT(m_type == IndexedDB::GetAllType::Keys);
    ASSERT(WTF::holds_alternative<Vector<IDBKeyData>>(m_results));
    return WTF::get<Vector<IDBKeyData>>(m_results);
}

const Vector<IDBValue>& IDBGetAllResult::values() const
{
    ASSERT(m_type == IndexedDB::GetAllType::Values);
    ASSERT(WTF::holds_alternative<Vector<IDBValue>>(m_results));
    return WTF::get<Vector<IDBValue>>(m_results);
}

Vector<String> IDBGetAllResult::allBlobFilePaths() const
{
    ASSERT(m_type == IndexedDB::GetAllType::Values);

    HashSet<String> pathSet;
    for (auto& value : WTF::get<Vector<IDBValue>>(m_results)) {
        for (auto& path : value.blobFilePaths())
            pathSet.add(path);
    }

    Vector<String> paths;
    copyToVector(pathSet, paths);

    return paths;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
