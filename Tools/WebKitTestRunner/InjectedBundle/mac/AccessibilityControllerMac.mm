/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "AccessibilityController.h"

#import "AccessibilityCommonCocoa.h"
#import "AccessibilityNotificationHandler.h"
#import "InjectedBundle.h"
#import "InjectedBundlePage.h"
#import "JSBasics.h"
#import "WebCoreTestSupport.h"
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebKit/WKBundle.h>
#import <WebKit/WKBundleFramePrivate.h>
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePagePrivate.h>

#import <pal/spi/mac/HIServicesSPI.h>

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#import <pal/spi/cocoa/AccessibilitySupportSPI.h>
#endif

namespace WTR {

void AccessibilityController::platformInitialize()
{
    // Override the client identifier to be kAXClientTypeWebKitTesting which is treated the same as the VoiceOver identifier in isolated tree mode.
    // This also allows to enable some APIs during testing only for unit test purposes, not for other clients consumption.
    _AXSetClientIdentificationOverride((AXClientType)kAXClientTypeWebKitTesting);
}

RefPtr<AccessibilityUIElement> AccessibilityController::focusedElement(JSContextRef context)
{
    if (!_WKAccessibilityRootObjectForTesting(WKBundleFrameForJavaScriptContext(context)))
        return nullptr;

    RetainPtr<PlatformUIElement> focus;
    executeOnAXThreadAndWait([&focus] () {
        focus = static_cast<PlatformUIElement>(WKAccessibilityFocusedUIElement());
    });
    if (focus)
        return AccessibilityUIElement::create(focus.get());
    return nullptr;
}

bool AccessibilityController::addNotificationListener(JSContextRef context, JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    if (m_globalNotificationHandler)
        return false;

    m_globalNotificationHandler = adoptNS([[AccessibilityNotificationHandler alloc] initWithContext:context]);
    [m_globalNotificationHandler.get() setCallback:functionCallback];
    [m_globalNotificationHandler.get() startObserving];

    return true;
}

bool AccessibilityController::removeNotificationListener()
{
    ASSERT(m_globalNotificationHandler);

    [m_globalNotificationHandler.get() stopObserving];
    m_globalNotificationHandler.clear();

    return true;
}

void AccessibilityController::resetToConsistentState()
{
    if (m_globalNotificationHandler)
        removeNotificationListener();
}

static id findAccessibleObjectById(id obj, NSString *idAttribute)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id objIdAttribute = [obj accessibilityAttributeValue:@"AXDOMIdentifier"];
    if ([objIdAttribute isKindOfClass:[NSString class]] && [objIdAttribute isEqualToString:idAttribute])
        return obj;
    END_AX_OBJC_EXCEPTIONS

    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *children = [obj accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
    NSUInteger childrenCount = [children count];
    for (NSUInteger i = 0; i < childrenCount; ++i) {
        id result = findAccessibleObjectById([children objectAtIndex:i], idAttribute);
        if (result)
            return result;
    }
    END_AX_OBJC_EXCEPTIONS

    return nil;
}

void AccessibilityController::injectAccessibilityPreference(JSStringRef domain, JSStringRef key, JSStringRef value)
{
    auto page = InjectedBundle::singleton().page()->page();
    NSNumber *numberValue = @([[NSString stringWithJSStringRef:value] integerValue]);
    NSData *encodedData = [NSKeyedArchiver archivedDataWithRootObject:numberValue requiringSecureCoding:YES error:nil];
    NSString *encodedString = [encodedData base64EncodedStringWithOptions:0];
    WKAccessibilityTestingInjectPreference(page, toWK(domain).get(), toWK(key).get(), toWK(encodedString).get());
}

RefPtr<AccessibilityUIElement> AccessibilityController::accessibleElementById(JSContextRef context, JSStringRef idAttribute)
{
    PlatformUIElement root = static_cast<PlatformUIElement>(_WKAccessibilityRootObjectForTesting(WKBundleFrameForJavaScriptContext(context)));

    NSString *attributeName = [NSString stringWithJSStringRef:idAttribute];
    RetainPtr<id> result;
    executeOnAXThreadAndWait([&root, &attributeName, &result] {
        result = findAccessibleObjectById(root, attributeName);
    });

    if (result)
        return AccessibilityUIElement::create(result.get());
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityController::platformName()
{
    return WTR::createJSString("mac");
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AccessibilityController::updateIsolatedTreeMode()
{
    WebCoreTestSupport::setAccessibilityIsolatedTreeEnabled(m_accessibilityIsolatedTreeMode);
    _AXSSetIsolatedTreeMode(m_accessibilityIsolatedTreeMode ? AXSIsolatedTreeModeSecondaryThread : AXSIsolatedTreeModeOff);
}
#endif

void AccessibilityController::overrideClient(JSStringRef clientType)
{
    NSString *clientString = [NSString stringWithJSStringRef:clientType];
    if ([clientString caseInsensitiveCompare:@"voiceover"] == NSOrderedSame)
        _AXSetClientIdentificationOverride(kAXClientTypeVoiceOver);
    else
        _AXSetClientIdentificationOverride(kAXClientTypeNoActiveRequestFound);
}

void AccessibilityController::printTrees(JSContextRef context)
{
    PlatformUIElement root = static_cast<PlatformUIElement>(_WKAccessibilityRootObjectForTesting(WKBundleFrameForJavaScriptContext(context)));
    [root accessibilityPerformAction:@"AXLogTrees"];
}

// AXThread implementation

void AXThread::initializeRunLoop()
{
    // Initialize the run loop.
    {
        Locker locker { m_initializeRunLoopMutex };

        m_threadRunLoop = CFRunLoopGetCurrent();

        CFRunLoopSourceContext context = { 0, this, 0, 0, 0, 0, 0, 0, 0, threadRunLoopSourceCallback };
        m_threadRunLoopSource = adoptCF(CFRunLoopSourceCreate(0, 0, &context));
        CFRunLoopAddSource(CFRunLoopGetCurrent(), m_threadRunLoopSource.get(), kCFRunLoopDefaultMode);

        m_initializeRunLoopConditionVariable.notifyAll();
    }

    ASSERT(isCurrentThread());

    CFRunLoopRun();
}

void AXThread::wakeUpRunLoop()
{
    CFRunLoopSourceSignal(m_threadRunLoopSource.get());
    CFRunLoopWakeUp(m_threadRunLoop.get());
}

void AXThread::threadRunLoopSourceCallback(void* axThread)
{
    static_cast<AXThread*>(axThread)->threadRunLoopSourceCallback();
}

void AXThread::threadRunLoopSourceCallback()
{
    @autoreleasepool {
        dispatchFunctionsFromAXThread();
    }
}

void AccessibilityController::platformInitializeClientAccessibility()
{
    setIsolatedTreeMode(true);

    // This triggers WebKit's normal accessibility initialization flow via IPC
    WTR::postSynchronousMessage("InitializeWebProcessAccessibility");

    // The above IPC triggers async initialization. Force the isolated tree to be built
    // now by accessing the root object ON THE AX THREAD, so it's ready before the first
    // axGetRoot() call from the test.
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(page);
    if (!mainFrame)
        return;

    // Access the root object on the AX thread to trigger isolated tree creation with proper thread setup
    RetainPtr<PlatformUIElement> root;
    executeOnAXThreadAndWait([&mainFrame, &root] () {
        root = static_cast<PlatformUIElement>(_WKAccessibilityRootObjectForTesting(mainFrame));
    });
}

} // namespace WTR
