/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "IDBDatabaseMetadata.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IDBDatabaseMetadata IDBDatabaseMetadata::isolatedCopy() const
{
    IDBDatabaseMetadata result;
    result.id = id;
    result.version = version;
    result.maxObjectStoreId = maxObjectStoreId;

    result.name = name.isolatedCopy();

    for (auto i = objectStores.begin(), end = objectStores.end(); i != end; ++i)
        result.objectStores.set(i->key, i->value.isolatedCopy());

    return result;
}

IDBObjectStoreMetadata IDBObjectStoreMetadata::isolatedCopy() const
{
    IDBObjectStoreMetadata result;
    result.id = id;
    result.autoIncrement = autoIncrement;
    result.maxIndexId = maxIndexId;

    result.name = name.isolatedCopy();
    result.keyPath = keyPath.isolatedCopy();

    for (auto i = indexes.begin(), end = indexes.end(); i != end; ++i)
        result.indexes.set(i->key, i->value.isolatedCopy());

    return result;
}

IDBIndexMetadata IDBIndexMetadata::isolatedCopy() const
{
    IDBIndexMetadata result;
    result.id = id;
    result.unique = unique;
    result.multiEntry = multiEntry;

    result.name = name.isolatedCopy();
    result.keyPath = keyPath.isolatedCopy();

    return result;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
