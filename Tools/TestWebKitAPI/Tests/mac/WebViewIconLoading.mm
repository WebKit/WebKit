
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
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebView.h>
#import <wtf/RetainPtr.h>

static NSString *MainFrameIconKeyPath = @"mainFrameIcon";
static bool messageReceived = false;

static const char defaultFaviconHTML[] =
"<html>" \
"</html>";

static const char customFaviconHTML[] =
"<head>" \
"<link rel=\"icon\" href=\"custom.ico\">" \
"</head>";

static const char* currentMainHTML;
static NSData *mainResourceData()
{
    return [NSData dataWithBytesNoCopy:(void*)currentMainHTML length:strlen(currentMainHTML) freeWhenDone:NO];
}

static NSData *defaultFaviconData()
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:url];
}

static NSData *customFaviconData()
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"large-red-square" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:url];
}

static NSImage *imageFromData(NSData *data)
{
    auto *image = [[NSImage alloc] initWithData:data];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [image setScalesWhenResized:YES];
#pragma clang diagnostic pop
    [image setSize:NSMakeSize(16, 16)];

    return [image autorelease];
}

@interface IconLoadingProtocol : NSURLProtocol
@end

@implementation IconLoadingProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    return [request.URL.scheme isEqual:@"http"];
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

- (void)startLoading
{
    NSData *resourceData = nil;
    RetainPtr<NSURLResponse> response;

    if ([self.request.URL.path hasSuffix:@"main"]) {
        resourceData = mainResourceData();
        response = adoptNS([[NSURLResponse alloc] initWithURL:self.request.URL MIMEType:@"text/html" expectedContentLength:-1 textEncodingName:nil]);
    } else if ([self.request.URL.path hasSuffix:@"favicon.ico"]) {
        resourceData = defaultFaviconData();
        response = adoptNS([[NSURLResponse alloc] initWithURL:self.request.URL MIMEType:@"image/png" expectedContentLength:-1 textEncodingName:nil]);
    } else if ([self.request.URL.path hasSuffix:@"custom.ico"]) {
        resourceData = customFaviconData();
        response = adoptNS([[NSURLResponse alloc] initWithURL:self.request.URL MIMEType:@"image/png" expectedContentLength:-1 textEncodingName:nil]);
    } else
        RELEASE_ASSERT_NOT_REACHED();

    [self.client URLProtocol:self didReceiveResponse:response.get() cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:resourceData];
    [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading
{
}

@end

@interface IconLoadingFrameLoadDelegate : NSObject <WebFrameLoadDelegate> {
@public
    RetainPtr<NSImage> receivedIcon;
}
@end

@implementation IconLoadingFrameLoadDelegate

- (void)webView:(WebView *)sender didReceiveIcon:(NSImage *)image forFrame:(WebFrame *)frame
{
    receivedIcon = image;
    messageReceived = true;
}

@end

@interface MainFrameIconKVO : NSObject {
@public
    RetainPtr<NSImage> oldImage;
    RetainPtr<NSImage> expectedImage;
}
@end

@implementation MainFrameIconKVO

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    ASSERT([keyPath isEqualToString:MainFrameIconKeyPath]);
    ASSERT([[object class] isEqual:[WebView class]]);

    EXPECT_TRUE([[[change objectForKey:NSKeyValueChangeOldKey] TIFFRepresentation] isEqual:[oldImage TIFFRepresentation]]);
    EXPECT_TRUE([[[change objectForKey:NSKeyValueChangeNewKey] TIFFRepresentation] isEqual:[expectedImage TIFFRepresentation]]);
    EXPECT_TRUE([[((WebView *)object).mainFrameIcon TIFFRepresentation] isEqual:[expectedImage TIFFRepresentation]]);
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, IconLoadingDelegateDefaultFirst)
{
    [NSURLProtocol registerClass:[IconLoadingProtocol class]];

    currentMainHTML = defaultFaviconHTML;
    @autoreleasepool {
        auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        auto frameLoadDelegate = adoptNS([[IconLoadingFrameLoadDelegate alloc] init]);

        auto kvo = adoptNS([[MainFrameIconKVO alloc] init]);
        kvo->oldImage = webView.get().mainFrameIcon;
        kvo->expectedImage = imageFromData(defaultFaviconData());
        [webView.get() addObserver:kvo.get() forKeyPath:MainFrameIconKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld context:nil];

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        [webView.get().mainFrame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://testserver/1/main"]]];

        Util::run(&messageReceived);

        EXPECT_TRUE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(defaultFaviconData()) TIFFRepresentation]]);
        EXPECT_TRUE([[webView.get().mainFrameIcon TIFFRepresentation] isEqual:[imageFromData(defaultFaviconData()) TIFFRepresentation]]);
        EXPECT_FALSE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(customFaviconData()) TIFFRepresentation]]);

        // Now load the next page which should have a custom icon and make sure things are as expected.
        messageReceived = false;
        kvo->oldImage = imageFromData(defaultFaviconData());
        kvo->expectedImage = imageFromData(customFaviconData());
        currentMainHTML = customFaviconHTML;
        [webView.get().mainFrame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://testserver/2/main"]]];

        Util::run(&messageReceived);

        EXPECT_TRUE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(customFaviconData()) TIFFRepresentation]]);
        EXPECT_TRUE([[webView.get().mainFrameIcon TIFFRepresentation] isEqual:[imageFromData(customFaviconData()) TIFFRepresentation]]);
        EXPECT_FALSE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(defaultFaviconData()) TIFFRepresentation]]);

    }

    [NSURLProtocol unregisterClass:[IconLoadingProtocol class]];
}

TEST(WebKitLegacy, IconLoadingDelegateCustomFirst)
{
    [NSURLProtocol registerClass:[IconLoadingProtocol class]];

    currentMainHTML = customFaviconHTML;
    @autoreleasepool {
        auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        auto frameLoadDelegate = adoptNS([[IconLoadingFrameLoadDelegate alloc] init]);

        auto kvo = adoptNS([[MainFrameIconKVO alloc] init]);
        kvo->oldImage = webView.get().mainFrameIcon;
        kvo->expectedImage = imageFromData(customFaviconData());
        [webView.get() addObserver:kvo.get() forKeyPath:MainFrameIconKeyPath options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld context:nil];

        webView.get().frameLoadDelegate = frameLoadDelegate.get();
        [webView.get().mainFrame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://testserver/1/main"]]];

        Util::run(&messageReceived);

        EXPECT_TRUE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(customFaviconData()) TIFFRepresentation]]);
        EXPECT_TRUE([[webView.get().mainFrameIcon TIFFRepresentation] isEqual:[imageFromData(customFaviconData()) TIFFRepresentation]]);
        EXPECT_FALSE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(defaultFaviconData()) TIFFRepresentation]]);

        // Now load the next page which should have the default icon and make sure things are as expected.
        messageReceived = false;
        kvo->oldImage = imageFromData(customFaviconData());
        kvo->expectedImage = imageFromData(defaultFaviconData());
        currentMainHTML = defaultFaviconHTML;
        [webView.get().mainFrame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://testserver/2/main"]]];

        Util::run(&messageReceived);

        EXPECT_TRUE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(defaultFaviconData()) TIFFRepresentation]]);
        EXPECT_TRUE([[webView.get().mainFrameIcon TIFFRepresentation] isEqual:[imageFromData(defaultFaviconData()) TIFFRepresentation]]);
        EXPECT_FALSE([[frameLoadDelegate->receivedIcon.get() TIFFRepresentation] isEqual:[imageFromData(customFaviconData()) TIFFRepresentation]]);
    }

    [NSURLProtocol unregisterClass:[IconLoadingProtocol class]];
}

} // namespace TestWebKitAPI
