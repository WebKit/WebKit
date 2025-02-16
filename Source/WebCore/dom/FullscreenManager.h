/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(FULLSCREEN_API)

#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "GCReachableRef.h"
#include "HTMLMediaElement.h"
#include "HTMLMediaElementEnums.h"
#include "LayoutRect.h"
#include "Page.h"
#include <wtf/Deque.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderStyle;

class FullscreenManager final : public CanMakeWeakPtr<FullscreenManager>, public CanMakeCheckedPtr<FullscreenManager> {
    WTF_MAKE_TZONE_ALLOCATED(FullscreenManager);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FullscreenManager);
public:
    FullscreenManager(Document&);
    ~FullscreenManager();

    Document& document() { return m_document.get(); }
    const Document& document() const { return m_document.get(); }
    Ref<Document> protectedDocument() const { return m_document.get(); }
    Page* page() const { return document().page(); }
    LocalFrame* frame() const { return document().frame(); }
    Element* documentElement() const { return document().documentElement(); }
    bool isSimpleFullscreenDocument() const;
    Document::BackForwardCacheState backForwardCacheState() const { return document().backForwardCacheState(); }

    // WHATWG Fullscreen API
    WEBCORE_EXPORT Element* fullscreenElement() const;
    RefPtr<Element> protectedFullscreenElement() const { return fullscreenElement(); }
    WEBCORE_EXPORT bool isFullscreenEnabled() const;
    WEBCORE_EXPORT void exitFullscreen(CompletionHandler<void(ExceptionOr<void>)>&&);

    // Mozilla versions.
    bool isFullscreen() const { return fullscreenElement(); }
    bool isFullscreenKeyboardInputAllowed() const { return fullscreenElement() && m_areKeysEnabledInFullscreen; }
    WEBCORE_EXPORT void cancelFullscreen();

    enum FullscreenCheckType {
        EnforceIFrameAllowFullscreenRequirement,
        ExemptIFrameAllowFullscreenRequirement,
    };
    WEBCORE_EXPORT void requestFullscreenForElement(Ref<Element>&&, FullscreenCheckType, CompletionHandler<void(ExceptionOr<void>)>&&, HTMLMediaElementEnums::VideoFullscreenMode = HTMLMediaElementEnums::VideoFullscreenModeStandard);
    WEBCORE_EXPORT void willEnterFullscreen(Element&, HTMLMediaElementEnums::VideoFullscreenMode, CompletionHandler<void(ExceptionOr<void>)>&&);
    WEBCORE_EXPORT bool didEnterFullscreen();
    WEBCORE_EXPORT bool willExitFullscreen();
    WEBCORE_EXPORT bool didExitFullscreen();

    void dispatchPendingEvents();

    enum class ExitMode : bool { Resize, NoResize };
    void finishExitFullscreen(Document&, ExitMode);

    void exitRemovedFullscreenElement(Element&);

    WEBCORE_EXPORT bool isAnimatingFullscreen() const;
    WEBCORE_EXPORT void setAnimatingFullscreen(bool);

    void clear();
    void emptyEventQueue();

protected:
    friend class Document;

    enum class EventType : bool { Change, Error };
    void dispatchFullscreenChangeOrErrorEvent(Deque<GCReachableRef<Node>>&, EventType, bool shouldNotifyMediaElement);
    void dispatchEventForNode(Node&, EventType);
    void queueFullscreenChangeEventForDocument(Document&);

private:
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return document().logger(); }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "FullscreenManager"_s; }
    WTFLogChannel& logChannel() const;
#endif

    Document* mainFrameDocument();
    RefPtr<Document> protectedMainFrameDocument();

    RefPtr<Element> fullscreenOrPendingElement() const { return m_fullscreenElement ? m_fullscreenElement : m_pendingFullscreenElement; }

    void addElementToChangeEventQueue(Node& target) { m_fullscreenChangeEventTargetQueue.append(GCReachableRef(target)); }

    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_topDocument;

    class FullscreenPromise {
    public:
        ~FullscreenPromise();
        FullscreenPromise& operator=(CompletionHandler<void(ExceptionOr<void>)>&&);

        void clear();
        void resolve();
        void rejectOrResolve(ExceptionOr<void>);
        void reject(Exception);
        operator bool() const;
    private:
        CompletionHandler<void(ExceptionOr<void>)> m_promise;
    };
    FullscreenPromise m_pendingPromise;

    RefPtr<Element> m_pendingFullscreenElement;
    RefPtr<Element> m_fullscreenElement;

    Deque<GCReachableRef<Node>> m_fullscreenChangeEventTargetQueue;
    Deque<GCReachableRef<Node>> m_fullscreenErrorEventTargetQueue;

    bool m_areKeysEnabledInFullscreen { false };
    bool m_isAnimatingFullscreen { false };
    bool m_pendingExitFullscreen { false };

#if !RELEASE_LOG_DISABLED
    const uint64_t m_logIdentifier;
#endif
};

}

#endif
