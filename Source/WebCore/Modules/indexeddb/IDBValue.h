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

#include <WebCore/FileSystemHandleGlobalIdentifier.h>
#include <WebCore/ThreadSafeDataBuffer.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FileSystemHandleStorageKeepAlive;
class SerializedScriptValue;
struct FileSystemHandleRecord;

class IDBValue {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(IDBValue, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT IDBValue();
    IDBValue(const SerializedScriptValue&);
    WEBCORE_EXPORT IDBValue(const ThreadSafeDataBuffer&);
    IDBValue(const SerializedScriptValue&, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths);
    WEBCORE_EXPORT IDBValue(const ThreadSafeDataBuffer&, Vector<String>&& blobURLs, Vector<String>&& blobFilePaths, Vector<FileSystemHandleGlobalIdentifier>&& fileSystemHandleGlobalIdentifiers = { });
    IDBValue(const ThreadSafeDataBuffer&, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths, const Vector<FileSystemHandleGlobalIdentifier>& fileSystemHandleGlobalIdentifiers = { });

    WEBCORE_EXPORT IDBValue(const IDBValue&);
    WEBCORE_EXPORT IDBValue(IDBValue&&);
    WEBCORE_EXPORT IDBValue& operator=(const IDBValue&);
    WEBCORE_EXPORT IDBValue& operator=(IDBValue&&);
    WEBCORE_EXPORT ~IDBValue();

    void setAsIsolatedCopy(const IDBValue&);
    WEBCORE_EXPORT IDBValue isolatedCopy() const;

    const ThreadSafeDataBuffer& data() const LIFETIME_BOUND { return m_data; }
    const Vector<String>& blobURLs() const LIFETIME_BOUND { return m_blobURLs; }
    const Vector<String>& blobFilePaths() const LIFETIME_BOUND { return m_blobFilePaths; }
    const Vector<FileSystemHandleGlobalIdentifier>& fileSystemHandleGlobalIdentifiers() const LIFETIME_BOUND { return m_fileSystemHandleGlobalIdentifiers; }

    WEBCORE_EXPORT const Vector<FileSystemHandleRecord>& fileSystemHandleRecords() const LIFETIME_BOUND;
    WEBCORE_EXPORT void setFileSystemHandleRecords(Vector<FileSystemHandleRecord>&&) const;

    WEBCORE_EXPORT void attachStorageKeepAlive(RefPtr<FileSystemHandleStorageKeepAlive>&&) const;

    size_t NODELETE size() const;
private:
    ThreadSafeDataBuffer m_data;
    Vector<String> m_blobURLs;
    Vector<String> m_blobFilePaths;
    Vector<FileSystemHandleGlobalIdentifier> m_fileSystemHandleGlobalIdentifiers;
    mutable Vector<FileSystemHandleRecord> m_fileSystemHandleRecords;
    mutable RefPtr<FileSystemHandleStorageKeepAlive> m_storageKeepAlive;
};

} // namespace WebCore
