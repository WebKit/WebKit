/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "DownloadProxyMessages.h"
#import "Logging.h"
#import "NetworkSessionCocoa.h"
#import "WKDownloadProgress.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSProgressSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

void Download::resume(std::span<const uint8_t> resumeData, const String& path, SandboxExtension::Handle&& sandboxExtensionHandle, std::span<const uint8_t> activityAccessToken)
{
    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    auto* networkSession = m_downloadManager->client().networkSession(m_sessionID);
    if (!networkSession) {
        WTFLogAlways("Could not find network session with given session ID");
        return;
    }
    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(*networkSession);
    RetainPtr nsData = toNSData(resumeData);

    NSMutableDictionary *dictionary = [NSPropertyListSerialization propertyListWithData:nsData.get() options:NSPropertyListMutableContainersAndLeaves format:0 error:nullptr];
    [dictionary setObject:static_cast<NSString*>(path) forKey:@"NSURLSessionResumeInfoLocalPath"];
    NSData *updatedData = [NSPropertyListSerialization dataWithPropertyList:dictionary format:NSPropertyListXMLFormat_v1_0 options:0 error:nullptr];

    // FIXME: Use nsData instead of updatedData once we've migrated from _WKDownload to WKDownload
    // because there's no reason to set the local path we got from the data back into the data.
    m_downloadTask = [cocoaSession.sessionWrapperForDownloadResume().session downloadTaskWithResumeData:updatedData];
    auto taskIdentifier = [m_downloadTask taskIdentifier];
    ASSERT(!cocoaSession.sessionWrapperForDownloadResume().downloadMap.contains(taskIdentifier));
    cocoaSession.sessionWrapperForDownloadResume().downloadMap.add(taskIdentifier, m_downloadID);
    m_downloadTask.get()._pathToDownloadTaskFile = path;

    [m_downloadTask resume];

#if HAVE(MODERN_DOWNLOADPROGRESS)
    if (RetainPtr<NSData> placeholderURLBookmark = [dictionary objectForKey:@"ResumePlaceholderURLBookmarkData"]) {
        RetainPtr nsActivityAccessToken = toNSData(activityAccessToken);
        RetainPtr pathString  = adoptNS([[NSString alloc] initWithUTF8String:WTF::FileSystemImpl::fileSystemRepresentation(path).data()]);
        RetainPtr destinationURL = adoptNS([[NSURL alloc] initFileURLWithPath:pathString.get() isDirectory:NO]);

        BOOL bookmarkDataIsStale = NO;
        NSError *bookmarkResolvingError = nil;
        RetainPtr placeholderURL = adoptNS([[NSURL alloc] initByResolvingBookmarkData:placeholderURLBookmark.get() options:0 relativeToURL:nil bookmarkDataIsStale:&bookmarkDataIsStale error:&bookmarkResolvingError]);
        BOOL usingSecurityScopedURL = [placeholderURL startAccessingSecurityScopedResource];

        if (placeholderURL) {
            m_progress = adoptNS([[WKModernDownloadProgress alloc] initWithDownloadTask:m_downloadTask.get() download:*this URL:destinationURL.get() useDownloadPlaceholder:YES resumePlaceholderURL:placeholderURL.get() liveActivityAccessToken:nsActivityAccessToken.get()]);
            startUpdatingProgress();
        } else
            RELEASE_LOG_ERROR(Network, "Download::resume: unable to create resume placeholder URL, error = %@", bookmarkResolvingError);

        if (usingSecurityScopedURL)
            [placeholderURL stopAccessingSecurityScopedResource];

        m_placeholderURL = placeholderURL;
    }
#else
    UNUSED_PARAM(activityAccessToken);
#endif
}
    
void Download::platformCancelNetworkLoad(CompletionHandler<void(std::span<const uint8_t>)>&& completionHandler)
{
    ASSERT(isMainRunLoop());
    ASSERT(m_downloadTask);
    [m_downloadTask cancelByProducingResumeData:makeBlockPtr([completionHandler = WTFMove(completionHandler), placeholderURL = m_placeholderURL] (NSData *resumeData) mutable {
        ensureOnMainRunLoop([resumeData = retainPtr(resumeData), completionHandler = WTFMove(completionHandler), placeholderURL = WTFMove(placeholderURL)] () mutable  {
#if HAVE(MODERN_DOWNLOADPROGRESS)
            auto resumeDataWithPlaceholder = updateResumeDataWithPlaceholderURL(placeholderURL.get(), span(resumeData.get()));
            completionHandler(resumeDataWithPlaceholder.span());
#else
            completionHandler(span(resumeData.get()));
#endif
        });
    }).get()];
}

void Download::platformDestroyDownload()
{
#if HAVE(MODERN_DOWNLOADPROGRESS)
    m_bookmarkURL = nil;
    [m_progress cancel];
#else
    if (m_progress)
#if HAVE(NSPROGRESS_PUBLISHING_SPI)
        [m_progress _unpublish];
#else
        [m_progress unpublish];
#endif // HAVE(NSPROGRESS_PUBLISHING_SPI)
#endif // HAVE(MODERN_DOWNLOADPROGRESS)
}

#if HAVE(MODERN_DOWNLOADPROGRESS)
void Download::publishProgress(const URL& url, std::span<const uint8_t> bookmarkData, UseDownloadPlaceholder useDownloadPlaceholder, std::span<const uint8_t> activityAccessToken)
{
    if (m_progress) {
        RELEASE_LOG(Network, "Progress is already being published for download.");
        return;
    }

    RetainPtr bookmark = toNSData(bookmarkData);
    m_bookmarkData = bookmark;

    RetainPtr accessToken = toNSData(activityAccessToken);

    BOOL bookmarkIsStale = NO;
    NSError* error = nil;
    m_bookmarkURL = [NSURL URLByResolvingBookmarkData:m_bookmarkData.get() options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:&bookmarkIsStale error:&error];
    ASSERT(m_bookmarkURL);
    if (!m_bookmarkURL)
        RELEASE_LOG(Network, "Unable to create bookmark URL, error = %@", error);

    if (enableModernDownloadProgress()) {
        bool isUsingPlaceholder = useDownloadPlaceholder == WebKit::UseDownloadPlaceholder::Yes;

        m_progress = adoptNS([[WKModernDownloadProgress alloc] initWithDownloadTask:m_downloadTask.get() download:*this URL:(NSURL *)url useDownloadPlaceholder:isUsingPlaceholder resumePlaceholderURL:nil liveActivityAccessToken:accessToken.get()]);

        // If we are using a placeholder, we will delay updating progress until the client has received the placeholder URL.
        // This is to make sure the placeholder has not been moved to the final download URL before the client received the placeholder URL.
        if (!isUsingPlaceholder)
            startUpdatingProgress();
    } else {
        m_progress = adoptNS([[WKDownloadProgress alloc] initWithDownloadTask:m_downloadTask.get() download:*this URL:(NSURL *)url sandboxExtension:nullptr]);
#if HAVE(NSPROGRESS_PUBLISHING_SPI)
        [m_progress _publish];
#else
        [m_progress publish];
#endif
    }
}

void Download::setPlaceholderURL(NSURL *placeholderURL, NSData *bookmarkData)
{
    if (!placeholderURL)
        return;

    m_placeholderURL = placeholderURL;

    BOOL usingSecurityScopedURL = [placeholderURL startAccessingSecurityScopedResource];

    SandboxExtension::Handle sandboxExtensionHandle;
    if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(StringView::fromLatin1(placeholderURL.fileSystemRepresentation), SandboxExtension::Type::ReadOnly))
        sandboxExtensionHandle = WTFMove(*handle);

    if (usingSecurityScopedURL)
        [placeholderURL stopAccessingSecurityScopedResource];

    CompletionHandler<void()> completionHandler = [weakThis = WeakPtr { *this }, this] {
        if (!weakThis)
            return;
        // Start updating download progress when the client has received the placeholder URL.
        // Otherwise, the placeholder might have been deleted by the time the client receives it.
        startUpdatingProgress();
    };

    sendWithAsyncReply(Messages::DownloadProxy::DidReceivePlaceholderURL(placeholderURL, span(bookmarkData), WTFMove(sandboxExtensionHandle)), WTFMove(completionHandler));
}

void Download::setFinalURL(NSURL *finalURL, NSData *bookmarkData)
{
    if (!finalURL)
        return;

    BOOL usingSecurityScopedURL = [finalURL startAccessingSecurityScopedResource];

    SandboxExtension::Handle sandboxExtensionHandle;
    if (auto handle = SandboxExtension::createHandleWithoutResolvingPath(StringView::fromLatin1(finalURL.fileSystemRepresentation), SandboxExtension::Type::ReadOnly))
        sandboxExtensionHandle = WTFMove(*handle);

    if (usingSecurityScopedURL)
        [finalURL stopAccessingSecurityScopedResource];

    send(Messages::DownloadProxy::DidReceiveFinalURL(finalURL, span(bookmarkData), WTFMove(sandboxExtensionHandle)));
}

void Download::startUpdatingProgress() const
{
    if (![m_progress isKindOfClass:WKModernDownloadProgress.class])
        return;
    auto *progress = (WKModernDownloadProgress *)m_progress;
    [progress startUpdatingDownloadProgress];
}

Vector<uint8_t> Download::updateResumeDataWithPlaceholderURL(NSURL *placeholderURL, std::span<const uint8_t> resumeData)
{
    if (!placeholderURL)
        return { };

    BOOL usingSecurityScopedURL = [placeholderURL startAccessingSecurityScopedResource];

    NSError *bookmarkError = nil;
    RetainPtr bookmarkData = [placeholderURL bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:&bookmarkError];

    RetainPtr data = toNSData(resumeData);
    RetainPtr dictionary = [NSPropertyListSerialization propertyListWithData:data.get() options:NSPropertyListMutableContainersAndLeaves format:0 error:nullptr];
    [dictionary setObject:bookmarkData.get() forKey:@"ResumePlaceholderURLBookmarkData"];
    NSError *error = nil;
    RetainPtr updatedData = [NSPropertyListSerialization dataWithPropertyList:dictionary.get() format:NSPropertyListXMLFormat_v1_0 options:0 error:&error];

    if (usingSecurityScopedURL)
        [placeholderURL stopAccessingSecurityScopedResource];

    return makeVector(updatedData.get());
}
#else
void Download::publishProgress(const URL& url, SandboxExtension::Handle&& sandboxExtensionHandle)
{
    ASSERT(!m_progress);
    ASSERT(url.isValid());

    auto sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));

    ASSERT(sandboxExtension);
    if (!sandboxExtension)
        return;

    m_progress = adoptNS([[WKDownloadProgress alloc] initWithDownloadTask:m_downloadTask.get() download:*this URL:(NSURL *)url sandboxExtension:sandboxExtension]);
#if HAVE(NSPROGRESS_PUBLISHING_SPI)
    [m_progress _publish];
#else
    [m_progress publish];
#endif
}
#endif

void Download::platformDidFinish(CompletionHandler<void()>&& completionHandler)
{
#if HAVE(MODERN_DOWNLOADPROGRESS)
    if (m_progress && [m_progress isKindOfClass:WKModernDownloadProgress.class]) {
        auto *progress = (WKModernDownloadProgress *)m_progress;
        [progress didFinish:makeBlockPtr(WTFMove(completionHandler)).get()];
        return;
    }
#endif
    completionHandler();
}

}
