/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
#import "Logging.h"
#import "MediaPlayerPrivateAVFoundationObjC.h"
#import "PlatformMediaResourceLoader.h"
#import "ResourceLoaderOptions.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import <AVFoundation/AVAssetResourceLoader.h>
#import <objc/runtime.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/LoggerHelper.h>
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

class CachedResourceMediaLoader final : public RefCounted<CachedResourceMediaLoader>, CachedRawResourceClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CachedResourceMediaLoader);
public:
    static RefPtr<CachedResourceMediaLoader> create(WebCoreAVFResourceLoader&, CachedResourceLoader&, ResourceRequest&&);
    ~CachedResourceMediaLoader() { stop(); }

    // CachedResourceClient.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    CachedResourceMediaLoader(WebCoreAVFResourceLoader&, CachedResourceHandle<CachedRawResource>&&);

    void stop();

    // CachedRawResourceClient
    void responseReceived(const CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) final;
    void dataReceived(CachedResource&, const SharedBuffer&) final;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;

    void fulfillRequestWithResource(CachedResource&);

    ThreadSafeWeakPtr<WebCoreAVFResourceLoader> m_parent;
    CachedResourceHandle<CachedRawResource> m_resource;
};

RefPtr<CachedResourceMediaLoader> CachedResourceMediaLoader::create(WebCoreAVFResourceLoader& parent, CachedResourceLoader& loader, ResourceRequest&& resourceRequest)
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
    CachedResourceRequest request(WTF::move(resourceRequest), loaderOptions);

    auto resource = loader.requestMedia(WTF::move(request)).value_or(nullptr);
    if (!resource)
        return nullptr;
    return adoptRef(*new CachedResourceMediaLoader { parent, WTF::move(resource) });
}

CachedResourceMediaLoader::CachedResourceMediaLoader(WebCoreAVFResourceLoader& parent, CachedResourceHandle<CachedRawResource>&& resource)
    : m_parent(parent)
    , m_resource(WTF::move(resource))
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

void CachedResourceMediaLoader::responseReceived(const CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    CompletionHandlerCallingScope completionHandlerCaller(WTF::move(completionHandler));

    m_parent.get()->responseReceived(response.mimeType(), response.httpStatusCode(), response.contentRange(), response.expectedContentLength());
}

void CachedResourceMediaLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    if (resource.loadFailedOrCanceled()) {
        m_parent.get()->loadFailed(resource.resourceError());
        return;
    }
    m_parent.get()->loadFinished();
}

void CachedResourceMediaLoader::dataReceived(CachedResource& resource, const SharedBuffer&)
{
    ASSERT(&resource == m_resource);
    if (RefPtr data = resource.resourceBuffer())
        m_parent.get()->newDataStoredInSharedBuffer(*data);
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
    void redirectReceived(PlatformMediaResource&, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) final { completionHandler(WTF::move(request)); }
    bool shouldCacheResponse(PlatformMediaResource&, const ResourceResponse&) final { return false; }
    void dataSent(PlatformMediaResource&, unsigned long long, unsigned long long) final { }
    void dataReceived(PlatformMediaResource&, const SharedBuffer&) final;
    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError& error) final { loadFailed(error); }
    void loadFailed(PlatformMediaResource&, const ResourceError& error) final { loadFailed(error); }
    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final { loadFinished(); }

    ThreadSafeWeakPtr<WebCoreAVFResourceLoader> m_parent;
    const Ref<GuaranteedSerialFunctionDispatcher> m_targetDispatcher;
    RefPtr<PlatformMediaResource> m_resource WTF_GUARDED_BY_CAPABILITY(m_targetDispatcher.get());
    SharedBufferBuilder m_buffer WTF_GUARDED_BY_CAPABILITY(m_targetDispatcher.get());
};

RefPtr<PlatformResourceMediaLoader> PlatformResourceMediaLoader::create(WebCoreAVFResourceLoader& parent, PlatformMediaResourceLoader& loader, ResourceRequest&& request)
{
    RefPtr resource = loader.requestResource(WTF::move(request), PlatformMediaResourceLoader::LoadOption::DisallowCaching);
    if (!resource)
        return nullptr;
    Ref client = adoptRef(*new PlatformResourceMediaLoader { parent, *resource });
    resource->setClient(client.copyRef());
    return client;
}

PlatformResourceMediaLoader::PlatformResourceMediaLoader(WebCoreAVFResourceLoader& parent, Ref<PlatformMediaResource>&& resource)
    : m_parent(parent)
    , m_targetDispatcher(parent.m_targetDispatcher)
    , m_resource(WTF::move(resource))
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

    m_parent.get()->responseReceived(response.mimeType(), response.httpStatusCode(), response.contentRange(), response.expectedContentLength());
    completionHandler(ShouldContinuePolicyCheck::Yes);
}

void PlatformResourceMediaLoader::loadFailed(const ResourceError& error)
{
    assertIsCurrent(m_targetDispatcher.get());

    m_parent.get()->loadFailed(error);
}

void PlatformResourceMediaLoader::loadFinished()
{
    assertIsCurrent(m_targetDispatcher.get());

    m_parent.get()->loadFinished();
}

void PlatformResourceMediaLoader::dataReceived(PlatformMediaResource&, const SharedBuffer& buffer)
{
    assertIsCurrent(m_targetDispatcher.get());

    m_buffer.append(buffer);
    m_parent.get()->newDataStoredInSharedBuffer(*m_buffer.buffer());
}

class DataURLResourceMediaLoader : public ThreadSafeRefCounted<DataURLResourceMediaLoader> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(DataURLResourceMediaLoader);
public:
    static Ref<DataURLResourceMediaLoader> create(WebCoreAVFResourceLoader& loader, ResourceRequest&& request)
    {
        return adoptRef(*new DataURLResourceMediaLoader(loader, WTF::move(request)));
    }

private:
    DataURLResourceMediaLoader(WebCoreAVFResourceLoader&, ResourceRequest&&);
    void handleDecodeResult(std::optional<DataURLDecoder::Result>&&);

    static WorkQueue& decodeQueue();

    ThreadSafeWeakPtr<WebCoreAVFResourceLoader> m_parent;
    ResourceRequest m_request;
};

WorkQueue& DataURLResourceMediaLoader::decodeQueue()
{
    static std::once_flag flag;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(flag, [] {
        workQueue.construct(WorkQueue::create("DataURLResourceMediaLoader decode queue"_s));
    });
    return workQueue.get();
}

DataURLResourceMediaLoader::DataURLResourceMediaLoader(WebCoreAVFResourceLoader& parent, ResourceRequest&& request)
    : m_parent(parent)
    , m_request { WTF::move(request) }
{
    RELEASE_ASSERT(m_request.url().protocolIsData());
    assertIsCurrent(parent.m_targetDispatcher);

    decodeQueue().dispatch([protectedThis = Ref { *this }, targetDispatcher = parent.m_targetDispatcher, url = m_request.url().isolatedCopy()] mutable {
        targetDispatcher->dispatch([protectedThis = WTF::move(protectedThis), result = DataURLDecoder::decode(url)] mutable {
            protectedThis->handleDecodeResult(WTF::move(result));
        });
    });
}

void DataURLResourceMediaLoader::handleDecodeResult(std::optional<DataURLDecoder::Result>&& result)
{
    RefPtr parent = m_parent.get();
    if (!parent)
        return;

    if (parent->m_dataURLMediaLoader.get() != this)
        return;

    if (!result) {
        parent->loadFailed(ResourceError(ResourceError::Type::General));
        return;
    }

    auto response = ResourceResponse::dataURLResponse(m_request.url(), *result);
    auto buffer = SharedBuffer::create(WTF::move(result->data));

    if (parent->responseReceived(response.mimeType(), response.httpStatusCode(), response.contentRange(), buffer->size()))
        return;

    if (parent->newDataStoredInSharedBuffer(buffer))
        return;

    parent->loadFinished();
}

Ref<WebCoreAVFResourceLoader> WebCoreAVFResourceLoader::create(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *avRequest, Ref<PlatformMediaResourceLoader>&& mediaResourceLoader, GuaranteedSerialFunctionDispatcher& targetQueue)
{
    ASSERT(avRequest);
    ASSERT(parent);
    return adoptRef(*new WebCoreAVFResourceLoader(parent, avRequest, WTF::move(mediaResourceLoader), targetQueue));
}

WebCoreAVFResourceLoader::WebCoreAVFResourceLoader(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *avRequest, Ref<PlatformMediaResourceLoader>&& platformMediaLoader, GuaranteedSerialFunctionDispatcher& targetDispatcher)
    : m_parent(parent)
    , m_avRequest(avRequest)
    , m_platformMediaLoader(WTF::move(platformMediaLoader))
    , m_targetDispatcher(targetDispatcher)
#if !RELEASE_LOG_DISABLED
    , m_logger { parent->logger() }
    , m_logIdentifier { parent->logIdentifier() }
#endif
{
}

WebCoreAVFResourceLoader::~WebCoreAVFResourceLoader()
{
}

void WebCoreAVFResourceLoader::startLoading()
{
    assertIsCurrent(m_targetDispatcher.get());

    if (m_dataURLMediaLoader)
        return;

    RetainPtr<NSURLRequest> nsRequest = [m_avRequest request];

    ResourceRequest request(nsRequest.get());
    request.setPriority(ResourceLoadPriority::Low);

    m_loadStartTime = MonotonicTime::now();

    RetainPtr<AVAssetResourceLoadingDataRequest> dataRequest = [m_avRequest dataRequest];
    m_currentOffset = m_requestedOffset = dataRequest ? [dataRequest requestedOffset] : -1;
    m_requestedLength = dataRequest ? [dataRequest requestedLength] : -1;

    ALWAYS_LOG(LOGIDENTIFIER, "protocol: ", request.url().protocol(), ", offset: ", m_currentOffset, ", length: ", m_requestedLength);

    if (dataRequest && m_requestedLength > 0
        && !request.hasHTTPHeaderField(HTTPHeaderName::Range)) {
        String rangeEnd = dataRequest.get().requestsAllDataToEndOfResource ? emptyString() : makeString(m_requestedOffset + m_requestedLength - 1);
        request.addHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, m_requestedOffset, '-', rangeEnd));
    }

    if (request.url().protocolIsData()) {
        m_dataURLMediaLoader = DataURLResourceMediaLoader::create(*this, WTF::move(request));
        return;
    }

#if PLATFORM(IOS_FAMILY)
    m_isBlob = request.url().protocolIsBlob();
#endif

    m_resourceMediaLoader = PlatformResourceMediaLoader::create(*this, m_platformMediaLoader, WTF::move(request));
    if (m_resourceMediaLoader)
        return;

    ERROR_LOG(LOGIDENTIFIER, "Failed to start load for media at url %s", request.url().string());
    [m_avRequest finishLoadingWithError:0];
}

// No code accessing `this` should ever be used after calling stopLoading().
void WebCoreAVFResourceLoader::stopLoading()
{
    assertIsCurrent(m_targetDispatcher.get());

    if (m_loadStartTime)
        ALWAYS_LOG(LOGIDENTIFIER, "duration: ", (MonotonicTime::now() - *m_loadStartTime).millisecondsAs<int>(), "ms");
    else
        ALWAYS_LOG(LOGIDENTIFIER);

    m_dataURLMediaLoader = nullptr;

    if (m_resourceMediaLoader)
        m_resourceMediaLoader->stop();

    callOnMainThread([weakParent = WTF::move(m_parent), loader = WTF::move(m_resourceMediaLoader), avRequest = WTF::move(m_avRequest)] {
        if (RefPtr parent = weakParent.get(); parent && avRequest)
            parent->didStopLoadingRequest(avRequest.get());
    });
}

bool WebCoreAVFResourceLoader::responseReceived(const String& mimeType, int status, const ParsedContentRange& contentRange, size_t expectedContentLength)
{
    assertIsCurrent(m_targetDispatcher.get());

    ALWAYS_LOG(LOGIDENTIFIER, "status: ", status, ", range: ", contentRange.firstBytePosition(), "-", contentRange.lastBytePosition(), "/", contentRange.instanceLength(), ", expectedContentLength: ", expectedContentLength);

    if (status && (status < 200 || status > 299)) {
        [m_avRequest finishLoadingWithError:0];
        return true;
    }

    m_responseOffset = contentRange.isValid() ? static_cast<NSUInteger>(contentRange.firstBytePosition()) : 0;

    if (RetainPtr<AVAssetResourceLoadingContentInformationRequest> contentInfo = [m_avRequest contentInformationRequest]) {
        String uti = UTIFromMIMEType(mimeType);

        [contentInfo setContentType:uti.createNSString().get()];

        [contentInfo setContentLength:contentRange.isValid() ? contentRange.instanceLength() : expectedContentLength];
        [contentInfo setByteRangeAccessSupported:YES];

        // Do not set "EntireLengthAvailableOnDemand" to YES when the loader is DataURLResourceMediaLoader.
        // When the property is YES, AVAssetResourceLoader will request small data ranges over and over again
        // during the playback. For DataURLResourceMediaLoader, that means it needs to decode the URL repeatedly,
        // which is very inefficient for long URLs.
        // FIXME: don't have blob exception once rdar://132719739 is fixed.
        if (!m_dataURLMediaLoader && [contentInfo respondsToSelector:@selector(setEntireLengthAvailableOnDemand:)])
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

    ERROR_LOG(LOGIDENTIFIER, error.sanitizedDescription());

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

    ALWAYS_LOG(LOGIDENTIFIER);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_avRequest finishLoading];
    END_BLOCK_OBJC_EXCEPTIONS
    stopLoading();
}

bool WebCoreAVFResourceLoader::newDataStoredInSharedBuffer(const FragmentedSharedBuffer& data)
{
    assertIsCurrent(m_targetDispatcher.get());

    RetainPtr<AVAssetResourceLoadingDataRequest> dataRequest = [m_avRequest dataRequest];
    if (!dataRequest)
        return true;

    // Check for possible unsigned overflow.
    ASSERT(m_currentOffset >= m_requestedOffset);
    ASSERT(m_requestedLength >= m_currentOffset - m_requestedOffset);

    NSUInteger remainingLength = m_requestedLength - (m_currentOffset - m_requestedOffset);

    auto bytesToSkip = m_currentOffset - m_responseOffset;
    auto array = data.createNSDataArray();
    for (NSData* segment in array.get()) {
        if (!remainingLength)
            break;
        NSUInteger usableBytes = segment.length;
        NSUInteger bytesOffset = 0;
        if (bytesToSkip) {
            if (bytesToSkip > segment.length) {
                bytesToSkip -= segment.length;
                continue;
            }
            usableBytes = segment.length - bytesToSkip;
            bytesOffset = bytesToSkip;
            bytesToSkip = 0;
        }
        m_currentOffset += usableBytes;
        if (!bytesOffset && usableBytes <= remainingLength)
            [dataRequest respondWithData:segment];
        else {
            usableBytes = std::min(usableBytes, remainingLength);
            [dataRequest respondWithData:[segment subdataWithRange:NSMakeRange(bytesOffset, usableBytes)]];
        }
        remainingLength -= usableBytes;
    }

    // There was not enough data in the buffer to satisfy the data request.
    if (remainingLength)
        return false;

    ASSERT(m_currentOffset >= m_requestedOffset + m_requestedLength);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_avRequest finishLoading];
    END_BLOCK_OBJC_EXCEPTIONS
    stopLoading();
    return true;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& WebCoreAVFResourceLoader::logChannel() const
{
    return LogMedia;
}
#endif

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
