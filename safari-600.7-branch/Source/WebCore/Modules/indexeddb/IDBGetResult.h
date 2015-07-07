/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef IDBGetResult_h
#define IDBGetResult_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "SharedBuffer.h"

namespace WebCore {

struct IDBGetResult {
    IDBGetResult()
    {
    }

    IDBGetResult(PassRefPtr<SharedBuffer> buffer)
        : valueBuffer(buffer)
    {
    }

    IDBGetResult(PassRefPtr<IDBKey> key)
        : keyData(key.get())
    {
    }

    IDBGetResult(const IDBKeyData& keyData)
        : keyData(keyData)
    {
    }

    IDBGetResult(PassRefPtr<SharedBuffer> buffer, PassRefPtr<IDBKey> key, const IDBKeyPath& path)
        : valueBuffer(buffer)
        , keyData(key.get())
        , keyPath(path)
    {
    }

    IDBGetResult isolatedCopy() const
    {
        IDBGetResult result;
        if (valueBuffer)
            result.valueBuffer = valueBuffer->copy();

        result.keyData = keyData.isolatedCopy();
        result.keyPath = keyPath.isolatedCopy();
        return result;
    }

    RefPtr<SharedBuffer> valueBuffer;
    IDBKeyData keyData;
    IDBKeyPath keyPath;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBGetResult_h
