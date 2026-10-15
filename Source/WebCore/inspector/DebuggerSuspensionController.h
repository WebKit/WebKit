/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include "DebuggerPauseSessionIdentifier.h"
#include <wtf/CheckedPtr.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class Document;
class LocalFrame;
class Page;
class WeakPtrImplWithEventTargetData;

class DebuggerSuspensionController final : public CanMakeCheckedPtr<DebuggerSuspensionController> {
    WTF_MAKE_TZONE_ALLOCATED(DebuggerSuspensionController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DebuggerSuspensionController);
    WTF_MAKE_NONCOPYABLE(DebuggerSuspensionController);
public:
    explicit DebuggerSuspensionController(Page&);
    ~DebuggerSuspensionController();

    // A paused FrameDebugger holds every page in the process, not only its own: pages in one process share the
    // main thread, and related pages (an opener and its popup) can call into each other synchronously.
    static void frameDebuggerDidPause(LocalFrame&);
    static void frameDebuggerDidContinue();

    void addSessionHold(DebuggerPauseSessionIdentifier);
    void removeSessionHold(DebuggerPauseSessionIdentifier);

    void documentDidBecomeCurrent(Document&);

private:
    static void updateHeldDocumentsInAllPages();

    bool shouldHoldDocuments() const;
    void updateHeldDocuments();
    void holdDocument(Document&);
    void releaseDocument(Document&);

    WeakRef<Page> m_page;
    WeakHashSet<Document, WeakPtrImplWithEventTargetData> m_heldDocuments;
    WeakHashSet<LocalFrame> m_pausedFrames;
    HashSet<DebuggerPauseSessionIdentifier> m_sessionHolds;
};

} // namespace WebCore
