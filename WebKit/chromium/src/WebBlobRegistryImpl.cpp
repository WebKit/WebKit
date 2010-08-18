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
#include "WebBlobRegistryImpl.h"

#include "BlobRegistryImpl.h"
#include "ResourceError.h"
#include "WebBlobData.h"
#include "WebURL.h"
#include <wtf/MainThread.h>

#if ENABLE(BLOB)

namespace WebKit {

WebBlobRegistry* WebBlobRegistry::create()
{
    return new WebBlobRegistryImpl();
}

WebBlobRegistryImpl::WebBlobRegistryImpl()
    : m_blobRegistry(new WebCore::BlobRegistryImpl())
{
}

WebBlobRegistryImpl::~WebBlobRegistryImpl()
{
}

void WebBlobRegistryImpl::registerBlobURL(const WebURL& url, WebBlobData& blobData)
{
    m_blobRegistry->registerBlobURL(url, blobData);
}

void WebBlobRegistryImpl::registerBlobURL(const WebURL& url, const WebURL& srcURL)
{
    m_blobRegistry->registerBlobURL(url, srcURL);
}

void WebBlobRegistryImpl::unregisterBlobURL(const WebURL& url)
{
    m_blobRegistry->unregisterBlobURL(url);
}

WebBlobStorageData WebBlobRegistryImpl::getBlobDataFromURL(const WebURL& url)
{
    return m_blobRegistry->getBlobDataFromURL(url);
}

} // namespace WebKit

#endif // ENABLE(BLOB)
