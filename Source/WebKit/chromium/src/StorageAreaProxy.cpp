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
#include "WebStorageArea.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "WebViewImpl.h"

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

String StorageAreaProxy::setItem(const String& key, const String& value, ExceptionCode& ec, Frame* frame)
{
    WebKit::WebStorageArea::Result result = WebKit::WebStorageArea::ResultOK;
    WebKit::WebString oldValue;
    if (!canAccessStorage(frame))
        ec = QUOTA_EXCEEDED_ERR;
    else {
        m_storageArea->setItem(key, value, frame->document()->url(), result, oldValue);
        ec = (result == WebKit::WebStorageArea::ResultOK) ? 0 : QUOTA_EXCEEDED_ERR;
        String oldValueString = oldValue;
        if (oldValueString != value && result == WebKit::WebStorageArea::ResultOK)
            storageEvent(key, oldValue, value, m_storageType, frame->document()->securityOrigin(), frame);
    }
    return oldValue;
}

String StorageAreaProxy::removeItem(const String& key, Frame* frame)
{
    if (!canAccessStorage(frame))
        return String();
    WebKit::WebString oldValue;
    m_storageArea->removeItem(key, frame->document()->url(), oldValue);
    if (!oldValue.isNull())
        storageEvent(key, oldValue, String(), m_storageType, frame->document()->securityOrigin(), frame);
    return oldValue;
}

bool StorageAreaProxy::clear(Frame* frame)
{
    if (!canAccessStorage(frame))
        return false;
    bool clearedSomething;
    m_storageArea->clear(frame->document()->url(), clearedSomething);
    if (clearedSomething)
        storageEvent(String(), String(), String(), m_storageType, frame->document()->securityOrigin(), frame);
    return clearedSomething;
}

bool StorageAreaProxy::contains(const String& key, Frame* frame) const
{
    return !getItem(key, frame).isNull();
}

// FIXME: remove this method and the calls to it from our setters after multi-side patch landing is done.
// Copied from WebCore/storage/StorageEventDispatcher.cpp out of necessity.  It's probably best to keep it current.
void StorageAreaProxy::storageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Frame* sourceFrame)
{
    Page* page = sourceFrame->page();
    if (!page)
        return;

    // We need to copy all relevant frames from every page to a vector since sending the event to one frame might mutate the frame tree
    // of any given page in the group or mutate the page group itself.
    Vector<RefPtr<Frame> > frames;
    if (storageType == SessionStorage) {
        // Send events only to our page.
        for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (sourceFrame != frame && frame->document()->securityOrigin()->equal(securityOrigin))
                frames.append(frame);
        }

        for (unsigned i = 0; i < frames.size(); ++i) {
            // FIXME: maybe only raise if the window has an onstorage listener
            // attached to avoid creating the Storage instance.
            ExceptionCode ec = 0;
            Storage* storage = frames[i]->domWindow()->sessionStorage(ec);
            if (!ec)
                frames[i]->document()->enqueueWindowEvent(StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, sourceFrame->document()->url(), storage));
        }
    } else {
        // Send events to every page.
        const HashSet<Page*>& pages = page->group().pages();
        HashSet<Page*>::const_iterator end = pages.end();
        for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
            for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
                if (sourceFrame != frame && frame->document()->securityOrigin()->equal(securityOrigin))
                    frames.append(frame);
            }
        }

        for (unsigned i = 0; i < frames.size(); ++i) {
            // FIXME: maybe only raise if the window has an onstorage listener
            // attached to avoid creating the Storage instance.
            ExceptionCode ec = 0;
            Storage* storage = frames[i]->domWindow()->localStorage(ec);
            if (!ec)
                frames[i]->document()->enqueueWindowEvent(StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, sourceFrame->document()->url(), storage));
        }
    }
}

bool StorageAreaProxy::canAccessStorage(Frame* frame) const
{
    if (!frame->page())
        return false;
    WebKit::WebFrameImpl* webFrame = WebKit::WebFrameImpl::fromFrame(frame);
    WebKit::WebViewImpl* webView = webFrame->viewImpl();
    return !webView->permissionClient() || webView->permissionClient()->allowStorage(webFrame, m_storageType == LocalStorage);
}

void StorageAreaProxy::dispatchLocalStorageEvent(const String& pageGroupName, const String& key, const String& oldValue, const String& newValue,
                                                 SecurityOrigin* securityOrigin, const KURL& pageURL, WebKit::WebStorageArea* sourceAreaInstance, bool originatedInProcess)
{
    // FIXME: Multi-sided patch engineering alert !
    // step 1: this method gets defined and implemented in webkit/webcore with the early return.
    // step 2: this method starts getting called by chromium still with the early return.
    // step 3: This class's setters are modified to no longer raise SessionStorage
    //         events for inprocess changes and this early return is removed.
    if (originatedInProcess)
        return;

    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroupName)->pages();
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

static Page* findPageWithSessionStorageNamespace(const String& pageGroupName, const WebKit::WebStorageNamespace& sessionNamespace)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroupName)->pages();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != pages.end(); ++it) {
        const bool createIfNeeded = true;
        StorageNamespaceProxy* proxy = static_cast<StorageNamespaceProxy*>((*it)->sessionStorage(createIfNeeded));
        if (proxy->isSameNamespace(sessionNamespace))
            return *it;
    }
    return 0;
}

void StorageAreaProxy::dispatchSessionStorageEvent(const String& pageGroupName, const String& key, const String& oldValue, const String& newValue,
                                                   SecurityOrigin* securityOrigin, const KURL& pageURL, const WebKit::WebStorageNamespace& sessionNamespace,
                                                   WebKit::WebStorageArea* sourceAreaInstance, bool originatedInProcess)
{
    // FIXME: Multi-sided patch engineering alert !
    // step 1: this method gets defined and implemented in webkit/webcore with the early return.
    // step 2: this method starts getting called by chromium still with the early return.
    // step 3: This class's setters are modified to no longer raise SessionStorage
    //         events for inprocess changes and this early return is removed.
    if (originatedInProcess)
        return;

    Page* page = findPageWithSessionStorageNamespace(pageGroupName, sessionNamespace);
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
