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
#import "WKDownloadDelegatePrivate.h"
#import "WKFrameInfoInternal.h"
#import "WKNSData.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <Foundation/Foundation.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/WeakObjCPtr.h>

class DownloadClient final : public API::DownloadClient {
public:
    explicit DownloadClient(id<WKDownloadDelegatePrivate> delegate)
        : m_delegate(delegate)
        , m_respondsToWillPerformHTTPRedirection([delegate respondsToSelector:@selector(download:willPerformHTTPRedirection:newRequest:decisionHandler:)])
        , m_respondsToDidReceiveAuthenticationChallenge([delegate respondsToSelector:@selector(download:didReceiveAuthenticationChallenge:completionHandler:)])
        , m_respondsToDidFinish([delegate respondsToSelector:@selector(downloadDidFinish:)])
        , m_respondsToDidFailWithError([delegate respondsToSelector:@selector(download:didFailWithError:resumeData:)])
        , m_respondsToDecidePlaceholderPolicy([delegate respondsToSelector:@selector(_download:decidePlaceholderPolicy:)])
        , m_respondsToDecidePlaceholderPolicyAPI([delegate respondsToSelector:@selector(download:decidePlaceholderPolicy:)])
#if HAVE(MODERN_DOWNLOADPROGRESS)
        , m_respondsToDidReceivePlaceholderURL([delegate respondsToSelector:@selector(_download:didReceivePlaceholderURL:completionHandler:)])
        , m_respondsToDidReceivePlaceholderURLAPI([delegate respondsToSelector:@selector(download:didReceivePlaceholderURL:completionHandler:)])
        , m_respondsToDidReceiveFinalURL([delegate respondsToSelector:@selector(_download:didReceiveFinalURL:)])
        , m_respondsToDidReceiveFinalURLAPI([delegate respondsToSelector:@selector(download:didReceiveFinalURL:)])
#endif

    {
        ASSERT([delegate respondsToSelector:@selector(download:decideDestinationUsingResponse:suggestedFilename:completionHandler:)]);
    }

private:
    void willSendRequest(WebKit::DownloadProxy& download, WebCore::ResourceRequest&& request, const WebCore::ResourceResponse& response, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler) final
    {
        RetainPtr delegate = m_delegate.get();
        if (!delegate || !m_respondsToWillPerformHTTPRedirection)
            return completionHandler(WTFMove(request));

        RetainPtr<NSURLRequest> nsRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
        [delegate download:protectedWrapper(download).get() willPerformHTTPRedirection:RetainPtr { checked_objc_cast<NSHTTPURLResponse>(response.nsURLResponse()) }.get() newRequest:nsRequest.get() decisionHandler:makeBlockPtr([request = WTFMove(request), completionHandler = WTFMove(completionHandler), checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(download:willPerformHTTPRedirection:newRequest:decisionHandler:))](WKDownloadRedirectPolicy policy) mutable {
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
        RetainPtr delegate = m_delegate.get();
        if (!delegate || !m_respondsToDidReceiveAuthenticationChallenge)
            return challenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);

        [delegate download:protectedWrapper(download).get() didReceiveAuthenticationChallenge:protectedWrapper(challenge).get() completionHandler:makeBlockPtr([challenge = Ref { challenge }, checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(download:didReceiveAuthenticationChallenge:completionHandler:))] (NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) mutable {
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
        RetainPtr delegate = m_delegate.get();
        if (!delegate)
            return completionHandler(WebKit::AllowOverwrite::No, { });

        [delegate download:protectedWrapper(download).get() decideDestinationUsingResponse:response.protectedNSURLResponse().get() suggestedFilename:suggestedFilename.createNSString().get() completionHandler:makeBlockPtr([download = Ref { download }, completionHandler = WTFMove(completionHandler), checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(download:decideDestinationUsingResponse:suggestedFilename:completionHandler:))] (NSURL *destination) mutable {
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
            if (![fileManager fileExistsAtPath:retainPtr([destination URLByDeletingLastPathComponent].path).get()])
                return completionHandler(WebKit::AllowOverwrite::No, { });
            if ([fileManager fileExistsAtPath:retainPtr(destination.path).get()])
                return completionHandler(WebKit::AllowOverwrite::No, { });

            protectedWrapper(download.get()).get().progress.fileURL = destination;

            completionHandler(WebKit::AllowOverwrite::No, destination.path);
        }).get()];
    }

    void decidePlaceholderPolicy(WebKit::DownloadProxy& download, CompletionHandler<void(WebKit::UseDownloadPlaceholder, const WTF::URL&)>&& completionHandler)
    {
        if (!m_respondsToDecidePlaceholderPolicy && !m_respondsToDecidePlaceholderPolicyAPI) {
            completionHandler(WebKit::UseDownloadPlaceholder::No, { });
            return;
        }
        if (m_respondsToDecidePlaceholderPolicy) {
            [m_delegate.get() _download:protectedWrapper(download).get() decidePlaceholderPolicy:makeBlockPtr([completionHandler = WTFMove(completionHandler)] (_WKPlaceholderPolicy policy, NSURL *alternatePlaceholderURL) mutable {
                switch (policy) {
                case _WKPlaceholderPolicyDisable: {
                    completionHandler(WebKit::UseDownloadPlaceholder::No, alternatePlaceholderURL);
                    break;
                }
                case _WKPlaceholderPolicyEnable: {
                    completionHandler(WebKit::UseDownloadPlaceholder::Yes, alternatePlaceholderURL);
                    break;
                }
                default:
                    [NSException raise:NSInvalidArgumentException format:@"Invalid WKPlaceholderPolicy (%ld)", (long)policy];
                }
            }).get()];
        } else {
            [m_delegate.get() download:protectedWrapper(download).get() decidePlaceholderPolicy:makeBlockPtr([completionHandler = WTFMove(completionHandler)] (WKDownloadPlaceholderPolicy policy, NSURL *alternatePlaceholderURL) mutable {
                switch (policy) {
                case WKDownloadPlaceholderPolicyDisable: {
                    completionHandler(WebKit::UseDownloadPlaceholder::No, alternatePlaceholderURL);
                    break;
                }
                case WKDownloadPlaceholderPolicyEnable: {
                    completionHandler(WebKit::UseDownloadPlaceholder::Yes, alternatePlaceholderURL);
                    break;
                }
                default:
                    [NSException raise:NSInvalidArgumentException format:@"Invalid WKDownloadPlaceholderPolicy (%ld)", (long)policy];
                }
            }).get()];
        }
    }

    void didReceiveData(WebKit::DownloadProxy& download, uint64_t, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite) final
    {
        RetainPtr<NSProgress> progress = protectedWrapper(download).get().progress;
        progress.get().totalUnitCount = totalBytesExpectedToWrite;
        progress.get().completedUnitCount = totalBytesWritten;
    }

    void didFinish(WebKit::DownloadProxy& download) final
    {
        RetainPtr delegate = m_delegate.get();
        if (!delegate || !m_respondsToDidFinish)
            return;

        [delegate downloadDidFinish:protectedWrapper(download).get()];
    }

    void didFail(WebKit::DownloadProxy& download, const WebCore::ResourceError& error, API::Data* resumeData) final
    {
        RetainPtr delegate = m_delegate.get();
        if (!delegate || !m_respondsToDidFailWithError)
            return;

        [delegate download:protectedWrapper(download).get() didFailWithError:error.protectedNSError().get() resumeData:protectedWrapper(resumeData).get()];
    }

    void processDidCrash(WebKit::DownloadProxy& download) final
    {
        RetainPtr delegate = m_delegate.get();
        if (!delegate || !m_respondsToDidFailWithError)
            return;

        [delegate download:protectedWrapper(download).get() didFailWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorNetworkConnectionLost userInfo:nil] resumeData:nil];
    }

#if HAVE(MODERN_DOWNLOADPROGRESS)
    void didReceivePlaceholderURL(WebKit::DownloadProxy& download, const WTF::URL& url, std::span<const uint8_t> bookmarkData, CompletionHandler<void()>&& completionHandler) final
    {
        RetainPtr delegate = m_delegate.get();
        if (!delegate || (!m_respondsToDidReceivePlaceholderURL && !m_respondsToDidReceivePlaceholderURLAPI)) {
            completionHandler();
            return;
        }

        BOOL bookmarkDataIsStale = NO;
        NSError *bookmarkResolvingError;
        RetainPtr data = toNSData(bookmarkData);
        RetainPtr urlFromBookmark = adoptNS([[NSURL alloc] initByResolvingBookmarkData:data.get() options:0 relativeToURL:nil bookmarkDataIsStale:&bookmarkDataIsStale error:&bookmarkResolvingError]);
        if (bookmarkResolvingError || bookmarkDataIsStale)
            RELEASE_LOG_ERROR(Network, "Failed to resolve URL from bookmark data");

        if (!urlFromBookmark)
            urlFromBookmark = url.createNSURL();

        if (m_respondsToDidReceivePlaceholderURL)
            [delegate _download:protectedWrapper(download).get() didReceivePlaceholderURL:urlFromBookmark.get() completionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
        else
            [delegate download:protectedWrapper(download).get() didReceivePlaceholderURL:urlFromBookmark.get() completionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
    }

    void didReceiveFinalURL(WebKit::DownloadProxy& download, const WTF::URL& url, std::span<const uint8_t> bookmarkData) final
    {
        RetainPtr delegate = m_delegate.get();
        if (!delegate || (!m_respondsToDidReceiveFinalURL && !m_respondsToDidReceiveFinalURLAPI))
            return;

        BOOL bookmarkDataIsStale = NO;
        NSError *bookmarkResolvingError;
        RetainPtr data = toNSData(bookmarkData);
        RetainPtr urlFromBookmark = adoptNS([[NSURL alloc] initByResolvingBookmarkData:data.get() options:0 relativeToURL:nil bookmarkDataIsStale:&bookmarkDataIsStale error:&bookmarkResolvingError]);
        if (bookmarkResolvingError || bookmarkDataIsStale)
            RELEASE_LOG_ERROR(Network, "Failed to resolve URL from bookmark data");

        if (!urlFromBookmark)
            urlFromBookmark = url.createNSURL();

        if (m_respondsToDidReceiveFinalURL)
            [delegate _download:protectedWrapper(download).get() didReceiveFinalURL:urlFromBookmark.get()];
        else
            [delegate download:protectedWrapper(download).get() didReceiveFinalURL:urlFromBookmark.get()];
    }
#endif

    WeakObjCPtr<id<WKDownloadDelegatePrivate>> m_delegate;

    bool m_respondsToWillPerformHTTPRedirection : 1;
    bool m_respondsToDidReceiveAuthenticationChallenge : 1;
    bool m_respondsToDidFinish : 1;
    bool m_respondsToDidFailWithError : 1;
    bool m_respondsToDecidePlaceholderPolicy : 1;
    bool m_respondsToDecidePlaceholderPolicyAPI : 1;
#if HAVE(MODERN_DOWNLOADPROGRESS)
    bool m_respondsToDidReceivePlaceholderURL : 1;
    bool m_respondsToDidReceivePlaceholderURLAPI : 1;
    bool m_respondsToDidReceiveFinalURL : 1;
    bool m_respondsToDidReceiveFinalURLAPI : 1;
#endif
};

@implementation WKDownload

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (RefPtr<WebKit::DownloadProxy>) protectedDownload
{
    return _download.get();
}

- (void)cancel:(void (^)(NSData *resumeData))completionHandler
{
    self.protectedDownload->cancel([completionHandler = makeBlockPtr(completionHandler)] (auto* data) {
        if (completionHandler)
            completionHandler(wrapper(data));
    });
}

- (NSURLRequest *)originalRequest
{
    return _download->request().protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody).autorelease();
}

- (WKWebView *)webView
{
    RefPtr page = self.protectedDownload->originatingPage();
    return page ? page->cocoaView().autorelease() : nil;
}

- (BOOL)isUserInitiated
{
    return _download->wasUserInitiated();
}

- (WKFrameInfo *)originatingFrame
{
    return WebKit::wrapper(_download->frameInfo());
}

- (id <WKDownloadDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (void)setDelegate:(id<WKDownloadDelegatePrivate>)delegate
{
    _delegate = delegate;
    self.protectedDownload->setClient(adoptRef(*new DownloadClient(delegate)));
}

#pragma mark NSProgressReporting protocol implementation

- (NSProgress *)progress
{
    RetainPtr downloadProgress = _download->progress();
    if (!downloadProgress) {
        constexpr auto indeterminateUnitCount = -1;
        downloadProgress = [NSProgress progressWithTotalUnitCount:indeterminateUnitCount];

        downloadProgress.get().kind = NSProgressKindFile;
        downloadProgress.get().fileOperationKind = NSProgressFileOperationKindDownloading;

        downloadProgress.get().cancellable = YES;
        downloadProgress.get().cancellationHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKDownload> { self }] () mutable {
            ensureOnMainRunLoop([weakSelf = WTFMove(weakSelf)] {
                if (RetainPtr strongSelf = weakSelf.get())
                    [strongSelf cancel:nil];
            });
        }).get();

        self.protectedDownload->setProgress(downloadProgress.get());
    }
    return downloadProgress.autorelease();
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKDownload.class, self))
        return;
    SUPPRESS_UNRETAINED_ARG _download->~DownloadProxy();
    [super dealloc];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_download;
}

@end
