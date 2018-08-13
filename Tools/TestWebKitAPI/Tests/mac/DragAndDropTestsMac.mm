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

TEST(DragAndDropTests, NumberOfValidItemsForDrop)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[@"file-name"] forType:NSFilenamesPboardType];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400)]);
    TestWKWebView *webView = [simulator webView];
    [simulator setExternalDragPasteboard:pasteboard];

    auto hostWindow = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:0 backing:NSBackingStoreBuffered defer:NO]);
    [hostWindow setFrameOrigin:NSMakePoint(0, 0)];
    [[hostWindow contentView] addSubview:webView];
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

#endif // WK_API_ENABLED && ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
