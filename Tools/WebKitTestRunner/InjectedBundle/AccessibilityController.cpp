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

#include "config.h"
#include "AccessibilityController.h"

#include "AccessibilityUIElement.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSAccessibilityController.h"

#include <WebKit/WKBundle.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace WTR {

Ref<AccessibilityController> AccessibilityController::create()
{
    return adoptRef(*new AccessibilityController);
}

AccessibilityController::AccessibilityController()
{
}

AccessibilityController::~AccessibilityController()
{
}

void AccessibilityController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "accessibilityController", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef AccessibilityController::wrapperClass()
{
    return JSAccessibilityController::accessibilityControllerClass();
}

void AccessibilityController::enableEnhancedAccessibility(bool enable)
{
    WKAccessibilityEnableEnhancedAccessibility(enable);
}

bool AccessibilityController::enhancedAccessibilityEnabled()
{
    return WKAccessibilityEnhancedAccessibilityEnabled();
}

#if PLATFORM(COCOA)
Ref<AccessibilityUIElement> AccessibilityController::rootElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    void* root = WKAccessibilityRootObject(page);
    
    return AccessibilityUIElement::create(static_cast<PlatformUIElement>(root));    
}

Ref<AccessibilityUIElement> AccessibilityController::focusedElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    void* root = WKAccessibilityFocusedObject(page);
    
    return AccessibilityUIElement::create(static_cast<PlatformUIElement>(root));    
}
#endif

RefPtr<AccessibilityUIElement> AccessibilityController::elementAtPoint(int x, int y)
{
    Ref<AccessibilityUIElement> uiElement = rootElement();
    return uiElement->elementAtPoint(x, y);
}

#if !HAVE(ACCESSIBILITY) && PLATFORM(GTK)
RefPtr<AccessibilityUIElement> AccessibilityController::rootElement() { return nullptr; }
RefPtr<AccessibilityUIElement> AccessibilityController::focusedElement() { return nullptr; }
#endif

} // namespace WTR
