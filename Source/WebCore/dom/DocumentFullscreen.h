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

#include <WebCore/Document.h>
#include <WebCore/FullscreenOptions.h>
#include <WebCore/GCReachableRef.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/Page.h>
#include <wtf/Deque.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderStyle;

class DocumentFullscreen final : public CanMakeWeakPtr<DocumentFullscreen> {
    WTF_MAKE_TZONE_ALLOCATED(DocumentFullscreen);
public:
    DocumentFullscreen(Document&);

    void ref() const { m_document->ref(); }
    void deref() const { m_document->deref(); }

    // Document+Fullscreen.idl methods.
    static void exitFullscreen(Document&, Ref<DeferredPromise>&&);
    static bool fullscreenEnabled(Document&);
    static bool webkitFullscreenEnabled(Document& document) { return protect(document.fullscreen())->enabledByPermissionsPolicy(); }
    static Element* webkitFullscreenElement(Document& document) { return document.ancestorElementInThisScope(protect(document.fullscreen())->protectedFullscreenElement().get()); };
    WEBCORE_EXPORT static void webkitExitFullscreen(Document&);
    static bool webkitIsFullScreen(Document& document) { return protect(document.fullscreen())->isFullscreen(); };
    static bool webkitFullScreenKeyboardInputAllowed(Document& document) { return protect(document.fullscreen())->isFullscreenKeyboardInputAllowed(); };
    static void webkitCancelFullScreen(Document& document) { protect(document.fullscreen())->fullyExitFullscreen(); };

    // Helpers.
    Document& document() { return m_document; }
    const Document& document() const { return m_document; }
    Ref<Document> protectedDocument() const { return m_document; }
    LocalFrame* frame() const;
    Element* documentElement() const { return document().documentElement(); }
    bool isSimpleFullscreenDocument() const;
    Document::BackForwardCacheState backForwardCacheState() const { return document().backForwardCacheState(); }

    // WHATWG Fullscreen API.
    WEBCORE_EXPORT Element* fullscreenElement() const;
    RefPtr<Element> protectedFullscreenElement() const { return fullscreenElement(); }
    WEBCORE_EXPORT bool enabledByPermissionsPolicy() const;
    WEBCORE_EXPORT void exitFullscreen(CompletionHandler<void(ExceptionOr<void>)>&&);
    WEBCORE_EXPORT void fullyExitFullscreen();

    // Legacy Mozilla API.
    bool isFullscreen() const { return fullscreenElement(); }
    bool isFullscreenKeyboardInputAllowed() const { return fullscreenElement() && m_areKeysEnabledInFullscreen; }

    // Fullscreen Keyboard Lock
    void setKeyboardLockMode(FullscreenOptions::KeyboardLock mode) { m_keyboardLockMode = mode; }
    bool isBrowserKeyboardLockEnabled() const { return m_keyboardLockMode == FullscreenOptions::KeyboardLock::Browser; }

    enum FullscreenCheckType {
        EnforceIFrameAllowFullscreenRequirement,
        ExemptIFrameAllowFullscreenRequirement,
    };
    WEBCORE_EXPORT void requestFullscreen(Ref<Element>&&, FullscreenCheckType, CompletionHandler<void(ExceptionOr<void>)>&&, HTMLMediaElementEnums::VideoFullscreenMode = HTMLMediaElementEnums::VideoFullscreenModeStandard);
    WEBCORE_EXPORT ExceptionOr<void> willEnterFullscreen(Element&, HTMLMediaElementEnums::VideoFullscreenMode);
    WEBCORE_EXPORT bool willExitFullscreen();
    WEBCORE_EXPORT void didExitFullscreen(CompletionHandler<void(ExceptionOr<void>)>&&);

    WEBCORE_EXPORT static void elementEnterFullscreen(Element&);

    void dispatchPendingEvents();

    enum class ExitMode : bool { Resize, NoResize };
    WEBCORE_EXPORT static void finishExitFullscreen(Frame&, ExitMode);

    void exitRemovedFullscreenElement(Element&);

    WEBCORE_EXPORT bool isAnimatingFullscreen() const;
    WEBCORE_EXPORT void setAnimatingFullscreen(bool);

    void clear();

protected:
    friend class Document;

    void clearPendingEvents() { m_pendingEvents.clear(); }

private:
    using CompletionHandlerScope = Document::CompletionHandlerScope;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return protectedDocument()->logger(); }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "DocumentFullscreen"_s; }
    WTFLogChannel& logChannel() const;
#endif

    Page* page() const;
    Document* mainFrameDocument() { return protectedDocument()->mainFrameDocument(); }

    RefPtr<Element> fullscreenOrPendingElement() const { return m_fullscreenElement ? m_fullscreenElement : m_pendingFullscreenElement; }

    bool didEnterFullscreen();

    enum class EventType : bool { Change, Error };
    static void queueFullscreenChangeEventForDocument(Document&);
    void queueFullscreenChangeEventForElement(Element& target) { m_pendingEvents.append({ EventType::Change, GCReachableRef(target) }); }

    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;

    RefPtr<Element> m_fullscreenElement;
    RefPtr<Element> m_pendingFullscreenElement;

    Deque<std::pair<EventType, GCReachableRef<Element>>> m_pendingEvents;

    bool m_areKeysEnabledInFullscreen { false };
    bool m_isAnimatingFullscreen { false };
    bool m_pendingExitFullscreen { false };

    // Fullscreen Keyboard Lock
    FullscreenOptions::KeyboardLock m_keyboardLockMode { FullscreenOptions::KeyboardLock::None };

#if !RELEASE_LOG_DISABLED
    const uint64_t m_logIdentifier;
#endif
};

}

#endif
