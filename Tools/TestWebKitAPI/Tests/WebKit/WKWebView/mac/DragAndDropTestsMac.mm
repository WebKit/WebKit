/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

#import "Helpers/cocoa/DragAndDropSimulator.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "InstanceMethodSwizzler.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/mac/TestDraggingInfo.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/PasteboardCustomData.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/FileSystem.h>

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

TEST(DragAndDropTests, NumberOfValidItemsForDrop)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[@"file-name"] forType:NSFilenamesPboardType];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [simulator setExternalDragPasteboard:pasteboard];
    [webView synchronouslyLoadTestPageNamed:@"full-page-dropzone"];

    NSInteger numberOfValidItemsForDrop = 0;
    [simulator setWillEndDraggingHandler:[&numberOfValidItemsForDrop, simulator] {
        numberOfValidItemsForDrop = [simulator draggingInfo].numberOfValidItemsForDrop;
    }];

    [simulator runFrom:NSMakePoint(0, 0) to:NSMakePoint(200, 200)];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"observedDragEnter"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"observedDragOver"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"observedDrop"].boolValue);
    EXPECT_EQ(1U, numberOfValidItemsForDrop);
}

TEST(DragAndDropTests, PerformDragWithLegacyFilesAfterWebProcessTermination)
{
    // Regression test: dropping files (legacy NSFilenamesPboardType) sends an async
    // AllowFilesAccessFromWebProcess IPC to the NetworkProcess, and the reply lambda
    // calls WebPageProxy::createSandboxExtensionsIfNeeded, which dereferences the
    // legacyMainFrameProcess connection. If the WebProcess is gone by then, this
    // used to RELEASE_ASSERT in AuxiliaryProcessProxy::connection().

    RetainPtr tempDirectory = FileSystem::createTemporaryDirectory(@"WebKitDragAndDropTest");
    RetainPtr tempFilePath = [tempDirectory stringByAppendingPathComponent:@"dropped-file.txt"];
    [@"hello" writeToFile:tempFilePath.get() atomically:YES encoding:NSUTF8StringEncoding error:nil];

    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[tempFilePath.get()] forType:NSFilenamesPboardType];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [simulator setExternalDragPasteboard:pasteboard];
    [webView synchronouslyLoadTestPageNamed:@"full-page-dropzone"];

    // Tear down the WebProcess connection synchronously right before the drop
    // is performed. _killWebContentProcessAndResetState routes through
    // requestTermination -> processDidTerminateOrFailedToLaunch -> shutDownProcess,
    // which sets m_connection to nullptr immediately.
    [simulator setWillEndDraggingHandler:[webView] {
        [webView _killWebContentProcessAndResetState];
    }];

    [simulator runFrom:NSMakePoint(0, 0) to:NSMakePoint(200, 200)];

    // Pump the runloop long enough for the AllowFilesAccessFromWebProcess reply
    // to be delivered. Prior to fix, this would RELEASE_ASSERT in
    // AuxiliaryProcessProxy::connection() inside the reply lambda.
    TestWebKitAPI::Util::runFor(0.5_s);

    [[NSFileManager defaultManager] removeItemAtPath:tempDirectory.get() error:nil];
}

TEST(DragAndDropTests, DragEndEventCoordinatesWithNestedIframes)
{
    static constexpr ASCIILiteral mainframeHTML = "<iframe width='500' height='500' style='position: absolute; top: 50px; left: 50px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe>"_s;

    static constexpr ASCIILiteral subframeHTML = "<script>"
    "   window.events = [];"
    "   addEventListener('message', function(event) {"
    "       console.log(event.data);"
    "       window.events.push(event.data);"
    "   });"
    "</script>"
    "<iframe width='500' height='500' style='position: absolute; top: 50px; left: 50px; border: 2px solid blue;' src='https://domain3.com/nestedSubframe'></iframe>"_s;

    static constexpr ASCIILiteral nestedSubframeHTML =
    "<body style='margin: 0; padding: 0; width: 100%; height: 100vh; background-color: lightblue;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: yellow; position: absolute; top: 50px; left: 50px;'>Drag me</div>"
    "<script>"
        "const draggable = document.getElementById('draggable');"
        "draggable.addEventListener('dragstart', (event) => {"
            "parent.postMessage('dragstart:' + event.clientX + ',' + event.clientY, '*');"
        "});"
        "draggable.addEventListener('dragend', (event) => {"
            "parent.postMessage('dragend:' + event.clientX + ',' + event.clientY, '*');"
        "});"
    "</script>"
    "</body>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } },
        { "/nestedSubframe"_s, { nestedSubframeHTML } }
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 800, 800) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(200, 200) to:CGPointMake(300, 300)];

    RetainPtr<NSArray<NSString *>> events = [webView objectByEvaluatingJavaScript:@"window.events" inFrame:[webView firstChildFrame]];
    EXPECT_GT([events count], 0U);
    bool foundDragStart = false;
    bool foundDragEnd = false;
    NSString *dragEndEvent = nil;

    for (NSString *event in events.get()) {
        if ([event hasPrefix:@"dragstart:"]) {
            foundDragStart = true;
        } else if ([event hasPrefix:@"dragend:"]) {
            foundDragEnd = true;
            dragEndEvent = event;
        }
    }

    EXPECT_TRUE(foundDragStart);
    EXPECT_TRUE(foundDragEnd);

    if (dragEndEvent) {
        RetainPtr coords = [dragEndEvent substringFromIndex:[@"dragend:" length]];
        RetainPtr components = [coords componentsSeparatedByString:@","];
        if ([components count] == 2) {
            int x = [components[0] intValue];
            int y = [components[1] intValue];
            EXPECT_TRUE(x >= 190 && x <= 200) << "Expected dragend x coordinate around 193, got " << x;
            EXPECT_TRUE(y >= 190 && y <= 200) << "Expected dragend y coordinate around 196, got " << y;
        }
    }

    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(200, 200) to:CGPointMake(0, 0)];

    events = [webView objectByEvaluatingJavaScript:@"window.events" inFrame:[webView firstChildFrame]];
    EXPECT_GT([events count], 0U);
    foundDragStart = false;
    foundDragEnd = false;
    dragEndEvent = nil;

    for (NSString *event in events.get()) {
        if ([event hasPrefix:@"dragstart:"]) {
            foundDragStart = true;
        } else if ([event hasPrefix:@"dragend:"]) {
            foundDragEnd = true;
            dragEndEvent = event;
        }
    }

    EXPECT_TRUE(foundDragStart);
    EXPECT_TRUE(foundDragEnd);

    if (dragEndEvent) {
        RetainPtr coords = [dragEndEvent substringFromIndex:[@"dragend:" length]];
        RetainPtr components = [coords componentsSeparatedByString:@","];
        if ([components count] == 2) {
            int x = [components[0] intValue];
            int y = [components[1] intValue];
            EXPECT_TRUE(x >= -105 && x <= -95) << "Expected dragend x coordinate around -100, got " << x;
            EXPECT_TRUE(y >= -105 && y <= -95) << "Expected dragend y coordinate around -100, got " << y;
        }
    }
}

TEST(DragAndDropTests, DropFileOnSiteIsolatedIframeRegistersBlobURL)
{
    static constexpr auto mainframeHTML =
        "<iframe id='child' width='400' height='400' "
        "style='position:absolute;top:0;left:0;border:0;' "
        "src='https://domain2.com/iframe'></iframe>"_s;

    static constexpr auto iframeHTML =
        "<body style='margin:0;width:100vw;height:100vh;background:lightblue;'"
        " ondragenter='event.preventDefault();'"
        " ondragover='event.preventDefault();'"
        " ondrop=\"event.preventDefault();"
        "   const f = event.dataTransfer.files[0];"
        "   if (!f) { window.webkit.messageHandlers.testHandler.postMessage('nofile'); }"
        "   else {"
        "     (async () => {"
        "       try {"
        "         const url = window.URL.createObjectURL(f);"
        "         const r = await fetch(url);"
        "         const b = await r.arrayBuffer();"
        "         window.webkit.messageHandlers.testHandler.postMessage('ok:bytes=' + b.byteLength);"
        "       } catch (e) {"
        "         window.webkit.messageHandlers.testHandler.postMessage('err:' + e.message);"
        "       }"
        "     })();"
        "   }\"></body>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    [[configuration preferences] _setSiteIsolationEnabled:YES];

    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    __block bool gotMessage = false;
    __block RetainPtr<NSString> dropResult;
    [messageHandler setDidReceiveScriptMessage:^(NSString *message) {
        if (gotMessage)
            return;
        dropResult = message;
        gotMessage = true;
    }];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate];

    RetainPtr fileURL = [NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"];
    [simulator writePromisedFiles:@[ fileURL ]];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [simulator runFrom:NSMakePoint(0, 0) to:NSMakePoint(200, 200)];

    TestWebKitAPI::Util::runFor(&gotMessage, 3_s);

    EXPECT_TRUE([dropResult hasPrefix:@"ok:bytes="])
        << "Expected the iframe to fetch the blob from URL.createObjectURL successfully. Got: "
        << (dropResult ? [dropResult UTF8String] : "(no message — iframe process likely killed)");
}

TEST(DragAndDropTests, DraggableElementWithTinyDragImageDoesNotCrash)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    RetainPtr webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"draggable-with-tiny-drag-image"];
    [simulator runFrom:NSMakePoint(150, 50) to:NSMakePoint(150, 200)];
    TestWebKitAPI::Util::waitForConditionWithLogging([&] -> bool {
        return [webView stringByEvaluatingJavaScript:@"window.dragStartFired"].boolValue;
    }, 2, @"Expected dragstart to fire for the tiny drag image case.");
}

TEST(DragAndDropTests, DraggableElementWithOnlyCustomPasteboardDataFiresDragEvents)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    RetainPtr webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"draggable-only-custom-data"];
    [simulator runFrom:NSMakePoint(150, 50) to:NSMakePoint(150, 200)];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] -> bool {
        return [webView stringByEvaluatingJavaScript:@"window.dragEventCount"].intValue > 0;
    }, 2, @"Expected drag events to fire on the source element.");

    TestWebKitAPI::Util::waitForConditionWithLogging([&] -> bool {
        return [webView stringByEvaluatingJavaScript:@"window.dragOverEventCount"].intValue > 0;
    }, 2, @"Expected dragover events to fire on the drop target.");

    // WebDummyPboardType should be on the drag pasteboard so
    // AppKit recognizes the view as a valid drag destination.
    EXPECT_TRUE([simulator containsDraggedType:@"Apple WebKit dummy pasteboard type"]);
}

TEST(DragAndDropTests, DropUserSelectAllUserDragElementDiv)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 320, 500)]);

    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-user-select-user-drag"];

    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    EXPECT_WK_STREQ(@"Text", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"editor\").textContent"]);
}

TEST(DragAndDropTests, DropColor)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[NSColorPboardType] owner:nil];
    [[NSColor colorWithRed:1 green:0 blue:0 alpha:1] writeToPasteboard:pasteboard];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [simulator setExternalDragPasteboard:pasteboard];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [simulator runFrom:NSMakePoint(0, 0) to:NSMakePoint(50, 50)];
    EXPECT_WK_STREQ(@"#ff0000", [webView stringByEvaluatingJavaScript:@"document.querySelector(\"input\").value"]);
}

TEST(DragAndDropTests, DragImageElementIntoFileUpload)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-file-upload"];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webView stringByEvaluatingJavaScript:@"imageload.textContent"].boolValue;
    }, 2, @"Expected image to finish loading.");
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"filecount.textContent"].integerValue);
}

TEST(DragAndDropTests, DragPromisedImageFileIntoFileUpload)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-file-upload"];

    NSURL *imageURL = [NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"];
    [simulator writePromisedFiles:@[ imageURL ]];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webView stringByEvaluatingJavaScript:@"imageload.textContent"].boolValue;
    }, 2, @"Expected image to finish loading.");
    static constexpr auto expectedDataTransferItems = "[{\"kind\":\"file\",\"type\":\"image/gif\",\"file\":null}]";
    EXPECT_WK_STREQ(expectedDataTransferItems, [webView stringByEvaluatingJavaScript:@"dragenterItems.textContent"]);
    EXPECT_WK_STREQ(expectedDataTransferItems, [webView stringByEvaluatingJavaScript:@"dragoverItems.textContent"]);
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"filecount.textContent"].integerValue);

    TestDraggingInfo *draggingInfo = [simulator draggingInfo];
    NSArray<NSFilePromiseReceiver *> *filePromiseReceivers = [draggingInfo filePromiseReceivers];
    EXPECT_EQ(1UL, [filePromiseReceivers count]);
    NSFilePromiseReceiver *filePromiseReceiver = filePromiseReceivers.firstObject;
    EXPECT_EQ(1UL, [filePromiseReceiver.fileTypes count]);
    EXPECT_WK_STREQ(UTTypeGIF.identifier, filePromiseReceiver.fileTypes.firstObject);
}

// https://bugs.webkit.org/show_bug.cgi?id=307601
TEST(DragAndDropTests, DragPromisedImageFileWithWebArchiveIntoFileUpload)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-file-upload"];

    NSURL *imageURL = [NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"];
    [simulator writePromisedFilesWithWebArchive:@[ imageURL ]];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webView stringByEvaluatingJavaScript:@"imageload.textContent"].boolValue;
    }, 2, @"Expected image to finish loading.");
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"filecount.textContent"].integerValue);
}

TEST(DragAndDropTests, ReadURLWhenDroppingPromisedWebLoc)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    auto *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];

    [simulator writePromisedWebLoc:[NSURL URLWithString:@"https://webkit.org/"]];
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(375, 375)];

    NSString *s = [webView stringByEvaluatingJavaScript:@"output.value"];
    BOOL success = TestWebKitAPI::Util::jsonMatchesExpectedValues(s, @{
        @"dragover" : @{
            @"Files": @"",
            @"text/uri-list": @""
        },
        @"drop": @{
            @"Files": @"",
            @"text/uri-list": @"https://webkit.org/"
        }
    });
    EXPECT_TRUE(success);
}

TEST(DragAndDropTests, DragImageFileIntoFileUpload)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-file-upload"];

    NSURL *imageURL = [NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"];
    [simulator writeFiles:@[ imageURL ]];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webView stringByEvaluatingJavaScript:@"imageload.textContent"].boolValue;
    }, 2, @"Expected image to finish loading.");
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"filecount.textContent"].integerValue);
}

static NSEvent *overrideCurrentEvent()
{
    return [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
        location:NSMakePoint(0, 200)
        modifierFlags:NSEventModifierFlagOption
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:0
        context:nil
        eventNumber:1
        clickCount:1
        pressure:1];
}

TEST(DragAndDropTests, DragImageWithOptionKeyDown)
{
    InstanceMethodSwizzler swizzler([NSApp class], @selector(currentEvent), reinterpret_cast<IMP>(overrideCurrentEvent));

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    auto pid = [webView _webProcessIdentifier];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    EXPECT_EQ(pid, [webView _webProcessIdentifier]);
}

TEST(DragAndDropTests, ProvideImageDataForMultiplePasteboards)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPasteboard *dragPasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    NSPasteboard *uniquePasteboard = [NSPasteboard pasteboardWithUniqueName];
    [webView pasteboard:dragPasteboard provideDataForType:NSTIFFPboardType];
    [webView pasteboard:uniquePasteboard provideDataForType:NSTIFFPboardType];
ALLOW_DEPRECATED_DECLARATIONS_END

    NSArray *allowedClasses = @[ NSImage.class ];
    NSImage *imageFromDragPasteboard = [dragPasteboard readObjectsForClasses:allowedClasses options:nil].firstObject;
    NSImage *imageFromUniquePasteboard = [uniquePasteboard readObjectsForClasses:allowedClasses options:nil].firstObject;

    EXPECT_EQ(imageFromUniquePasteboard.TIFFRepresentation.length, imageFromDragPasteboard.TIFFRepresentation.length);
    EXPECT_TRUE(NSEqualSizes(imageFromDragPasteboard.size, imageFromUniquePasteboard.size));
    EXPECT_FALSE(NSEqualSizes(NSZeroSize, imageFromUniquePasteboard.size));
    EXPECT_GT([dragPasteboard dataForType:@(WebCore::PasteboardCustomData::cocoaType().characters())].length, 0u);
}

TEST(DragAndDropTests, ProvideImageDataAsTypeIdentifiers)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setLargeImageAsyncDecodingEnabled:NO];

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];

    auto uniquePasteboard = retainPtr(NSPasteboard.pasteboardWithUniqueName);

    [webView synchronouslyLoadHTMLString:@"<img src='sunset-in-cupertino-600px.jpg'></img>"];
    [simulator runFrom:NSMakePoint(25, 25) to:NSMakePoint(300, 300)];
    [webView pasteboard:uniquePasteboard.get() provideDataForType:UTTypeJPEG.identifier];
    EXPECT_GT([uniquePasteboard dataForType:UTTypeJPEG.identifier].length, 0u);

    [webView synchronouslyLoadHTMLString:@"<img src='icon.png'></img>"];
    [simulator runFrom:NSMakePoint(25, 25) to:NSMakePoint(300, 300)];
    [webView pasteboard:uniquePasteboard.get() provideDataForType:UTTypePNG.identifier];
    EXPECT_GT([uniquePasteboard dataForType:UTTypePNG.identifier].length, 0u);

    [webView synchronouslyLoadHTMLString:@"<img src='apple.gif'></img>"];
    [simulator runFrom:NSMakePoint(25, 25) to:NSMakePoint(300, 300)];
    [webView pasteboard:uniquePasteboard.get() provideDataForType:UTTypeGIF.identifier];
    EXPECT_GT([uniquePasteboard dataForType:UTTypeGIF.identifier].length, 0u);
}

TEST(DragAndDropTests, DragLocationForImageInScrolledSubframe)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration preferences]._largeImageAsyncDecodingEnabled = NO;

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-in-scrolled-subframe"];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] -> bool {
        return [webView stringByEvaluatingJavaScript:@"doneLoadingSubframe"].boolValue;
    }, 3, @"Expected subframe to finish loading.");

    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(200, 200)];

    CGPoint dragLocation = [simulator initialDragImageLocationInView];
    EXPECT_NEAR(dragLocation.x, 0, 20);
    EXPECT_NEAR(dragLocation.y, 0, 20);

    RetainPtr dragTypes = [[[simulator draggingInfo] draggingPasteboard] types];
    EXPECT_TRUE([dragTypes containsObject:UTTypePNG.identifier]);
}

TEST(DragAndDropTests, DragPreviewOriginForImage)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    RetainPtr webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    auto imageMidpoint = [webView getElementMidpoint:@"#source"];
    NSPoint dragStart = imageMidpoint.value();
    [simulator runFrom:dragStart to:NSMakePoint(dragStart.x, dragStart.y + 200)];

    // A missing flipped-coordinate adjustment (rdar://175267103) would
    // shift the preview down by the image height, placing the origin below.
    NSPoint previewOrigin = [simulator initialDragImageLocationInView];
    EXPECT_LT(previewOrigin.y, dragStart.y);
}

TEST(DragAndDropTests, DragEnterAndLeaveRelatedTarget)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 320, 500)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"drag-relatedTarget"];

    [simulator runFrom:NSMakePoint(160, 90) to:NSMakePoint(160, 400)];

    EXPECT_WK_STREQ("null", [webView stringByEvaluatingJavaScript:@"enterARelatedTarget"]);
    EXPECT_WK_STREQ("zoneB", [webView stringByEvaluatingJavaScript:@"leaveARelatedTarget"]);
    EXPECT_WK_STREQ("zoneA", [webView stringByEvaluatingJavaScript:@"enterBRelatedTarget"]);
}

#if ENABLE(IPC_TESTING_API)
TEST(DragAndDropTests, PasteboardPathnamesRequireDataAccess)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"IPCTestingAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }

    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:@R"TESTHTML(
        <!DOCTYPE html>
        <body style="width: 100vw; height: 100vh; margin: 0;">
        <script>
        var pathnameCount = -1;

        document.body.addEventListener('dragenter', function(e) {
            e.preventDefault();
        });

        document.body.addEventListener('dragover', function(e) {
            e.preventDefault();
            if (pathnameCount >= 0 || !window.IPC || !window.pasteboardName)
                return;

            try {
                var reply = IPC.sendSyncMessage('UI', 0,
                    IPC.messages.WebPasteboardProxy_GetPasteboardPathnamesForType.name,
                    1000,
                    [
                        {type: 'String', value: window.pasteboardName},
                        {type: 'String', value: 'NSFilenamesPboardType'},
                        {type: 'bool', value: 0}
                    ]);
                if (reply && reply.buffer) {
                    var buf = new Uint8Array(reply.buffer);
                    pathnameCount = buf.length > 32 ? 1 : 0;
                } else {
                    pathnameCount = 0;
                }
            } catch (ex) {
                pathnameCount = -2;
            }
        });

        document.body.addEventListener('drop', function(e) {
            e.preventDefault();
        });
        </script>
        </body>
        )TESTHTML"];

    NSString *tempFile = [NSTemporaryDirectory() stringByAppendingPathComponent:@"test-pasteboard-access.txt"];
    [@"test content" writeToFile:tempFile atomically:YES encoding:NSUTF8StringEncoding error:nil];

    [simulator writeFiles:@[[NSURL fileURLWithPath:tempFile]]];

    NSString *pbName = [simulator externalDragPasteboard].name;
    [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"window.pasteboardName = '%@'", pbName]];

    [simulator runFrom:NSMakePoint(0, 0) to:NSMakePoint(200, 200)];

    auto count = [webView stringByEvaluatingJavaScript:@"pathnameCount"].integerValue;
    EXPECT_GE(count, 0);
    EXPECT_EQ(0, count);

    [[NSFileManager defaultManager] removeItemAtPath:tempFile error:nil];
}
#endif // ENABLE(IPC_TESTING_API)

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
