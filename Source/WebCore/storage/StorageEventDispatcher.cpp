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

void StorageEventDispatcher::dispatchSessionStorageEvents(const String& key, const String& oldValue, const String& newValue, const SecurityOrigin& securityOrigin, Frame* sourceFrame)
{
    Page* page = sourceFrame->page();
    if (!page)
        return;

    Vector<Ref<Frame>> frames;

    // Send events only to our page.
    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* window = frame->window(); !window || !window->hasEventListeners(eventNames().storageEvent))
            continue;
        if (sourceFrame != frame && frame->document()->securityOrigin().equal(&securityOrigin))
            frames.append(*frame);
    }

    dispatchSessionStorageEventsToFrames(*page, frames, key, oldValue, newValue, sourceFrame->document()->url().string(), securityOrigin);
}

void StorageEventDispatcher::dispatchLocalStorageEvents(const String& key, const String& oldValue, const String& newValue, const SecurityOrigin& securityOrigin, Frame* sourceFrame)
{
    Page* page = sourceFrame->page();
    if (!page)
        return;

    Vector<Ref<Frame>> frames;

    // Send events to every page.
    for (auto& pageInGroup : page->group().pages()) {
        for (auto* frame = &pageInGroup.mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (auto* window = frame->window(); !window || !window->hasEventListeners(eventNames().storageEvent))
                continue;
            if (sourceFrame != frame && frame->document()->securityOrigin().equal(&securityOrigin))
                frames.append(*frame);
        }
    }

    dispatchLocalStorageEventsToFrames(page->group(), frames, key, oldValue, newValue, sourceFrame->document()->url().string(), securityOrigin);
}

void StorageEventDispatcher::dispatchSessionStorageEventsToFrames(Page& page, const Vector<Ref<Frame>>& frames, const String& key, const String& oldValue, const String& newValue, const String& url, const SecurityOrigin& securityOrigin)
{
    InspectorInstrumentation::didDispatchDOMStorageEvent(page, key, oldValue, newValue, StorageType::Session, securityOrigin);

    for (auto& frame : frames) {
        RefPtr document = frame->document();
        auto result = document->domWindow()->sessionStorage();
        if (!result.hasException()) // https://html.spec.whatwg.org/multipage/webstorage.html#the-storage-event:event-storage
            document->queueTaskToDispatchEventOnWindow(TaskSource::DOMManipulation, StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, url, result.releaseReturnValue()));
    }
}

void StorageEventDispatcher::dispatchLocalStorageEventsToFrames(PageGroup& pageGroup, const Vector<Ref<Frame>>& frames, const String& key, const String& oldValue, const String& newValue, const String& url, const SecurityOrigin& securityOrigin)
{
    for (auto& page : pageGroup.pages())
        InspectorInstrumentation::didDispatchDOMStorageEvent(page, key, oldValue, newValue, StorageType::Local, securityOrigin);

    for (auto& frame : frames) {
        RefPtr document = frame->document();
        auto result = document->domWindow()->localStorage();
        if (!result.hasException()) // https://html.spec.whatwg.org/multipage/webstorage.html#the-storage-event:event-storage
            document->queueTaskToDispatchEventOnWindow(TaskSource::DOMManipulation, StorageEvent::create(eventNames().storageEvent, key, oldValue, newValue, url, result.releaseReturnValue()));
    }
}

} // namespace WebCore
