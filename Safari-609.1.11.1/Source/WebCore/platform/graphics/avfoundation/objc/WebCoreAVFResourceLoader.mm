/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#import "WebCoreAVFResourceLoader.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_LOADER_DELEGATE)

#import "CachedRawResource.h"
#import "CachedResourceLoader.h"
#import "CachedResourceRequest.h"
#import "MediaPlayerPrivateAVFoundationObjC.h"
#import "ResourceLoaderOptions.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import <AVFoundation/AVAssetResourceLoader.h>
#import <objc/runtime.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/CString.h>

namespace WebCore {

Ref<WebCoreAVFResourceLoader> WebCoreAVFResourceLoader::create(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *avRequest)
{
    ASSERT(avRequest);
    ASSERT(parent);
    return adoptRef(*new WebCoreAVFResourceLoader(parent, avRequest));
}

WebCoreAVFResourceLoader::WebCoreAVFResourceLoader(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *avRequest)
    : m_parent(parent)
    , m_avRequest(avRequest)
{
}

WebCoreAVFResourceLoader::~WebCoreAVFResourceLoader()
{
    stopLoading();
}

void WebCoreAVFResourceLoader::startLoading()
{
    if (m_resource || !m_parent)
        return;

    NSURLRequest *nsRequest = [m_avRequest.get() request];

    ResourceRequest resourceRequest(nsRequest);
    resourceRequest.setPriority(ResourceLoadPriority::Low);

    // FIXME: Skip Content Security Policy check if the element that inititated this request
    // is in a user-agent shadow tree. See <https://bugs.webkit.org/show_bug.cgi?id=173498>.
    CachedResourceRequest request(WTFMove(resourceRequest), ResourceLoaderOptions(
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::DoNotSniffContent,
        DataBufferingPolicy::BufferData,
        StoredCredentialsPolicy::DoNotUse,
        ClientCredentialPolicy::CannotAskClientForCredentials,
        FetchOptions::Credentials::Omit,
        SecurityCheckPolicy::DoSecurityCheck,
        FetchOptions::Mode::NoCors,
        CertificateInfoPolicy::DoNotIncludeCertificateInfo,
        ContentSecurityPolicyImposition::DoPolicyCheck,
        DefersLoadingPolicy::AllowDefersLoading,
        CachingPolicy::DisallowCaching));
    if (auto* loader = m_parent->player()->cachedResourceLoader())
        m_resource = loader->requestMedia(WTFMove(request)).value_or(nullptr);

    if (m_resource)
        m_resource->addClient(*this);
    else {
        LOG_ERROR("Failed to start load for media at url %s", [[[nsRequest URL] absoluteString] UTF8String]);
        [m_avRequest.get() finishLoadingWithError:0];
    }
}

void WebCoreAVFResourceLoader::stopLoading()
{
    if (!m_resource)
        return;

    m_resource->removeClient(*this);
    m_resource = 0;

    if (m_parent && m_avRequest)
        m_parent->didStopLoadingRequest(m_avRequest.get());
}

void WebCoreAVFResourceLoader::invalidate()
{
    if (!m_parent)
        return;

    m_parent = nullptr;

    callOnMainThread([protectedThis = makeRef(*this)] () mutable {
        protectedThis->stopLoading();
    });
}

void WebCoreAVFResourceLoader::responseReceived(CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));

    int status = response.httpStatusCode();
    if (status && (status < 200 || status > 299)) {
        [m_avRequest.get() finishLoadingWithError:0];
        return;
    }

    if (AVAssetResourceLoadingContentInformationRequest* contentInfo = [m_avRequest.get() contentInformationRequest]) {
        String uti = UTIFromMIMEType(response.mimeType());

        [contentInfo setContentType:uti];

        const ParsedContentRange& contentRange = m_resource->response().contentRange();
        [contentInfo setContentLength:contentRange.isValid() ? contentRange.instanceLength() : response.expectedContentLength()];
        [contentInfo setByteRangeAccessSupported:YES];

        if (![m_avRequest dataRequest]) {
            [m_avRequest.get() finishLoading];
            stopLoading();
        }
    }
}

void WebCoreAVFResourceLoader::dataReceived(CachedResource& resource, const char*, int)
{
    fulfillRequestWithResource(resource);
}

void WebCoreAVFResourceLoader::notifyFinished(CachedResource& resource)
{
    if (resource.loadFailedOrCanceled()) {
        // <rdar://problem/13987417> Set the contentType of the contentInformationRequest to an empty
        // string to trigger AVAsset's playable value to complete loading.
        if ([m_avRequest.get() contentInformationRequest] && ![[m_avRequest.get() contentInformationRequest] contentType])
            [[m_avRequest.get() contentInformationRequest] setContentType:@""];

        NSError* error = resource.errorOccurred() ? resource.resourceError().nsError() : nil;
        [m_avRequest.get() finishLoadingWithError:error];
    } else {
        fulfillRequestWithResource(resource);
        [m_avRequest.get() finishLoading];
    }
    stopLoading();
}

void WebCoreAVFResourceLoader::fulfillRequestWithResource(CachedResource& resource)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    AVAssetResourceLoadingDataRequest* dataRequest = [m_avRequest dataRequest];
    if (!dataRequest)
        return;

    SharedBuffer* data = m_resource->resourceBuffer();
    if (!data)
        return;

    NSUInteger responseOffset = 0;
    const ParsedContentRange& contentRange = m_resource->response().contentRange();
    if (contentRange.isValid())
        responseOffset = static_cast<NSUInteger>(contentRange.firstBytePosition());

    // Check for possible unsigned overflow.
    ASSERT(dataRequest.currentOffset >= dataRequest.requestedOffset);
    ASSERT(dataRequest.requestedLength >= (dataRequest.currentOffset - dataRequest.requestedOffset));

    NSUInteger remainingLength = dataRequest.requestedLength - static_cast<NSUInteger>(dataRequest.currentOffset - dataRequest.requestedOffset);

    auto bytesToSkip = dataRequest.currentOffset - responseOffset;
    RetainPtr<NSArray> array = data->createNSDataArray();
    for (NSData *segment in array.get()) {
        if (bytesToSkip) {
            if (bytesToSkip > segment.length) {
                bytesToSkip -= segment.length;
                continue;
            }
            auto bytesToUse = segment.length - bytesToSkip;
            [dataRequest respondWithData:[segment subdataWithRange:NSMakeRange(static_cast<NSUInteger>(bytesToSkip), static_cast<NSUInteger>(segment.length - bytesToSkip))]];
            bytesToSkip = 0;
            remainingLength -= bytesToUse;
            continue;
        }
        if (segment.length <= remainingLength) {
            [dataRequest respondWithData:segment];
            remainingLength -= segment.length;
            continue;
        }
        [dataRequest respondWithData:[segment subdataWithRange:NSMakeRange(0, remainingLength)]];
        remainingLength = 0;
        break;
    }

    // There was not enough data in the buffer to satisfy the data request.
    if (remainingLength)
        return;

    if (dataRequest.currentOffset + dataRequest.requestedLength >= dataRequest.requestedOffset) {
        [m_avRequest.get() finishLoading];
        stopLoading();
    }
}

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
