/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IndexValueEntry.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {
namespace IDBServer {

IndexValueEntry::IndexValueEntry(bool unique)
    : m_unique(unique)
{
    if (m_unique)
        m_key = nullptr;
    else
        m_orderedKeys = new std::set<IDBKeyData>;
}

IndexValueEntry::~IndexValueEntry()
{
    if (m_unique)
        delete m_key;
    else
        delete m_orderedKeys;
}

void IndexValueEntry::addKey(const IDBKeyData& key)
{
    if (m_unique) {
        delete m_key;
        m_key = new IDBKeyData(key);
        return;
    }

    m_orderedKeys->insert(key);
}

bool IndexValueEntry::removeKey(const IDBKeyData& key)
{
    if (m_unique && m_key && *m_key == key) {
        delete m_key;
        m_key = nullptr;
        return true;
    }

    m_orderedKeys->erase(key);
    return m_orderedKeys->empty();
}

const IDBKeyData* IndexValueEntry::getLowest() const
{
    if (m_unique)
        return m_key;

    if (m_orderedKeys->empty())
        return nullptr;

    return &(*m_orderedKeys->begin());
}

uint64_t IndexValueEntry::getCount() const
{
    if (m_unique)
        return m_key ? 1 : 0;

    return m_orderedKeys->size();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
