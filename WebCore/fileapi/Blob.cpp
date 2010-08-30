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
#include "Blob.h"

#include "BlobData.h"
#include "BlobItem.h"
#include "BlobURL.h"
#include "FileSystem.h"
#include "ScriptExecutionContext.h"
#include "ThreadableBlobRegistry.h"

namespace WebCore {

// FIXME: To be removed when we switch to using BlobData.
Blob::Blob(ScriptExecutionContext* scriptExecutionContext, const String& type, const BlobItemList& items)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_type(type)
    , m_size(0)
{
    m_scriptExecutionContext->addBlob(this);
    for (size_t i = 0; i < items.size(); ++i)
        m_items.append(items[i]);
}

// FIXME: To be removed when we switch to using BlobData.
Blob::Blob(ScriptExecutionContext* scriptExecutionContext, const PassRefPtr<BlobItem>& item)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_size(0)
{
    m_scriptExecutionContext->addBlob(this);
    m_items.append(item);
}

// FIXME: To be removed when we switch to using BlobData.
Blob::Blob(ScriptExecutionContext* scriptExecutionContext, const String& path)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_size(0)
{
    m_scriptExecutionContext->addBlob(this);
    // Note: this doesn't initialize the type unlike File(path).
    m_items.append(FileBlobItem::create(path));
}

Blob::Blob(ScriptExecutionContext* scriptExecutionContext, PassOwnPtr<BlobData> blobData, long long size)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_type(blobData->contentType())
    , m_size(size)
{
    ASSERT(blobData.get() && !blobData->items().isEmpty());

    m_scriptExecutionContext->addBlob(this);

    // Create a new internal URL and register it with the provided blob data.
    m_url = BlobURL::createURL(scriptExecutionContext);
    ThreadableBlobRegistry::registerBlobURL(scriptExecutionContext, m_url, blobData);
}

Blob::Blob(ScriptExecutionContext* scriptExecutionContext, const KURL& srcURL, const String& type, long long size)
    : m_scriptExecutionContext(scriptExecutionContext)
    , m_type(type)
    , m_size(size)
{
    m_scriptExecutionContext->addBlob(this);

    // FIXME: To be removed when we switch to using BlobData.
    if (srcURL.isEmpty())
        return;

    // Create a new internal URL and register it with the same blob data as the source URL.
    m_url = BlobURL::createURL(scriptExecutionContext);
    ThreadableBlobRegistry::registerBlobURL(scriptExecutionContext, m_url, srcURL);
}

Blob::~Blob()
{
    // The internal URL is only used to refer to the Blob object. So we need to unregister the URL when the object is GC-ed.
    if (m_scriptExecutionContext) {
        m_scriptExecutionContext->removeBlob(this);
        ThreadableBlobRegistry::unregisterBlobURL(m_scriptExecutionContext, m_url);
    }
}

void Blob::contextDestroyed()
{
    ASSERT(m_scriptExecutionContext);

    // Unregister the internal URL before the context is gone.
    ThreadableBlobRegistry::unregisterBlobURL(m_scriptExecutionContext, m_url);
    m_scriptExecutionContext = 0;
}

unsigned long long Blob::size() const
{
    // FIXME: JavaScript cannot represent sizes as large as unsigned long long, we need to
    // come up with an exception to throw if file size is not represetable.
    unsigned long long size = 0;
    for (size_t i = 0; i < m_items.size(); ++i)
        size += m_items[i]->size();
    return size;
}

// FIXME: To be removed when we switch to using BlobData.
const String& Blob::path() const
{
    ASSERT(m_items.size() == 1 && m_items[0]->toFileBlobItem());
    return m_items[0]->toFileBlobItem()->path();
}

#if ENABLE(BLOB)
PassRefPtr<Blob> Blob::slice(ScriptExecutionContext* scriptExecutionContext, long long start, long long length, const String& contentType) const
{
    if (start < 0)
        start = 0;
    if (length < 0)
        length = 0;

    // Clamp the range if it exceeds the size limit.
    unsigned long long totalSize = size();
    if (static_cast<unsigned long long>(start) > totalSize) {
        start = 0;
        length = 0;
    } else if (static_cast<unsigned long long>(start + length) > totalSize)
        length = totalSize - start;

    size_t i = 0;
    BlobItemList items;
    for (; i < m_items.size() && static_cast<unsigned long long>(start) >= m_items[i]->size(); ++i)
        start -= m_items[i]->size();
    for (; length > 0 && i < m_items.size(); ++i) {
        items.append(m_items[i]->slice(start, length));
        length -= items.last()->size();
        start = 0;
    }
    return Blob::create(scriptExecutionContext, contentType, items);
}
#endif // ENABLE(BLOB)

} // namespace WebCore
