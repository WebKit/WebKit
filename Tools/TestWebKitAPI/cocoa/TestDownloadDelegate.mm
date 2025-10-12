/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TestDownloadDelegate.h"
#import "Utilities.h"

#import <WebKit/WKNavigationDelegatePrivate.h>

@implementation TestDownloadDelegate {
    Vector<DownloadCallback> _callbackRecord;
}

- (void)download:(WKDownload *)download willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request decisionHandler:(void (^)(WKDownloadRedirectPolicy))decisionHandler
{
    _callbackRecord.append(DownloadCallback::WillRedirect);

    ASSERT(_willPerformHTTPRedirection);
    _willPerformHTTPRedirection(download, response, request, decisionHandler);
}

- (void)download:(WKDownload *)download decideDestinationUsingResponse:(NSURLResponse *)response suggestedFilename:(NSString *)suggestedFilename completionHandler:(void (^)(NSURL *destination))completionHandler
{
    _callbackRecord.append(DownloadCallback::DecideDestination);

    ASSERT(_decideDestinationUsingResponse);
    _decideDestinationUsingResponse(download, response, suggestedFilename, completionHandler);
}

- (void)download:(WKDownload *)download didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completionHandler
{
    _callbackRecord.append(DownloadCallback::AuthenticationChallenge);

    if (_didReceiveAuthenticationChallenge)
        _didReceiveAuthenticationChallenge(download, challenge, completionHandler);
    else
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}

#if HAVE(MODERN_DOWNLOADPROGRESS)
- (void)_download:(WKDownload *)download decidePlaceholderPolicy:(void (^)(WKDownloadPlaceholderPolicy, NSURL *))completionHandler
{
    _callbackRecord.append(DownloadCallback::DecidePlaceholderPolicy);
    completionHandler(WKDownloadPlaceholderPolicyDisable, nil);
}

- (void)_download:(WKDownload *)download didReceivePlaceholderURL:(NSURL *)url completionHandler:(void (^)(void))completionHandler
{
    _callbackRecord.append(DownloadCallback::DidReceivePlaceholderURL);
    completionHandler();
}

- (void)_download:(WKDownload *)download didReceiveFinalURL:(NSURL *)url
{
    _callbackRecord.append(DownloadCallback::DidReceiveFinalURL);
}
#endif

- (void)downloadDidFinish:(WKDownload *)download
{
    _callbackRecord.append(DownloadCallback::DidFinish);

    if (_downloadDidFinish)
        _downloadDidFinish(download);
}

- (void)download:(WKDownload *)download didFailWithError:(NSError *)error resumeData:(NSData *)resumeData
{
    _callbackRecord.append(DownloadCallback::DidFailWithError);

    if (_didFailWithError)
        _didFailWithError(download, error, resumeData);
}

- (void)webView:(WKWebView *)webView navigationAction:(WKNavigationAction *)navigationAction didBecomeDownload:(WKDownload *)download
{
    _callbackRecord.append(DownloadCallback::NavigationActionBecameDownload);

    if (_navigationActionDidBecomeDownload)
        _navigationActionDidBecomeDownload(webView, navigationAction, download);
}

- (void)webView:(WKWebView *)webView navigationResponse:(WKNavigationResponse *)navigationResponse didBecomeDownload:(WKDownload *)download
{
    _callbackRecord.append(DownloadCallback::NavigationResponseBecameDownload);

    if (_navigationResponseDidBecomeDownload)
        _navigationResponseDidBecomeDownload(webView, navigationResponse, download);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    _callbackRecord.append(DownloadCallback::NavigationResponse);

    if (_decidePolicyForNavigationResponse)
        _decidePolicyForNavigationResponse(navigationResponse, decisionHandler);
    else
        decisionHandler(WKNavigationResponsePolicyDownload);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    _callbackRecord.append(DownloadCallback::NavigationAction);

    if (_decidePolicyForNavigationAction)
        _decidePolicyForNavigationAction(navigationAction, decisionHandler);
    else
        decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)waitForDownloadDidFinish
{
    __block bool finished = false;
    ASSERT(!_downloadDidFinish);
    _downloadDidFinish = ^(WKDownload *) {
        finished = true;
    };
    TestWebKitAPI::Util::run(&finished);
}

- (Vector<DownloadCallback>)takeCallbackRecord
{
    return std::exchange(_callbackRecord, { });
}

@end
