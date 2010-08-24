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
#include "WebBlobStorageData.h"

#include "BlobStorageData.h"

using namespace WebCore;

namespace WebKit {

class WebBlobStorageDataPrivate : public BlobStorageData {
};

void WebBlobStorageData::reset()
{
    assign(0);
}

size_t WebBlobStorageData::itemCount() const
{
    ASSERT(!isNull());
    return m_private->items().size();
}

bool WebBlobStorageData::itemAt(size_t index, WebBlobData::Item& result) const
{
    ASSERT(!isNull());

    if (index >= m_private->items().size())
        return false;

    const BlobDataItem& item = m_private->items()[index];
    result.offset = item.offset;
    result.length = item.length;
    result.expectedModificationTime = item.expectedModificationTime;
    if (item.type == BlobDataItem::Data) {
        result.type = WebBlobData::Item::TypeData;
        result.data.assign(item.data.data(), static_cast<size_t>(item.data.length()));
        return true;
    } else {
        ASSERT(item.type == BlobDataItem::File);
        result.type = WebBlobData::Item::TypeFile;
        result.filePath = item.path;
        return true;
    }
}

WebString WebBlobStorageData::contentType() const
{
    ASSERT(!isNull());
    return m_private->contentType();
}

WebString WebBlobStorageData::contentDisposition() const
{
    ASSERT(!isNull());
    return m_private->contentDisposition();
}

WebBlobStorageData::WebBlobStorageData(const PassRefPtr<BlobStorageData>& data)
    : m_private(0)
{
    assign(data);
}

WebBlobStorageData& WebBlobStorageData::operator=(const PassRefPtr<BlobStorageData>& data)
{
    assign(data);
    return *this;
}

WebBlobStorageData::operator PassRefPtr<BlobStorageData>() const
{
    return m_private;
}

void WebBlobStorageData::assign(const PassRefPtr<BlobStorageData>& data)
{
    if (m_private)
        m_private->deref();
    m_private = static_cast<WebBlobStorageDataPrivate*>(data.leakRef());
}

} // namespace WebKit
