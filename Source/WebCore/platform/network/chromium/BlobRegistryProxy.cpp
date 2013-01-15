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

#if ENABLE(BLOB)

#include "BlobRegistryProxy.h"

#include "BlobData.h"
#include "KURL.h"
#include "ResourceHandle.h"
#include <public/Platform.h>
#include <public/WebBlobData.h>
#include <public/WebBlobRegistry.h>
#include <public/WebURL.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>

// We are part of the WebKit implementation.
using namespace WebKit;

namespace WebCore {

BlobRegistry& blobRegistry()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(BlobRegistryProxy, instance, ());
    return instance;
}

BlobRegistryProxy::BlobRegistryProxy()
    : m_webBlobRegistry(WebKit::Platform::current()->blobRegistry())
{
}

void BlobRegistryProxy::registerBlobURL(const KURL& url, PassOwnPtr<BlobData> blobData)
{
    if (m_webBlobRegistry) {
        WebBlobData webBlobData(blobData);
        m_webBlobRegistry->registerBlobURL(url, webBlobData);
    }
}

void BlobRegistryProxy::registerBlobURL(const KURL& url, const KURL& srcURL)
{
    if (m_webBlobRegistry)
        m_webBlobRegistry->registerBlobURL(url, srcURL);
}

void BlobRegistryProxy::unregisterBlobURL(const KURL& url)
{
    if (m_webBlobRegistry)
        m_webBlobRegistry->unregisterBlobURL(url);
}

} // namespace WebCore

#endif
