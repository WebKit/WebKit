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

#import "config.h"

#if WK_API_ENABLED && ENABLE(DRAG_SUPPORT)

#import "DragAndDropSimulator.h"
#import "PlatformUtilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WebArchive.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

TEST(DragAndDropTests, ModernWebArchiveType)
{
    NSData *markupData = [@"<strong><i>Hello world</i></strong>" dataUsingEncoding:NSUTF8StringEncoding];
    auto mainResource = adoptNS([[WebResource alloc] initWithData:markupData URL:[NSURL URLWithString:@"foo.html"] MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    auto archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:@[ ] subframeArchives:@[ ]]);
    NSString *webArchiveType = (__bridge NSString *)kUTTypeWebArchive;

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body style='width: 100%; height: 100%;' contenteditable>"];
#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[webArchiveType, (__bridge NSString *)kUTTypeUTF8PlainText] owner:nil];
    [pasteboard setData:[archive data] forType:webArchiveType];
    [pasteboard setData:[@"Hello world" dataUsingEncoding:NSUTF8StringEncoding] forType:(__bridge NSString *)kUTTypeUTF8PlainText];
    [simulator setExternalDragPasteboard:pasteboard];
#else
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerDataRepresentationForTypeIdentifier:webArchiveType visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler([archive data], nil);
        return nil;
    }];
    [item registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler([@"Hello world" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];
    [simulator setExternalItemProviders:@[ item.get() ]];
#endif
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    [webView stringByEvaluatingJavaScript:@"document.body.focus(); getSelection().setBaseAndExtent(document.body, 0, document.body, 1)"];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('bold')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('italic')"].boolValue);
}

TEST(DragAndDropTests, DragImageLocationForLinkInSubframe)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 400, 400)]);
    [[simulator webView] synchronouslyLoadTestPageNamed:@"link-in-iframe-and-input"];
    [simulator runFrom:CGPointMake(200, 375) to:CGPointMake(200, 125)];

    EXPECT_WK_STREQ("https://www.apple.com/", [[simulator webView] stringByEvaluatingJavaScript:@"document.querySelector('input').value"]);

#if PLATFORM(MAC)
    EXPECT_TRUE(NSPointInRect([simulator initialDragImageLocationInView], NSMakeRect(0, 250, 400, 250)));
#endif
}

TEST(DragAndDropTests, ExposeMultipleURLsInDataTransfer)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];
    WKPreferencesSetCustomPasteboardDataEnabled((__bridge WKPreferencesRef)[webView configuration].preferences, true);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];

    NSString *stringData = @"Hello world";
    NSURL *firstURL = [NSURL URLWithString:@"https://webkit.org/"];
    NSURL *secondURL = [NSURL URLWithString:@"https://apple.com/"];

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard writeObjects:@[ stringData, firstURL, secondURL ]];
    [simulator setExternalDragPasteboard:pasteboard];
#else
    auto stringItem = adoptNS([[NSItemProvider alloc] initWithObject:stringData]);
    auto firstURLItem = adoptNS([[NSItemProvider alloc] initWithObject:firstURL]);
    auto secondURLItem = adoptNS([[NSItemProvider alloc] initWithObject:secondURL]);
    for (NSItemProvider *item in @[ stringItem.get(), firstURLItem.get(), secondURLItem.get() ])
        item.preferredPresentationStyle = UIPreferredPresentationStyleInline;
    [simulator setExternalItemProviders:@[ stringItem.get(), firstURLItem.get(), secondURLItem.get() ]];
#endif

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("text/plain, text/uri-list", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/plain), (STRING, text/uri-list)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("https://webkit.org/\nhttps://apple.com/", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
}

TEST(DragAndDropTests, PreventingMouseDownShouldPreventDragStart)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];
    WKPreferencesSetCustomPasteboardDataEnabled((__bridge WKPreferencesRef)[webView configuration].preferences, true);
    [webView synchronouslyLoadTestPageNamed:@"link-and-target-div"];
    [simulator runFrom:CGPointMake(160, 100) to:CGPointMake(160, 300)];
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    EXPECT_WK_STREQ("dragstart dragend", [webView stringByEvaluatingJavaScript:@"output.textContent"]);

    // Now verify that preventing default on the 'mousedown' event cancels the drag altogether.
    [webView evaluateJavaScript:@"target.textContent = output.textContent = ''" completionHandler:nil];
    [webView evaluateJavaScript:@"source.addEventListener('mousedown', event => { event.preventDefault(); window.observedMousedown = true; })" completionHandler:nil];
    [simulator runFrom:CGPointMake(160, 100) to:CGPointMake(160, 300)];
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"output.textContent"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"observedMousedown"].boolValue);
}

#if ENABLE(INPUT_TYPE_COLOR)

TEST(DragAndDropTests, ColorInputToColorInput)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, ColorInputToDisabledColorInput)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").disabled = true"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#ff0000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, DisabledColorInputToColorInput)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").disabled = true"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#ff0000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, ReadOnlyColorInputToReadOnlyColorInput)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").readOnly = true"];
    [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").readOnly = true"];
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drag-target\").value"]);
    EXPECT_WK_STREQ(@"#000000", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"drop-target\").value"]);
}

TEST(DragAndDropTests, ColorInputEvents)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    auto webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];

    __block bool changeEventFired = false;
    [webView performAfterReceivingMessage:@"change" action:^() {
        changeEventFired = true;
    }];

    __block bool inputEventFired = false;
    [webView performAfterReceivingMessage:@"input" action:^() {
        inputEventFired = true;
    }];

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    TestWebKitAPI::Util::run(&inputEventFired);
    TestWebKitAPI::Util::run(&changeEventFired);
}

#endif // ENABLE(INPUT_TYPE_COLOR)

#endif // WK_API_ENABLED && ENABLE(DRAG_SUPPORT)
