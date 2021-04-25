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
#include "IDBValue.h"

namespace WebCore {

struct IDBCursorRecord {
    IDBKeyData key;
    IDBKeyData primaryKey;
    IDBValue value;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, IDBCursorRecord&);

    IDBCursorRecord isolatedCopy() const;
    size_t size() const { return key.size() + primaryKey.size() + value.size(); }
};

template<class Encoder>
void IDBCursorRecord::encode(Encoder& encoder) const
{
    encoder << key << primaryKey << value;
}

template<class Decoder>
bool IDBCursorRecord::decode(Decoder& decoder, IDBCursorRecord& record)
{
    if (!decoder.decode(record.key))
        return false;

    if (!decoder.decode(record.primaryKey))
        return false;

    if (!decoder.decode(record.value))
        return false;

    return true;
}

inline IDBCursorRecord IDBCursorRecord::isolatedCopy() const
{
    return { key.isolatedCopy(), primaryKey.isolatedCopy(), value.isolatedCopy() };
}

} // namespace WebCore
