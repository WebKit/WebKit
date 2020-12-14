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
#import "WKDownloadInternal.h"

#import "APIDownloadClient.h"
#import "CompletionHandlerCallChecker.h"
#import "DownloadProxy.h"
#import "WKDownloadDelegate.h"
#import "WKNSData.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKWebViewInternal.h"
#import <Foundation/Foundation.h>
#import <wtf/WeakObjCPtr.h>

class DownloadClient final : public API::DownloadClient {
public:
    explicit DownloadClient(id <WKDownloadDelegate> delegate)
        : m_delegate(delegate)
        , m_respondsToWillPerformHTTPRedirection([delegate respondsToSelector:@selector(download:willPerformHTTPRedirection:newRequest:decisionHandler:)])
        , m_respondsToDidReceiveAuthenticationChallenge([delegate respondsToSelector:@selector(download:didReceiveAuthenticationChallenge:completionHandler:)])
        , m_respondsToDidFinish([m_delegate respondsToSelector:@selector(downloadDidFinish:)])
        , m_respondsToDidFailWithError([delegate respondsToSelector:@selector(download:didFailWithError:resumeData:)])
    {
        ASSERT([delegate respondsToSelector:@selector(download:decideDestinationUsingResponse:suggestedFilename:completionHandler:)]);
    }

private:
    void willSendRequest(WebKit::DownloadProxy& download, WebCore::ResourceRequest&& request, const WebCore::ResourceResponse& response, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler) final
    {
        if (!m_delegate || !m_respondsToWillPerformHTTPRedirection)
            return completionHandler(WTFMove(request));

        RetainPtr<NSURLRequest> nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
        [m_delegate download:wrapper(download) willPerformHTTPRedirection:static_cast<NSHTTPURLResponse *>(response.nsURLResponse()) newRequest:nsRequest.get() decisionHandler:makeBlockPtr([request = WTFMove(request), completionHandler = WTFMove(completionHandler), checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(download:willPerformHTTPRedirection:newRequest:decisionHandler:))](WKDownloadRedirectPolicy policy) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            switch (policy) {
            case WKDownloadRedirectPolicyCancel:
                return completionHandler({ });
            case WKDownloadRedirectPolicyAllow:
                return completionHandler(WTFMove(request));
            default:
                [NSException raise:NSInvalidArgumentException format:@"Invalid WKDownloadRedirectPolicy (%ld)", (long)policy];
            }
        }).get()];
    }

    void didReceiveAuthenticationChallenge(WebKit::DownloadProxy& download, WebKit::AuthenticationChallengeProxy& challenge) final
    {
        if (!m_delegate || !m_respondsToDidReceiveAuthenticationChallenge)
            return challenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);

        [m_delegate download:wrapper(download) didReceiveAuthenticationChallenge:wrapper(challenge) completionHandler:makeBlockPtr([challenge = makeRef(challenge), checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(download:didReceiveAuthenticationChallenge:completionHandler:))] (NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            switch (disposition) {
            case NSURLSessionAuthChallengeUseCredential:
                challenge->listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::UseCredential, credential ? WebCore::Credential(credential) : WebCore::Credential());
                break;
            case NSURLSessionAuthChallengePerformDefaultHandling:
                challenge->listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling);
                break;
            case NSURLSessionAuthChallengeCancelAuthenticationChallenge:
                challenge->listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::Cancel);
                break;
            case NSURLSessionAuthChallengeRejectProtectionSpace:
                challenge->listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);
                break;
            default:
                [NSException raise:NSInvalidArgumentException format:@"Invalid NSURLSessionAuthChallengeDisposition (%ld)", (long)disposition];
            }
        }).get()];
    }

    void decideDestinationWithSuggestedFilename(WebKit::DownloadProxy& download, const WebCore::ResourceResponse& response, const WTF::String& suggestedFilename, CompletionHandler<void(WebKit::AllowOverwrite, WTF::String)>&& completionHandler) final
    {
        if (!m_delegate)
            return completionHandler(WebKit::AllowOverwrite::No, { });

        [m_delegate download:wrapper(download) decideDestinationUsingResponse:response.nsURLResponse() suggestedFilename:suggestedFilename completionHandler:makeBlockPtr([download = makeRef(download), completionHandler = WTFMove(completionHandler), checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(download:decideDestinationUsingResponse:suggestedFilename:completionHandler:))] (NSURL *destination) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            if (!destination)
                return completionHandler(WebKit::AllowOverwrite::No, { });
            
            if (!destination.isFileURL) {
                completionHandler(WebKit::AllowOverwrite::No, { });
                [NSException raise:NSInvalidArgumentException format:@"destination must be a file URL"];
                return;
            }

            NSFileManager *fileManager = [NSFileManager defaultManager];
            if (![fileManager fileExistsAtPath:[destination URLByDeletingLastPathComponent].path]) {
                RunLoop::main().dispatch([download] {
                    download->didFail([NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorCannotCreateFile userInfo:nil], { });
                });
                completionHandler(WebKit::AllowOverwrite::No, { });
                return;
            }
            if ([fileManager fileExistsAtPath:destination.path]) {
                RunLoop::main().dispatch([download] {
                    download->didFail([NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorCannotCreateFile userInfo:nil], { });
                });
                completionHandler(WebKit::AllowOverwrite::No, { });
                return;
            }

            wrapper(download.get()).progress.fileURL = destination;

            completionHandler(WebKit::AllowOverwrite::No, destination.path);
        }).get()];
    }

    void didReceiveData(WebKit::DownloadProxy& download, uint64_t, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite) final
    {
        NSProgress *progress = wrapper(download).progress;
        progress.totalUnitCount = totalBytesExpectedToWrite;
        progress.completedUnitCount = totalBytesWritten;
    }

    void didFinish(WebKit::DownloadProxy& download) final
    {
        if (!m_delegate || !m_respondsToDidFinish)
            return;

        [m_delegate downloadDidFinish:wrapper(download)];
    }

    void didFail(WebKit::DownloadProxy& download, const WebCore::ResourceError& error, API::Data* resumeData) final
    {
        if (!m_delegate || !m_respondsToDidFailWithError)
            return;

        [m_delegate download:wrapper(download) didFailWithError:error.nsError() resumeData:wrapper(resumeData)];
    }

    void processDidCrash(WebKit::DownloadProxy& download) final
    {
        if (!m_delegate || !m_respondsToDidFailWithError)
            return;

        [m_delegate download:wrapper(download) didFailWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorNetworkConnectionLost userInfo:nil] resumeData:nil];
    }

    WeakObjCPtr<id <WKDownloadDelegate> > m_delegate;

    bool m_respondsToWillPerformHTTPRedirection : 1;
    bool m_respondsToDidReceiveAuthenticationChallenge : 1;
    bool m_respondsToDidFinish : 1;
    bool m_respondsToDidFailWithError : 1;
};

@implementation WKDownload

- (void)cancel:(void (^)(NSData *resumeData))completionHandler
{
    _download->cancel([completionHandler = makeBlockPtr(completionHandler)] (auto* data) {
        if (completionHandler)
            completionHandler(wrapper(data));
    });
}

- (NSURLRequest *)originalRequest
{
    return _download->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

- (WKWebView *)webView
{
    auto* page = _download->originatingPage();
    if (!page)
        return nil;
    return fromWebPageProxy(*page);
}

- (id <WKDownloadDelegate>)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id <WKDownloadDelegate>)delegate
{
    _delegate = delegate;
    _download->setClient(adoptRef(*new DownloadClient(delegate)));
}

#pragma mark NSProgressReporting protocol implementation

- (NSProgress *)progress
{
    NSProgress* downloadProgress = _download->progress();
    if (!downloadProgress) {
        constexpr auto indeterminateUnitCount = -1;
        downloadProgress = [NSProgress progressWithTotalUnitCount:indeterminateUnitCount];

        downloadProgress.kind = NSProgressKindFile;
        downloadProgress.fileOperationKind = NSProgressFileOperationKindDownloading;

        downloadProgress.cancellable = YES;
        downloadProgress.cancellationHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKDownload> { self }] () mutable {
            if (!RunLoop::isMain()) {
                RunLoop::main().dispatch([weakSelf = WTFMove(weakSelf)] {
                    [weakSelf cancel:nil];
                });
                return;
            }
            [weakSelf cancel:nil];
        }).get();

        _download->setProgress(downloadProgress);
    }
    return downloadProgress;
}

- (void)dealloc
{
    _download->~DownloadProxy();
    [super dealloc];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_download;
}

@end
