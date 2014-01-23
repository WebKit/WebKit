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

#ifndef IDBKeyRangeData_h
#define IDBKeyRangeData_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyData.h"
#include "IDBKeyRange.h"

namespace WebCore {

struct IDBKeyRangeData {
    IDBKeyRangeData()
        : isNull(true)
        , lowerOpen(false)
        , upperOpen(false)
    {
    }

    IDBKeyRangeData(IDBKeyRange* keyRange)
        : isNull(!keyRange)
        , lowerOpen(false)
        , upperOpen(false)
    {
        if (isNull)
            return;

        lowerKey = keyRange->lower().get();
        upperKey = keyRange->upper().get();
        lowerOpen = keyRange->lowerOpen();
        upperOpen = keyRange->upperOpen();
    }

    IDBKeyRangeData isolatedCopy() const;

    bool isNull;

    IDBKeyData lowerKey;
    IDBKeyData upperKey;

    bool lowerOpen;
    bool upperOpen;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBKeyRangeData_h
