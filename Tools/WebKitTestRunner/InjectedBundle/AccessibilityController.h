/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
#include "JSWrappable.h"
#include <JavaScriptCore/JSObjectRef.h>
#include <wtf/FastMalloc.h>
#include <wtf/Platform.h>
#if USE(ATK)
#include "AccessibilityNotificationHandlerAtk.h"
#endif

namespace WTR {
    
class AccessibilityController : public JSWrappable {
public:
    static Ref<AccessibilityController> create();
    ~AccessibilityController();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);
    virtual JSClassRef wrapperClass();
    
    // Enhanced accessibility.
    void enableEnhancedAccessibility(bool);
    bool enhancedAccessibilityEnabled();
    
    JSRetainPtr<JSStringRef> platformName();

    // Controller Methods - platform-independent implementations.
#if HAVE(ACCESSIBILITY)
    Ref<AccessibilityUIElement> rootElement();
    Ref<AccessibilityUIElement> focusedElement();
#endif
    RefPtr<AccessibilityUIElement> elementAtPoint(int x, int y);
    RefPtr<AccessibilityUIElement> accessibleElementById(JSStringRef idAttribute);

    bool addNotificationListener(JSValueRef functionCallback);
    bool removeNotificationListener();

    // Here for consistency with DRT. Not implemented because they don't do anything on the Mac.
    void logFocusEvents() { }
    void logValueChangeEvents() { }
    void logScrollingStartEvents() { }
    void logAccessibilityEvents() { };

    void resetToConsistentState();

#if !HAVE(ACCESSIBILITY) && (PLATFORM(GTK) || PLATFORM(WPE))
    RefPtr<AccessibilityUIElement> rootElement() { return nullptr; }
    RefPtr<AccessibilityUIElement> focusedElement() { return nullptr; }
#endif

private:
    AccessibilityController();

#if PLATFORM(COCOA)
    RetainPtr<NotificationHandler> m_globalNotificationHandler;
#elif USE(ATK)
    RefPtr<AccessibilityNotificationHandler> m_globalNotificationHandler;
#endif
};

} // namespace WTR
