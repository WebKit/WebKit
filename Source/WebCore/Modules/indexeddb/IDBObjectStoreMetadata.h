/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBObjectStoreMetadata_h
#define IDBObjectStoreMetadata_h

#include "IDBIndexMetadata.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

struct IDBObjectStoreMetadata {
    IDBObjectStoreMetadata()
    {
    }

    IDBObjectStoreMetadata(const String& name, int64_t id, const IDBKeyPath& keyPath, bool autoIncrement, int64_t maxIndexId)
        : name(name)
        , id(id)
        , keyPath(keyPath)
        , autoIncrement(autoIncrement)
        , maxIndexId(maxIndexId)
    {
    }

    String name;
    int64_t id;
    IDBKeyPath keyPath;
    bool autoIncrement;
    int64_t maxIndexId;

    static const int64_t InvalidId = -1;

    typedef HashMap<int64_t, IDBIndexMetadata> IndexMap;
    IndexMap indexes;

    IDBObjectStoreMetadata isolatedCopy() const;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBObjectStoreMetadata_h
