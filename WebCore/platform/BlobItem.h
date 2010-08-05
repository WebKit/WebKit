/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#ifndef BlobItem_h
#define BlobItem_h

#include "PlatformString.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {

class ByteArrayBlobItem;
class DataBlobItem;
class DataRangeBlobItem;
class FileBlobItem;
class FileRangeBlobItem;
class StringBlobItem;

// A base interface for all arbitrary-data-object component items.
// The BlobItem and its subclasses are structured like the following:
//    BlobItem
//       |
//       +-------> DataBlobItem
//       |              |
//       |              +---------> StringBlobItem
//       |              +---------> ByteArrayBlobItem
//       |              +---------> DataRangeBlobItem
//       |
//       +-------> FileBlobItem
//                      |
//                      +---------> FileRangeBlobItem
//
class BlobItem : public RefCounted<BlobItem> {
public:
    virtual ~BlobItem() { }
    virtual unsigned long long size() const = 0;

    virtual const DataBlobItem* toDataBlobItem() const { return 0; }
    virtual const StringBlobItem* toStringBlobItem() const { return 0; }
    virtual const ByteArrayBlobItem* toByteArrayBlobItem() const { return 0; }
    virtual const FileBlobItem* toFileBlobItem() const { return 0; }
#if ENABLE(BLOB)
    virtual const DataRangeBlobItem* toDataRangeBlobItem() const { return 0; }
    virtual const FileRangeBlobItem* toFileRangeBlobItem() const { return 0; }

    // Note: no external methods except for Blob::slice should call this.
    virtual PassRefPtr<BlobItem> slice(long long start, long long length) = 0;
#endif // ENABLE(BLOB)

protected:
    BlobItem() { }
};

typedef Vector<RefPtr<BlobItem> > BlobItemList;

class DataBlobItem : public BlobItem {
public:
    virtual const char* data() const = 0;

    // BlobItem methods.
    virtual const DataBlobItem* toDataBlobItem() const { return this; }
#if ENABLE(BLOB)
    virtual PassRefPtr<BlobItem> slice(long long start, long long length);
#endif // ENABLE(BLOB)
};

class FileBlobItem : public BlobItem {
public:
    static PassRefPtr<BlobItem> create(const String& path);
#if ENABLE(DIRECTORY_UPLOAD)
    static PassRefPtr<BlobItem> create(const String& path, const String& relativePath);
#endif
    virtual const String& name() const { return m_fileName; }
    virtual const String& path() const { return m_path; }
#if ENABLE(DIRECTORY_UPLOAD)
    const String& relativePath() const { return m_relativePath; }
#endif

    // BlobItem methods.
    virtual unsigned long long size() const;
    virtual const FileBlobItem* toFileBlobItem() const { return this; }
#if ENABLE(BLOB)
    virtual PassRefPtr<BlobItem> slice(long long start, long long length);
#endif // ENABLE(BLOB)

protected:
    FileBlobItem(const String& path);
#if ENABLE(DIRECTORY_UPLOAD)
    FileBlobItem(const String& path, const String& relativePath);
#endif
    String m_path;
    String m_fileName;
#if ENABLE(DIRECTORY_UPLOAD)
    String m_relativePath;
#endif
};

class StringBlobItem : public DataBlobItem {
public:
    static PassRefPtr<BlobItem> create(const CString&);
    const CString& cstr() const { return m_data; }

    // BlobItem methods.
    virtual unsigned long long size() const { return m_data.length(); }
    virtual const StringBlobItem* toStringBlobItem() const { return this; }

    // DataBlobItem methods.
    virtual const char* data() const { return m_data.data(); }

private:
    StringBlobItem(const CString&);
    CString m_data;
};

class ByteArrayBlobItem : public DataBlobItem {
public:
    static PassRefPtr<BlobItem> create(const char* data, size_t size);

    // BlobItem methods.
    virtual unsigned long long size() const { return m_bytesArray.size(); }
    virtual const ByteArrayBlobItem* toByteArrayBlobItem() const { return this; }

    // DataBlobItem methods.
    virtual const char* data() const { return m_bytesArray.data(); }

private:
    ByteArrayBlobItem(const char* data, size_t size);
    Vector<char> m_bytesArray;
};

#if ENABLE(BLOB)

// BlobItem class for sliced data (string or bytes-array).
class DataRangeBlobItem : public DataBlobItem {
public:
    static PassRefPtr<BlobItem> create(PassRefPtr<DataBlobItem>, long long start, long long length);

    // BlobItem methods.
    virtual unsigned long long size() const { return m_length; }
    virtual const DataRangeBlobItem* toDataRangeBlobItem() const { return this; }

    // DataBlobItem methods.
    virtual const char* data() const;

private:
    DataRangeBlobItem(PassRefPtr<DataBlobItem>, long long start, long long length);
    RefPtr<DataBlobItem> m_item;
    long long m_start;
    long long m_length;
};

class FileRangeBlobItem : public FileBlobItem {
public:
    static PassRefPtr<BlobItem> create(const String& path, long long start, long long length, double snapshotModificationTime);
    long long start() const { return m_start; }
    double snapshotModificationTime() const { return m_snapshotModificationTime; }

    // BlobItem methods.
    virtual unsigned long long size() const { return m_length; }
    virtual const FileRangeBlobItem* toFileRangeBlobItem() const { return this; }

    // FileBlobItem methods.
    virtual const String& name() const { return m_uniqueName; }

private:
    FileRangeBlobItem(const String& path, long long start, long long length, double snapshotModificationTime);
    long long m_start;
    long long m_length;
    String m_uniqueName;

    double m_snapshotModificationTime;
};

#endif // ENABLE(BLOB)

} // namespace WebCore

#endif // BlobItem_h
