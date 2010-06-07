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

namespace WebCore {

#if ENABLE(BLOB_SLICE)
static const double invalidModificationTime = 0;

double getFileSnapshotModificationTime(const String& path)
{
    // FIXME: synchronized file call
    time_t modificationTime;
    if (getFileModificationTime(path, modificationTime))
        return static_cast<double>(modificationTime);
    return invalidModificationTime;
}
#endif // ENABLE(BLOB_SLICE)

// DataBlobItem ----------------------------------------------------------------

#if ENABLE(BLOB_SLICE)
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
#endif // ENABLE(BLOB_SLICE)

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

unsigned long long FileBlobItem::size() const
{
    // FIXME: synchronized file call
    long long size;
    if (!getFileSize(m_path, size))
        return 0;
    return static_cast<unsigned long long>(size);
}

#if ENABLE(BLOB_SLICE)
PassRefPtr<BlobItem> FileBlobItem::slice(long long start, long long length)
{
    ASSERT(start >= 0 && length >= 0);
    ASSERT(static_cast<unsigned long long>(start) < size());
    if (!start && size() <= static_cast<unsigned long long>(length))
        return this;
    const FileRangeBlobItem* fileRangeItem = toFileRangeBlobItem();
    double modificationTime = fileRangeItem ? fileRangeItem->snapshotModificationTime() : getFileSnapshotModificationTime(path());
    return FileRangeBlobItem::create(path(), start, length, modificationTime);
}
#endif // ENABLE(BLOB_SLICE)

// StringBlobItem --------------------------------------------------------------

PassRefPtr<BlobItem> StringBlobItem::create(const String& text, LineEnding ending, TextEncoding encoding)
{
    return adoptRef(static_cast<BlobItem*>(new StringBlobItem(text, ending, encoding)));
}

PassRefPtr<BlobItem> StringBlobItem::create(const CString& text)
{
    return adoptRef(static_cast<BlobItem*>(new StringBlobItem(text)));
}

StringBlobItem::StringBlobItem(const String& text, LineEnding ending, TextEncoding encoding)
    : m_data(StringBlobItem::convertToCString(text, ending, encoding))
{
}

StringBlobItem::StringBlobItem(const CString& text)
    : m_data(text)
{
}

CString StringBlobItem::convertToCString(const String& text, LineEnding ending, TextEncoding encoding)
{
    CString from = encoding.encode(text.characters(), text.length(), EntitiesForUnencodables);
    CString result = from;

    if (ending == EndingTransparent)
        return result;

    const char* endingChars = (ending == EndingCRLF) ? "\r\n" : ((ending == EndingCR) ? "\r" : "\n");

    int endingLength = (ending == EndingCRLF) ? 2 : 1;
    int needFix = 0;

    // Calculate the final length.
    int calculatedLength = from.length();
    const char* p = from.data();
    while (char c = *p++) {
        if (c == '\r') {
            // Safe to look ahead because of trailing '\0'.
            if (*p == '\n' && ending != EndingCRLF) {
                calculatedLength += (endingLength - 2);
                ++needFix;
            } else if (ending != EndingCR) {
                calculatedLength += (endingLength - 1);
                ++needFix;
            }
        } else if (c == '\n' && ending != EndingLF) {
            calculatedLength += (endingLength - 1);
            ++needFix;
        }
    }

    if (!needFix)
        return result;

    // Convert the endings and create a data buffer.
    p = from.data();
    char* q;
    result = CString::newUninitialized(calculatedLength, q);
    while (char c = *p++) {
        if (c == '\r') {
            if (*p == '\n' && ending != EndingCRLF) {
                memcpy(q, endingChars, endingLength);
                q += endingLength;
            } else if (*p != '\n' && ending != EndingCR) {
                memcpy(q, endingChars, endingLength);
                q += endingLength;
            }
        } else if (c == '\n' && ending != EndingLF) {
            memcpy(q, endingChars, endingLength);
            q += endingLength;
        } else {
            // Leave other characters alone.
            *q++ = c;
        }
    }
    return result;
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

#if ENABLE(BLOB_SLICE)

// DataRangeBlobItem -----------------------------------------------------------

PassRefPtr<BlobItem> DataRangeBlobItem::create(PassRefPtr<DataBlobItem> item, long long start, long long length)
{
    return adoptRef(static_cast<BlobItem*>(new DataRangeBlobItem(item, start, length)));
}

DataRangeBlobItem::DataRangeBlobItem(PassRefPtr<DataBlobItem> item, long long start, long long length)
    : m_length(length)
{
    const DataRangeBlobItem* rangeItem = m_item->toDataRangeBlobItem();
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

#endif // ENABLE(BLOB_SLICE)

} // namespace WebCore
