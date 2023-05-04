/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "EventTarget.h"
#include "GlobalWindowIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class Frame;
class SecurityOrigin;

class DOMWindow : public RefCounted<DOMWindow>, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(DOMWindow);
public:
    virtual ~DOMWindow();

    static HashMap<GlobalWindowIdentifier, DOMWindow*>& allWindows();

    const GlobalWindowIdentifier& identifier() const { return m_identifier; }
    virtual Frame* frame() const = 0;

    virtual bool isLocalDOMWindow() const = 0;
    virtual bool isRemoteDOMWindow() const = 0;

    using RefCounted::ref;
    using RefCounted::deref;

protected:
    explicit DOMWindow(GlobalWindowIdentifier&&);

    ExceptionOr<RefPtr<SecurityOrigin>> createTargetOriginForPostMessage(const String&, Document&);

    EventTargetInterface eventTargetInterface() const final { return LocalDOMWindowEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

private:
    GlobalWindowIdentifier m_identifier;
    explicit DOMWindow(Document&);

    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    bool isLocalDOMWindow() const final { return true; }
    bool isRemoteDOMWindow() const final { return false; }
    void eventListenersDidChange() final;

    bool allowedToChangeWindowGeometry() const;

    static ExceptionOr<RefPtr<Frame>> createWindow(const String& urlString, const AtomString& frameName, const WindowFeatures&, DOMWindow& activeWindow, Frame& firstFrame, Frame& openerFrame, const Function<void(DOMWindow&)>& prepareDialogFunction = nullptr);
    bool isInsecureScriptAccess(DOMWindow& activeWindow, const String& urlString);

#if ENABLE(DEVICE_ORIENTATION)
    bool isAllowedToUseDeviceMotionOrOrientation(String& message) const;
    bool hasPermissionToReceiveDeviceMotionOrOrientationEvents(String& message) const;
    void failedToRegisterDeviceMotionEventListener();
#endif

    bool isSameSecurityOriginAsMainFrame() const;

#if ENABLE(GAMEPAD)
    void incrementGamepadEventListenerCount();
    void decrementGamepadEventListenerCount();
#endif

    bool m_shouldPrintWhenFinishedLoading { false };
    bool m_suspendedForDocumentSuspension { false };
    bool m_isSuspendingObservers { false };
    std::optional<bool> m_canShowModalDialogOverride;

    HashSet<Observer*> m_observers;

    mutable RefPtr<Crypto> m_crypto;
    mutable RefPtr<History> m_history;
    mutable RefPtr<BarProp> m_locationbar;
    mutable RefPtr<StyleMedia> m_media;
    mutable RefPtr<BarProp> m_menubar;
    mutable RefPtr<Navigator> m_navigator;
    mutable RefPtr<BarProp> m_personalbar;
    mutable RefPtr<Screen> m_screen;
    mutable RefPtr<BarProp> m_scrollbars;
    mutable RefPtr<DOMSelection> m_selection;
    mutable RefPtr<BarProp> m_statusbar;
    mutable RefPtr<BarProp> m_toolbar;
    mutable RefPtr<Location> m_location;
    mutable RefPtr<VisualViewport> m_visualViewport;

    String m_status;
    String m_defaultStatus;

    enum class PageStatus { None, Shown, Hidden };
    PageStatus m_lastPageStatus { PageStatus::None };

#if PLATFORM(IOS_FAMILY)
    unsigned m_scrollEventListenerCount { 0 };
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    unsigned m_touchAndGestureEventListenerCount { 0 };
#endif

#if ENABLE(GAMEPAD)
    uint64_t m_gamepadEventListenerCount { 0 };
#endif

    mutable RefPtr<Storage> m_sessionStorage;
    mutable RefPtr<Storage> m_localStorage;
    mutable RefPtr<DOMApplicationCache> m_applicationCache;

    RefPtr<CustomElementRegistry> m_customElementRegistry;

    mutable RefPtr<Performance> m_performance;

    std::optional<ReducedResolutionSeconds> m_frozenNowTimestamp;

    // For the purpose of tracking user activation, each Window W has a last activation timestamp. This is a number indicating the last time W got
    // an activation notification. It corresponds to a DOMHighResTimeStamp value except for two cases: positive infinity indicates that W has never
    // been activated, while negative infinity indicates that a user activation-gated API has consumed the last user activation of W. The initial
    // value is positive infinity.
    MonotonicTime m_lastActivationTimestamp { MonotonicTime::infinity() };

    bool m_wasWrappedWithoutInitializedSecurityOrigin { false };
    bool m_mayReuseForNavigation { true };
#if ENABLE(USER_MESSAGE_HANDLERS)
    mutable RefPtr<WebKitNamespace> m_webkitNamespace;
#endif
};

} // namespace WebCore
