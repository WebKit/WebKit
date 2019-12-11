/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTask.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKIconLoadingDelegate.h>
#import <WebKit/_WKLinkIconParameters.h>
#import <wtf/RetainPtr.h>

static bool doneWithIcons;
static bool alreadyProvidedIconData;

@interface IconLoadingDelegate : NSObject <_WKIconLoadingDelegate> {
    @public
    RetainPtr<NSData> receivedFaviconData;
    bool receivedFaviconDataCallback;
    bool shouldSaveCallback;
    bool didSaveCallback;
    void (^savedCallback)(void (^)(NSData*));

    RetainPtr<_WKLinkIconParameters> favicon;
    RetainPtr<_WKLinkIconParameters> touch;
    RetainPtr<_WKLinkIconParameters> touchPrecomposed;
}
@end

@implementation IconLoadingDelegate

- (void)webView:(WKWebView *)webView shouldLoadIconWithParameters:(_WKLinkIconParameters *)parameters completionHandler:(void (^)(void (^)(NSData*)))completionHandler
{
    switch (parameters.iconType) {
    case WKLinkIconTypeFavicon:
        favicon = parameters;
        EXPECT_TRUE([[parameters.url absoluteString] isEqual:@"testing:///favicon.ico"]);
        break;
    case WKLinkIconTypeTouchIcon:
        touch = parameters;
        break;
    case WKLinkIconTypeTouchPrecomposedIcon:
        touchPrecomposed = parameters;
    }

    if (favicon && touch && touchPrecomposed)
        doneWithIcons = true;

    if (parameters.iconType == WKLinkIconTypeFavicon) {
        if (shouldSaveCallback) {
            savedCallback = [completionHandler retain];
            didSaveCallback = true;
            return;
        }

        completionHandler([self](NSData *iconData) {
            receivedFaviconData = iconData;
            receivedFaviconDataCallback = true;
        });
    } else
        completionHandler(nil);
}

@end

@interface IconLoadingSchemeHandler : NSObject <WKURLSchemeHandler> {
    @public
    bool shouldIgnoreFaviconTask;
    bool receivedFaviconTask;
    bool faviconTaskStopped;
}
- (instancetype)initWithData:(NSData *)data;
- (void)setFaviconData:(NSData *)data;
@end

@implementation IconLoadingSchemeHandler {
    RetainPtr<NSData> mainResourceData;
    RetainPtr<NSData> faviconData;
}

- (instancetype)initWithData:(NSData *)data
{
    self = [super init];
    if (!self)
        return nil;

    mainResourceData = data;

    return self;
}

- (void)setFaviconData:(NSData *)data
{
    faviconData = data;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    RetainPtr<NSURLResponse> response;
    NSData *data = nil;

    if ([[task.request.URL absoluteString] isEqual:@"testing:///favicon.ico"]) {
        EXPECT_FALSE(alreadyProvidedIconData);
        if (shouldIgnoreFaviconTask) {
            receivedFaviconTask = true;
            return;
        }
        response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"image/png" expectedContentLength:1 textEncodingName:nil]);
        data = faviconData.get();
        alreadyProvidedIconData = true;
    } else {
        response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:1 textEncodingName:nil]);
        data = mainResourceData.get();
    }

    [task didReceiveResponse:response.get()];
    [task didReceiveData:data];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if ([[task.request.URL absoluteString] isEqual:@"testing:///favicon.ico"])
        faviconTaskStopped = true;
}

@end

static const char mainBytes[] =
"<head>" \
"<link rel=\"apple-touch-icon\" sizes=\"57x57\" non-standard-attribute href=\"http://example.com/my-apple-touch-icon.png\">" \
"<link rel=\"apple-touch-icon-precomposed\" sizes=\"57x57\" href=\"http://example.com/my-apple-touch-icon-precomposed.png\">" \
"</head>";

TEST(IconLoading, DefaultFavicon)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<IconLoadingSchemeHandler> handler = adoptNS([[IconLoadingSchemeHandler alloc] initWithData:[NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes) freeWhenDone:NO]]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<IconLoadingDelegate> iconDelegate = adoptNS([[IconLoadingDelegate alloc] init]);

    webView.get()._iconLoadingDelegate = iconDelegate.get();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&doneWithIcons);
    TestWebKitAPI::Util::run(&iconDelegate.get()->receivedFaviconDataCallback);

    auto* faviconParameters = iconDelegate.get()->favicon.get();
    EXPECT_WK_STREQ("testing:///favicon.ico", faviconParameters.url.absoluteString);
    EXPECT_EQ(WKLinkIconTypeFavicon, faviconParameters.iconType);
    EXPECT_EQ(static_cast<unsigned long>(0), faviconParameters.attributes.count);

    auto* touchParameters = iconDelegate.get()->touch.get();
    EXPECT_WK_STREQ("http://example.com/my-apple-touch-icon.png", touchParameters.url.absoluteString);
    EXPECT_EQ(WKLinkIconTypeTouchIcon, touchParameters.iconType);
    EXPECT_EQ(static_cast<unsigned long>(4), touchParameters.attributes.count);
    EXPECT_WK_STREQ("apple-touch-icon", [touchParameters.attributes valueForKey:@"rel"]);
    EXPECT_WK_STREQ("57x57", [touchParameters.attributes valueForKey:@"sizes"]);
    EXPECT_WK_STREQ("http://example.com/my-apple-touch-icon.png", [touchParameters.attributes valueForKey:@"href"]);
    EXPECT_TRUE([touchParameters.attributes.allKeys containsObject:@"non-standard-attribute"]);
    EXPECT_FALSE([touchParameters.attributes.allKeys containsObject:@"nonexistent-attribute"]);
}

static const char mainBytes2[] =
"Oh, hello there!";

TEST(IconLoading, AlreadyCachedIcon)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    NSData *mainData = [NSData dataWithBytesNoCopy:(void*)mainBytes2 length:sizeof(mainBytes2) freeWhenDone:NO];
    RetainPtr<IconLoadingSchemeHandler> handler = adoptNS([[IconLoadingSchemeHandler alloc] initWithData:mainData]);

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"large-red-square-image" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSData> iconDataFromDisk = [NSData dataWithContentsOfURL:url];
    [handler.get() setFaviconData:iconDataFromDisk.get()];

    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<IconLoadingDelegate> iconDelegate = adoptNS([[IconLoadingDelegate alloc] init]);

    webView.get()._iconLoadingDelegate = iconDelegate.get();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&iconDelegate.get()->receivedFaviconDataCallback);

    EXPECT_TRUE([iconDataFromDisk.get() isEqual:iconDelegate.get()->receivedFaviconData.get()]);

    iconDelegate.get()->receivedFaviconDataCallback = false;
    iconDelegate.get()->receivedFaviconData = nil;

    // Load another main resource that results in the same icon being loaded (which should come from the memory cache).
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main2"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&iconDelegate.get()->receivedFaviconDataCallback);

    EXPECT_TRUE([iconDataFromDisk.get() isEqual:iconDelegate.get()->receivedFaviconData.get()]);
}

TEST(IconLoading, IconLoadCancelledCallback)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    NSData *mainData = [NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes) freeWhenDone:NO];
    RetainPtr<IconLoadingSchemeHandler> handler = adoptNS([[IconLoadingSchemeHandler alloc] initWithData:mainData]);
    handler.get()->shouldIgnoreFaviconTask = true;
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<IconLoadingDelegate> iconDelegate = adoptNS([[IconLoadingDelegate alloc] init]);
    webView.get()._iconLoadingDelegate = iconDelegate.get();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&handler.get()->receivedFaviconTask);

    // Our scheme handler never replies to the favicon task, so our icon delegate load callback is still pending.
    // Stop the documentloader's loading and verify the icon delegate callback is called.
    [webView stopLoading];

    // Wait until the data callback is called, *and* the task is stopped
    TestWebKitAPI::Util::run(&handler.get()->faviconTaskStopped);
    TestWebKitAPI::Util::run(&iconDelegate.get()->receivedFaviconDataCallback);

    EXPECT_EQ(iconDelegate.get()->receivedFaviconData.get().length, (unsigned long)0);
}

TEST(IconLoading, IconLoadCancelledCallback2)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    NSData *mainData = [NSData dataWithBytesNoCopy:(void*)mainBytes length:sizeof(mainBytes) freeWhenDone:NO];
    RetainPtr<IconLoadingSchemeHandler> handler = adoptNS([[IconLoadingSchemeHandler alloc] initWithData:mainData]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<IconLoadingDelegate> iconDelegate = adoptNS([[IconLoadingDelegate alloc] init]);
    iconDelegate.get()->shouldSaveCallback = true;
    webView.get()._iconLoadingDelegate = iconDelegate.get();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:///main"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&iconDelegate.get()->didSaveCallback);

    // Our scheme handler never replies to the favicon task, so our icon delegate load callback is still pending.
    // Stop the documentloader's loading and verify the icon delegate callback is called.
    [webView stopLoading];


    // Even though loading has already been stopped (and therefore IconLoaders were cancelled),
    // we should still get the callback.
    static bool iconCallbackCalled;
    iconDelegate.get()->savedCallback([iconCallbackCalled = &iconCallbackCalled](NSData *data) {
        EXPECT_EQ(data.length, (unsigned long)0);

        *iconCallbackCalled = true;
    });

    TestWebKitAPI::Util::run(&iconCallbackCalled);
}
