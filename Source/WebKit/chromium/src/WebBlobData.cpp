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
#include "WebBlobData.h"

#include "BlobData.h"
#include <wtf/PassOwnPtr.h>

using namespace WebCore;

namespace WebKit {

class WebBlobDataPrivate : public BlobData {
};

void WebBlobData::initialize()
{
    assign(BlobData::create());
}

void WebBlobData::reset()
{
    assign(0);
}

size_t WebBlobData::itemCount() const
{
    ASSERT(!isNull());
    return m_private->items().size();
}

bool WebBlobData::itemAt(size_t index, Item& result) const
{
    ASSERT(!isNull());

    if (index >= m_private->items().size())
        return false;

    const BlobDataItem& item = m_private->items()[index];
    result.data.reset();
    result.filePath.reset();
    result.blobURL = KURL();
    result.offset = item.offset;
    result.length = item.length;
    result.expectedModificationTime = item.expectedModificationTime;

    switch (item.type) {
    case BlobDataItem::Data:
        result.type = Item::TypeData;
        result.data = item.data;
        return true;
    case BlobDataItem::File:
        result.type = Item::TypeFile;
        result.filePath = item.path;
        return true;
    case BlobDataItem::Blob:
        result.type = Item::TypeBlob;
        result.blobURL = item.url;
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

WebString WebBlobData::contentType() const
{
    ASSERT(!isNull());
    return m_private->contentType();
}

WebString WebBlobData::contentDisposition() const
{
    ASSERT(!isNull());
    return m_private->contentDisposition();
}

WebBlobData::WebBlobData(const PassOwnPtr<BlobData>& data)
    : m_private(0)
{
    assign(data);
}

WebBlobData& WebBlobData::operator=(const PassOwnPtr<BlobData>& data)
{
    assign(data);
    return *this;
}

WebBlobData::operator PassOwnPtr<BlobData>()
{
    WebBlobDataPrivate* temp = m_private;
    m_private = 0;
    return adoptPtr(temp);
}

void WebBlobData::assign(const PassOwnPtr<BlobData>& data)
{
    if (m_private)
        delete m_private;
    m_private = static_cast<WebBlobDataPrivate*>(data.leakPtr());
}

} // namespace WebKit
