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
#include "BlobData.h"

namespace WebCore {

const long long BlobDataItem::toEndOfFile = -1;
const double BlobDataItem::doNotCheckFileChange = 0;

void BlobDataItem::copy(const BlobDataItem& item)
{
    type = item.type;
    data = item.data; // This is OK because the underlying storage is Vector<char>.
    path = item.path.crossThreadString();
    url = item.url.copy();
    offset = item.offset;
    length = item.length;
    expectedModificationTime = item.expectedModificationTime;
}

PassOwnPtr<BlobData> BlobData::copy() const
{
    OwnPtr<BlobData> blobData = adoptPtr(new BlobData());
    blobData->m_contentType = m_contentType.crossThreadString();
    blobData->m_contentDisposition = m_contentDisposition.crossThreadString();
    blobData->m_items.resize(m_items.size());
    for (size_t i = 0; i < m_items.size(); ++i)
        blobData->m_items.at(i).copy(m_items.at(i));

    return blobData.release();
}

void BlobData::appendData(const CString& data)
{
    m_items.append(BlobDataItem(data));
}

void BlobData::appendData(const CString& data, long long offset, long long length)
{
    m_items.append(BlobDataItem(data, offset, length));
}

void BlobData::appendFile(const String& path)
{
    m_items.append(BlobDataItem(path));
}

void BlobData::appendFile(const String& path, long long offset, long long length, double expectedModificationTime)
{
    m_items.append(BlobDataItem(path, offset, length, expectedModificationTime));
}

void BlobData::appendBlob(const KURL& url, long long offset, long long length)
{
    m_items.append(BlobDataItem(url, offset, length));
}

void BlobData::swapItems(BlobDataItemList& items)
{
    m_items.swap(items);
}

} // namespace WebCore
