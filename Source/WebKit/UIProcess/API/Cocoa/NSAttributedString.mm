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

#import "NSAttributedStringPrivate.h"

#import "WKErrorInternal.h"
#import "WKNavigationActionPrivate.h"
#import "WKNavigationDelegate.h"
#import "WKPreferences.h"
#import "WKProcessPoolPrivate.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewPrivate.h"
#import "WKWebsiteDataStore.h"
#import "_WKProcessPoolConfiguration.h"
#import <wtf/Deque.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKitSPI.h>
#endif

NSString * const NSReadAccessURLDocumentOption = @"ReadAccessURL";

constexpr NSRect webViewRect = {{0, 0}, {800, 600}};
constexpr NSTimeInterval defaultTimeoutInterval = 60;
constexpr NSTimeInterval purgeWebViewCacheDelay = 15;
constexpr NSUInteger maximumWebViewCacheSize = 3;

@interface _WKAttributedStringNavigationDelegate : NSObject <WKNavigationDelegate>

@property (nonatomic, copy) void (^webContentProcessDidTerminate)(WKWebView *);
@property (nonatomic, copy) void (^decidePolicyForNavigationAction)(WKNavigationAction *, void (^)(WKNavigationActionPolicy));
@property (nonatomic, copy) void (^didFailProvisionalNavigation)(WKWebView *, WKNavigation *, NSError *);
@property (nonatomic, copy) void (^didFailNavigation)(WKWebView *, WKNavigation *, NSError *);
@property (nonatomic, copy) void (^didFinishNavigation)(WKWebView *, WKNavigation *);

@end

@implementation _WKAttributedStringNavigationDelegate

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (_webContentProcessDidTerminate)
        _webContentProcessDidTerminate(webView);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (_decidePolicyForNavigationAction)
        return _decidePolicyForNavigationAction(navigationAction, decisionHandler);
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (_didFailProvisionalNavigation)
        _didFailProvisionalNavigation(webView, navigation, error);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (_didFailNavigation)
        _didFailNavigation(webView, navigation, error);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    if (_didFinishNavigation)
        _didFinishNavigation(webView, navigation);
}

@end

@interface _WKAttributedStringWebViewCache : NSObject

+ (RetainPtr<WKWebView>)retrieveOrCreateWebView;
+ (void)cacheWebView:(WKWebView *)webView;

@end

@implementation _WKAttributedStringWebViewCache

+ (NSMutableArray<WKWebView *> *)cache
{
    static auto* cache = [[NSMutableArray alloc] initWithCapacity:maximumWebViewCacheSize];
    return cache;
}

static WKWebViewConfiguration *configuration;

+ (WKWebViewConfiguration *)configuration
{
    if (!configuration) {
        configuration = [[WKWebViewConfiguration alloc] init];
        configuration.processPool = [[[WKProcessPool alloc] init] autorelease];
        configuration.websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
        configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAll;
        configuration._allowsJavaScriptMarkup = NO;
        configuration._allowsMetaRefresh = NO;
        configuration._attachmentElementEnabled = YES;
        configuration._invisibleAutoplayNotPermitted = YES;
        configuration._mediaDataLoadsAutomatically = NO;
        configuration._needsStorageAccessFromFileURLsQuirk = NO;
#if PLATFORM(IOS_FAMILY)
        configuration.allowsInlineMediaPlayback = NO;
        configuration._alwaysRunsAtForegroundPriority = YES;
#endif
    }

    return configuration;
}

+ (void)clearConfiguration
{
    [configuration release];
    configuration = nil;
}

+ (RetainPtr<WKWebView>)retrieveOrCreateWebView
{
    [self resetPurgeDelay];

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        auto& memoryPressureHandler = MemoryPressureHandler::singleton();
        memoryPressureHandler.setLowMemoryHandler([self] (Critical, Synchronous) {
            [self purgeAllWebViews];
        });
    });

    auto* cache = self.cache;
    if (cache.count) {
        RetainPtr<WKWebView> webView = cache.lastObject;
        [cache removeLastObject];
        return webView;
    }

    return adoptNS([[WKWebView alloc] initWithFrame:webViewRect configuration:self.configuration]);
}

+ (void)cacheWebView:(WKWebView *)webView
{
    auto* cache = self.cache;
    if (cache.count >= maximumWebViewCacheSize)
        return;

    [cache addObject:webView];
}

+ (void)resetPurgeDelay
{
    static const auto purgeSelector = @selector(purgeSingleWebView);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:purgeSelector object:nil];
    [self performSelector:purgeSelector withObject:nil afterDelay:purgeWebViewCacheDelay];
}

+ (void)purgeSingleWebView
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:_cmd object:nil];

    auto* cache = self.cache;
    if (!cache.count)
        return;

    [cache.lastObject _close];
    [cache removeLastObject];

    if (!cache.count) {
        [self clearConfiguration];
        return;
    }

    // Keep going until every view is removed, or the delay is reset.
    [self performSelector:_cmd withObject:nil afterDelay:purgeWebViewCacheDelay];
}

+ (void)purgeAllWebViews
{
    auto* cache = self.cache;
    if (!cache.count)
        return;

    [cache makeObjectsPerformSelector:@selector(_close)];
    [cache removeAllObjects];

    [self clearConfiguration];
}

@end

@implementation NSAttributedString (WKPrivate)

+ (void)_loadFromHTMLWithOptions:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options contentLoader:(WKNavigation *(^)(WKWebView *))loadWebContent completionHandler:(NSAttributedStringCompletionHandler)completionHandler
{
    if (options[NSWebPreferencesDocumentOption])
        [NSException raise:NSInvalidArgumentException format:@"NSWebPreferencesDocumentOption option is not supported"];
    if (options[NSWebResourceLoadDelegateDocumentOption])
        [NSException raise:NSInvalidArgumentException format:@"NSWebResourceLoadDelegateDocumentOption option is not supported"];
    if (options[@"WebPolicyDelegate"])
        [NSException raise:NSInvalidArgumentException format:@"WebPolicyDelegate option is not supported"];

    auto runConversion = ^{
        __block auto finished = NO;
        __block auto webView = [_WKAttributedStringWebViewCache retrieveOrCreateWebView];
        __block auto navigationDelegate = adoptNS([[_WKAttributedStringNavigationDelegate alloc] init]);

        __block RetainPtr<WKNavigation> contentNavigation;

        webView.get().navigationDelegate = navigationDelegate.get();

        if (auto* textZoomFactor = dynamic_objc_cast<NSNumber>(options[NSTextSizeMultiplierDocumentOption]))
            webView.get()._textZoomFactor = textZoomFactor.doubleValue;
        else
            webView.get()._textZoomFactor = 1;

        auto finish = ^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
            if (finished)
                return;

            finished = YES;

            webView.get().navigationDelegate = nil;
            navigationDelegate = nil;
            contentNavigation = nil;

            if (!error)
                [_WKAttributedStringWebViewCache cacheWebView:webView.get()];
            webView = nil;

            // Make the string be an instance of the receiver class.
            if (attributedString && self != attributedString.class)
                attributedString = [[[self alloc] initWithAttributedString:attributedString] autorelease];

            // Make the document attributes immutable.
            if ([attributes isKindOfClass:NSMutableDictionary.class])
                attributes = [[[NSDictionary alloc] initWithDictionary:attributes] autorelease];

            completionHandler(attributedString, attributes, error);
        };

        auto cancel = ^(WKErrorCode errorCode, NSError* underlyingError) {
            finish(nil, nil, createNSError(errorCode, underlyingError).get());
        };

        navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
            if ([action._mainFrameNavigation isEqual:contentNavigation.get()])
                return decisionHandler(WKNavigationActionPolicyAllow);
            decisionHandler(WKNavigationActionPolicyCancel);
        };

        navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *) {
            cancel(WKErrorWebContentProcessTerminated, nil);
        };

        navigationDelegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
            cancel(WKErrorAttributedStringContentFailedToLoad, error);
        };

        navigationDelegate.get().didFailNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
            cancel(WKErrorAttributedStringContentFailedToLoad, error);
        };

        navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
            if (finished)
                return;

            navigationDelegate = nil;

            [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *documentAttributes, NSError *error) {
                if (error)
                    return cancel(WKErrorUnknown, error);
                finish([[attributedString retain] autorelease], [[documentAttributes retain] autorelease], nil);
            }];
        };

        auto timeoutInterval = defaultTimeoutInterval;
        if (auto* timeoutOption = dynamic_objc_cast<NSNumber>(options[NSTimeoutDocumentOption])) {
            if (timeoutOption.doubleValue >= 0)
                timeoutInterval = timeoutOption.doubleValue;
        }

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeoutInterval * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            if (finished)
                return;
            cancel(WKErrorAttributedStringContentLoadTimedOut, nil);
        });

        contentNavigation = loadWebContent(webView.get());

        ASSERT(contentNavigation);
        ASSERT(webView.get().loading);
    };

    if ([NSThread isMainThread])
        runConversion();
    else
        dispatch_async(dispatch_get_main_queue(), runConversion);
}

@end

@implementation NSAttributedString (NSAttributedStringWebKitAdditions)

+ (void)loadFromHTMLWithRequest:(NSURLRequest *)request options:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options completionHandler:(NSAttributedStringCompletionHandler)completionHandler
{
    [self _loadFromHTMLWithOptions:options contentLoader:^WKNavigation *(WKWebView *webView) {
        return [webView loadRequest:request];
    } completionHandler:completionHandler];
}

+ (void)loadFromHTMLWithFileURL:(NSURL *)fileURL options:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options completionHandler:(NSAttributedStringCompletionHandler)completionHandler
{
    [self _loadFromHTMLWithOptions:options contentLoader:^WKNavigation *(WKWebView *webView) {
        auto* readAccessURL = dynamic_objc_cast<NSURL>(options[NSReadAccessURLDocumentOption]);
        return [webView loadFileURL:fileURL allowingReadAccessToURL:readAccessURL];
    } completionHandler:completionHandler];
}

+ (void)loadFromHTMLWithString:(NSString *)string options:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options completionHandler:(NSAttributedStringCompletionHandler)completionHandler
{
    [self _loadFromHTMLWithOptions:options contentLoader:^WKNavigation *(WKWebView *webView) {
        auto* baseURL = dynamic_objc_cast<NSURL>(options[NSBaseURLDocumentOption]);
        return [webView loadHTMLString:string baseURL:baseURL];
    } completionHandler:completionHandler];
}

+ (void)loadFromHTMLWithData:(NSData *)data options:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options completionHandler:(NSAttributedStringCompletionHandler)completionHandler
{
    [self _loadFromHTMLWithOptions:options contentLoader:^WKNavigation *(WKWebView *webView) {
        auto* textEncodingName = dynamic_objc_cast<NSString>(options[NSTextEncodingNameDocumentOption]);
        auto characterEncoding = static_cast<NSStringEncoding>(dynamic_objc_cast<NSNumber>(options[NSCharacterEncodingDocumentOption]).unsignedIntegerValue);
        auto* baseURL = dynamic_objc_cast<NSURL>(options[NSBaseURLDocumentOption]);

        if (characterEncoding && !textEncodingName) {
            auto stringEncoding = CFStringConvertNSStringEncodingToEncoding(characterEncoding);
            if (stringEncoding != kCFStringEncodingInvalidId)
                textEncodingName = (__bridge NSString *)CFStringConvertEncodingToIANACharSetName(stringEncoding);
        }

        return [webView loadData:data MIMEType:@"text/html" characterEncodingName:textEncodingName baseURL:baseURL];
    } completionHandler:completionHandler];
}

@end
