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

#include "BlobRegistryImpl.h"

#include "FileStream.h"
#include "FileStreamProxy.h"
#include "FileSystem.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

bool BlobRegistryImpl::shouldLoadResource(const ResourceRequest& request) const
{
    // If the resource is not fetched using the GET method, bail out.
    if (!equalIgnoringCase(request.httpMethod(), "GET"))
        return false;

    return true;
}

PassRefPtr<ResourceHandle> BlobRegistryImpl::createResourceHandle(const ResourceRequest& request, ResourceHandleClient*)
{
    if (!shouldLoadResource(request))
        return 0;

    // FIXME: To be implemented.
    return 0;
}

bool BlobRegistryImpl::loadResourceSynchronously(const ResourceRequest& request, ResourceError&, ResourceResponse&, Vector<char>&)
{
    if (!shouldLoadResource(request))
        return false;

    // FIXME: To be implemented.
    return false;
}

BlobRegistry& BlobRegistry::instance()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(BlobRegistryImpl, instance, ());
    return instance;
}

void BlobRegistryImpl::appendStorageItems(BlobStorageData* blobStorageData, const BlobStorageDataItemList& items)
{
    for (BlobStorageDataItemList::const_iterator iter = items.begin(); iter != items.end(); ++iter) {
        if (iter->type == BlobStorageDataItem::Data)
            blobStorageData->appendData(iter->data, iter->offset, iter->length);
        else {
            ASSERT(iter->type == BlobStorageDataItem::File);
            blobStorageData->appendFile(iter->path, iter->offset, iter->length, iter->expectedModificationTime);
        }
    }
}

void BlobRegistryImpl::appendStorageItems(BlobStorageData* blobStorageData, const BlobStorageDataItemList& items, long long offset, long long length)
{
    ASSERT(length != BlobDataItem::toEndOfFile);

    BlobStorageDataItemList::const_iterator iter = items.begin();
    if (offset) {
        for (; iter != items.end(); ++iter) {
            if (offset >= iter->length)
                offset -= iter->length;
            else
                break;
        }
    }

    for (; iter != items.end() && length > 0; ++iter) {
        long long currentLength = iter->length - offset;
        long long newLength = currentLength > length ? length : currentLength;
        if (iter->type == BlobStorageDataItem::Data)
            blobStorageData->appendData(iter->data, iter->offset + offset, newLength);
        else {
            ASSERT(iter->type == BlobStorageDataItem::File);
            blobStorageData->appendFile(iter->path, iter->offset + offset, newLength, iter->expectedModificationTime);
        }
        offset = 0;
    }
}

void BlobRegistryImpl::registerBlobURL(const KURL& url, PassOwnPtr<BlobData> blobData)
{
    ASSERT(isMainThread());

    RefPtr<BlobStorageData> blobStorageData = BlobStorageData::create();
    blobStorageData->setContentType(blobData->contentType());
    blobStorageData->setContentDisposition(blobData->contentDisposition());

    for (BlobDataItemList::const_iterator iter = blobData->items().begin(); iter != blobData->items().end(); ++iter) {
        switch (iter->type) {
        case BlobDataItem::Data:
            blobStorageData->appendData(iter->data, 0, iter->data.length());
            break;
        case BlobDataItem::File:
            blobStorageData->appendFile(iter->path, iter->offset, iter->length, iter->expectedModificationTime);
            break;
        case BlobDataItem::Blob:
            if (m_blobs.contains(iter->url.string()))
                appendStorageItems(blobStorageData.get(), m_blobs.get(iter->url.string())->items(), iter->offset, iter->length);
            break;
        }
    }


    m_blobs.set(url.string(), blobStorageData);
}

void BlobRegistryImpl::registerBlobURL(const KURL& url, const KURL& srcURL)
{
    ASSERT(isMainThread());

    RefPtr<BlobStorageData> src = m_blobs.get(srcURL.string());
    ASSERT(src);
    if (!src)
        return;

    RefPtr<BlobStorageData> blobStorageData = BlobStorageData::create();
    blobStorageData->setContentType(src->contentType());
    blobStorageData->setContentDisposition(src->contentDisposition());
    appendStorageItems(blobStorageData.get(), src->items());
    
    m_blobs.set(url.string(), blobStorageData);
}

void BlobRegistryImpl::unregisterBlobURL(const KURL& url)
{
    ASSERT(isMainThread());
    m_blobs.remove(url.string());
}

PassRefPtr<BlobStorageData> BlobRegistryImpl::getBlobDataFromURL(const KURL& url) const
{
    ASSERT(isMainThread());
    return m_blobs.get(url.string());
}

} // namespace WebCore
