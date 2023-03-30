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

#import "DragAndDropSimulator.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestDraggingInfo.h"
#import <WebCore/PasteboardCustomData.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

TEST(DragAndDropTests, NumberOfValidItemsForDrop)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[@"file-name"] forType:NSFilenamesPboardType];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
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

TEST(DragAndDropTests, DropUserSelectAllUserDragElementDiv)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 320, 500)]);

    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-user-select-user-drag"];

    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    EXPECT_WK_STREQ(@"Text", [webView stringByEvaluatingJavaScript:@"document.getElementById(\"editor\").textContent"]);
}

#if ENABLE(INPUT_TYPE_COLOR)
TEST(DragAndDropTests, DropColor)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[NSColorPboardType] owner:nil];
    [[NSColor colorWithRed:1 green:0 blue:0 alpha:1] writeToPasteboard:pasteboard];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [simulator setExternalDragPasteboard:pasteboard];

    [webView synchronouslyLoadTestPageNamed:@"color-drop"];
    [simulator runFrom:NSMakePoint(0, 0) to:NSMakePoint(50, 50)];
    EXPECT_WK_STREQ(@"#ff0000", [webView stringByEvaluatingJavaScript:@"document.querySelector(\"input\").value"]);
}
#endif // ENABLE(INPUT_TYPE_COLOR)

TEST(DragAndDropTests, DragImageElementIntoFileUpload)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
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
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-file-upload"];

    NSURL *imageURL = [NSBundle.mainBundle URLForResource:@"apple" withExtension:@"gif" subdirectory:@"TestWebKitAPI.resources"];
    [simulator writePromisedFiles:@[ imageURL ]];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
        return [webView stringByEvaluatingJavaScript:@"imageload.textContent"].boolValue;
    }, 2, @"Expected image to finish loading.");
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"filecount.textContent"].integerValue);

    TestDraggingInfo *draggingInfo = [simulator draggingInfo];
    NSArray<NSFilePromiseReceiver *> *filePromiseReceivers = [draggingInfo filePromiseReceivers];
    EXPECT_EQ(1UL, [filePromiseReceivers count]);
    NSFilePromiseReceiver *filePromiseReceiver = filePromiseReceivers.firstObject;
    EXPECT_EQ(1UL, [filePromiseReceiver.fileTypes count]);
    EXPECT_WK_STREQ((__bridge NSString *)kUTTypeGIF, filePromiseReceiver.fileTypes.firstObject);
}

TEST(DragAndDropTests, ReadURLWhenDroppingPromisedWebLoc)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
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
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-file-upload"];

    NSURL *imageURL = [NSBundle.mainBundle URLForResource:@"apple" withExtension:@"gif" subdirectory:@"TestWebKitAPI.resources"];
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

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];

    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    auto pid = [webView _webProcessIdentifier];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    EXPECT_EQ(pid, [webView _webProcessIdentifier]);
}

TEST(DragAndDropTests, ProvideImageDataForMultiplePasteboards)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
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
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setLargeImageAsyncDecodingEnabled:NO];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    TestWKWebView *webView = [simulator webView];

    auto uniquePasteboard = retainPtr(NSPasteboard.pasteboardWithUniqueName);

    [webView synchronouslyLoadHTMLString:@"<img src='sunset-in-cupertino-600px.jpg'></img>"];
    [simulator runFrom:NSMakePoint(25, 25) to:NSMakePoint(300, 300)];
    [webView pasteboard:uniquePasteboard.get() provideDataForType:(__bridge NSString *)kUTTypeJPEG];
    EXPECT_GT([uniquePasteboard dataForType:(__bridge NSString *)kUTTypeJPEG].length, 0u);

    [webView synchronouslyLoadHTMLString:@"<img src='icon.png'></img>"];
    [simulator runFrom:NSMakePoint(25, 25) to:NSMakePoint(300, 300)];
    [webView pasteboard:uniquePasteboard.get() provideDataForType:(__bridge NSString *)kUTTypePNG];
    EXPECT_GT([uniquePasteboard dataForType:(__bridge NSString *)kUTTypePNG].length, 0u);

    [webView synchronouslyLoadHTMLString:@"<img src='apple.gif'></img>"];
    [simulator runFrom:NSMakePoint(25, 25) to:NSMakePoint(300, 300)];
    [webView pasteboard:uniquePasteboard.get() provideDataForType:(__bridge NSString *)kUTTypeGIF];
    EXPECT_GT([uniquePasteboard dataForType:(__bridge NSString *)kUTTypeGIF].length, 0u);
}

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
