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
#include "LocalStorageArea.h"

#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"

namespace WebCore {

LocalStorageArea::LocalStorageArea(SecurityOrigin* origin, LocalStorage* localStorage)
    : StorageArea(origin)
    , m_localStorage(localStorage)
{
}

void LocalStorageArea::itemChanged(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    ASSERT(isMainThread());

    // FIXME: Flag this change to be written out to the persistent store

    dispatchStorageEvent(key, oldValue, newValue, sourceFrame);
}

void LocalStorageArea::itemRemoved(const String& key, const String& oldValue, Frame* sourceFrame)
{
    ASSERT(isMainThread());

    // FIXME: Flag this removal to be written out to the persistent store

    dispatchStorageEvent(key, oldValue, String(), sourceFrame);
}

void LocalStorageArea::areaCleared(Frame* sourceFrame)
{
    ASSERT(isMainThread());

    // FIXME: Flag this clearing to be written out to the persistent store

    dispatchStorageEvent(String(), String(), String(), sourceFrame);
}

void LocalStorageArea::dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    ASSERT(isMainThread());

    Page* page = sourceFrame->page();
    if (!page)
        return;

    // Need to copy all relevant frames from every page to a vector, since sending the event to one frame might mutate the frame tree
    // of any given page in the group, or mutate the page group itself
    Vector<RefPtr<Frame> > frames;
    const HashSet<Page*>& pages = page->group().pages();
    
    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (Document* document = frame->document())
                if (document->securityOrigin()->equal(securityOrigin()))
                    frames.append(frame);
        }
    }

    for (unsigned i = 0; i < frames.size(); ++i) {
        if (HTMLElement* body = frames[i]->document()->body())
            body->dispatchStorageEvent(EventNames::storageEvent, key, oldValue, newValue, sourceFrame);        
    }
}

void LocalStorageArea::performImport()
{
    ASSERT(!isMainThread());
    
    // FIXME: Perform initial import of all items here
}

void LocalStorageArea::performSync()
{
    ASSERT(!isMainThread());

    // FIXME: Write out all changed items here
}

} // namespace WebCore
