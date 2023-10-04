/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#import "CachedRawResource.h"
#import "CachedResourceLoader.h"
#import "CachedResourceRequest.h"
#import "DataURLDecoder.h"
#import "MediaPlayerPrivateAVFoundationObjC.h"
#import "PlatformMediaResourceLoader.h"
#import "ResourceLoaderOptions.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import <AVFoundation/AVAssetResourceLoader.h>
#import <objc/runtime.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/CString.h>

@interface AVAssetResourceLoadingContentInformationRequest (WebKitExtensions)
@property (nonatomic, getter=isEntireLengthAvailableOnDemand) BOOL entireLengthAvailableOnDemand;
@end

namespace WebCore {

class CachedResourceMediaLoader final : CachedRawResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<CachedResourceMediaLoader> create(WebCoreAVFResourceLoader&, CachedResourceLoader&, ResourceRequest&&);
    ~CachedResourceMediaLoader() { stop(); }

private:
    CachedResourceMediaLoader(WebCoreAVFResourceLoader&, CachedResourceHandle<CachedRawResource>&&);

    void stop();

    // CachedRawResourceClient
    void responseReceived(CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) final;
    void dataReceived(CachedResource&, const SharedBuffer&) final;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;

    void fulfillRequestWithResource(CachedResource&);

    WebCoreAVFResourceLoader& m_parent;
    CachedResourceHandle<CachedRawResource> m_resource;
};

std::unique_ptr<CachedResourceMediaLoader> CachedResourceMediaLoader::create(WebCoreAVFResourceLoader& parent, CachedResourceLoader& loader, ResourceRequest&& resourceRequest)
{
    // FIXME: Skip Content Security Policy check if the element that inititated this request
    // is in a user-agent shadow tree. See <https://bugs.webkit.org/show_bug.cgi?id=173498>.
    ResourceLoaderOptions loaderOptions(
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
        CachingPolicy::DisallowCaching
    );
    loaderOptions.destination = FetchOptions::Destination::Video;
    CachedResourceRequest request(WTFMove(resourceRequest), loaderOptions);

    auto resource = loader.requestMedia(WTFMove(request)).value_or(nullptr);
    if (!resource)
        return nullptr;
    return std::unique_ptr<CachedResourceMediaLoader>(new CachedResourceMediaLoader { parent, WTFMove(resource) });
}

CachedResourceMediaLoader::CachedResourceMediaLoader(WebCoreAVFResourceLoader& parent, CachedResourceHandle<CachedRawResource>&& resource)
    : m_parent(parent)
    , m_resource(WTFMove(resource))
{
    m_resource->addClient(*this);
}

void CachedResourceMediaLoader::stop()
{
    if (!m_resource)
        return;

    m_resource->removeClient(*this);
    m_resource = nullptr;
}

void CachedResourceMediaLoader::responseReceived(CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));

    m_parent.responseReceived(response);
}

void CachedResourceMediaLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    if (resource.loadFailedOrCanceled()) {
        m_parent.loadFailed(resource.resourceError());
        return;
    }
    m_parent.loadFinished();
}

void CachedResourceMediaLoader::dataReceived(CachedResource& resource, const SharedBuffer&)
{
    ASSERT(&resource == m_resource);
    if (auto* data = resource.resourceBuffer())
        m_parent.newDataStoredInSharedBuffer(*data);
}

class PlatformResourceMediaLoader final : public PlatformMediaResourceClient, public CanMakeWeakPtr<PlatformResourceMediaLoader> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static WeakPtr<PlatformResourceMediaLoader> create(WebCoreAVFResourceLoader&, PlatformMediaResourceLoader&, ResourceRequest&&);
    ~PlatformResourceMediaLoader() { stop(); }

    void stop();

private:
    PlatformResourceMediaLoader(WebCoreAVFResourceLoader&, Ref<PlatformMediaResource>&&);

    void loadFailed(const ResourceError&);
    void loadFinished();

    // PlatformMediaResourceClient
    void responseReceived(PlatformMediaResource&, const ResourceResponse&, CompletionHandler<void(ShouldContinuePolicyCheck)>&&) final;
    void redirectReceived(PlatformMediaResource&, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) final { completionHandler(WTFMove(request)); }
    bool shouldCacheResponse(PlatformMediaResource&, const ResourceResponse&) final { return false; }
    void dataSent(PlatformMediaResource&, unsigned long long, unsigned long long) final { }
    void dataReceived(PlatformMediaResource&, const SharedBuffer&) final;
    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError& error) final { loadFailed(error); }
    void loadFailed(PlatformMediaResource&, const ResourceError& error) final { loadFailed(error); }
    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final { loadFinished(); }

    WebCoreAVFResourceLoader& m_parent;
    RefPtr<PlatformMediaResource> m_resource;
    SharedBufferBuilder m_buffer;
};

WeakPtr<PlatformResourceMediaLoader> PlatformResourceMediaLoader::create(WebCoreAVFResourceLoader& parent, PlatformMediaResourceLoader& loader, ResourceRequest&& request)
{
    auto resource = loader.requestResource(WTFMove(request), PlatformMediaResourceLoader::LoadOption::DisallowCaching);
    if (!resource)
        return nullptr;
    auto client = adoptRef(*new PlatformResourceMediaLoader { parent, Ref { *resource } });
    WeakPtr result = client;

    resource->setClient(WTFMove(client));
    return result;
}

PlatformResourceMediaLoader::PlatformResourceMediaLoader(WebCoreAVFResourceLoader& parent, Ref<PlatformMediaResource>&& resource)
    : m_parent(parent)
    , m_resource(WTFMove(resource))
{
}

void PlatformResourceMediaLoader::stop()
{
    if (!m_resource)
        return;

    auto resource = WTFMove(m_resource);
    resource->stop();
    resource->setClient(nullptr);
}

void PlatformResourceMediaLoader::responseReceived(PlatformMediaResource&, const ResourceResponse& response, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler)
{
    m_parent.responseReceived(response);
    completionHandler(ShouldContinuePolicyCheck::Yes);
}

void PlatformResourceMediaLoader::loadFailed(const ResourceError& error)
{
    m_parent.loadFailed(error);
}

void PlatformResourceMediaLoader::loadFinished()
{
    m_parent.loadFinished();
}

void PlatformResourceMediaLoader::dataReceived(PlatformMediaResource&, const SharedBuffer& buffer)
{
    m_buffer.append(buffer);
    m_parent.newDataStoredInSharedBuffer(*m_buffer.get());
}

class DataURLResourceMediaLoader : public CanMakeWeakPtr<DataURLResourceMediaLoader> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DataURLResourceMediaLoader(WebCoreAVFResourceLoader&, ResourceRequest&&);

private:
    WebCoreAVFResourceLoader& m_parent;
    ResourceResponse m_response;
    RefPtr<SharedBuffer> m_buffer;
};

DataURLResourceMediaLoader::DataURLResourceMediaLoader(WebCoreAVFResourceLoader& parent, ResourceRequest&& request)
    : m_parent(parent)
{
    RELEASE_ASSERT(request.url().protocolIsData());

    if (auto result = DataURLDecoder::decode(request.url())) {
        m_response = ResourceResponse::dataURLResponse(request.url(), *result);
        m_buffer = SharedBuffer::create(WTFMove(result->data));
    }

    callOnMainThread([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        if (!m_buffer || m_response.isNull()) {
            m_parent.loadFailed(ResourceError(ResourceError::Type::General));
            return;
        }
        m_parent.responseReceived(m_response);
        if (!weakThis)
            return;

        m_parent.newDataStoredInSharedBuffer(*m_buffer);
        if (!weakThis)
            return;

        m_parent.loadFinished();
    });
}

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
    if (m_dataURLMediaLoader || m_resourceMediaLoader || m_platformMediaLoader || !m_parent)
        return;

    NSURLRequest *nsRequest = [m_avRequest request];

    ResourceRequest request(nsRequest);
    request.setPriority(ResourceLoadPriority::Low);

    if (AVAssetResourceLoadingDataRequest *dataRequest = [m_avRequest dataRequest]; dataRequest.requestedLength
        && !request.hasHTTPHeaderField(HTTPHeaderName::Range)) {
        String rangeEnd = dataRequest.requestsAllDataToEndOfResource ? emptyString() : makeString(dataRequest.requestedOffset + dataRequest.requestedLength - 1);
        request.addHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes=", dataRequest.requestedOffset, '-', rangeEnd));
    }

    if (request.url().protocolIsData()) {
        m_dataURLMediaLoader = WTF::makeUnique<DataURLResourceMediaLoader>(*this, WTFMove(request));
        return;
    }

    if (auto* loader = m_parent->player()->cachedResourceLoader()) {
        m_resourceMediaLoader = CachedResourceMediaLoader::create(*this, *loader, ResourceRequest(request));
        if (m_resourceMediaLoader)
            return;
    }

    if (auto loader = m_parent->player()->createResourceLoader()) {
        m_platformMediaLoader = PlatformResourceMediaLoader::create(*this, *loader, WTFMove(request));
        if (m_platformMediaLoader)
            return;
    }

    LOG_ERROR("Failed to start load for media at url %s", [[[nsRequest URL] absoluteString] UTF8String]);
    [m_avRequest finishLoadingWithError:0];
}

void WebCoreAVFResourceLoader::stopLoading()
{
    m_dataURLMediaLoader = nullptr;
    m_resourceMediaLoader = nullptr;

    if (m_platformMediaLoader) {
        m_platformMediaLoader->stop();
        m_platformMediaLoader = nullptr;
    }
    if (m_parent && m_avRequest)
        m_parent->didStopLoadingRequest(m_avRequest.get());
}

void WebCoreAVFResourceLoader::invalidate()
{
    if (!m_parent)
        return;

    m_parent = nullptr;

    callOnMainThread([protectedThis = Ref { *this }] () mutable {
        protectedThis->stopLoading();
    });
}

void WebCoreAVFResourceLoader::responseReceived(const ResourceResponse& response)
{
    int status = response.httpStatusCode();
    if (status && (status < 200 || status > 299)) {
        [m_avRequest finishLoadingWithError:0];
        return;
    }

    auto& contentRange = response.contentRange();
    if (contentRange.isValid())
        m_responseOffset = static_cast<NSUInteger>(contentRange.firstBytePosition());

    if (AVAssetResourceLoadingContentInformationRequest* contentInfo = [m_avRequest contentInformationRequest]) {
        String uti = UTIFromMIMEType(response.mimeType());

        [contentInfo setContentType:uti];

        [contentInfo setContentLength:contentRange.isValid() ? contentRange.instanceLength() : response.expectedContentLength()];
        [contentInfo setByteRangeAccessSupported:YES];

        // Do not set "EntireLengthAvailableOnDemand" to YES when the loader is DataURLResourceMediaLoader.
        // When the property is YES, AVAssetResourceLoader will request small data ranges over and over again
        // during the playback. For DataURLResourceMediaLoader, that means it needs to decode the URL repeatedly,
        // which is very inefficient for long URLs.
        if (!m_dataURLMediaLoader && [contentInfo respondsToSelector:@selector(setEntireLengthAvailableOnDemand:)])
            [contentInfo setEntireLengthAvailableOnDemand:YES];

        if (![m_avRequest dataRequest]) {
            [m_avRequest finishLoading];
            stopLoading();
        }
    }
}

void WebCoreAVFResourceLoader::loadFailed(const ResourceError& error)
{
    // <rdar://problem/13987417> Set the contentType of the contentInformationRequest to an empty
    // string to trigger AVAsset's playable value to complete loading.
    if ([m_avRequest contentInformationRequest] && ![[m_avRequest contentInformationRequest] contentType])
        [[m_avRequest contentInformationRequest] setContentType:@""];

    [m_avRequest finishLoadingWithError:error.nsError()];
    stopLoading();
}

void WebCoreAVFResourceLoader::loadFinished()
{
    [m_avRequest finishLoading];
    stopLoading();
}

void WebCoreAVFResourceLoader::newDataStoredInSharedBuffer(const FragmentedSharedBuffer& data)
{
    AVAssetResourceLoadingDataRequest* dataRequest = [m_avRequest dataRequest];
    if (!dataRequest)
        return;

    // Check for possible unsigned overflow.
    ASSERT(dataRequest.currentOffset >= dataRequest.requestedOffset);
    ASSERT(dataRequest.requestedLength >= (dataRequest.currentOffset - dataRequest.requestedOffset));

    NSUInteger remainingLength = dataRequest.requestedLength - static_cast<NSUInteger>(dataRequest.currentOffset - dataRequest.requestedOffset);

    auto bytesToSkip = dataRequest.currentOffset - m_responseOffset;
    auto array = data.createNSDataArray();
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
        [m_avRequest finishLoading];
        stopLoading();
    }
}

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
