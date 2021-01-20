/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(POINTER_LOCK)

#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class Element;
class Document;
class Page;
class PlatformMouseEvent;
class PlatformWheelEvent;
class VoidCallback;

class PointerLockController {
    WTF_MAKE_NONCOPYABLE(PointerLockController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PointerLockController(Page&);
    void requestPointerLock(Element* target);

    void requestPointerUnlock();
    void requestPointerUnlockAndForceCursorVisible();
    void elementRemoved(Element&);
    void documentDetached(Document&);
    bool isLocked() const;
    WEBCORE_EXPORT bool lockPending() const;
    WEBCORE_EXPORT Element* element() const;

    WEBCORE_EXPORT void didAcquirePointerLock();
    WEBCORE_EXPORT void didNotAcquirePointerLock();
    WEBCORE_EXPORT void didLosePointerLock();
    void dispatchLockedMouseEvent(const PlatformMouseEvent&, const AtomString& eventType);
    void dispatchLockedWheelEvent(const PlatformWheelEvent&);

private:
    void clearElement();
    void enqueueEvent(const AtomString& type, Element*);
    void enqueueEvent(const AtomString& type, Document*);
    Page& m_page;
    bool m_lockPending { false };
    bool m_unlockPending { false };
    bool m_forceCursorVisibleUponUnlock { false };
    RefPtr<Element> m_element;
    WeakPtr<Document> m_documentOfRemovedElementWhileWaitingForUnlock;
    WeakPtr<Document> m_documentAllowedToRelockWithoutUserGesture;
};

} // namespace WebCore

#endif // ENABLE(POINTER_LOCK)
