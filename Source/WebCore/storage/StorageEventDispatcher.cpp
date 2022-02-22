/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StorageEventDispatcher.h"

#include "Document.h"
#include "DOMWindow.h"
#include "EventNames.h"
#include "Frame.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "StorageEvent.h"
#include "StorageType.h"

namespace WebCore {

static void dispatchSessionStorageEventsToWindows(Page& page, const Vector<Ref<DOMWindow>>& windows, const String& key, const String& oldValue, const String& newValue, const String& url, const SecurityOrigin& securityOrigin)
{
    InspectorInstrumentation::didDispatchDOMStorageEvent(page, key, oldValue, newValue, StorageType::Session, securityOrigin);

    for (auto& window : windows) {
        RefPtr document = window->document();
        auto result = window->sessionStorage();
        if (!result.hasException()) // https://html.spec.whatwg.org/multipage/webstorage.html#the-storage-event:event-storage
            document->queueTaskToDispatchEventOnWindow(TaskSource::DOMManipulation, StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, url, result.releaseReturnValue()));
    }
}

static void dispatchLocalStorageEventsToWindows(PageGroup& pageGroup, const Vector<Ref<DOMWindow>>& windows, const String& key, const String& oldValue, const String& newValue, const String& url, const SecurityOrigin& securityOrigin)
{
    for (auto& page : pageGroup.pages())
        InspectorInstrumentation::didDispatchDOMStorageEvent(page, key, oldValue, newValue, StorageType::Local, securityOrigin);

    for (auto& window : windows) {
        RefPtr document = window->document();
        auto result = window->localStorage();
        if (!result.hasException()) // https://html.spec.whatwg.org/multipage/webstorage.html#the-storage-event:event-storage
            document->queueTaskToDispatchEventOnWindow(TaskSource::DOMManipulation, StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, url, result.releaseReturnValue()));
    }
}

void StorageEventDispatcher::dispatchSessionStorageEvents(const String& key, const String& oldValue, const String& newValue, Page& page, const SecurityOrigin& securityOrigin, const String& url, const Function<bool(Storage&)>& isSourceStorage)
{
    Vector<Ref<DOMWindow>> windows;
    DOMWindow::forEachWindowInterestedInStorageEvents([&](auto& window) {
        if (!window.optionalSessionStorage())
            return;
        // Send events only to our page.
        if (window.page() != &page)
            return;
        if (isSourceStorage(*window.optionalSessionStorage()))
            return;
        if (!securityOrigin.equal(window.securityOrigin()))
            return;
        windows.append(window);
    });

    dispatchSessionStorageEventsToWindows(page, windows, key, oldValue, newValue, url, securityOrigin);
}

void StorageEventDispatcher::dispatchLocalStorageEvents(const String& key, const String& oldValue, const String& newValue, PageGroup& pageGroup, const SecurityOrigin& securityOrigin, const String& url, const Function<bool(Storage&)>& isSourceStorage)
{
    auto& pagesInGroup = pageGroup.pages();
    Vector<Ref<DOMWindow>> windows;
    DOMWindow::forEachWindowInterestedInStorageEvents([&](auto& window) {
        if (!window.optionalLocalStorage())
            return;
        // Send events to every page in the group.
        if (!window.page() || !pagesInGroup.contains(*window.page()))
            return;
        if (isSourceStorage(*window.optionalLocalStorage()))
            return;
        if (!securityOrigin.equal(window.securityOrigin()))
            return;
        windows.append(window);
    });

    dispatchLocalStorageEventsToWindows(pageGroup, windows, key, oldValue, newValue, url, securityOrigin);
}

} // namespace WebCore
