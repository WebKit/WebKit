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

#if ENABLE(BLOB_SLICE)
static const double invalidModificationTime = 0;

static double getFileSnapshotModificationTime(const String& path)
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

// Normalize all line-endings to CRLF.
static CString convertToCRLF(const CString& from)
{
    unsigned newLen = 0;
    const char* p = from.data();
    while (char c = *p++) {
        if (c == '\r') {
            // Safe to look ahead because of trailing '\0'.
            if (*p != '\n') {
                // Turn CR into CRLF.
                newLen += 2;
            }
        } else if (c == '\n') {
            // Turn LF into CRLF.
            newLen += 2;
        } else {
            // Leave other characters alone.
            newLen += 1;
        }
    }
    if (newLen == from.length())
        return from;

    // Make a copy of the string.
    p = from.data();
    char* q;
    CString result = CString::newUninitialized(newLen, q);
    while (char c = *p++) {
        if (c == '\r') {
            // Safe to look ahead because of trailing '\0'.
            if (*p != '\n') {
                // Turn CR into CRLF.
                *q++ = '\r';
                *q++ = '\n';
            }
        } else if (c == '\n') {
            // Turn LF into CRLF.
            *q++ = '\r';
            *q++ = '\n';
        } else {
            // Leave other characters alone.
            *q++ = c;
        }
    }
    return result;
}

// Normalize all line-endings to CR or LF.
static CString convertToCROrLF(const CString& from, bool toCR)
{
    unsigned newLen = 0;
    bool needFix = false;
    const char* p = from.data();
    char fromEndingChar = toCR ? '\n' : '\r';
    char toEndingChar = toCR ? '\r' : '\n';
    while (char c = *p++) {
        if (c == '\r' && *p == '\n') {
            // Turn CRLF into CR or LF.
            p++;
            needFix = true;
        } else if (c == fromEndingChar) {
            // Turn CR/LF into LF/CR.
            needFix = true;
        }
        newLen += 1;
    }
    if (!needFix)
        return from;

    // Make a copy of the string.
    p = from.data();
    char* q;
    CString result = CString::newUninitialized(newLen, q);
    while (char c = *p++) {
        if (c == '\r' && *p == '\n') {
            // Turn CRLF or CR into CR or LF.
            p++;
            *q++ = toEndingChar;
        } else if (c == fromEndingChar) {
            // Turn CR/LF into LF/CR.
            *q++ = toEndingChar;
        } else {
            // Leave other characters alone.
            *q++ = c;
        }
    }
    return result;
}

CString StringBlobItem::convertToCString(const String& text, LineEnding ending, TextEncoding encoding)
{
    CString from = encoding.encode(text.characters(), text.length(), EntitiesForUnencodables);

    if (ending == EndingNative) {
#if OS(WINDOWS)
        ending = EndingCRLF;
#else
        ending = EndingLF;
#endif
    }

    switch (ending) {
    case EndingTransparent:
        return from;
    case EndingCRLF:
        return convertToCRLF(from);
    case EndingCR:
        return convertToCROrLF(from, true);
    case EndingLF:
        return convertToCROrLF(from, false);
    default:
        ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
    return from;
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

#endif // ENABLE(BLOB_SLICE)

} // namespace WebCore
