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

#ifndef BlobData_h
#define BlobData_h

#include "BlobDataFileReference.h"
#include "URL.h"
#include <wtf/Forward.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RawData : public RefCounted<RawData> {
public:
    static PassRefPtr<RawData> create(Vector<char>&& data)
    {
        return adoptRef(new RawData(WTF::move(data)));
    }

    static PassRefPtr<RawData> create(const char* data, size_t size)
    {
        Vector<char> dataVector(size);
        memcpy(dataVector.data(), data, size);
        return adoptRef(new RawData(WTF::move(dataVector)));
    }

    const char* data() const { return m_data.data(); }
    size_t length() const { return m_data.size(); }

private:
    RawData(Vector<char>&& data)
        : m_data(WTF::move(data))
    {
    }

    Vector<char> m_data;
};

struct BlobDataItem {
    static const long long toEndOfFile;

    enum {
        Data,
        File
    } type;

    // For Data type.
    RefPtr<RawData> data;

    // For File type.
    RefPtr<BlobDataFileReference> file;

    long long offset() const { return m_offset; }
    long long length() const; // Computes file length if it's not known yet.

private:
    friend class BlobData;

    explicit BlobDataItem(PassRefPtr<BlobDataFileReference> file)
        : type(File)
        , file(file)
        , m_offset(0)
        , m_length(toEndOfFile)
    {
    }

    BlobDataItem(PassRefPtr<RawData> data, long long offset, long long length)
        : type(Data)
        , data(data)
        , m_offset(offset)
        , m_length(length)
    {
    }

    BlobDataItem(BlobDataFileReference* file, long long offset, long long length)
        : type(File)
        , file(file)
        , m_offset(offset)
        , m_length(length)
    {
    }

    long long m_offset;
    long long m_length;
};

typedef Vector<BlobDataItem> BlobDataItemList;

class BlobData : public RefCounted<BlobData> {
public:
    static PassRefPtr<BlobData> create()
    {
        return adoptRef(new BlobData);
    }

    const String& contentType() const { return m_contentType; }
    void setContentType(const String&);

    const BlobDataItemList& items() const { return m_items; }
    void swapItems(BlobDataItemList&);

    void appendData(PassRefPtr<RawData>);
    void appendFile(PassRefPtr<BlobDataFileReference>);

private:
    friend class BlobRegistryImpl;

    void appendData(PassRefPtr<RawData>, long long offset, long long length);
    void appendFile(BlobDataFileReference*, long long offset, long long length);

    String m_contentType;
    BlobDataItemList m_items;
};

} // namespace WebCore

#endif // BlobData_h
