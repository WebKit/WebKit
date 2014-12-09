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
#import "ActionMenusBundleSPI.h"
#import "InjectedBundleTest.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import <Foundation/Foundation.h>
#import <WebKit/WKBundleFramePrivate.h>
#import <WebKit/WKBundleHitTestResult.h>
#import <WebKit/WKBundleNodeHandlePrivate.h>
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePageOverlay.h>
#import <WebKit/WKBundleRangeHandlePrivate.h>
#import <WebKit/WKDictionary.h>
#import <WebKit/WKNumber.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

RetainPtr<DDActionContext> createActionContextForPhoneNumber()
{
    RetainPtr<CFStringRef> plainText = CFSTR("(408) 996-1010");
    RetainPtr<DDScannerRef> scanner = adoptCF(DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    RetainPtr<DDScanQueryRef> scanQuery = adoptCF(DDScanQueryCreateFromString(kCFAllocatorDefault, plainText.get(), CFRangeMake(0, CFStringGetLength(plainText.get()))));

    if (!DDScannerScanQuery(scanner.get(), scanQuery.get()))
        return nullptr;

    RetainPtr<CFArrayRef> results = adoptCF(DDScannerCopyResultsWithOptions(scanner.get(), DDScannerCopyResultsOptionsNoOverlap));

    CFIndex resultCount = CFArrayGetCount(results.get());
    if (resultCount != 1)
        return nullptr;

    DDResultRef mainResult = (DDResultRef)CFArrayGetValueAtIndex(results.get(), 0);

    if (!mainResult)
        return nullptr;

    RetainPtr<DDActionContext> actionContext = adoptNS([[getDDActionContextClass() alloc] init]);
    [actionContext setAllResults:@[ (id)mainResult ]];
    [actionContext setMainResult:mainResult];

    return actionContext;
}
    
class ActionMenuTest : public InjectedBundleTest {
public:
    ActionMenuTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    static void prepareForActionMenu(WKBundlePageRef page, WKBundleHitTestResultRef hitTestResult, WKTypeRef* userData, const void* clientInfo)
    {
        WKStringRef keys[1];
        WKTypeRef values[1];

        WKRetainPtr<WKStringRef> hasLinkKey = adoptWK(WKStringCreateWithUTF8CString("hasLinkURL"));
        WKRetainPtr<WKURLRef> absoluteLinkURL = adoptWK(WKBundleHitTestResultCopyAbsoluteLinkURL(hitTestResult));
        WKRetainPtr<WKBooleanRef> hasLinkValue = adoptWK(WKBooleanCreate(!!absoluteLinkURL));
        keys[0] = hasLinkKey.get();
        values[0] = hasLinkValue.get();

        *userData = WKDictionaryCreate(keys, values, 1);
    }

    static void* actionContextForResultAtPoint(WKBundlePageOverlayRef pageOverlay, WKPoint position, WKBundleRangeHandleRef* rangeHandle, const void* clientInfo)
    {
        if (position.x > 700) {
            RetainPtr<DDActionContext> actionContext = createActionContextForPhoneNumber();
            *rangeHandle = (WKBundleRangeHandleRef)clientInfo;
            return (void*)actionContext.autorelease();
        }
        return nullptr;
    }

    static void drawRect(WKBundlePageOverlayRef pageOverlay, void* graphicsContext, WKRect dirtyRect, const void* clientInfo)
    {
        CGContextRef ctx = static_cast<CGContextRef>(graphicsContext);
        CGContextSetRGBFillColor(ctx, 0, 1, 0, 1);
        CGContextFillRect(ctx, CGRectMake(700, 0, 100, 600));
    }

    virtual void didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
    {
        WKBundlePageContextMenuClientV1 contextMenuClient;
        memset(&contextMenuClient, 0, sizeof(contextMenuClient));
        contextMenuClient.base.version = 1;
        contextMenuClient.prepareForActionMenu = prepareForActionMenu;
        WKBundlePageSetContextMenuClient(page, &contextMenuClient.base);

        WKRetainPtr<WKBundleHitTestResultRef> hitTestResult = adoptWK(WKBundleFrameCreateHitTestResult(WKBundlePageGetMainFrame(page), WKPointMake(708, 8)));
        WKRetainPtr<WKBundleNodeHandleRef> nodeHandle = adoptWK(WKBundleHitTestResultCopyNodeHandle(hitTestResult.get()));
        _rangeHandle = adoptWK(WKBundleNodeHandleCopyVisibleRange(nodeHandle.get()));

        WKBundlePageOverlayClientV1 pageOverlayClient;
        memset(&pageOverlayClient, 0, sizeof(pageOverlayClient));
        pageOverlayClient.base.version = 1;
        pageOverlayClient.base.clientInfo = _rangeHandle.get();

        pageOverlayClient.drawRect = drawRect;
        pageOverlayClient.actionContextForResultAtPoint = actionContextForResultAtPoint;

        _bundlePageOverlay = adoptWK(WKBundlePageOverlayCreate(&pageOverlayClient.base));
        WKBundlePageInstallPageOverlay(page, _bundlePageOverlay.get());
        WKBundlePageOverlaySetNeedsDisplay(_bundlePageOverlay.get(), WKRectMake(0, 0, 800, 600));
    }

    WKRetainPtr<WKBundlePageOverlayRef> _bundlePageOverlay;
    WKRetainPtr<WKBundleRangeHandleRef> _rangeHandle;
};

static InjectedBundleTest::Register<ActionMenuTest> registrar("ActionMenusTest");

} // namespace TestWebKitAPI
