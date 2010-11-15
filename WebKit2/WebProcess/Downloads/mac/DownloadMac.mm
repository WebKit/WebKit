/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "Download.h"

#include <WebCore/ResourceResponse.h>
#include "NotImplemented.h"

@interface WKDownloadAsDelegate : NSObject <NSURLConnectionDelegate> {
    WebKit::Download* _download;
}
- (id)initWithDownload:(WebKit::Download*)download;
- (void)invalidate;
@end


using namespace WebCore;

namespace WebKit {

void Download::start()
{
    ASSERT(!m_nsURLDownload);
    ASSERT(!m_delegate);

    m_delegate.adoptNS([[WKDownloadAsDelegate alloc] initWithDownload:this]);
    m_nsURLDownload.adoptNS([[NSURLDownload alloc] initWithRequest:m_request.nsURLRequest() delegate:m_delegate.get()]);

    // FIXME: Allow this to be changed by the client.
    [m_nsURLDownload.get() setDeletesFileUponFailure:NO];
}

void Download::platformInvalidate()
{
    ASSERT(m_nsURLDownload);
    ASSERT(m_delegate);

    [m_delegate.get() invalidate];
    m_delegate = nullptr;
    m_nsURLDownload = nullptr;
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
    // FIXME: Implement.
    notImplemented();
}

- (void)download:(NSURLDownload *)download didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    // FIXME: Implement.
    notImplemented();
}

- (BOOL)downloadShouldUseCredentialStorage:(NSURLDownload *)download
{
    // FIXME: Implement.
    notImplemented();
    return YES;
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
    // FIXME: Implement.
    notImplemented();
    return YES;
}

- (void)download:(NSURLDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename
{
    String destination;
    if (_download)
        destination = _download->decideDestinationWithSuggestedFilename(filename);

    // FIXME: AllowOverwrite should come from the client too.
    if (!destination.isNull())
        [download setDestination:destination allowOverwrite:YES];
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
    // FIXME: Implement.
    notImplemented();
}

@end
