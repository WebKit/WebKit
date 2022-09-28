/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/Deque.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKitSPI.h>
#endif

NSString * const NSReadAccessURLDocumentOption = @"ReadAccessURL";
NSString * const _WKReadAccessFileURLsOption = @"_WKReadAccessFileURLsOption";

constexpr NSRect webViewRect = {{0, 0}, {800, 600}};
constexpr NSTimeInterval defaultTimeoutInterval = 60;
constexpr NSTimeInterval purgeWebViewCacheDelay = 15;
constexpr NSUInteger maximumWebViewCacheSize = 3;
constexpr NSUInteger maximumReadOnlyAccessPaths = 2;

@interface _WKAttributedStringNavigationDelegate : NSObject <WKNavigationDelegate>

@property (nonatomic, copy) void (^webContentProcessDidTerminate)(WKWebView *);
@property (nonatomic, copy) void (^decidePolicyForNavigationAction)(WKNavigationAction *, void (^)(WKNavigationActionPolicy));
@property (nonatomic, copy) void (^didFailProvisionalNavigation)(WKWebView *, WKNavigation *, NSError *);
@property (nonatomic, copy) void (^didFailNavigation)(WKWebView *, WKNavigation *, NSError *);
@property (nonatomic, copy) void (^didFinishNavigation)(WKWebView *, WKNavigation *);

@end

@implementation _WKAttributedStringNavigationDelegate

- (void)dealloc
{
    [_webContentProcessDidTerminate release];
    _webContentProcessDidTerminate = nil;
    [_decidePolicyForNavigationAction release];
    _decidePolicyForNavigationAction = nil;
    [_didFailProvisionalNavigation release];
    _didFailProvisionalNavigation = nil;
    [_didFailNavigation release];
    _didFailNavigation = nil;
    [_didFinishNavigation release];
    _didFinishNavigation = nil;

    [super dealloc];
}

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
+ (void)maybeConsumeBundlePaths:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options;
+ (void)validateEntry:(id)maybeFileURL;

@end

@implementation _WKAttributedStringWebViewCache

+ (NSMutableArray<WKWebView *> *)cache
{
    static auto* cache = [[NSMutableArray alloc] initWithCapacity:maximumWebViewCacheSize];
    return cache;
}

static RetainPtr<WKWebViewConfiguration>& globalConfiguration()
{
    static NeverDestroyed<RetainPtr<WKWebViewConfiguration>> configuration;
    return configuration;
}

static NSMutableArray<NSURL *> *readOnlyAccessPaths()
{
    static NeverDestroyed<RetainPtr<NSMutableArray>> readOnlyAccessPaths = adoptNS([[NSMutableArray alloc] initWithCapacity:maximumReadOnlyAccessPaths]);
    return readOnlyAccessPaths.get().get();
}

+ (WKWebViewConfiguration *)configuration
{
    auto& configuration = globalConfiguration();
    if (!configuration) {
        configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        RetainPtr<WKProcessPool> processPool;
        if (readOnlyAccessPaths().count) {
            RELEASE_ASSERT(readOnlyAccessPaths().count <= 2);
            auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
            [processPoolConfiguration setAdditionalReadAccessAllowedURLs:readOnlyAccessPaths()];
            processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
        } else
            processPool = adoptNS([[WKProcessPool alloc] init]).get();

        [configuration setProcessPool:processPool.get()];
        [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
        [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeAll];
        [configuration _setAllowsJavaScriptMarkup:NO];
        [configuration _setAllowsMetaRefresh:NO];
        [configuration _setAttachmentElementEnabled:YES];
        [configuration preferences]._extensibleSSOEnabled = NO;
        [configuration _setInvisibleAutoplayNotPermitted:YES];
        [configuration _setMediaDataLoadsAutomatically:NO];
        [configuration _setNeedsStorageAccessFromFileURLsQuirk:NO];
#if PLATFORM(IOS_FAMILY)
        [configuration setAllowsInlineMediaPlayback:NO];
        [configuration _setClientNavigationsRunAtForegroundPriority:YES];
#endif

        [configuration preferences]._defaultFontSize = 12;
    }

    return configuration.get();
}

+ (void)clearConfiguration
{
    globalConfiguration() = nil;
}

+ (void)clearConfigurationAndRaiseExceptionIfNecessary:(NSString *)errorMessage
{
    if (!errorMessage)
        return;

    if (readOnlyAccessPaths().count) {
        [readOnlyAccessPaths() removeAllObjects];
        [self clearConfiguration];
    }
    [NSException raise:NSInvalidArgumentException format:@"%@", errorMessage];
}

+ (void)validateEntry:(id)maybeFileURL
{
    NSString *errorMessage = nil;
    auto* url = dynamic_objc_cast<NSURL>(maybeFileURL);
    if (!url)
        errorMessage = @"The NSArray associated with _WKReadAccessFileURLsOption may only contain NSURL objects.";
    else if (!url.isFileURL)
        errorMessage = @"_WKReadAccessFileURLsOption requires its NSURL objects to be file URLs.";

    [self clearConfigurationAndRaiseExceptionIfNecessary:errorMessage];
}

+ (void)maybeConsumeBundlePaths:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options
{
    id maybeReadAccessFileURLs = options[_WKReadAccessFileURLsOption];
    if (!maybeReadAccessFileURLs)
        return;

    NSString *errorMessage = nil;
    auto* readAccessFileURLs = dynamic_objc_cast<NSArray<NSURL *>>(maybeReadAccessFileURLs);
    if (!readAccessFileURLs)
        errorMessage = @"The value associated with _WKReadAccessFileURLsOption must be an NSArray of NSURL objects.";
    else if (readAccessFileURLs.count > maximumReadOnlyAccessPaths)
        errorMessage = @"_WKReadAccessFileURLsOption may have at most two additional directories.";

    [self clearConfigurationAndRaiseExceptionIfNecessary:errorMessage];

    for (id fileURL in readAccessFileURLs)
        [self validateEntry:fileURL];

    if ([readAccessFileURLs isEqualToArray:readOnlyAccessPaths()])
        return;

    if (readAccessFileURLs)
        [readOnlyAccessPaths() setArray:readAccessFileURLs];
    else
        [readOnlyAccessPaths() removeAllObjects];
    [self clearConfiguration];
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

        [_WKAttributedStringWebViewCache maybeConsumeBundlePaths:options];
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
            RetainPtr<NSAttributedString> newAttributedString;
            if (attributedString && self != attributedString.class) {
                newAttributedString = adoptNS([[self alloc] initWithAttributedString:attributedString]);
                attributedString = newAttributedString.get();
            }

            // Make the document attributes immutable.
            RetainPtr<NSDictionary<NSAttributedStringDocumentAttributeKey, id>> newAttributes;
            if ([attributes isKindOfClass:NSMutableDictionary.class]) {
                newAttributes = adoptNS([[NSDictionary alloc] initWithDictionary:attributes]);
                attributes = newAttributes.get();
            }

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
                finish(attributedString, documentAttributes, nil);
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
