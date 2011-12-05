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

#include "Chrome.h"
#include "ChromeClientImpl.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "StorageAreaProxy.h"
#include "WebKit.h"
#include "platform/WebKitPlatformSupport.h"
#include "WebStorageNamespace.h"
#include "platform/WebString.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

namespace WebCore {

PassRefPtr<StorageNamespace> StorageNamespace::localStorageNamespace(const String& path, unsigned quota)
{
    return adoptRef(new StorageNamespaceProxy(WebKit::webKitPlatformSupport()->createLocalStorageNamespace(path, quota), LocalStorage));
}

PassRefPtr<StorageNamespace> StorageNamespace::sessionStorageNamespace(Page* page, unsigned quota)
{
    WebKit::WebViewClient* webViewClient = static_cast<WebKit::WebViewImpl*>(page->chrome()->client()->webView())->client();
    return adoptRef(new StorageNamespaceProxy(webViewClient->createSessionStorageNamespace(quota), SessionStorage));
}

// FIXME: storageNamespace argument should be a PassOwnPtr.
StorageNamespaceProxy::StorageNamespaceProxy(WebKit::WebStorageNamespace* storageNamespace, StorageType storageType)
    : m_storageNamespace(adoptPtr(storageNamespace))
    , m_storageType(storageType)
{
}

StorageNamespaceProxy::~StorageNamespaceProxy()
{
}

PassRefPtr<StorageNamespace> StorageNamespaceProxy::copy()
{
    ASSERT(m_storageType == SessionStorage);
    WebKit::WebStorageNamespace* newNamespace = m_storageNamespace->copy();
    // Some embedders hook into WebViewClient::createView to make the copy of
    // session storage and then return the object lazily.  Other embedders
    // choose to make the copy now and return a pointer immediately.  So handle
    // both cases.
    if (!newNamespace)
        return 0;
    return adoptRef(new StorageNamespaceProxy(newNamespace, m_storageType));
}

PassRefPtr<StorageArea> StorageNamespaceProxy::storageArea(PassRefPtr<SecurityOrigin> origin)
{
    return adoptRef(new StorageAreaProxy(m_storageNamespace->createStorageArea(origin->toString()), m_storageType));
}

void StorageNamespaceProxy::close()
{
    m_storageNamespace->close();
}

void StorageNamespaceProxy::clearOriginForDeletion(SecurityOrigin* origin)
{
    ASSERT_NOT_REACHED();
}

void StorageNamespaceProxy::clearAllOriginsForDeletion()
{
    ASSERT_NOT_REACHED();
}

void StorageNamespaceProxy::sync()
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
