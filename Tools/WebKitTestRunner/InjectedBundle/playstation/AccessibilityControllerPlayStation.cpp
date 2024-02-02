/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "AccessibilityController.h"

#include "AccessibilityUIElement.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include <WebKit/WKBundlePagePrivate.h>

namespace WTR {

void AccessibilityController::resetToConsistentState()
{
}

RefPtr<AccessibilityUIElement> AccessibilityController::accessibleElementById(JSStringRef)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityController::platformName()
{
    JSRetainPtr<JSStringRef> platformName(Adopt, JSStringCreateWithUTF8CString("playstation"));
    return platformName;
}

Ref<AccessibilityUIElement> AccessibilityController::rootElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    PlatformUIElement root = static_cast<PlatformUIElement>(WKAccessibilityRootObject(page));
    return AccessibilityUIElement::create(root);
}

RefPtr<AccessibilityUIElement> AccessibilityController::focusedElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    PlatformUIElement focusedElement = static_cast<PlatformUIElement>(WKAccessibilityFocusedObject(page));
    return AccessibilityUIElement::create(focusedElement);
}

bool AccessibilityController::addNotificationListener(JSValueRef)
{
    return false;
}

void AccessibilityController::injectAccessibilityPreference(JSStringRef, JSStringRef, JSStringRef)
{
}

bool AccessibilityController::removeNotificationListener()
{
    return false;
}

void AccessibilityController::overrideClient(JSStringRef)
{
}

} // namespace WTR
