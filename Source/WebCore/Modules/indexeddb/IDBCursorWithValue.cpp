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
#include "IDBCursorWithValue.h"

#if ENABLE(INDEXED_DATABASE)

#include <heap/HeapInlines.h>

namespace WebCore {

Ref<IDBCursorWithValue> IDBCursorWithValue::create(IDBTransaction& transaction, IDBObjectStore& objectStore, const IDBCursorInfo& info)
{
    return adoptRef(*new IDBCursorWithValue(transaction, objectStore, info));
}

Ref<IDBCursorWithValue> IDBCursorWithValue::create(IDBTransaction& transaction, IDBIndex& index, const IDBCursorInfo& info)
{
    return adoptRef(*new IDBCursorWithValue(transaction, index, info));
}

IDBCursorWithValue::IDBCursorWithValue(IDBTransaction& transaction, IDBObjectStore& objectStore, const IDBCursorInfo& info)
    : IDBCursor(transaction, objectStore, info)
{
}

IDBCursorWithValue::IDBCursorWithValue(IDBTransaction& transaction, IDBIndex& index, const IDBCursorInfo& info)
    : IDBCursor(transaction, index, info)
{
}

IDBCursorWithValue::~IDBCursorWithValue()
{
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
