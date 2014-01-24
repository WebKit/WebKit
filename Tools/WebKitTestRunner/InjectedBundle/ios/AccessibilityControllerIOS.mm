/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AccessibilityCommonMac.h"
#import "AccessibilityController.h"
#import "AccessibilityNotificationHandler.h"
#import "InjectedBundle.h"
#import "InjectedBundlePage.h"

#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebKit2/WKBundle.h>
#import <WebKit2/WKBundlePage.h>
#import <WebKit2/WKBundlePagePrivate.h>

namespace WTR {

bool AccessibilityController::addNotificationListener(JSValueRef functionCallback)
{
    return false;
}

bool AccessibilityController::removeNotificationListener()
{
    return false;
}


JSRetainPtr<JSStringRef> AccessibilityController::platformName()
{
    JSRetainPtr<JSStringRef> platformName(Adopt, JSStringCreateWithUTF8CString("ios"));
    return platformName;
}

void AccessibilityController::logAccessibilityEvents()
{
}

void AccessibilityController::resetToConsistentState()
{
    if (m_globalNotificationHandler)
        removeNotificationListener();
}

static id findAccessibleObjectById(id obj, NSString *idAttribute)
{
    return 0;
}

PassRefPtr<AccessibilityUIElement> AccessibilityController::accessibleElementById(JSStringRef idAttribute)
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    id root = static_cast<PlatformUIElement>(WKAccessibilityRootObject(page));

    id result = findAccessibleObjectById(root, [NSString stringWithJSStringRef:idAttribute]);
    if (result)
        return AccessibilityUIElement::create(result);

    return 0;
}

} // namespace WTR
