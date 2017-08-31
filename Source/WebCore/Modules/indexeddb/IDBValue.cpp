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
#include "IDBValue.h"

#if ENABLE(INDEXED_DATABASE)

#include "SerializedScriptValue.h"
#include <wtf/CrossThreadTask.h>

namespace WebCore {

IDBValue::IDBValue()
{
}

IDBValue::IDBValue(const SerializedScriptValue& scriptValue)
    : m_data(ThreadSafeDataBuffer::copyVector(scriptValue.data()))
    , m_blobURLs(scriptValue.blobURLsIsolatedCopy())
{
}

IDBValue::IDBValue(const ThreadSafeDataBuffer& value)
    : m_data(value)
{
}

IDBValue::IDBValue(const SerializedScriptValue& scriptValue, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths)
    : m_data(ThreadSafeDataBuffer::copyVector(scriptValue.data()))
    , m_blobURLs(blobURLs)
    , m_blobFilePaths(blobFilePaths)
{
    ASSERT(m_data.data());
}

IDBValue::IDBValue(const ThreadSafeDataBuffer& value, Vector<String>&& blobURLs, Vector<String>&& blobFilePaths)
    : m_data(value)
    , m_blobURLs(WTFMove(blobURLs))
    , m_blobFilePaths(WTFMove(blobFilePaths))
{
}

IDBValue::IDBValue(const ThreadSafeDataBuffer& value, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths)
    : m_data(value)
    , m_blobURLs(blobURLs)
    , m_blobFilePaths(blobFilePaths)
{
}

void IDBValue::setAsIsolatedCopy(const IDBValue& other)
{
    ASSERT(m_blobURLs.isEmpty() && m_blobFilePaths.isEmpty());

    m_data = other.m_data;
    m_blobURLs = crossThreadCopy(other.m_blobURLs);
    m_blobFilePaths = crossThreadCopy(other.m_blobFilePaths);
}

IDBValue IDBValue::isolatedCopy() const
{
    IDBValue result;
    result.setAsIsolatedCopy(*this);
    return result;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
