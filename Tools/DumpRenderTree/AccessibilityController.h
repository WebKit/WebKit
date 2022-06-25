/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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

#include "AccessibilityUIElement.h"
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <string>
#include <wtf/HashMap.h>
#include <wtf/Platform.h>

#if PLATFORM(WIN)
#include <windows.h>
#endif

class AccessibilityController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AccessibilityController();
    ~AccessibilityController();

    void makeWindowObject(JSContextRef);

    // Controller Methods - platform-independent implementations
    AccessibilityUIElement rootElement();
    AccessibilityUIElement focusedElement();
    AccessibilityUIElement elementAtPoint(int x, int y);
    AccessibilityUIElement accessibleElementById(JSStringRef id);

    void setLogFocusEvents(bool);
    void setLogValueChangeEvents(bool);
    void setLogScrollingStartEvents(bool);
    void setLogAccessibilityEvents(bool);

    void resetToConsistentState();

    // Global notification listener, captures notifications on any object.
    bool addNotificationListener(JSObjectRef functionCallback);
    void removeNotificationListener();

    // Enhanced accessibility.
    void enableEnhancedAccessibility(bool);
    bool enhancedAccessibilityEnabled();

    JSRetainPtr<JSStringRef> platformName() const;

#if PLATFORM(WIN)
    // Helper methods so this class can add the listeners on behalf of AccessibilityUIElement.
    void winAddNotificationListener(PlatformUIElement, JSObjectRef functionCallback);
    void winNotificationReceived(PlatformUIElement, const std::string& eventName);
#endif

private:
    static JSRetainPtr<JSClassRef> createJSClass();

#if PLATFORM(WIN)
    HWINEVENTHOOK m_focusEventHook { nullptr };
    HWINEVENTHOOK m_valueChangeEventHook { nullptr };
    HWINEVENTHOOK m_scrollingStartEventHook { nullptr };

    HWINEVENTHOOK m_allEventsHook { nullptr };
    HWINEVENTHOOK m_notificationsEventHook { nullptr };
    HashMap<PlatformUIElement, JSObjectRef> m_notificationListeners;
#endif

#if PLATFORM(COCOA)
    RetainPtr<id> m_globalNotificationHandler;
#endif

    void platformResetToConsistentState();
};
