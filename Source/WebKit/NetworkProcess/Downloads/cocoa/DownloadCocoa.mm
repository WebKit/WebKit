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

#import "DataReference.h"
#import "NetworkSessionCocoa.h"
#import "WKDownloadProgress.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSProgressSPI.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

void Download::resume(const IPC::DataReference& resumeData, const String& path, SandboxExtension::Handle&& sandboxExtensionHandle)
{
    m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle));
    if (m_sandboxExtension)
        m_sandboxExtension->consume();

    auto* networkSession = m_downloadManager.client().networkSession(m_sessionID);
    if (!networkSession) {
        WTFLogAlways("Could not find network session with given session ID");
        return;
    }
    auto& cocoaSession = static_cast<NetworkSessionCocoa&>(*networkSession);
    auto nsData = adoptNS([[NSData alloc] initWithBytes:resumeData.data() length:resumeData.size()]);

    // FIXME: This is a temporary workaround for <rdar://problem/34745171>. Fixed in iOS 13 and macOS 10.15, but we still need to support macOS 10.14 for now.
#if USE(LEGACY_CFNETWORK_DOWNLOADS)
    static NSSet<Class> *plistClasses = nil;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        plistClasses = [[NSSet setWithObjects:[NSDictionary class], [NSArray class], [NSString class], [NSNumber class], [NSData class], [NSURL class], [NSURLRequest class], nil] retain];
    });
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:nsData.get() error:nil]);
    [unarchiver setDecodingFailurePolicy:NSDecodingFailurePolicyRaiseException];
    auto dictionary = adoptNS(static_cast<NSMutableDictionary *>([[unarchiver decodeObjectOfClasses:plistClasses forKey:@"NSKeyedArchiveRootObjectKey"] mutableCopy]));
    [unarchiver finishDecoding];
    [dictionary setObject:static_cast<NSString*>(path) forKey:@"NSURLSessionResumeInfoLocalPath"];
    auto encoder = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);
    [encoder encodeObject:dictionary.get() forKey:@"NSKeyedArchiveRootObjectKey"];
    NSData *updatedData = [encoder encodedData];
#else
    NSMutableDictionary *dictionary = [NSPropertyListSerialization propertyListWithData:nsData.get() options:NSPropertyListMutableContainersAndLeaves format:0 error:nullptr];
    [dictionary setObject:static_cast<NSString*>(path) forKey:@"NSURLSessionResumeInfoLocalPath"];
    NSData *updatedData = [NSPropertyListSerialization dataWithPropertyList:dictionary format:NSPropertyListXMLFormat_v1_0 options:0 error:nullptr];
#endif

    // FIXME: Use nsData instead of updatedData once we've migrated from _WKDownload to WKDownload
    // because there's no reason to set the local path we got from the data back into the data.
    m_downloadTask = [cocoaSession.sessionWrapperForDownloads().session downloadTaskWithResumeData:updatedData];
    auto taskIdentifier = [m_downloadTask taskIdentifier];
    ASSERT(!cocoaSession.sessionWrapperForDownloads().downloadMap.contains(taskIdentifier));
    cocoaSession.sessionWrapperForDownloads().downloadMap.add(taskIdentifier, m_downloadID);
    m_downloadTask.get()._pathToDownloadTaskFile = path;

    [m_downloadTask resume];
}
    
void Download::platformCancelNetworkLoad(CompletionHandler<void(const IPC::DataReference&)>&& completionHandler)
{
    ASSERT(m_downloadTask);
    [m_downloadTask cancelByProducingResumeData:makeBlockPtr([path = retainPtr(m_downloadTask.get()._pathToDownloadTaskFile), completionHandler = WTFMove(completionHandler)] (NSData *resumeData) mutable {
#if USE(LEGACY_CFNETWORK_DOWNLOADS)
        if (resumeData) {
            // Mojave does not include the download location in the resume data from CFNetwork. Add it from the task.
            auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:resumeData error:nil]);
            [unarchiver setDecodingFailurePolicy:NSDecodingFailurePolicyRaiseException];
            auto dictionary = adoptNS(static_cast<NSMutableDictionary *>([[unarchiver decodeObjectOfClasses:[NSSet setWithObjects:[NSDictionary class], [NSArray class], [NSString class], [NSNumber class], [NSData class], [NSURL class], [NSURLRequest class], nil] forKey:@"NSKeyedArchiveRootObjectKey"] mutableCopy]));
            [unarchiver finishDecoding];
            [dictionary setObject:path.get() forKey:@"NSURLSessionResumeInfoLocalPath"];
            auto encoder = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);
            [encoder encodeObject:dictionary.get() forKey:@"NSKeyedArchiveRootObjectKey"];
            resumeData = [encoder encodedData];
        }
#else
        UNUSED_PARAM(path);
#endif
        auto resumeDataReference = resumeData ? IPC::DataReference { static_cast<const uint8_t*>(resumeData.bytes), resumeData.length } : IPC::DataReference { };
        completionHandler(resumeDataReference);
    }).get()];
}

void Download::platformDestroyDownload()
{
    if (m_progress)
#if HAVE(NSPROGRESS_PUBLISHING_SPI)
        [m_progress _unpublish];
#else
        [m_progress unpublish];
#endif
}

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

}
