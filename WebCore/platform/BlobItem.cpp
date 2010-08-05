/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BlobItem.h"

#include "FileSystem.h"
#include "UUID.h"
#include <wtf/Assertions.h>

namespace WebCore {

#if ENABLE(BLOB)
static const double invalidModificationTime = 0;

static double getFileSnapshotModificationTime(const String& path)
{
    // FIXME: synchronized file call
    time_t modificationTime;
    if (getFileModificationTime(path, modificationTime))
        return static_cast<double>(modificationTime);
    return invalidModificationTime;
}
#endif // ENABLE(BLOB)

// DataBlobItem ----------------------------------------------------------------

#if ENABLE(BLOB)
PassRefPtr<BlobItem> DataBlobItem::slice(long long start, long long length)
{
    ASSERT(start >= 0 && length >= 0);
    ASSERT(static_cast<unsigned long long>(start) < size());
    if (!start && size() <= static_cast<unsigned long long>(length))
        return this;
    if (static_cast<unsigned long long>(start + length) > size())
        length = size() - start;
    return DataRangeBlobItem::create(this, start, length);
}
#endif

// FileBlobItem ----------------------------------------------------------------

PassRefPtr<BlobItem> FileBlobItem::create(const String& path)
{
    return adoptRef(static_cast<BlobItem*>(new FileBlobItem(path)));
}

FileBlobItem::FileBlobItem(const String& path)
    : m_path(path)
    , m_fileName(pathGetFileName(m_path))
{
}

#if ENABLE(DIRECTORY_UPLOAD)
PassRefPtr<BlobItem> FileBlobItem::create(const String& path, const String& relativePath)
{
    return adoptRef(static_cast<BlobItem*>(new FileBlobItem(path, relativePath)));
}

FileBlobItem::FileBlobItem(const String& path, const String& relativePath)
    : m_path(path)
    , m_fileName(pathGetFileName(m_path))
    , m_relativePath(relativePath)
{
}
#endif

unsigned long long FileBlobItem::size() const
{
    // FIXME: synchronized file call
    long long size;
    if (!getFileSize(m_path, size))
        return 0;
    return static_cast<unsigned long long>(size);
}

#if ENABLE(BLOB)
PassRefPtr<BlobItem> FileBlobItem::slice(long long start, long long length)
{
    ASSERT(start >= 0 && length >= 0);
    long long fileSize = size();
    ASSERT(start < fileSize);
    if (!start && fileSize <= length)
        return this;
    if (start + length > fileSize)
        length = fileSize - start;
    const FileRangeBlobItem* fileRangeItem = toFileRangeBlobItem();
    double modificationTime = fileRangeItem ? fileRangeItem->snapshotModificationTime() : getFileSnapshotModificationTime(path());
    return FileRangeBlobItem::create(path(), start, length, modificationTime);
}
#endif // ENABLE(BLOB)

// StringBlobItem --------------------------------------------------------------

PassRefPtr<BlobItem> StringBlobItem::create(const CString& text)
{
    return adoptRef(static_cast<BlobItem*>(new StringBlobItem(text)));
}

StringBlobItem::StringBlobItem(const CString& text)
    : m_data(text)
{
}

// ByteArrayBlobItem ----------------------------------------------------------

PassRefPtr<BlobItem> ByteArrayBlobItem::create(const char* data, size_t size)
{
    return adoptRef(static_cast<BlobItem*>(new ByteArrayBlobItem(data, size)));
}

ByteArrayBlobItem::ByteArrayBlobItem(const char* data, size_t size)
{
    m_bytesArray.append(data, size);
}

#if ENABLE(BLOB)

// DataRangeBlobItem -----------------------------------------------------------

PassRefPtr<BlobItem> DataRangeBlobItem::create(PassRefPtr<DataBlobItem> item, long long start, long long length)
{
    return adoptRef(static_cast<BlobItem*>(new DataRangeBlobItem(item, start, length)));
}

DataRangeBlobItem::DataRangeBlobItem(PassRefPtr<DataBlobItem> item, long long start, long long length)
    : m_length(length)
{
    const DataRangeBlobItem* rangeItem = item->toDataRangeBlobItem();
    if (rangeItem) {
        m_item = rangeItem->m_item;
        m_start = start + rangeItem->m_start;
        ASSERT(!m_item->toDataRangeBlobItem());
        ASSERT(static_cast<unsigned long long>(m_start + m_length) <= m_item->size());
    } else {
        m_item = item;
        m_start = start;
    }
}

const char* DataRangeBlobItem::data() const
{
    return m_item->data() + m_start;
}

// FileRangeBlobItem -----------------------------------------------------------

PassRefPtr<BlobItem> FileRangeBlobItem::create(const String& path, long long start, long long length, double snapshotModificationTime)
{
    return adoptRef(static_cast<BlobItem*>(new FileRangeBlobItem(path, start, length, snapshotModificationTime)));
}

FileRangeBlobItem::FileRangeBlobItem(const String& path, long long start, long long length, double modificationTime)
    : FileBlobItem(path)
    , m_start(start)
    , m_length(length)
    , m_snapshotModificationTime(modificationTime)
{
    m_uniqueName = "Blob" + createCanonicalUUIDString();
    m_uniqueName.replace("-", ""); // For safty, remove '-' from the filename snce some servers may not like it.
}

#endif // ENABLE(BLOB)

} // namespace WebCore
