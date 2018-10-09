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
#import "PlatformUtilities.h"

#if WK_API_ENABLED && ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

static void waitForConditionWithLogging(BOOL(^condition)(), NSTimeInterval loggingTimeout, NSString *message, ...)
{
    NSDate *startTime = [NSDate date];
    BOOL exceededLoggingTimeout = NO;
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        if (condition())
            break;

        if (exceededLoggingTimeout || [[NSDate date] timeIntervalSinceDate:startTime] < loggingTimeout)
            continue;

        va_list args;
        va_start(args, message);
        NSLogv(message, args);
        va_end(args);
        exceededLoggingTimeout = YES;
    }
}

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

    waitForConditionWithLogging([&] () -> BOOL {
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

    waitForConditionWithLogging([&] () -> BOOL {
        return [webView stringByEvaluatingJavaScript:@"imageload.textContent"].boolValue;
    }, 2, @"Expected image to finish loading.");
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"filecount.textContent"].integerValue);
}

TEST(DragAndDropTests, DragImageFileIntoFileUpload)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [webView synchronouslyLoadTestPageNamed:@"image-and-file-upload"];

    NSURL *imageURL = [NSBundle.mainBundle URLForResource:@"apple" withExtension:@"gif" subdirectory:@"TestWebKitAPI.resources"];
    [simulator writeFiles:@[ imageURL ]];
    [simulator runFrom:NSMakePoint(100, 100) to:NSMakePoint(100, 300)];

    waitForConditionWithLogging([&] () -> BOOL {
        return [webView stringByEvaluatingJavaScript:@"imageload.textContent"].boolValue;
    }, 2, @"Expected image to finish loading.");
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"filecount.textContent"].integerValue);
}

#endif // WK_API_ENABLED && ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
