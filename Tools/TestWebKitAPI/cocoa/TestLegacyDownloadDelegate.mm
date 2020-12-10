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
#import "TestLegacyDownloadDelegate.h"

@implementation TestLegacyDownloadDelegate

- (void)_downloadDidStart:(_WKDownload *)download
{
    if (_downloadDidStart)
        _downloadDidStart(download);
}

- (void)_download:(_WKDownload *)download didReceiveServerRedirectToURL:(NSURL *)url
{
    if (_didReceiveServerRedirectToURL)
        _didReceiveServerRedirectToURL(download, url);
}

- (void)_download:(_WKDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    if (_didReceiveResponse)
        _didReceiveResponse(download, response);
}

- (void)_download:(_WKDownload *)download didWriteData:(uint64_t)bytesWritten totalBytesWritten:(uint64_t)totalBytesWritten totalBytesExpectedToWrite:(uint64_t)totalBytesExpectedToWrite
{
    if (_didWriteData)
        _didWriteData(download, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
}

- (void)_download:(_WKDownload *)download decideDestinationWithSuggestedFilename:(NSString *)filename completionHandler:(void (^)(BOOL allowOverwrite, NSString *destination))completionHandler
{
    ASSERT(_decideDestinationWithSuggestedFilename);
    _decideDestinationWithSuggestedFilename(download, filename, completionHandler);
}

- (void)_downloadDidFinish:(_WKDownload *)download
{
    if (_downloadDidFinish)
        _downloadDidFinish(download);
}

- (void)_download:(_WKDownload *)download didFailWithError:(NSError *)error
{
    if (_didFailWithError)
        _didFailWithError(download, error);
}

- (void)_downloadDidCancel:(_WKDownload *)download
{
    if (_downloadDidCancel)
        _downloadDidCancel(download);
}

- (void)_download:(_WKDownload *)download didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completionHandler
{
    if (_didReceiveAuthenticationChallenge)
        _didReceiveAuthenticationChallenge(download, challenge, completionHandler);
    else
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}

- (void)_download:(_WKDownload *)download didCreateDestination:(NSString *)destination
{
    if (_didCreateDestination)
        _didCreateDestination(download, destination);
}

@end
