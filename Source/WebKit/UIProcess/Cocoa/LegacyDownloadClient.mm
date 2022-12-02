/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#import "LegacyDownloadClient.h"

#import "AuthenticationChallengeDisposition.h"
#import "AuthenticationChallengeProxy.h"
#import "AuthenticationDecisionListener.h"
#import "CompletionHandlerCallChecker.h"
#import "DownloadProxy.h"
#import "Logging.h"
#import "SystemPreviewController.h"
#import "WKDownloadInternal.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLExtras.h"
#import "WebCredential.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKDownloadDelegate.h"
#import "_WKDownloadInternal.h"
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceResponse.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>

namespace WebKit {

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

LegacyDownloadClient::LegacyDownloadClient(id <_WKDownloadDelegate> delegate)
    : m_delegate(delegate)
{
    m_delegateMethods.downloadDidStart = [delegate respondsToSelector:@selector(_downloadDidStart:)];
    m_delegateMethods.downloadDidReceiveResponse = [delegate respondsToSelector:@selector(_download:didReceiveResponse:)];
    m_delegateMethods.downloadDidReceiveData = [delegate respondsToSelector:@selector(_download:didReceiveData:)];
    m_delegateMethods.downloadDidWriteDataTotalBytesWrittenTotalBytesExpectedToWrite = [delegate respondsToSelector:@selector(_download:didWriteData:totalBytesWritten:totalBytesExpectedToWrite:)];
    m_delegateMethods.downloadDecideDestinationWithSuggestedFilenameAllowOverwrite = [delegate respondsToSelector:@selector(_download:decideDestinationWithSuggestedFilename:allowOverwrite:)];
    m_delegateMethods.downloadDecideDestinationWithSuggestedFilenameCompletionHandler = [delegate respondsToSelector:@selector(_download:decideDestinationWithSuggestedFilename:completionHandler:)];
    m_delegateMethods.downloadDidFinish = [delegate respondsToSelector:@selector(_downloadDidFinish:)];
    m_delegateMethods.downloadDidFail = [delegate respondsToSelector:@selector(_download:didFailWithError:)];
    m_delegateMethods.downloadDidCancel = [delegate respondsToSelector:@selector(_downloadDidCancel:)];
    m_delegateMethods.downloadDidReceiveServerRedirectToURL = [delegate respondsToSelector:@selector(_download:didReceiveServerRedirectToURL:)];
    m_delegateMethods.downloadDidReceiveAuthenticationChallengeCompletionHandler = [delegate respondsToSelector:@selector(_download:didReceiveAuthenticationChallenge:completionHandler:)];
    m_delegateMethods.downloadShouldDecodeSourceDataOfMIMEType = [delegate respondsToSelector:@selector(_download:shouldDecodeSourceDataOfMIMEType:)];
    m_delegateMethods.downloadDidCreateDestination = [delegate respondsToSelector:@selector(_download:didCreateDestination:)];
    m_delegateMethods.downloadProcessDidCrash = [delegate respondsToSelector:@selector(_downloadProcessDidCrash:)];
}

#if USE(SYSTEM_PREVIEW)
static SystemPreviewController* systemPreviewController(DownloadProxy& downloadProxy)
{
    auto* page = downloadProxy.originatingPage();
    if (!page)
        return nullptr;
    return page->systemPreviewController();
}
#endif

void LegacyDownloadClient::legacyDidStart(DownloadProxy& downloadProxy)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        if (auto* webPage = downloadProxy.originatingPage()) {
            // FIXME: Update the MIME-type once it is known in the ResourceResponse.
            webPage->systemPreviewController()->start(URL { webPage->currentURL() }, "application/octet-stream"_s, downloadProxy.systemPreviewDownloadInfo());
        }
        takeActivityToken(downloadProxy);
        return;
    }
#endif

    if (m_delegateMethods.downloadDidStart)
        [m_delegate _downloadDidStart:[_WKDownload downloadWithDownload:wrapper(downloadProxy)]];
}

void LegacyDownloadClient::didReceiveResponse(DownloadProxy& downloadProxy, const WebCore::ResourceResponse& response)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload() && response.isSuccessful()) {
        if (auto* controller = systemPreviewController(downloadProxy))
            controller->updateProgress(0);
        return;
    }
#endif

    if (m_delegateMethods.downloadDidReceiveResponse)
        [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] didReceiveResponse:response.nsURLResponse()];
}

void LegacyDownloadClient::didReceiveData(DownloadProxy& downloadProxy, uint64_t bytesWritten, uint64_t totalBytesWritten, uint64_t totalBytesExpectedToWrite)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        if (auto* controller = systemPreviewController(downloadProxy))
            controller->updateProgress(static_cast<float>(totalBytesWritten) / totalBytesExpectedToWrite);
        return;
    }
#endif

    if (m_delegateMethods.downloadDidWriteDataTotalBytesWrittenTotalBytesExpectedToWrite)
        [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] didWriteData:bytesWritten totalBytesWritten:totalBytesWritten totalBytesExpectedToWrite:totalBytesExpectedToWrite];
    else if (m_delegateMethods.downloadDidReceiveData)
        [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] didReceiveData:bytesWritten];
}

void LegacyDownloadClient::didReceiveAuthenticationChallenge(DownloadProxy& downloadProxy, AuthenticationChallengeProxy& authenticationChallenge)
{
    // FIXME: System Preview needs code here.
    if (!m_delegateMethods.downloadDidReceiveAuthenticationChallengeCompletionHandler) {
        authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling);
        return;
    }

    [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] didReceiveAuthenticationChallenge:wrapper(authenticationChallenge) completionHandler:makeBlockPtr([authenticationChallenge = Ref { authenticationChallenge }, checker = CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(_download:didReceiveAuthenticationChallenge:completionHandler:))] (NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        switch (disposition) {
        case NSURLSessionAuthChallengeUseCredential:
            authenticationChallenge->listener().completeChallenge(AuthenticationChallengeDisposition::UseCredential, credential ? WebCore::Credential(credential) : WebCore::Credential());
            break;
        case NSURLSessionAuthChallengePerformDefaultHandling:
            authenticationChallenge->listener().completeChallenge(AuthenticationChallengeDisposition::PerformDefaultHandling);
            break;
            
        case NSURLSessionAuthChallengeCancelAuthenticationChallenge:
            authenticationChallenge->listener().completeChallenge(AuthenticationChallengeDisposition::Cancel);
            break;
            
        case NSURLSessionAuthChallengeRejectProtectionSpace:
            authenticationChallenge->listener().completeChallenge(AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);
            break;
            
        default:
            [NSException raise:NSInvalidArgumentException format:@"Invalid NSURLSessionAuthChallengeDisposition (%ld)", (long)disposition];
        }
    }).get()];
}

void LegacyDownloadClient::didCreateDestination(DownloadProxy& downloadProxy, const String& destination)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        downloadProxy.setDestinationFilename(destination);
        if (auto* controller = systemPreviewController(downloadProxy))
            controller->setDestinationURL(URL::fileURLWithFileSystemPath(downloadProxy.destinationFilename()));
        return;
    }
#endif

    if (m_delegateMethods.downloadDidCreateDestination)
        [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] didCreateDestination:destination];
}

void LegacyDownloadClient::processDidCrash(DownloadProxy& downloadProxy)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        if (auto* controller = systemPreviewController(downloadProxy))
            controller->cancel();
        releaseActivityTokenIfNecessary(downloadProxy);
        return;
    }
#endif

    if (m_delegateMethods.downloadProcessDidCrash)
        [m_delegate _downloadProcessDidCrash:[_WKDownload downloadWithDownload:wrapper(downloadProxy)]];
}

void LegacyDownloadClient::decideDestinationWithSuggestedFilename(DownloadProxy& downloadProxy, const WebCore::ResourceResponse& response, const String& filename, CompletionHandler<void(AllowOverwrite, String)>&& completionHandler)
{
    didReceiveResponse(downloadProxy, response);

#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        NSString *temporaryDirectory = FileSystem::createTemporaryDirectory(@"SystemPreviews");
        NSString *destination = [temporaryDirectory stringByAppendingPathComponent:filename];
        completionHandler(AllowOverwrite::Yes, destination);
        return;
    }
#endif

    if (!m_delegateMethods.downloadDecideDestinationWithSuggestedFilenameAllowOverwrite && !m_delegateMethods.downloadDecideDestinationWithSuggestedFilenameCompletionHandler)
        return completionHandler(AllowOverwrite::No, { });

    if (m_delegateMethods.downloadDecideDestinationWithSuggestedFilenameAllowOverwrite) {
        BOOL allowOverwrite = NO;
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        NSString *destination = [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] decideDestinationWithSuggestedFilename:filename allowOverwrite:&allowOverwrite];
        ALLOW_DEPRECATED_DECLARATIONS_END
        completionHandler(allowOverwrite ? AllowOverwrite::Yes : AllowOverwrite::No, destination);
    } else {
        [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] decideDestinationWithSuggestedFilename:filename completionHandler:makeBlockPtr([checker = CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(_download:decideDestinationWithSuggestedFilename:completionHandler:)), completionHandler = WTFMove(completionHandler)] (BOOL allowOverwrite, NSString *destination) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(allowOverwrite ? AllowOverwrite::Yes : AllowOverwrite::No, destination);
        }).get()];
    }
}

void LegacyDownloadClient::didFinish(DownloadProxy& downloadProxy)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        if (auto* controller = systemPreviewController(downloadProxy)) {
            auto destinationURL = WTF::URL::fileURLWithFileSystemPath(downloadProxy.destinationFilename());
            if (!destinationURL.hasFragmentIdentifier())
                destinationURL.setFragmentIdentifier(downloadProxy.request().url().fragmentIdentifier());
            controller->finish(destinationURL);
        }
        releaseActivityTokenIfNecessary(downloadProxy);
        return;
    }
#endif

    if (m_delegateMethods.downloadDidFinish)
        [m_delegate _downloadDidFinish:[_WKDownload downloadWithDownload:wrapper(downloadProxy)]];
}

void LegacyDownloadClient::didFail(DownloadProxy& downloadProxy, const WebCore::ResourceError& error, API::Data*)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        if (auto* controller = systemPreviewController(downloadProxy))
            controller->fail(error);
        releaseActivityTokenIfNecessary(downloadProxy);
        return;
    }
#endif

    if (m_delegateMethods.downloadDidFail)
        [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] didFailWithError:error.nsError()];
}

void LegacyDownloadClient::legacyDidCancel(DownloadProxy& downloadProxy)
{
#if USE(SYSTEM_PREVIEW)
    if (downloadProxy.isSystemPreviewDownload()) {
        if (auto* controller = systemPreviewController(downloadProxy))
            controller->cancel();
        releaseActivityTokenIfNecessary(downloadProxy);
        return;
    }
#endif

    if (m_delegateMethods.downloadDidCancel)
        [m_delegate _downloadDidCancel:[_WKDownload downloadWithDownload:wrapper(downloadProxy)]];
}

void LegacyDownloadClient::willSendRequest(DownloadProxy& downloadProxy, WebCore::ResourceRequest&& request, const WebCore::ResourceResponse&, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    if (m_delegateMethods.downloadDidReceiveServerRedirectToURL)
        [m_delegate _download:[_WKDownload downloadWithDownload:wrapper(downloadProxy)] didReceiveServerRedirectToURL:request.url()];

    completionHandler(WTFMove(request));
}

#if USE(SYSTEM_PREVIEW)
void LegacyDownloadClient::takeActivityToken(DownloadProxy& downloadProxy)
{
#if USE(RUNNINGBOARD)
    if (auto* webPage = downloadProxy.originatingPage()) {
        RELEASE_LOG(ProcessSuspension, "%p - UIProcess is taking a background assertion because it is downloading a system preview", this);
        ASSERT(!m_activity);
        m_activity = webPage->process().throttler().backgroundActivity("System preview download"_s).moveToUniquePtr();
    }
#else
    UNUSED_PARAM(downloadProxy);
#endif
}

void LegacyDownloadClient::releaseActivityTokenIfNecessary(DownloadProxy& downloadProxy)
{
#if PLATFORM(IOS_FAMILY)
    if (m_activity) {
        RELEASE_LOG(ProcessSuspension, "%p UIProcess is releasing a background assertion because a system preview download completed", this);
        m_activity = nullptr;
    }
#else
    UNUSED_PARAM(downloadProxy);
#endif
}
#endif

ALLOW_DEPRECATED_DECLARATIONS_END

} // namespace WebKit
