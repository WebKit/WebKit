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

#include "FileSystemHandleRecord.h"
#include "FileSystemHandleStorageKeepAlive.h"
#include "SerializedScriptValue.h"
#include <wtf/CrossThreadTask.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBValue);

IDBValue::IDBValue() = default;

IDBValue::IDBValue(const SerializedScriptValue& scriptValue)
    : m_data(ThreadSafeDataBuffer::copyData(scriptValue.wireBytes()))
    , m_blobURLs(scriptValue.blobURLs())
    , m_fileSystemHandleGlobalIdentifiers(scriptValue.fileSystemHandleGlobalIdentifiers())
{
}

IDBValue::IDBValue(const ThreadSafeDataBuffer& value)
    : m_data(value)
{
}

IDBValue::IDBValue(const SerializedScriptValue& scriptValue, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths)
    : m_data(ThreadSafeDataBuffer::copyData(scriptValue.wireBytes()))
    , m_blobURLs(blobURLs)
    , m_blobFilePaths(blobFilePaths)
    , m_fileSystemHandleGlobalIdentifiers(scriptValue.fileSystemHandleGlobalIdentifiers())
{
    ASSERT(m_data.data());
}

IDBValue::IDBValue(const ThreadSafeDataBuffer& value, Vector<String>&& blobURLs, Vector<String>&& blobFilePaths, Vector<FileSystemHandleGlobalIdentifier>&& fileSystemHandleGlobalIdentifiers)
    : m_data(value)
    , m_blobURLs(WTF::move(blobURLs))
    , m_blobFilePaths(WTF::move(blobFilePaths))
    , m_fileSystemHandleGlobalIdentifiers(WTF::move(fileSystemHandleGlobalIdentifiers))
{
}

IDBValue::IDBValue(const ThreadSafeDataBuffer& value, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths, const Vector<FileSystemHandleGlobalIdentifier>& fileSystemHandleGlobalIdentifiers)
    : m_data(value)
    , m_blobURLs(blobURLs)
    , m_blobFilePaths(blobFilePaths)
    , m_fileSystemHandleGlobalIdentifiers(fileSystemHandleGlobalIdentifiers)
{
}

IDBValue::IDBValue(const IDBValue&) = default;
IDBValue::IDBValue(IDBValue&&) = default;
IDBValue& IDBValue::operator=(const IDBValue&) = default;
IDBValue& IDBValue::operator=(IDBValue&&) = default;
IDBValue::~IDBValue() = default;

void IDBValue::attachStorageKeepAlive(RefPtr<FileSystemHandleStorageKeepAlive>&& keepAlive) const
{
    ASSERT(!m_storageKeepAlive);
    m_storageKeepAlive = WTF::move(keepAlive);
}

const Vector<FileSystemHandleRecord>& IDBValue::fileSystemHandleRecords() const
{
    return m_fileSystemHandleRecords;
}

void IDBValue::setFileSystemHandleRecords(Vector<FileSystemHandleRecord>&& records) const
{
    m_fileSystemHandleRecords = WTF::move(records);
}

void IDBValue::setAsIsolatedCopy(const IDBValue& other)
{
    ASSERT(m_blobURLs.isEmpty() && m_blobFilePaths.isEmpty() && m_fileSystemHandleGlobalIdentifiers.isEmpty() && m_fileSystemHandleRecords.isEmpty() && !m_storageKeepAlive);

    m_data = other.m_data;
    m_blobURLs = crossThreadCopy(other.m_blobURLs);
    m_blobFilePaths = crossThreadCopy(other.m_blobFilePaths);
    m_fileSystemHandleGlobalIdentifiers = other.m_fileSystemHandleGlobalIdentifiers;
    m_fileSystemHandleRecords = crossThreadCopy(other.m_fileSystemHandleRecords);
    m_storageKeepAlive = other.m_storageKeepAlive;
}

IDBValue IDBValue::isolatedCopy() const
{
    IDBValue result;
    result.setAsIsolatedCopy(*this);
    return result;
}

size_t IDBValue::size() const
{
    size_t totalSize = 0;

    for (auto& url : m_blobURLs)
        totalSize += url.sizeInBytes();

    for (auto& path : m_blobFilePaths)
        totalSize += path.sizeInBytes();

    totalSize += m_fileSystemHandleGlobalIdentifiers.size() * sizeof(FileSystemHandleGlobalIdentifier);

    totalSize += m_data.size();

    return totalSize;
}

} // namespace WebCore
