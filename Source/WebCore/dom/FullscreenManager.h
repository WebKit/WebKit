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
#include "LayoutRect.h"
#include <wtf/Deque.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class RenderStyle;

class FullscreenManager final : public CanMakeWeakPtr<FullscreenManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FullscreenManager(Document&);
    ~FullscreenManager();

    Document& document() { return m_document; }
    const Document& document() const { return m_document; }
    Document& topDocument() const { return m_document.topDocument(); }
    Page* page() const { return m_document.page(); }
    Frame* frame() const { return m_document.frame(); }
    Element* documentElement() const { return m_document.documentElement(); }
    Document::BackForwardCacheState backForwardCacheState() const { return m_document.backForwardCacheState(); }

    // W3C Fullscreen API
    Element* fullscreenElement() const { return !m_fullscreenElementStack.isEmpty() ? m_fullscreenElementStack.last().get() : nullptr; }
    WEBCORE_EXPORT bool isFullscreenEnabled() const;
    WEBCORE_EXPORT void exitFullscreen();

    // Mozilla versions.
    bool isFullscreen() const { return m_fullscreenElement.get(); }
    bool isFullscreenKeyboardInputAllowed() const { return m_fullscreenElement.get() && m_areKeysEnabledInFullscreen; }
    Element* currentFullscreenElement() const { return m_fullscreenElement.get(); }
    WEBCORE_EXPORT void cancelFullscreen();

    enum FullscreenCheckType {
        EnforceIFrameAllowFullscreenRequirement,
        ExemptIFrameAllowFullscreenRequirement,
    };
    WEBCORE_EXPORT void requestFullscreenForElement(Ref<Element>&&, RefPtr<DeferredPromise>&&, FullscreenCheckType);

    WEBCORE_EXPORT bool willEnterFullscreen(Element&);
    WEBCORE_EXPORT bool didEnterFullscreen();
    WEBCORE_EXPORT bool willExitFullscreen();
    WEBCORE_EXPORT bool didExitFullscreen();

    void dispatchFullscreenChangeEvents();
    void fullscreenElementRemoved();

    void adjustFullscreenElementOnNodeRemoval(Node&, Document::NodeRemoval = Document::NodeRemoval::Node);

    WEBCORE_EXPORT bool isAnimatingFullscreen() const;
    WEBCORE_EXPORT void setAnimatingFullscreen(bool);

    WEBCORE_EXPORT bool areFullscreenControlsHidden() const;
    WEBCORE_EXPORT void setFullscreenControlsHidden(bool);

    void clear();
    void emptyEventQueue();

protected:
    friend class Document;

    enum class EventType : bool { Change, Error };
    void dispatchFullscreenChangeOrErrorEvent(Deque<GCReachableRef<Node>>&, EventType, bool shouldNotifyMediaElement);
    void dispatchEventForNode(Node&, EventType);
    void clearFullscreenElementStack();
    void popFullscreenElementStack();
    void pushFullscreenElementStack(Element&);
    void addDocumentToFullscreenChangeEventQueue(Document&);

private:
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_document.logger(); }
    const void* logIdentifier() const { return m_logIdentifier; }
    const char* logClassName() const { return "FullscreenManager"; }
    WTFLogChannel& logChannel() const;
#endif

    Document& m_document;

    RefPtr<Element> fullscreenOrPendingElement() const { return m_fullscreenElement ? m_fullscreenElement : m_pendingFullscreenElement; }

    bool m_pendingExitFullscreen { false };
    RefPtr<Element> m_pendingFullscreenElement;
    RefPtr<Element> m_fullscreenElement;
    Vector<RefPtr<Element>> m_fullscreenElementStack;
    Deque<GCReachableRef<Node>> m_fullscreenChangeEventTargetQueue;
    Deque<GCReachableRef<Node>> m_fullscreenErrorEventTargetQueue;

    bool m_areKeysEnabledInFullscreen { false };
    bool m_isAnimatingFullscreen { false };
    bool m_areFullscreenControlsHidden { false };

#if !RELEASE_LOG_DISABLED
    const void* m_logIdentifier;
#endif
};

}

#endif
