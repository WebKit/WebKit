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

#include "Blob.h"
#include "BlobURL.h"

namespace WebCore {

BlobData::BlobData(const String& contentType)
    : m_contentType(contentType)
{
    ASSERT(Blob::isNormalizedContentType(contentType));
}

const long long BlobDataItem::toEndOfFile = -1;

long long BlobDataItem::length() const
{
    if (m_length != toEndOfFile)
        return m_length;

    switch (m_type) {
    case Type::Data:
        ASSERT_NOT_REACHED();
        return m_length;
    case Type::File:
        return m_file->size();
    }

    ASSERT_NOT_REACHED();
    return m_length;
}

void BlobData::appendData(const ThreadSafeDataBuffer& data)
{
    size_t dataSize = data.data() ? data.data()->size() : 0;
    appendData(data, 0, dataSize);
}

void BlobData::appendData(const ThreadSafeDataBuffer& data, long long offset, long long length)
{
    m_items.append(BlobDataItem(data, offset, length));
}

void BlobData::appendFile(Ref<BlobDataFileReference>&& file)
{
    file->startTrackingModifications();
    m_items.append(BlobDataItem(WTFMove(file)));
}

Ref<BlobData> BlobData::clone() const
{
    auto blobData = BlobData::create(m_contentType);
    blobData->m_policyContainer = m_policyContainer;
    blobData->m_items = m_items;
    return blobData;
}

void BlobData::appendFile(BlobDataFileReference* file, long long offset, long long length)
{
    m_items.append(BlobDataItem(file, offset, length));
}

void BlobData::swapItems(BlobDataItemList& items)
{
    m_items.swap(items);
}

} // namespace WebCore
