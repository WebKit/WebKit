/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StorageNamespaceProxy.h"

#if ENABLE(DOM_STORAGE)

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "StorageAreaProxy.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebStorageNamespace.h"
#include "WebString.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

namespace WebCore {

PassRefPtr<StorageNamespace> StorageNamespace::localStorageNamespace(const String& path, unsigned quota)
{
    return adoptRef(new StorageNamespaceProxy(WebKit::webKitClient()->createLocalStorageNamespace(path, quota), LocalStorage));
}

PassRefPtr<StorageNamespace> StorageNamespace::sessionStorageNamespace(Page* page)
{
    WebKit::ChromeClientImpl* chromeClientImpl = static_cast<WebKit::ChromeClientImpl*>(page->chrome()->client());
    WebKit::WebViewClient* webViewClient = chromeClientImpl->webView()->client();
    return adoptRef(new StorageNamespaceProxy(webViewClient->createSessionStorageNamespace(), SessionStorage));
}

StorageNamespaceProxy::StorageNamespaceProxy(WebKit::WebStorageNamespace* storageNamespace, StorageType storageType)
    : m_storageNamespace(storageNamespace)
    , m_storageType(storageType)
{
}

StorageNamespaceProxy::~StorageNamespaceProxy()
{
}

PassRefPtr<StorageNamespace> StorageNamespaceProxy::copy()
{
    ASSERT(m_storageType == SessionStorage);
    // The WebViewClient knows what its session storage namespace id is but we
    // do not.  Returning 0 here causes it to be fetched (via the WebViewClient)
    // on its next use.  Note that it is WebViewClient::createView's
    // responsibility to clone the session storage namespace id and that the
    // only time copy() is called is directly after the createView call...which
    // is why all of this is safe.
    return 0;
}

PassRefPtr<StorageArea> StorageNamespaceProxy::storageArea(PassRefPtr<SecurityOrigin> origin)
{
    return adoptRef(new StorageAreaProxy(m_storageNamespace->createStorageArea(origin->toString()), m_storageType));
}

void StorageNamespaceProxy::close()
{
    m_storageNamespace->close();
}

void StorageNamespaceProxy::unlock()
{
    // FIXME: Implement.
}

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)
