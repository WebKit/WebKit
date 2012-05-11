/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *           (C) 2008 Apple Inc.
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
#include "StorageAreaProxy.h"

#include "DOMWindow.h"
#include "Document.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"
#include "Storage.h"
#include "StorageEvent.h"
#include "StorageNamespaceProxy.h"

#include "WebFrameImpl.h"
#include "WebPermissionClient.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "WebViewImpl.h"

#include <public/WebStorageArea.h>

namespace WebCore {

// FIXME: storageArea argument should be a PassOwnPtr.
StorageAreaProxy::StorageAreaProxy(WebKit::WebStorageArea* storageArea, StorageType storageType)
    : m_storageArea(adoptPtr(storageArea))
    , m_storageType(storageType)
{
}

StorageAreaProxy::~StorageAreaProxy()
{
}

unsigned StorageAreaProxy::length(Frame* frame) const
{
    if (canAccessStorage(frame))
        return m_storageArea->length();
    return 0;
}

String StorageAreaProxy::key(unsigned index, Frame* frame) const
{
    if (canAccessStorage(frame))
        return m_storageArea->key(index);
    return String();
}

String StorageAreaProxy::getItem(const String& key, Frame* frame) const
{
    if (canAccessStorage(frame))
        return m_storageArea->getItem(key);
    return String();
}

void StorageAreaProxy::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
{
    if (!canAccessStorage(frame))
        ec = QUOTA_EXCEEDED_ERR;
    else {
        WebKit::WebStorageArea::Result result = WebKit::WebStorageArea::ResultOK;
        m_storageArea->setItem(key, value, frame->document()->url(), result);
        ec = (result == WebKit::WebStorageArea::ResultOK) ? 0 : QUOTA_EXCEEDED_ERR;
    }
}

void StorageAreaProxy::removeItem(const String& key, Frame* frame)
{
    if (!canAccessStorage(frame))
        return;
    m_storageArea->removeItem(key, frame->document()->url());
}

void StorageAreaProxy::clear(Frame* frame)
{
    if (!canAccessStorage(frame))
        return;
    m_storageArea->clear(frame->document()->url());
}

bool StorageAreaProxy::contains(const String& key, Frame* frame) const
{
    return !getItem(key, frame).isNull();
}

bool StorageAreaProxy::canAccessStorage(Frame* frame) const
{
    if (!frame->page())
        return false;
    WebKit::WebFrameImpl* webFrame = WebKit::WebFrameImpl::fromFrame(frame);
    WebKit::WebViewImpl* webView = webFrame->viewImpl();
    return !webView->permissionClient() || webView->permissionClient()->allowStorage(webFrame, m_storageType == LocalStorage);
}

void StorageAreaProxy::dispatchLocalStorageEvent(PageGroup* pageGroup, const String& key, const String& oldValue, const String& newValue,
                                                 SecurityOrigin* securityOrigin, const KURL& pageURL, WebKit::WebStorageArea* sourceAreaInstance, bool originatedInProcess)
{
    const HashSet<Page*>& pages = pageGroup->pages();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != pages.end(); ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (frame->document()->securityOrigin()->equal(securityOrigin) && !isEventSource(frame->domWindow()->optionalLocalStorage(), sourceAreaInstance)) {
                // FIXME: maybe only raise if the window has an onstorage listener attached to avoid creating the Storage instance.
                ExceptionCode ec = 0;
                Storage* storage = frame->domWindow()->localStorage(ec);
                if (!ec)
                    frame->document()->enqueueWindowEvent(StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, pageURL, storage));
            }
        }
    }
}

static Page* findPageWithSessionStorageNamespace(PageGroup* pageGroup, const WebKit::WebStorageNamespace& sessionNamespace)
{
    const HashSet<Page*>& pages = pageGroup->pages();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != pages.end(); ++it) {
        const bool createIfNeeded = true;
        StorageNamespaceProxy* proxy = static_cast<StorageNamespaceProxy*>((*it)->sessionStorage(createIfNeeded));
        if (proxy->isSameNamespace(sessionNamespace))
            return *it;
    }
    return 0;
}

void StorageAreaProxy::dispatchSessionStorageEvent(PageGroup* pageGroup, const String& key, const String& oldValue, const String& newValue,
                                                   SecurityOrigin* securityOrigin, const KURL& pageURL, const WebKit::WebStorageNamespace& sessionNamespace,
                                                   WebKit::WebStorageArea* sourceAreaInstance, bool originatedInProcess)
{
    Page* page = findPageWithSessionStorageNamespace(pageGroup, sessionNamespace);
    if (!page)
        return;

    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document()->securityOrigin()->equal(securityOrigin) && !isEventSource(frame->domWindow()->optionalSessionStorage(), sourceAreaInstance)) {
            // FIXME: maybe only raise if the window has an onstorage listener attached to avoid creating the Storage instance.
            ExceptionCode ec = 0;
            Storage* storage = frame->domWindow()->sessionStorage(ec);
            if (!ec)
                frame->document()->enqueueWindowEvent(StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, pageURL, storage));
        }
    }
}

bool StorageAreaProxy::isEventSource(Storage* storage, WebKit::WebStorageArea* sourceAreaInstance)
{
    if (!storage)
        return false;
    StorageAreaProxy* areaProxy = static_cast<StorageAreaProxy*>(storage->area());
    return areaProxy->m_storageArea == sourceAreaInstance;
}

} // namespace WebCore
