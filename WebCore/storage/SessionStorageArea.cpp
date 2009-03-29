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
#include "SessionStorageArea.h"

#include "EventNames.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLElement.h"
#include "Page.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "StorageMap.h"

namespace WebCore {

PassRefPtr<SessionStorageArea> SessionStorageArea::copy(SecurityOrigin* origin, Page* page)
{
    return adoptRef(new SessionStorageArea(origin, page, this));
}

SessionStorageArea::SessionStorageArea(SecurityOrigin* origin, Page* page)
    : StorageArea(origin)
    , m_page(page)
{
    ASSERT(page);
}

SessionStorageArea::SessionStorageArea(SecurityOrigin* origin, Page* page, SessionStorageArea* area)
    : StorageArea(origin, area)
    , m_page(page)
{
    ASSERT(page);
}

void SessionStorageArea::itemChanged(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    dispatchStorageEvent(key, oldValue, newValue, sourceFrame);
}

void SessionStorageArea::itemRemoved(const String& key, const String& oldValue, Frame* sourceFrame)
{
    dispatchStorageEvent(key, oldValue, String(), sourceFrame);
}

void SessionStorageArea::areaCleared(Frame* sourceFrame)
{
    dispatchStorageEvent(String(), String(), String(), sourceFrame);
}

void SessionStorageArea::dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame)
{
    // For SessionStorage events, each frame in the page's frametree with the same origin as this StorageArea needs to be notified of the change
    Vector<RefPtr<Frame> > frames;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document()->securityOrigin()->equal(securityOrigin()))
            frames.append(frame);
    }
        
    for (unsigned i = 0; i < frames.size(); ++i) {
        if (HTMLElement* body = frames[i]->document()->body())
            body->dispatchStorageEvent(eventNames().storageEvent, key, oldValue, newValue, sourceFrame);        
    }
}

} // namespace WebCore
