/*
 * Copyright (C) 2009-2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PDFIncrementalLoader.h"

#if ENABLE(PDF_PLUGIN) && HAVE(INCREMENTAL_PDF_APIS)

#import "Logging.h"
#import "PDFKitSPI.h"
#import "PDFPluginBase.h"
#import <WebCore/HTTPStatusCodes.h>
#import <WebCore/NetscapePlugInStreamLoader.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/Identified.h>
#import <wtf/ObjectIdentifier.h>
#import <wtf/Range.h>
#import <wtf/RangeSet.h>
#import <wtf/Scope.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/MakeString.h>

#import "PDFKitSoftLink.h"

namespace WebKit {
using namespace WebCore;

// <rdar://problem/61473378> - PDFKit asks for a "way too large" range when the PDF it is loading
// incrementally turns out to be non-linearized.
// We'll assume any size over 4GB is PDFKit noticing non-linearized data.
static const uint32_t nonLinearizedPDFSentinel = std::numeric_limits<uint32_t>::max();

class ByteRangeRequest : public Identified<ByteRangeRequestIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(ByteRangeRequest);
public:
    ByteRangeRequest() = default;
    ByteRangeRequest(uint64_t position, size_t count, DataRequestCompletionHandler&& completionHandler)
        : m_position(position)
        , m_count(count)
        , m_completionHandler(WTFMove(completionHandler))
    {
    }

    NetscapePlugInStreamLoader* streamLoader() { return m_streamLoader.get(); }
    void setStreamLoader(NetscapePlugInStreamLoader* loader) { m_streamLoader = loader; }
    void clearStreamLoader();
    void addData(std::span<const uint8_t> data) { m_accumulatedData.append(data); }

    void completeWithBytes(std::span<const uint8_t>, PDFIncrementalLoader&);
    void completeWithAccumulatedData(PDFIncrementalLoader&);
    bool completeIfPossible(PDFIncrementalLoader&);
    void completeUnconditionally(PDFIncrementalLoader&);

    uint64_t position() const { return m_position; }
    size_t count() const { return m_count; }

    const Vector<uint8_t>& accumulatedData() const { return m_accumulatedData; };

private:
    uint64_t m_position { 0 };
    size_t m_count { 0 };
    DataRequestCompletionHandler m_completionHandler;
    Vector<uint8_t> m_accumulatedData;
    WeakPtr<NetscapePlugInStreamLoader> m_streamLoader;
};

#pragma mark -

WTF_MAKE_TZONE_ALLOCATED_IMPL(ByteRangeRequest);

void ByteRangeRequest::clearStreamLoader()
{
    ASSERT(m_streamLoader);
    m_streamLoader = nullptr;
    m_accumulatedData.clear();
}

void ByteRangeRequest::completeWithBytes(std::span<const uint8_t> data, PDFIncrementalLoader& loader)
{
    ASSERT(isMainRunLoop());

    m_completionHandler(data);
    loader.requestDidCompleteWithBytes(*this, data.size());
}

void ByteRangeRequest::completeWithAccumulatedData(PDFIncrementalLoader& loader)
{
    ASSERT(isMainRunLoop());

    auto data = m_accumulatedData.span();
    if (data.size() > m_count) {
        RELEASE_LOG_ERROR(IncrementalPDF, "PDF byte range request got more bytes back from the server than requested. This is likely due to a misconfigured server. Capping result at the requested number of bytes.");
        data = data.first(m_count);
    }

    m_completionHandler(data);
    loader.requestDidCompleteWithAccumulatedData(*this, data.size());
}

bool ByteRangeRequest::completeIfPossible(PDFIncrementalLoader& loader)
{
    ASSERT(isMainRunLoop());

    return loader.requestCompleteIfPossible(*this);
}

void ByteRangeRequest::completeUnconditionally(PDFIncrementalLoader& loader)
{
    ASSERT(isMainRunLoop());

    auto availableBytes = loader.availableDataSize();
    if (m_position >= availableBytes) {
        completeWithBytes({ }, loader);
        return;
    }

    auto availableRequestBytes = std::min<uint64_t>(m_count, availableBytes - m_position);

    auto data = loader.dataSpanForRange(m_position, availableRequestBytes, CheckValidRanges::No);
    if (!data.data())
        return;

    completeWithBytes(data, loader);
}

#pragma mark -

class PDFPluginStreamLoaderClient : public ThreadSafeRefCounted<PDFPluginStreamLoaderClient>,
    public NetscapePlugInStreamLoaderClient {
public:
    PDFPluginStreamLoaderClient(PDFIncrementalLoader& loader)
        : m_loader(loader)
    {
    }

    ~PDFPluginStreamLoaderClient() = default;

    void willSendRequest(NetscapePlugInStreamLoader*, ResourceRequest&&, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&&) final;
    void didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse&) final;
    void didReceiveData(NetscapePlugInStreamLoader*, const SharedBuffer&) final;
    void didFail(NetscapePlugInStreamLoader*, const ResourceError&) final;
    void didFinishLoading(NetscapePlugInStreamLoader*) final;

private:
    ThreadSafeWeakPtr<PDFIncrementalLoader> m_loader;
};

#pragma mark -

void PDFPluginStreamLoaderClient::willSendRequest(NetscapePlugInStreamLoader* streamLoader, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&)
{
    ASSERT(isMainRunLoop());

    RefPtr loader = m_loader.get();
    if (!loader)
        return;

    // Redirections for range requests are unexpected.
    loader->cancelAndForgetStreamLoader(*streamLoader);
}

void PDFPluginStreamLoaderClient::didReceiveResponse(NetscapePlugInStreamLoader* streamLoader, const ResourceResponse& response)
{
    ASSERT(isMainRunLoop());

    RefPtr loader = m_loader.get();
    if (!loader)
        return;

    auto* request = loader->byteRangeRequestForStreamLoader(*streamLoader);
    if (!request) {
        loader->cancelAndForgetStreamLoader(*streamLoader);
        return;
    }

    ASSERT(request->streamLoader() == streamLoader);

    // Range success! We'll expect to receive the data in future didReceiveData callbacks.
    if (response.httpStatusCode() == httpStatus206PartialContent)
        return;

    LOG_WITH_STREAM(IncrementalPDF, stream << "Range request " << request->identifier() << " response was not 206, it was " << response.httpStatusCode());

    // If the response wasn't a successful range response, we don't need this stream loader anymore.
    // This can happen, for example, if the server doesn't support range requests.
    // We'll still resolve the ByteRangeRequest later once enough of the full resource has loaded.
    loader->cancelAndForgetStreamLoader(*streamLoader);

    // The server might support range requests and explicitly told us this range was not satisfiable.
    // In this case, we can reject the ByteRangeRequest right away.
    if (response.httpStatusCode() == httpStatus416RangeNotSatisfiable && request) {
        request->completeWithAccumulatedData(*loader);
        loader->removeOutstandingByteRangeRequest(request->identifier());
    }
}

void PDFPluginStreamLoaderClient::didReceiveData(NetscapePlugInStreamLoader* streamLoader, const SharedBuffer& data)
{
    ASSERT(isMainRunLoop());

    RefPtr loader = m_loader.get();
    if (!loader)
        return;

    auto* request = loader->byteRangeRequestForStreamLoader(*streamLoader);
    if (!request)
        return;

    request->addData(data.span());
}

void PDFPluginStreamLoaderClient::didFail(NetscapePlugInStreamLoader* streamLoader, const ResourceError&)
{
    ASSERT(isMainRunLoop());

    RefPtr loader = m_loader.get();
    if (!loader)
        return;

    if (loader->documentFinishedLoading()) {
        if (auto identifier = loader->identifierForLoader(streamLoader))
            loader->removeOutstandingByteRangeRequest(*identifier);
    }

    loader->forgetStreamLoader(*streamLoader);
}

void PDFPluginStreamLoaderClient::didFinishLoading(NetscapePlugInStreamLoader* streamLoader)
{
    ASSERT(isMainRunLoop());

    RefPtr loader = m_loader.get();
    if (!loader)
        return;

    auto* request = loader->byteRangeRequestForStreamLoader(*streamLoader);
    if (!request)
        return;

    request->completeWithAccumulatedData(*loader);
    loader->removeOutstandingByteRangeRequest(request->identifier());
}

#pragma mark -

struct PDFIncrementalLoader::RequestData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    HashMap<ByteRangeRequestIdentifier, ByteRangeRequest> outstandingByteRangeRequests;
    HashMap<RefPtr<WebCore::NetscapePlugInStreamLoader>, ByteRangeRequestIdentifier> streamLoaderMap;
};

#pragma mark -

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFIncrementalLoader);

Ref<PDFIncrementalLoader> PDFIncrementalLoader::create(PDFPluginBase& plugin)
{
    return adoptRef(*new PDFIncrementalLoader(plugin));
}

PDFIncrementalLoader::PDFIncrementalLoader(PDFPluginBase& plugin)
    : m_plugin(plugin)
    , m_streamLoaderClient(adoptRef(*new PDFPluginStreamLoaderClient(*this)))
    , m_requestData(makeUnique<RequestData>())
{
    m_pdfThread = Thread::create("PDF document thread"_s, [protectedThis = Ref { *this }, this] () mutable {
        threadEntry(WTFMove(protectedThis));
    });
}

PDFIncrementalLoader::~PDFIncrementalLoader() = default;

void PDFIncrementalLoader::clear()
{
    // By clearing out the resource data and handling all outstanding range requests,
    // we can force the PDFThread to complete quickly
    if (m_pdfThread) {
        unconditionalCompleteOutstandingRangeRequests();
        {
            Locker locker { m_wasPDFThreadTerminationRequestedLock };
            m_wasPDFThreadTerminationRequested = true;
            m_dataSemaphores.forEach([](auto& dataSemaphore) {
                dataSemaphore.signal();
            });
        }
        m_pdfThread->waitForCompletion();
    }
}

void PDFIncrementalLoader::receivedNonLinearizedPDFSentinel()
{
#if !LOG_DISABLED
    incrementalLoaderLog(makeString("Cancelling all "_s, m_requestData->streamLoaderMap.size(), " range request loaders"_s));
#endif

    for (auto iterator = m_requestData->streamLoaderMap.begin(); iterator != m_requestData->streamLoaderMap.end(); iterator = m_requestData->streamLoaderMap.begin()) {
        removeOutstandingByteRangeRequest(iterator->value);
        cancelAndForgetStreamLoader(Ref { *iterator->key.get() });
    }
}

bool PDFIncrementalLoader::documentFinishedLoading() const
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return true;

    return plugin->documentFinishedLoading();
}

void PDFIncrementalLoader::appendAccumulatedDataToDataBuffer(ByteRangeRequest& request)
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    plugin->insertRangeRequestData(request.position(), request.accumulatedData());
}

uint64_t PDFIncrementalLoader::availableDataSize() const
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return 0;

    return plugin->streamedBytes();
}

std::span<const uint8_t> PDFIncrementalLoader::dataSpanForRange(uint64_t position, size_t count, CheckValidRanges checkValidRanges) const
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return { };

    return plugin->dataSpanForRange(position, count, checkValidRanges);
}

void PDFIncrementalLoader::incrementalPDFStreamDidFinishLoading()
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

#if !LOG_DISABLED
    incrementalLoaderLog(makeString("PDF document finished loading with a total of "_s, availableDataSize(), " bytes"_s));
#endif
    // At this point we know all data for the document, and therefore we know how to answer any outstanding range requests.
    unconditionalCompleteOutstandingRangeRequests();
    plugin->maybeClearHighLatencyDataProviderFlag();
    return;
}

void PDFIncrementalLoader::incrementalPDFStreamDidReceiveData(const SharedBuffer& buffer)
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    HashSet<ByteRangeRequestIdentifier> handledRequests;
    for (auto& request : m_requestData->outstandingByteRangeRequests.values()) {
        if (request.completeIfPossible(*this))
            handledRequests.add(request.identifier());
    }

    for (auto identifier : handledRequests) {
        auto request = m_requestData->outstandingByteRangeRequests.take(identifier);
        if (RefPtr streamLoader = request.streamLoader())
            cancelAndForgetStreamLoader(*streamLoader);
    }
}

void PDFIncrementalLoader::incrementalPDFStreamDidFail()
{
    ASSERT(isMainRunLoop());

    unconditionalCompleteOutstandingRangeRequests();
}

void PDFIncrementalLoader::unconditionalCompleteOutstandingRangeRequests()
{
    ASSERT(isMainRunLoop());

    for (auto& request : m_requestData->outstandingByteRangeRequests.values())
        request.completeUnconditionally(*this);

    m_requestData->outstandingByteRangeRequests.clear();
}

size_t PDFIncrementalLoader::getResourceBytesAtPositionAfterLoadingComplete(std::span<uint8_t> buffer, off_t position)
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return 0;

    return plugin->copyDataAtPosition(buffer, position);
}

void PDFIncrementalLoader::getResourceBytesAtPosition(size_t count, off_t position, DataRequestCompletionHandler&& completionHandler)
{
    ASSERT(isMainRunLoop());
    ASSERT(position >= 0);

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    auto request = ByteRangeRequest(static_cast<uint64_t>(position), count, WTFMove(completionHandler));
    if (request.completeIfPossible(*this))
        return;

    if (documentFinishedLoading()) {
        request.completeUnconditionally(*this);
        return;
    }

    auto identifier = request.identifier();
    m_requestData->outstandingByteRangeRequests.set(identifier, WTFMove(request));

#if !LOG_DISABLED
    incrementalLoaderLog(makeString("Scheduling a stream loader for request "_s, identifier, " ("_s, count, " bytes at "_s, position, ')'));
#endif

    plugin->startByteRangeRequest(m_streamLoaderClient.get(), identifier, position, count);
}

void PDFIncrementalLoader::streamLoaderDidStart(ByteRangeRequestIdentifier requestIdentifier, RefPtr<NetscapePlugInStreamLoader>&& streamLoader)
{
    if (!streamLoader)
        return;

    auto iterator = m_requestData->outstandingByteRangeRequests.find(requestIdentifier);
    if (iterator == m_requestData->outstandingByteRangeRequests.end()) {
        streamLoader->cancel(streamLoader->cancelledError());
        return;
    }

    iterator->value.setStreamLoader(streamLoader.get());
    m_requestData->streamLoaderMap.set(WTFMove(streamLoader), requestIdentifier);

#if !LOG_DISABLED
    incrementalLoaderLog(makeString("There are now "_s, m_requestData->streamLoaderMap.size(), " stream loaders in flight"_s));
#endif
}

auto PDFIncrementalLoader::byteRangeRequestForStreamLoader(NetscapePlugInStreamLoader& loader) -> ByteRangeRequest*
{
    auto identifier = identifierForLoader(&loader);
    if (!identifier)
        return nullptr;

    auto it = m_requestData->outstandingByteRangeRequests.find(*identifier);
    if (it == m_requestData->outstandingByteRangeRequests.end())
        return nullptr;

    return &(it->value);
}

void PDFIncrementalLoader::forgetStreamLoader(NetscapePlugInStreamLoader& loader)
{
    auto identifier = identifierForLoader(&loader);
    if (!identifier) {
        ASSERT(!m_requestData->streamLoaderMap.contains(&loader));
        return;
    }

    if (auto* request = byteRangeRequestForStreamLoader(loader)) {
        if (request->streamLoader()) {
            ASSERT(request->streamLoader() == &loader);
            request->clearStreamLoader();
        }
    }

    m_requestData->streamLoaderMap.remove(&loader);

#if !LOG_DISABLED
    incrementalLoaderLog(makeString("Forgot stream loader for range request "_s, *identifier, ". There are now "_s, m_requestData->streamLoaderMap.size(), " stream loaders remaining"_s));
#endif
}

void PDFIncrementalLoader::cancelAndForgetStreamLoader(NetscapePlugInStreamLoader& streamLoader)
{
    Ref protectedLoader { streamLoader };
    forgetStreamLoader(streamLoader);
    streamLoader.cancel(streamLoader.cancelledError());
}

std::optional<ByteRangeRequestIdentifier> PDFIncrementalLoader::identifierForLoader(WebCore::NetscapePlugInStreamLoader* loader)
{
    return m_requestData->streamLoaderMap.get(loader);
}

void PDFIncrementalLoader::removeOutstandingByteRangeRequest(ByteRangeRequestIdentifier identifier)
{
    m_requestData->outstandingByteRangeRequests.remove(identifier);
}

bool PDFIncrementalLoader::requestCompleteIfPossible(ByteRangeRequest& request)
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return false;

    auto data = plugin->dataSpanForRange(request.position(), request.count(), CheckValidRanges::Yes);
    if (!data.data())
        return false;

    request.completeWithBytes(data, *this);
    return true;
}

void PDFIncrementalLoader::requestDidCompleteWithBytes(ByteRangeRequest& request, size_t byteCount)
{
#if !LOG_DISABLED
    ++m_completedRangeRequests;
    incrementalLoaderLog(makeString("Completing range request "_s, request.identifier(), " ("_s, request.count(), " bytes at "_s, request.position(), ") with "_s, byteCount, " bytes from the main PDF buffer"_s));
#else
    UNUSED_PARAM(byteCount);
#endif

    if (auto* streamLoader = request.streamLoader())
        forgetStreamLoader(*streamLoader);
}

void PDFIncrementalLoader::requestDidCompleteWithAccumulatedData(ByteRangeRequest& request, size_t completionSize)
{
#if !LOG_DISABLED
    ++m_completedRangeRequests;
    ++m_completedNetworkRangeRequests;
    incrementalLoaderLog(makeString("Completing range request "_s, request.identifier(), " ("_s, request.count(), " bytes at "_s, request.position(), ") with "_s, request.accumulatedData().size(), " bytes from the network"_s));
#endif

    appendAccumulatedDataToDataBuffer(request);

    if (auto* streamLoader = request.streamLoader())
        forgetStreamLoader(*streamLoader);
}

static void dataProviderGetByteRangesCallback(void* info, CFMutableArrayRef buffers, const CFRange* ranges, size_t count)
{
    RefPtr loader = reinterpret_cast<PDFIncrementalLoader*>(info);
    loader->dataProviderGetByteRanges(buffers, unsafeMakeSpan(ranges, count));
}

static size_t dataProviderGetBytesAtPositionCallback(void* info, void* buffer, off_t position, size_t count)
{
    RefPtr loader = reinterpret_cast<PDFIncrementalLoader*>(info);
    return loader->dataProviderGetBytesAtPosition(unsafeMakeSpan(static_cast<uint8_t*>(buffer), count), position);
}

static void dataProviderReleaseInfoCallback(void* info)
{
    RefPtr loader = reinterpret_cast<PDFIncrementalLoader*>(info);
    loader->deref(); // Balance the ref() in PDFIncrementalLoader::threadEntry.
}

size_t PDFIncrementalLoader::dataProviderGetBytesAtPosition(std::span<uint8_t> buffer, off_t position)
{
    if (isMainRunLoop()) {
        LOG_WITH_STREAM(IncrementalPDF, stream << "Handling request for " << buffer.size() << " bytes at position " << position << " synchronously on the main thread. Finished loading:" << documentFinishedLoading());
        // FIXME: if documentFinishedLoading() is false, we may not be able to fulfill this request, but that should only happen if we trigger painting on the main thread.
        return getResourceBytesAtPositionAfterLoadingComplete(buffer, position);
    }

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return 0;

    // It's possible we previously encountered an invalid range and therefore disabled incremental loading,
    // but PDFDocument is still trying to read data using a different strategy.
    // Always ignore such requests.
    if (!plugin->incrementalPDFLoadingEnabled())
        return 0;

#if !LOG_DISABLED
    incrementThreadsWaitingOnCallback();
    incrementalLoaderLog(makeString("PDF data provider requesting "_s, buffer.size(), " bytes at position "_s, position));
#endif

    if (position > nonLinearizedPDFSentinel) {
#if !LOG_DISABLED
        incrementalLoaderLog(makeString("Received invalid range request for "_s, buffer.size(), " bytes at position "_s, position, ". PDF is probably not linearized. Falling back to non-incremental loading."_s));
#endif
        // FIXME: Confusing to jump to the plugin
        plugin->receivedNonLinearizedPDFSentinel();
        return 0;
    }

    if (auto data = plugin->dataSpanForRange(position, buffer.size(), CheckValidRanges::Yes); data.data()) {
        memcpySpan(buffer, data);
#if !LOG_DISABLED
        decrementThreadsWaitingOnCallback();
        incrementalLoaderLog(makeString("Satisfied range request for "_s, data.size(), " bytes at position "_s, position, " synchronously"_s));
#endif
        return data.size();
    }

    RefPtr dataSemaphore = createDataSemaphore();
    if (!dataSemaphore)
        return 0;

    size_t bytesProvided = 0;

    RunLoop::protectedMain()->dispatch([protectedLoader = Ref { *this }, dataSemaphore, position, buffer, &bytesProvided] {
        if (dataSemaphore->wasSignaled())
            return;
        protectedLoader->getResourceBytesAtPosition(buffer.size(), position, [buffer, dataSemaphore, &bytesProvided](std::span<const uint8_t> bytes) {
            if (dataSemaphore->wasSignaled())
                return;
            RELEASE_ASSERT(bytes.size() <= buffer.size());
            memcpySpan(buffer, bytes);
            bytesProvided = bytes.size();
            dataSemaphore->signal();
        });
    });

    dataSemaphore->wait();

#if !LOG_DISABLED
    decrementThreadsWaitingOnCallback();
    incrementalLoaderLog(makeString("PDF data provider received "_s, bytesProvided, " bytes of requested "_s, buffer.size()));
#endif

    return bytesProvided;
}

auto PDFIncrementalLoader::createDataSemaphore() -> RefPtr<SemaphoreWrapper>
{
    Ref dataSemaphore = SemaphoreWrapper::create();
    {
        Locker locker { m_wasPDFThreadTerminationRequestedLock };
        if (m_wasPDFThreadTerminationRequested)
            return nullptr;

        m_dataSemaphores.add(dataSemaphore.get());
    }
    return dataSemaphore;
}

void PDFIncrementalLoader::dataProviderGetByteRanges(CFMutableArrayRef buffers, std::span<const CFRange> ranges)
{
    ASSERT(!isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

#if !LOG_DISABLED
    incrementThreadsWaitingOnCallback();
    TextStream stream;
    stream << "PDF data provider requesting " << ranges.size() << " byte ranges (";
    for (size_t i = 0; i < ranges.size(); ++i) {
        stream << ranges[i].length << " at " << ranges[i].location;
        if (i < ranges.size() - 1)
            stream << ", ";
    }
    stream << ")";
    incrementalLoaderLog(stream.release());
#endif

    if (plugin->getByteRanges(buffers, ranges)) {
#if !LOG_DISABLED
        decrementThreadsWaitingOnCallback();
        incrementalLoaderLog(makeString("Satisfied "_s, ranges.size(), " get byte ranges synchronously"_s));
#endif
        return;
    }

    RefPtr dataSemaphore = createDataSemaphore();
    if (!dataSemaphore)
        return;

    Vector<RetainPtr<CFDataRef>> dataResults(ranges.size());

    // FIXME: Once we support multi-range requests, make a single request for all ranges instead of <ranges.size()> individual requests.
    RunLoop::protectedMain()->dispatch([protectedLoader = Ref { *this }, &dataResults, ranges, dataSemaphore]() mutable {
        if (dataSemaphore->wasSignaled())
            return;
        Ref callbackAggregator = CallbackAggregator::create([dataSemaphore] {
            dataSemaphore->signal();
        });
        for (size_t i = 0; i < ranges.size(); ++i) {
            protectedLoader->getResourceBytesAtPosition(ranges[i].length, ranges[i].location, [i, &dataResults, dataSemaphore, callbackAggregator](std::span<const uint8_t> bytes) {
                if (dataSemaphore->wasSignaled())
                    return;
                dataResults[i] = adoptCF(CFDataCreate(kCFAllocatorDefault, bytes.data(), bytes.size()));
            });
        }
    });

    dataSemaphore->wait();

#if !LOG_DISABLED
    decrementThreadsWaitingOnCallback();
    incrementalLoaderLog(makeString("PDF data provider finished receiving the requested "_s, ranges.size(), " byte ranges"_s));
#endif

    for (auto& result : dataResults) {
        if (!result)
            result = adoptCF(CFDataCreate(kCFAllocatorDefault, 0, 0));
        CFArrayAppendValue(buffers, result.get());
    }
}

void PDFIncrementalLoader::transitionToMainThreadDocument()
{
    ASSERT(isMainRunLoop());

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    plugin->adoptBackgroundThreadDocument(WTFMove(m_backgroundThreadDocument));

    // If the plugin was manually destroyed, the m_pdfThread might already be gone.
    if (m_pdfThread) {
        m_pdfThread->waitForCompletion();
        m_pdfThread = nullptr;
    }
}

void PDFIncrementalLoader::threadEntry(Ref<PDFIncrementalLoader>&& protectedLoader)
{
    CGDataProviderDirectAccessRangesCallbacks dataProviderCallbacks {
        0,
        dataProviderGetBytesAtPositionCallback,
        dataProviderGetByteRangesCallback,
        dataProviderReleaseInfoCallback,
    };

    auto scopeExit = makeScopeExit([protectedLoader = WTFMove(protectedLoader)] () mutable {
        // Keep the PDFPlugin alive until the end of this function and the end
        // of the last main thread task submitted by this function.
        callOnMainRunLoop([protectedLoader = WTFMove(protectedLoader)] { });
    });

    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    // Balanced by a deref inside of the dataProviderReleaseInfoCallback
    ref();

    RetainPtr dataProvider = adoptCF(CGDataProviderCreateMultiRangeDirectAccess(this, kCGDataProviderIndeterminateSize, &dataProviderCallbacks));
    CGDataProviderSetProperty(dataProvider.get(), kCGDataProviderHasHighLatency, kCFBooleanTrue);
    m_backgroundThreadDocument = adoptNS([allocPDFDocumentInstance() initWithProvider:dataProvider.get()]);

    // [PDFDocument initWithProvider:] will return nil in cases where the PDF is non-linearized.
    // In those cases we'll just keep buffering the entire PDF on the main thread.
    if (!m_backgroundThreadDocument) {
        LOG(IncrementalPDF, "Background thread [PDFDocument initWithProvider:] returned nil. PDF is not linearized. Reverting to main thread.");
        // FIXME: Confusing to jump to the plugin
        plugin->receivedNonLinearizedPDFSentinel();
        return;
    }

    if (!plugin->incrementalPDFLoadingEnabled()) {
        m_backgroundThreadDocument = nil;
        return;
    }

    BinarySemaphore firstPageSemaphore;
    auto firstPageQueue = WorkQueue::create("PDF first page work queue"_s);

    [m_backgroundThreadDocument preloadDataOfPagesInRange:NSMakeRange(0, 1) onQueue:firstPageQueue->dispatchQueue() completion:[&firstPageSemaphore, protectedThis = Ref { *this }] (NSIndexSet *) mutable {
        callOnMainRunLoop([protectedThis] {
            protectedThis->transitionToMainThreadDocument();
        });

        firstPageSemaphore.signal();
    }];

    firstPageSemaphore.wait();

#if !LOG_DISABLED
    incrementalLoaderLog("Finished preloading first page"_s);
#endif
}

#if !LOG_DISABLED
void PDFIncrementalLoader::incrementalLoaderLog(const String& message)
{
    RefPtr plugin = m_plugin.get();
    if (!plugin)
        return;

    if (!isMainRunLoop()) {
        callOnMainRunLoop([protectedPlugin = plugin, message = message.isolatedCopy()] {
            protectedPlugin->incrementalLoaderLog(message);
        });
        return;
    }

    plugin->incrementalLoaderLog(message);
}

void PDFIncrementalLoader::logStreamLoader(TextStream& stream, NetscapePlugInStreamLoader& streamLoader)
{
    ASSERT(isMainRunLoop());

    auto* request = byteRangeRequestForStreamLoader(streamLoader);
    stream << "(";
    if (!request) {
        stream << "no range request found) ";
        return;
    }

    stream << request->position() << "-" << request->position() + request->count() - 1 << ") ";
}

void PDFIncrementalLoader::logState(TextStream& ts)
{
    if (m_pdfThread)
        ts << "Initial PDF thread is still in progress\n";
    else
        ts << "Initial PDF thread has completed\n";

    ts << "Have completed " << m_completedRangeRequests << " range requests (" << m_completedNetworkRangeRequests << " from the network)\n";
    ts << "There are " << m_threadsWaitingOnCallback << " data provider threads waiting on a reply\n";
    ts << "There are " << m_requestData->outstandingByteRangeRequests.size() << " byte range requests outstanding\n";

    ts << "There are " << m_requestData->streamLoaderMap.size() << " active network stream loaders: ";
    for (auto& loader : m_requestData->streamLoaderMap.keys())
        logStreamLoader(ts, *loader);
    ts << "\n";
}

#endif

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN) && HAVE(INCREMENTAL_PDF_APIS)
