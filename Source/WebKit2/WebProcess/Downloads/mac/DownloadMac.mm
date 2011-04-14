/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#import "Download.h"

#import <WebCore/AuthenticationMac.h>
#import <WebCore/BackForwardController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/ResourceResponse.h>
#import "DataReference.h"
#import "WebPage.h"

@interface NSURLDownload (WebNSURLDownloadDetails)
+(id)_downloadWithLoadingConnection:(NSURLConnection *)connection
                            request:(NSURLRequest *)request
                           response:(NSURLResponse *)r
                           delegate:(id)delegate
                              proxy:(id)proxy;
- (void)_setOriginatingURL:(NSURL *)originatingURL;
@end

@interface WKDownloadAsDelegate : NSObject <NSURLDownloadDelegate> {
    WebKit::Download* _download;
}
- (id)initWithDownload:(WebKit::Download*)download;
- (void)invalidate;
@end

using namespace WebCore;

namespace WebKit {

static KURL originatingURLFromBackForwardList(WebPage *webPage)
{
    if (!webPage)
        return KURL();

    Page* page = webPage->corePage();
    if (!page)
        return KURL();

    KURL originalURL;
    int backCount = page->backForward()->backCount();
    for (int backIndex = 0; backIndex <= backCount; backIndex++) {
        // FIXME: At one point we had code here to check a "was user gesture" flag.
        // Do we need to restore that logic?
        HistoryItem* historyItem = page->backForward()->itemAtIndex(-backIndex);
        if (!historyItem)
            continue;

        originalURL = historyItem->originalURL(); 
        if (!originalURL.isNull()) 
            return originalURL;
    }

    return KURL();
}

static void setOriginalURLForDownload(WebPage *webPage, NSURLDownload *download, const ResourceRequest& initialRequest)
{
    KURL originalURL;
    
    // If there was no referrer, don't traverse the back/forward history
    // since this download was initiated directly. <rdar://problem/5294691>
    if (!initialRequest.httpReferrer().isNull()) {
        // find the first item in the history that was originated by the user
        originalURL = originatingURLFromBackForwardList(webPage);
    }

    if (originalURL.isNull())
        originalURL = initialRequest.url();

    NSURL *originalNSURL = originalURL;

    NSString *scheme = [originalNSURL scheme];
    NSString *host = [originalNSURL host];
    if (scheme && host && [scheme length] && [host length]) {
        NSNumber *port = [originalNSURL port];
        if (port && [port intValue] < 0)
            port = nil;
        RetainPtr<NSString> hostOnlyURLString;
        if (port)
            hostOnlyURLString.adoptNS([[NSString alloc] initWithFormat:@"%@://%@:%d", scheme, host, [port intValue]]);
        else
            hostOnlyURLString.adoptNS([[NSString alloc] initWithFormat:@"%@://%@", scheme, host]);

        RetainPtr<NSURL> hostOnlyURL = [[NSURL alloc] initWithString:hostOnlyURLString.get()];

        ASSERT([download respondsToSelector:@selector(_setOriginatingURL:)]);
        [download _setOriginatingURL:hostOnlyURL.get()];
    }
}

void Download::start(WebPage* initiatingPage)
{
    ASSERT(!m_nsURLDownload);
    ASSERT(!m_delegate);

    m_delegate.adoptNS([[WKDownloadAsDelegate alloc] initWithDownload:this]);
    m_nsURLDownload.adoptNS([[NSURLDownload alloc] initWithRequest:m_request.nsURLRequest() delegate:m_delegate.get()]);

    // FIXME: Allow this to be changed by the client.
    [m_nsURLDownload.get() setDeletesFileUponFailure:NO];

    setOriginalURLForDownload(initiatingPage, m_nsURLDownload.get(), m_request);
}

void Download::startWithHandle(WebPage* initiatingPage, ResourceHandle* handle, const ResourceRequest& initialRequest, const ResourceResponse& response)
{
    ASSERT(!m_nsURLDownload);
    ASSERT(!m_delegate);

    id proxy = handle->releaseProxy();
    ASSERT(proxy);

    m_delegate.adoptNS([[WKDownloadAsDelegate alloc] initWithDownload:this]);
    m_nsURLDownload = [NSURLDownload _downloadWithLoadingConnection:handle->connection()
                                                            request:m_request.nsURLRequest()
                                                           response:response.nsURLResponse()
                                                            delegate:m_delegate.get()
                                                               proxy:proxy];

    // FIXME: Allow this to be changed by the client.
    [m_nsURLDownload.get() setDeletesFileUponFailure:NO];
                                                            
    setOriginalURLForDownload(initiatingPage, m_nsURLDownload.get(), initialRequest);
}

void Download::cancel()
{
    [m_nsURLDownload.get() cancel];

    RetainPtr<NSData> resumeData = [m_nsURLDownload.get() resumeData];
    didCancel(CoreIPC::DataReference(reinterpret_cast<const uint8_t*>([resumeData.get() bytes]), [resumeData.get() length]));
}

void Download::platformInvalidate()
{
    ASSERT(m_nsURLDownload);
    ASSERT(m_delegate);

    [m_delegate.get() invalidate];
    m_delegate = nullptr;
    m_nsURLDownload = nullptr;
}

void Download::didDecideDestination(const String& destination, bool allowOverwrite)
{
}

void Download::platformDidFinish()
{
}

void Download::receivedCredential(const AuthenticationChallenge& authenticationChallenge, const Credential& credential)
{
    [authenticationChallenge.sender() useCredential:mac(credential) forAuthenticationChallenge:authenticationChallenge.nsURLAuthenticationChallenge()];
}

void Download::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& authenticationChallenge)
{
    [authenticationChallenge.sender() continueWithoutCredentialForAuthenticationChallenge:authenticationChallenge.nsURLAuthenticationChallenge()];
}

void Download::receivedCancellation(const AuthenticationChallenge& authenticationChallenge)
{
    [authenticationChallenge.sender() cancelAuthenticationChallenge:authenticationChallenge.nsURLAuthenticationChallenge()];
}

} // namespace WebKit

@implementation WKDownloadAsDelegate

- (id)initWithDownload:(WebKit::Download*)download
{
    self = [super init];
    if (!self)
        return nil;

    _download = download;
    return self;
}

- (void)invalidate
{
    _download = 0;
}

- (void)downloadDidBegin:(NSURLDownload *)download
{
    if (_download)
        _download->didStart();
}

- (NSURLRequest *)download:(NSURLDownload *)download willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
    return request;
}

- (BOOL)download:(NSURLDownload *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace
{
    // FIXME: Implement.
    notImplemented();
    return NO;
}

- (void)download:(NSURLDownload *)download didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (_download)
        _download->didReceiveAuthenticationChallenge(core(challenge));
}

- (void)download:(NSURLDownload *)download didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    // FIXME: Implement.
    notImplemented();
}

- (BOOL)downloadShouldUseCredentialStorage:(NSURLDownload *)download
{
    return NO;
}

- (void)download:(NSURLDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    if (_download)
        _download->didReceiveResponse(response);
}

- (void)download:(NSURLDownload *)download willResumeWithResponse:(NSURLResponse *)response fromByte:(long long)startingByte
{
    // FIXME: Implement.
    notImplemented();
}

- (void)download:(NSURLDownload *)download didReceiveDataOfLength:(NSUInteger)length
{
    if (_download)
        _download->didReceiveData(length);
}

- (BOOL)download:(NSURLDownload *)download shouldDecodeSourceDataOfMIMEType:(NSString *)encodingType
{
    if (_download)
        return _download->shouldDecodeSourceDataOfMIMEType(encodingType);

    return YES;
}

- (void)download:(NSURLDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename
{
    String destination;
    bool allowOverwrite;
    if (_download)
        destination = _download->decideDestinationWithSuggestedFilename(filename, allowOverwrite);

    if (!destination.isNull())
        [download setDestination:destination allowOverwrite:allowOverwrite];
}

- (void)download:(NSURLDownload *)download didCreateDestination:(NSString *)path
{
    if (_download)
        _download->didCreateDestination(path);
}

- (void)downloadDidFinish:(NSURLDownload *)download
{
    if (_download)
        _download->didFinish();
}

- (void)download:(NSURLDownload *)download didFailWithError:(NSError *)error
{
    if (!_download)
        return;

    RetainPtr<NSData> resumeData = [download resumeData];
    CoreIPC::DataReference dataReference(reinterpret_cast<const uint8_t*>([resumeData.get() bytes]), [resumeData.get() length]);

    _download->didFail(error, dataReference);
}

@end
