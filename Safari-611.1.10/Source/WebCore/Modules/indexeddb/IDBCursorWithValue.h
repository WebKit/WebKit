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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursor.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

class IDBCursorWithValue final : public IDBCursor {
    WTF_MAKE_ISO_ALLOCATED(IDBCursorWithValue);
public:
    static Ref<IDBCursorWithValue> create(IDBObjectStore&, const IDBCursorInfo&);
    static Ref<IDBCursorWithValue> create(IDBIndex&, const IDBCursorInfo&);

    virtual ~IDBCursorWithValue();

    bool isKeyCursorWithValue() const  override { return true; }

private:
    IDBCursorWithValue(IDBObjectStore&, const IDBCursorInfo&);
    IDBCursorWithValue(IDBIndex&, const IDBCursorInfo&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::IDBCursorWithValue)
    static bool isType(const WebCore::IDBCursor& cursor) { return cursor.isKeyCursorWithValue(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(INDEXED_DATABASE)
