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
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Scope.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>
#import <wtf/text/CString.h>
#import <wtf/text/MakeString.h>

@interface AVAssetResourceLoadingContentInformationRequest (WebKitExtensions)
@property (nonatomic, getter=isEntireLengthAvailableOnDemand) BOOL entireLengthAvailableOnDemand;
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCoreAVFResourceLoader);

class CachedResourceMediaLoader final : CachedRawResourceClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CachedResourceMediaLoader);
public:
    static std::unique_ptr<CachedResourceMediaLoader> create(WebCoreAVFResourceLoader&, CachedResourceLoader&, ResourceRequest&&);
    ~CachedResourceMediaLoader() { stop(); }

private:
    CachedResourceMediaLoader(WebCoreAVFResourceLoader&, CachedResourceHandle<CachedRawResource>&&);

    void stop();

    // CachedRawResourceClient
    void responseReceived(CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) final;
    void dataReceived(CachedResource&, const SharedBuffer&) final;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;

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

    m_parent.responseReceived(response.mimeType(), response.httpStatusCode(), response.contentRange(), response.expectedContentLength());
}

void CachedResourceMediaLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
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

class PlatformResourceMediaLoader final : public PlatformMediaResourceClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PlatformResourceMediaLoader);
public:
    static RefPtr<PlatformResourceMediaLoader> create(WebCoreAVFResourceLoader&, PlatformMediaResourceLoader&, ResourceRequest&&);
    ~PlatformResourceMediaLoader() = default;

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
    Ref<GuaranteedSerialFunctionDispatcher> m_targetDispatcher;
    RefPtr<PlatformMediaResource> m_resource WTF_GUARDED_BY_CAPABILITY(m_targetDispatcher.get());
    SharedBufferBuilder m_buffer WTF_GUARDED_BY_CAPABILITY(m_targetDispatcher.get());
};

RefPtr<PlatformResourceMediaLoader> PlatformResourceMediaLoader::create(WebCoreAVFResourceLoader& parent, PlatformMediaResourceLoader& loader, ResourceRequest&& request)
{
    auto resource = loader.requestResource(WTFMove(request), PlatformMediaResourceLoader::LoadOption::DisallowCaching);
    if (!resource)
        return nullptr;
    auto* resourcePointer = resource.get();
    auto client = adoptRef(*new PlatformResourceMediaLoader { parent, resource.releaseNonNull() });
    resourcePointer->setClient(client.copyRef());
    return client;
}

PlatformResourceMediaLoader::PlatformResourceMediaLoader(WebCoreAVFResourceLoader& parent, Ref<PlatformMediaResource>&& resource)
    : m_parent(parent)
    , m_targetDispatcher(parent.m_targetDispatcher)
    , m_resource(WTFMove(resource))
{
}

void PlatformResourceMediaLoader::stop()
{
    assertIsCurrent(m_targetDispatcher.get());

    if (!m_resource)
        return;

    m_resource->shutdown();
    m_resource = nullptr;
}

void PlatformResourceMediaLoader::responseReceived(PlatformMediaResource&, const ResourceResponse& response, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler)
{
    assertIsCurrent(m_targetDispatcher.get());

    m_parent.responseReceived(response.mimeType(), response.httpStatusCode(), response.contentRange(), response.expectedContentLength());
    completionHandler(ShouldContinuePolicyCheck::Yes);
}

void PlatformResourceMediaLoader::loadFailed(const ResourceError& error)
{
    assertIsCurrent(m_targetDispatcher.get());

    m_parent.loadFailed(error);
}

void PlatformResourceMediaLoader::loadFinished()
{
    assertIsCurrent(m_targetDispatcher.get());

    m_parent.loadFinished();
}

void PlatformResourceMediaLoader::dataReceived(PlatformMediaResource&, const SharedBuffer& buffer)
{
    assertIsCurrent(m_targetDispatcher.get());

    m_buffer.append(buffer);
    m_parent.newDataStoredInSharedBuffer(*m_buffer.get());
}

class DataURLResourceMediaLoader {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(DataURLResourceMediaLoader);
public:
    DataURLResourceMediaLoader(WebCoreAVFResourceLoader&, ResourceRequest&&);

private:
    WebCoreAVFResourceLoader& m_parent;
};

DataURLResourceMediaLoader::DataURLResourceMediaLoader(WebCoreAVFResourceLoader& parent, ResourceRequest&& request)
    : m_parent(parent)
{
    RELEASE_ASSERT(request.url().protocolIsData());
    ASSERT(isMainThread());

    if (auto result = DataURLDecoder::decode(request.url())) {
        // data URLs can end up being unreasonably big so we want to avoid having to call isolatedCopy() here when passing the URL String to the loading thread.
        auto response = ResourceResponse::dataURLResponse(request.url(), *result);
        auto buffer = SharedBuffer::create(WTFMove(result->data));
        m_parent.m_targetDispatcher->dispatch([this, parent = Ref { m_parent }, buffer = WTFMove(buffer), mimeType = response.mimeType().isolatedCopy(), status =  response.httpStatusCode(), contentRange = response.contentRange()] {

            if (m_parent.m_dataURLMediaLoader.get() != this)
                return;

            if (m_parent.responseReceived(mimeType, status, contentRange, buffer->size()))
                return;

            if (m_parent.newDataStoredInSharedBuffer(buffer))
                return;

            m_parent.loadFinished();
        });
    } else {
        m_parent.m_targetDispatcher->dispatch([this, parent = Ref { m_parent }] {
            if (m_parent.m_dataURLMediaLoader.get() != this)
                return;
            m_parent.loadFailed(ResourceError(ResourceError::Type::General));
        });
    }
}

Ref<WebCoreAVFResourceLoader> WebCoreAVFResourceLoader::create(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *avRequest, GuaranteedSerialFunctionDispatcher& targetQueue)
{
    ASSERT(avRequest);
    ASSERT(parent);
    return adoptRef(*new WebCoreAVFResourceLoader(parent, avRequest, targetQueue));
}

WebCoreAVFResourceLoader::WebCoreAVFResourceLoader(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *avRequest, GuaranteedSerialFunctionDispatcher& targetDispatcher)
    : m_parent(parent)
    , m_avRequest(avRequest)
    , m_targetDispatcher(targetDispatcher)
{
}

WebCoreAVFResourceLoader::~WebCoreAVFResourceLoader()
{
}

void WebCoreAVFResourceLoader::startLoading()
{
    // Called from the main thread, before any loaders are created.
    assertIsMainThread();

    RefPtr parent = m_parent.get();

    if (m_dataURLMediaLoader || m_resourceMediaLoader || m_platformMediaLoader || !parent)
        return;

    NSURLRequest *nsRequest = [m_avRequest request];

    ResourceRequest request(nsRequest);
    request.setPriority(ResourceLoadPriority::Low);

    AVAssetResourceLoadingDataRequest* dataRequest = [m_avRequest dataRequest];
    m_currentOffset = m_requestedOffset = dataRequest ? [dataRequest requestedOffset] : -1;
    m_requestedLength = dataRequest ? [dataRequest requestedLength] : -1;

    if (dataRequest && m_requestedLength > 0
        && !request.hasHTTPHeaderField(HTTPHeaderName::Range)) {
        String rangeEnd = dataRequest.requestsAllDataToEndOfResource ? emptyString() : makeString(m_requestedOffset + m_requestedLength - 1);
        request.addHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, m_requestedOffset, '-', rangeEnd));
    }

    if (request.url().protocolIsData()) {
        m_dataURLMediaLoader = WTF::makeUnique<DataURLResourceMediaLoader>(*this, WTFMove(request));
        return;
    }

#if PLATFORM(IOS_FAMILY)
    m_isBlob = request.url().protocolIsBlob();
#endif

    if (auto* loader = parent->player()->cachedResourceLoader()) {
        m_resourceMediaLoader = CachedResourceMediaLoader::create(*this, *loader, ResourceRequest(request));
        if (m_resourceMediaLoader)
            return;
    }

    m_platformMediaLoader = PlatformResourceMediaLoader::create(*this, parent->player()->mediaResourceLoader(), WTFMove(request));
    if (m_platformMediaLoader)
        return;

    LOG_ERROR("Failed to start load for media at url %s", [[[nsRequest URL] absoluteString] UTF8String]);
    [m_avRequest finishLoadingWithError:0];
}

// No code accessing `this` should ever be used after calling stopLoading().
void WebCoreAVFResourceLoader::stopLoading()
{
    assertIsCurrent(m_targetDispatcher.get());

    m_dataURLMediaLoader = nullptr;
    m_resourceMediaLoader = nullptr;

    if (m_platformMediaLoader)
        m_platformMediaLoader->stop();

    callOnMainThread([m_parent = WTFMove(m_parent), loader = WTFMove(m_platformMediaLoader), avRequest = WTFMove(m_avRequest)] {
        if (RefPtr parent = m_parent.get(); parent && avRequest)
            parent->didStopLoadingRequest(avRequest.get());
    });

    ASSERT(!m_platformMediaLoader && !m_avRequest);
}

bool WebCoreAVFResourceLoader::responseReceived(const String& mimeType, int status, const ParsedContentRange& contentRange, size_t expectedContentLength)
{
    assertIsCurrent(m_targetDispatcher.get());

    if (status && (status < 200 || status > 299)) {
        [m_avRequest finishLoadingWithError:0];
        return true;
    }

    if (contentRange.isValid())
        m_responseOffset = static_cast<NSUInteger>(contentRange.firstBytePosition());

    if (AVAssetResourceLoadingContentInformationRequest* contentInfo = [m_avRequest contentInformationRequest]) {
        String uti = UTIFromMIMEType(mimeType);

        [contentInfo setContentType:uti];

        [contentInfo setContentLength:contentRange.isValid() ? contentRange.instanceLength() : expectedContentLength];
        [contentInfo setByteRangeAccessSupported:YES];

        // Do not set "EntireLengthAvailableOnDemand" to YES when the loader is DataURLResourceMediaLoader.
        // When the property is YES, AVAssetResourceLoader will request small data ranges over and over again
        // during the playback. For DataURLResourceMediaLoader, that means it needs to decode the URL repeatedly,
        // which is very inefficient for long URLs.
        // FIXME: don't have blob exception once rdar://132719739 is fixed.
        if (!m_dataURLMediaLoader && !m_isBlob && [contentInfo respondsToSelector:@selector(setEntireLengthAvailableOnDemand:)])
            [contentInfo setEntireLengthAvailableOnDemand:YES];

        if (![m_avRequest dataRequest]) {
            BEGIN_BLOCK_OBJC_EXCEPTIONS
            [m_avRequest finishLoading];
            END_BLOCK_OBJC_EXCEPTIONS
            stopLoading();
            return true;
        }
    }
    return false;
}

void WebCoreAVFResourceLoader::loadFailed(const ResourceError& error)
{
    assertIsCurrent(m_targetDispatcher.get());

    // <rdar://problem/13987417> Set the contentType of the contentInformationRequest to an empty
    // string to trigger AVAsset's playable value to complete loading.
    if ([m_avRequest contentInformationRequest] && ![[m_avRequest contentInformationRequest] contentType])
        [[m_avRequest contentInformationRequest] setContentType:@""];

    [m_avRequest finishLoadingWithError:error.nsError()];
    stopLoading();
}

void WebCoreAVFResourceLoader::loadFinished()
{
    assertIsCurrent(m_targetDispatcher.get());

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_avRequest finishLoading];
    END_BLOCK_OBJC_EXCEPTIONS
    stopLoading();
}

bool WebCoreAVFResourceLoader::newDataStoredInSharedBuffer(const FragmentedSharedBuffer& data)
{
    assertIsCurrent(m_targetDispatcher.get());

    AVAssetResourceLoadingDataRequest* dataRequest = [m_avRequest dataRequest];
    if (!dataRequest)
        return true;

    // Check for possible unsigned overflow.
    ASSERT(m_currentOffset >= m_requestedOffset);
    ASSERT(m_requestedLength >= m_currentOffset - m_requestedOffset);

    NSUInteger remainingLength = m_requestedLength - (m_currentOffset - m_requestedOffset);

    auto bytesToSkip = m_currentOffset - m_responseOffset;
    auto array = data.createNSDataArray();
    for (NSData* segment in array.get()) {
        if (bytesToSkip) {
            if (bytesToSkip > segment.length) {
                bytesToSkip -= segment.length;
                continue;
            }
            auto bytesToUse = segment.length - bytesToSkip;
            [dataRequest respondWithData:[segment subdataWithRange:NSMakeRange(static_cast<NSUInteger>(bytesToSkip), static_cast<NSUInteger>(bytesToUse))]];
            bytesToSkip = 0;
            remainingLength -= bytesToUse;
            m_currentOffset += bytesToUse;
            continue;
        }
        if (segment.length <= remainingLength) {
            [dataRequest respondWithData:segment];
            remainingLength -= segment.length;
            m_currentOffset += segment.length;
            continue;
        }
        [dataRequest respondWithData:[segment subdataWithRange:NSMakeRange(0, remainingLength)]];
        m_currentOffset += remainingLength;
        remainingLength = 0;
        break;
    }

    // There was not enough data in the buffer to satisfy the data request.
    if (remainingLength)
        return false;

    if (m_currentOffset >= m_requestedOffset + m_requestedLength) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [m_avRequest finishLoading];
        END_BLOCK_OBJC_EXCEPTIONS
        stopLoading();
        return true;
    }
    return false;
}

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
