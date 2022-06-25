/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#if USE(QUICK_LOOK)

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import <CoreServices/CoreServices.h>
#import <WebCore/WebCoreThread.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebUIKitSupport.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/_WKDownload.h>
#import <WebKit/_WKDownloadDelegate.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

using namespace TestWebKitAPI;

static bool downloadIsDone;

static NSString * const pagesDocumentPreviewMIMEType = @"application/pdf";

static RetainPtr<NSURL> pagesDocumentURL()
{
    static NeverDestroyed<RetainPtr<NSURL>> pagesDocumentURL = [NSBundle.mainBundle URLForResource:@"pages" withExtension:@"pages" subdirectory:@"TestWebKitAPI.resources"];
    return pagesDocumentURL;
}

@interface QuickLookDelegate : NSObject <WKNavigationDelegatePrivate, _WKDownloadDelegate>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithExpectedFileURL:(NSURL *)fileURL responsePolicy:(WKNavigationResponsePolicy)responsePolicy;
- (instancetype)initWithExpectedFileURL:(NSURL *)fileURL previewMIMEType:(NSString *)mimeType responsePolicy:(WKNavigationResponsePolicy)responsePolicy;
- (void)verifyDownload;

@property (nonatomic, readonly) BOOL didFailNavigation;
@property (nonatomic, readonly) BOOL didFinishNavigation;
@property (nonatomic, readonly) BOOL didFinishQuickLookLoad;
@property (nonatomic, readonly) BOOL didStartQuickLookLoad;
@property (nonatomic, readonly) NSError *navigationError;

@end

@implementation QuickLookDelegate {
@protected
    NSUInteger _downloadFileSize;
    NSUInteger _expectedFileSize;
    RetainPtr<NSData> _expectedFileData;
    RetainPtr<NSError> _navigationError;
    RetainPtr<NSString> _expectedFileName;
    RetainPtr<NSString> _expectedFileType;
    RetainPtr<NSString> _expectedMIMEType;
    RetainPtr<NSURL> _downloadDestinationURL;
    WKNavigationResponsePolicy _responsePolicy;
}

static void readFile(NSURL *fileURL, NSUInteger& fileSize, RetainPtr<NSString>& fileName, RetainPtr<NSString>& fileType, RetainPtr<NSString>& mimeType, RetainPtr<NSData>& fileData)
{
    if (NSDictionary *attributes = [NSFileManager.defaultManager attributesOfItemAtPath:fileURL.path error:nil])
        fileSize = [attributes[NSFileSize] unsignedIntegerValue];

    fileName = fileURL.lastPathComponent;

    NSString *typeIdentifier = nil;
    if ([fileURL getResourceValue:&typeIdentifier forKey:NSURLTypeIdentifierKey error:nil])
        fileType = typeIdentifier;

    mimeType = bridge_cast(adoptCF(UTTypeCopyPreferredTagWithClass(bridge_cast(typeIdentifier), kUTTagClassMIMEType)));
    fileData = adoptNS([[NSData alloc] initWithContentsOfURL:fileURL]);
}

- (instancetype)initWithExpectedFileURL:(NSURL *)fileURL responsePolicy:(WKNavigationResponsePolicy)responsePolicy
{
    if (!(self = [super init]))
        return nil;

    readFile(fileURL, _expectedFileSize, _expectedFileName, _expectedFileType, _expectedMIMEType, _expectedFileData);

    _responsePolicy = responsePolicy;
    return self;
}

- (instancetype)initWithExpectedFileURL:(NSURL *)fileURL previewMIMEType:(NSString *)mimeType responsePolicy:(WKNavigationResponsePolicy)responsePolicy
{
    if (!(self = [self initWithExpectedFileURL:fileURL responsePolicy:responsePolicy]))
        return nil;
    
    _expectedMIMEType = mimeType;
    return self;
}

- (NSError *)navigationError
{
    return _navigationError.get();
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    _didFailNavigation = NO;
    _didFinishNavigation = NO;
    _didFinishQuickLookLoad = NO;
    _didStartQuickLookLoad = NO;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    EXPECT_TRUE(navigationResponse.canShowMIMEType);
    EXPECT_WK_STREQ(_expectedFileName.get(), navigationResponse.response.URL.lastPathComponent);
    EXPECT_WK_STREQ(_expectedMIMEType.get(), navigationResponse.response.MIMEType);
    decisionHandler(_responsePolicy);
}

- (void)_webView:(WKWebView *)webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:(NSString *)fileName uti:(NSString *)uti
{
    EXPECT_FALSE(_didFinishQuickLookLoad);
    EXPECT_FALSE(_didStartQuickLookLoad);
    EXPECT_WK_STREQ(_expectedFileName.get(), fileName);
    EXPECT_WK_STREQ(_expectedFileType.get(), uti);
    _didStartQuickLookLoad = YES;
}

- (void)_webView:(WKWebView *)webView didFinishLoadForQuickLookDocumentInMainFrame:(NSData *)documentData
{
    EXPECT_EQ(_expectedFileSize, documentData.length);
    EXPECT_TRUE([_expectedFileData isEqualToData:documentData]);
    EXPECT_FALSE(_didFinishQuickLookLoad);
    EXPECT_TRUE(_didStartQuickLookLoad);
    _didFinishQuickLookLoad = YES;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    EXPECT_FALSE(_didFailNavigation);
    EXPECT_FALSE(_didFinishNavigation);
    _didFinishNavigation = YES;
    isDone = true;
}

- (void)_webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error userInfo:(id<NSSecureCoding>)userInfo
{
    EXPECT_FALSE(_didFailNavigation);
    EXPECT_FALSE(_didFinishNavigation);
    EXPECT_NULL(_navigationError);
    _didFailNavigation = YES;
    _navigationError = error;
    isDone = true;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    EXPECT_FALSE(_didFailNavigation);
    EXPECT_FALSE(_didFinishNavigation);
    EXPECT_NULL(_navigationError);
    _didFailNavigation = YES;
    _navigationError = error;
    isDone = true;
}

- (void)_webViewWebProcessDidCrash:(WKWebView *)webView
{
    RELEASE_ASSERT_NOT_REACHED();
}

- (void)_downloadDidStart:(_WKDownload *)download
{
    EXPECT_WK_STREQ(_expectedFileName.get(), download.request.URL.lastPathComponent);
}

- (void)_download:(_WKDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    EXPECT_WK_STREQ(_expectedFileName.get(), response.URL.lastPathComponent);
    EXPECT_WK_STREQ(_expectedMIMEType.get(), response.MIMEType);
}

- (void)_download:(_WKDownload *)download didReceiveData:(uint64_t)length
{
    _downloadFileSize += length;
}

- (void)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename completionHandler:(void (^)(BOOL, NSString *))completionHandler
{
    EXPECT_WK_STREQ(_expectedFileName.get(), filename);
    
    NSURL *tempDirectoryURL = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
    _downloadDestinationURL = [NSURL fileURLWithPath:filename relativeToURL:tempDirectoryURL];
    completionHandler(YES, [_downloadDestinationURL path]);
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    EXPECT_EQ(_expectedFileSize, _downloadFileSize);
    downloadIsDone = true;
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    RELEASE_ASSERT_NOT_REACHED();
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    RELEASE_ASSERT_NOT_REACHED();
}

- (void)verifyDownload
{
    EXPECT_TRUE([NSFileManager.defaultManager fileExistsAtPath:[_downloadDestinationURL path]]);

    NSUInteger downloadFileSize;
    RetainPtr<NSData> downloadFileData;
    RetainPtr<NSString> downloadFileName;
    RetainPtr<NSString> downloadFileType;
    RetainPtr<NSString> downloadMIMEType;
    readFile(_downloadDestinationURL.get(), downloadFileSize, downloadFileName, downloadFileType, downloadMIMEType, downloadFileData);

    EXPECT_EQ(_expectedFileSize, downloadFileSize);
    EXPECT_TRUE([_expectedFileData isEqualToData:downloadFileData.get()]);
    EXPECT_WK_STREQ(_expectedFileName.get(), downloadFileName.get());
    EXPECT_WK_STREQ(_expectedFileType.get(), downloadFileType.get());
    EXPECT_WK_STREQ(_expectedMIMEType.get(), downloadMIMEType.get());
}

@end

static RetainPtr<WKWebView> runTest(QuickLookDelegate *delegate, NSURLRequest *request, BOOL shouldDecidePolicyBeforeLoading)
{
    auto processPool = adoptNS([[WKProcessPool alloc] init]);
    [processPool _setDownloadDelegate:delegate];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setShouldDecidePolicyBeforeLoadingQuickLookPreview:shouldDecidePolicyBeforeLoading];
    [configuration setProcessPool:processPool.get()];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate];
    [webView loadRequest:request];

    isDone = false;
    Util::run(&isDone);

    return webView;
}

static RetainPtr<WKWebView> runTestDecideBeforeLoading(QuickLookDelegate *delegate, NSURLRequest *request)
{
    return runTest(delegate, request, YES);
}

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED > 130400
static RetainPtr<WKWebView> runTestDecideAfterLoading(QuickLookDelegate *delegate, NSURLRequest *request)
{
    return runTest(delegate, request, NO);
}

TEST(QuickLook, AllowResponseBeforeLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() responsePolicy:WKNavigationResponsePolicyAllow]);
    runTestDecideBeforeLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}

TEST(QuickLook, AllowResponseAfterLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() previewMIMEType:pagesDocumentPreviewMIMEType responsePolicy:WKNavigationResponsePolicyAllow]);
    runTestDecideAfterLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}
#endif

@interface QuickLookAsyncDelegate : QuickLookDelegate
@end

@implementation QuickLookAsyncDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);

    dispatch_after(when, dispatch_get_main_queue(), ^{
        decisionHandler(_responsePolicy);
    });
}

@end

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED > 130400
TEST(QuickLook, AsyncAllowResponseBeforeLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookAsyncDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() responsePolicy:WKNavigationResponsePolicyAllow]);
    runTestDecideBeforeLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}

TEST(QuickLook, AsyncAllowResponseAfterLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookAsyncDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() previewMIMEType:pagesDocumentPreviewMIMEType responsePolicy:WKNavigationResponsePolicyAllow]);
    runTestDecideAfterLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}
#endif

TEST(QuickLook, CancelResponseBeforeLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() responsePolicy:WKNavigationResponsePolicyCancel]);
    runTestDecideBeforeLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_EQ(WebKitErrorFrameLoadInterruptedByPolicyChange, [delegate navigationError].code);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_FALSE([delegate didFinishQuickLookLoad]);
    EXPECT_FALSE([delegate didStartQuickLookLoad]);
    EXPECT_TRUE([delegate didFailNavigation]);
}

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED > 130400
TEST(QuickLook, CancelResponseAfterLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() previewMIMEType:pagesDocumentPreviewMIMEType responsePolicy:WKNavigationResponsePolicyCancel]);
    runTestDecideAfterLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_EQ(WebKitErrorFrameLoadInterruptedByPolicyChange, [delegate navigationError].code);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}
#endif

TEST(QuickLook, DownloadResponseBeforeLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() responsePolicy:WKNavigationResponsePolicyDownload]);
    runTestDecideBeforeLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_EQ(WebKitErrorFrameLoadInterruptedByPolicyChange, [delegate navigationError].code);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_FALSE([delegate didFinishQuickLookLoad]);
    EXPECT_FALSE([delegate didStartQuickLookLoad]);
    EXPECT_TRUE([delegate didFailNavigation]);

    Util::run(&downloadIsDone);
    [delegate verifyDownload];
}

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED > 130400
TEST(QuickLook, DownloadResponseAfterLoadingPreview)
{
    auto delegate = adoptNS([[QuickLookDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() previewMIMEType:pagesDocumentPreviewMIMEType responsePolicy:WKNavigationResponsePolicyDownload]);
    runTestDecideAfterLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_EQ(WebKitErrorFrameLoadInterruptedByPolicyChange, [delegate navigationError].code);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}
#endif

@interface QuickLookPasswordDelegate : QuickLookDelegate
@property (nonatomic) BOOL didRequestPassword;
@end

@implementation QuickLookPasswordDelegate

- (void)_webViewDidRequestPasswordForQuickLookDocument:(WKWebView *)webView
{
    _didRequestPassword = YES;
    isDone = true;
}

@end

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED > 130400
TEST(QuickLook, RequestPasswordBeforeLoadingPreview)
{
    NSURL *passwordProtectedDocumentURL = [NSBundle.mainBundle URLForResource:@"password-protected" withExtension:@"pages" subdirectory:@"TestWebKitAPI.resources"];
    auto delegate = adoptNS([[QuickLookPasswordDelegate alloc] initWithExpectedFileURL:passwordProtectedDocumentURL responsePolicy:WKNavigationResponsePolicyAllow]);
    runTestDecideBeforeLoading(delegate.get(), [NSURLRequest requestWithURL:passwordProtectedDocumentURL]);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didRequestPassword]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}

TEST(QuickLook, RequestPasswordAfterLoadingPreview)
{
    NSURL *passwordProtectedDocumentURL = [NSBundle.mainBundle URLForResource:@"password-protected" withExtension:@"pages" subdirectory:@"TestWebKitAPI.resources"];
    auto delegate = adoptNS([[QuickLookPasswordDelegate alloc] initWithExpectedFileURL:passwordProtectedDocumentURL previewMIMEType:pagesDocumentPreviewMIMEType responsePolicy:WKNavigationResponsePolicyAllow]);
    runTestDecideAfterLoading(delegate.get(), [NSURLRequest requestWithURL:passwordProtectedDocumentURL]);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_FALSE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didRequestPassword]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);
}

TEST(QuickLook, ReloadAndSameDocumentNavigation)
{
    auto delegate = adoptNS([[QuickLookDelegate alloc] initWithExpectedFileURL:pagesDocumentURL().get() responsePolicy:WKNavigationResponsePolicyAllow]);
    auto webView = runTestDecideBeforeLoading(delegate.get(), [NSURLRequest requestWithURL:pagesDocumentURL().get()]);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);

    isDone = false;
    [webView evaluateJavaScript:@"window.location.reload()" completionHandler:nil];
    Util::run(&isDone);
    EXPECT_FALSE([delegate didFailNavigation]);
    EXPECT_TRUE([delegate didFinishNavigation]);
    EXPECT_TRUE([delegate didFinishQuickLookLoad]);
    EXPECT_TRUE([delegate didStartQuickLookLoad]);

    isDone = false;
    [webView evaluateJavaScript:@"window.location = '#test'; window.location.hash" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_WK_STREQ(@"#test", value);
        isDone = true;
    }];
    Util::run(&isDone);
}
#endif

@interface QuickLookLegacyDelegate : NSObject <WebFrameLoadDelegate, WebPolicyDelegate>
@end

@implementation QuickLookLegacyDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    isDone = true;
}

- (void)webView:(WebView *)webView decidePolicyForMIMEType:(NSString *)type request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    EXPECT_WK_STREQ(pagesDocumentPreviewMIMEType, type);
    [listener use];
}

@end

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED > 130400
TEST(QuickLook, LegacyQuickLookContent)
{
    WebKitInitialize();
    WebThreadLock();

    auto webView = adoptNS([[WebView alloc] init]);

    auto delegate = adoptNS([[QuickLookLegacyDelegate alloc] init]);
    [webView setFrameLoadDelegate:delegate.get()];
    [webView setPolicyDelegate:delegate.get()];

    auto webPreferences = adoptNS([[WebPreferences alloc] initWithIdentifier:@"LegacyQuickLookContent"]);
    [webPreferences setQuickLookDocumentSavingEnabled:YES];
    [webView setPreferences:webPreferences.get()];

    WebFrame *mainFrame = [webView mainFrame];

    [mainFrame loadRequest:[NSURLRequest requestWithURL:pagesDocumentURL().get()]];
    Util::run(&isDone);
    WebThreadLock();

    NSUInteger expectedFileSize;
    RetainPtr<NSData> expectedFileData;
    RetainPtr<NSString> expectedFileName;
    RetainPtr<NSString> expectedFileType;
    RetainPtr<NSString> expectedMIMEType;
    readFile(pagesDocumentURL().get(), expectedFileSize, expectedFileName, expectedFileType, expectedMIMEType, expectedFileData);

    NSDictionary *quickLookContent = mainFrame.dataSource._quickLookContent;
    NSString *filePath = quickLookContent[WebQuickLookFileNameKey];
    EXPECT_TRUE([NSFileManager.defaultManager fileExistsAtPath:filePath]);
    EXPECT_WK_STREQ(expectedFileName.get(), filePath.lastPathComponent);
    EXPECT_WK_STREQ(expectedFileType.get(), quickLookContent[WebQuickLookUTIKey]);

    NSDictionary *fileAttributes = [NSFileManager.defaultManager attributesOfItemAtPath:filePath error:nil];
    EXPECT_EQ(expectedFileSize, [fileAttributes[NSFileSize] unsignedIntegerValue]);
    
    EXPECT_TRUE([expectedFileData isEqualToData:[NSData dataWithContentsOfFile:filePath]]);

    isDone = false;
    [mainFrame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    Util::run(&isDone);
    WebThreadLock();

    EXPECT_NULL(mainFrame.dataSource._quickLookContent);
}

TEST(QuickLook, LoadFromMemoryCache)
{
    [TestProtocol registerWithScheme:@"http"];
    TestProtocol.additionalResponseHeaders = @{ @"Cache-Control" : @"max-age=3600" };

    auto webView = adoptNS([[TestWKWebView alloc] init]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://bundle-file/simple.html"]]];

    NSString * const contentTypeJavaScript = @"document.contentType";
    NSString * const simpleMIMEType = @"text/html";
    EXPECT_WK_STREQ(simpleMIMEType, [webView stringByEvaluatingJavaScript:contentTypeJavaScript]);

    NSURL * const pagesDocumentHTTPURL = [NSURL URLWithString:@"http://bundle-file/pages.pages"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:pagesDocumentHTTPURL]];
    EXPECT_WK_STREQ(pagesDocumentPreviewMIMEType, [webView stringByEvaluatingJavaScript:contentTypeJavaScript]);

    [webView synchronouslyGoBack];
    EXPECT_WK_STREQ(simpleMIMEType, [webView stringByEvaluatingJavaScript:contentTypeJavaScript]);

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:pagesDocumentHTTPURL]];
    EXPECT_WK_STREQ(pagesDocumentPreviewMIMEType, [webView stringByEvaluatingJavaScript:contentTypeJavaScript]);

    TestProtocol.additionalResponseHeaders = nil;
    [TestProtocol unregister];
}
#endif

#endif // USE(QUICK_LOOK)
