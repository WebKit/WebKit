/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "DownloadClient.h"

#if WK_API_ENABLED

#import "_WKDownloadDelegate.h"
#import "_WKDownloadInternal.h"
#import "DownloadProxy.h"
#import <WebCore/ResourceResponse.h>

namespace WebKit {

static inline _WKDownload *wrapper(DownloadProxy& download)
{
    ASSERT([download.wrapper() isKindOfClass:[_WKDownload class]]);
    return (_WKDownload *)download.wrapper();
}

DownloadClient::DownloadClient(id <_WKDownloadDelegate> delegate)
    : m_delegate(delegate)
{
    m_delegateMethods.downloadDidStart = [delegate respondsToSelector:@selector(_downloadDidStart:)];
    m_delegateMethods.downloadDidReceiveResponse = [delegate respondsToSelector:@selector(_download:didReceiveResponse:)];
    m_delegateMethods.downloadDidReceiveData = [delegate respondsToSelector:@selector(_download:didReceiveData:)];
    m_delegateMethods.downloadDecideDestinationWithSuggestedFilenameAllowOverwrite = [delegate respondsToSelector:@selector(_download:decideDestinationWithSuggestedFilename:allowOverwrite:)];
    m_delegateMethods.downloadDidFinish = [delegate respondsToSelector:@selector(_downloadDidFinish:)];
}

void DownloadClient::didStart(WebContext*, DownloadProxy* downloadProxy)
{
    if (m_delegateMethods.downloadDidStart)
        [m_delegate.get() _downloadDidStart:wrapper(*downloadProxy)];
}

void DownloadClient::didReceiveResponse(WebContext*, DownloadProxy* downloadProxy, const WebCore::ResourceResponse& response)
{
    if (m_delegateMethods.downloadDidReceiveResponse)
        [m_delegate.get() _download:wrapper(*downloadProxy) didReceiveResponse:response.nsURLResponse()];
}

void DownloadClient::didReceiveData(WebContext*, DownloadProxy* downloadProxy, uint64_t length)
{
    if (m_delegateMethods.downloadDidReceiveData)
        [m_delegate.get() _download:wrapper(*downloadProxy) didReceiveData:length];
}

String DownloadClient::decideDestinationWithSuggestedFilename(WebContext*, DownloadProxy* downloadProxy, const String& filename, bool& allowOverwriteParam)
{
    if (!m_delegateMethods.downloadDecideDestinationWithSuggestedFilenameAllowOverwrite)
        return String();
    
    BOOL allowOverwrite;
    NSString *destination = [m_delegate.get() _download:wrapper(*downloadProxy) decideDestinationWithSuggestedFilename:filename allowOverwrite:&allowOverwrite];
    allowOverwriteParam = allowOverwrite;
    return destination;
}

void DownloadClient::didFinish(WebContext*, DownloadProxy* downloadProxy)
{
    if (m_delegateMethods.downloadDidFinish)
        [m_delegate.get() _downloadDidFinish:wrapper(*downloadProxy)];
}

} // namespace WebKit

#endif // WK_API_ENABLED
