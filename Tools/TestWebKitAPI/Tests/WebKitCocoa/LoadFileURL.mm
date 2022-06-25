/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

TEST(WKWebView, LoadFileWithLoadRequest)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];
}

TEST(WKWebView, LoadTwoFiles)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURL *file = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadFileURL:file allowingReadAccessToURL:file.URLByDeletingLastPathComponent];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get()._resourceDirectoryURL.path, file.URLByDeletingLastPathComponent.path);

    // Load a second file: resource that is in a completely different directory from the above
    file = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURL *targetURL = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/simple2.html" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] createDirectoryAtURL:targetURL.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] removeItemAtURL:targetURL error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:file toURL:targetURL error:nil];

    // If this second load succeeds (e.g. doesn't timeout due to a sandbox violation) the test passes
    [webView loadFileURL:targetURL allowingReadAccessToURL:targetURL.URLByDeletingLastPathComponent];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get()._resourceDirectoryURL.path, targetURL.URLByDeletingLastPathComponent.path);

    [webView _killWebContentProcessAndResetState];
    [webView reload];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get()._resourceDirectoryURL.path, targetURL.URLByDeletingLastPathComponent.path);

    [webView goBack];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get()._resourceDirectoryURL.path, file.URLByDeletingLastPathComponent.path);
}

TEST(WKWebView, LoadRelativeFileURL)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    
    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    
    NSString *path = [NSBundle.mainBundle pathForResource:@"simple" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"];
    NSURL *baseURL = [NSURL fileURLWithPath:path.stringByDeletingLastPathComponent];
    NSURL *fileURL = [NSURL fileURLWithPath:path.lastPathComponent relativeToURL:baseURL];
    EXPECT_NOT_NULL([webView loadFileURL:fileURL allowingReadAccessToURL:fileURL.URLByDeletingLastPathComponent]);
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView _resourceDirectoryURL].path, fileURL.URLByDeletingLastPathComponent.path);
}

TEST(WKWebView, RepeatLoadFileURL)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    
    NSString *path = [NSBundle.mainBundle pathForResource:@"simple" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"];
    NSURL *baseURL = [NSURL fileURLWithPath:path.stringByDeletingLastPathComponent];
    NSURL *fileURL = [NSURL fileURLWithPath:path.lastPathComponent relativeToURL:baseURL];
    EXPECT_NOT_NULL([webView loadFileURL:fileURL allowingReadAccessToURL:fileURL.URLByDeletingLastPathComponent]);
    [delegate waitForDidFinishNavigation];
    NSURL *aboutBlankURL = [NSURL URLWithString:@"about:blank"];
    [webView loadRequest:[NSURLRequest requestWithURL:aboutBlankURL]];
    [delegate waitForDidFinishNavigation];
    EXPECT_NOT_NULL([webView loadFileURL:fileURL allowingReadAccessToURL:fileURL.URLByDeletingLastPathComponent]);
    [delegate waitForDidFinishNavigation];
}
