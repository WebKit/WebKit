/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#import "RangeResponseGenerator.h"

#import "HTTPStatusCodes.h"
#import "NetworkLoadMetrics.h"
#import "ParsedRequestRange.h"
#import "PlatformMediaResourceLoader.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import "WebCoreNSURLSession.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/FastMalloc.h>
#import <wtf/FunctionDispatcher.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>

namespace WebCore {
struct RangeResponseGeneratorDataTaskData;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::RangeResponseGeneratorDataTaskData> : std::true_type { };
}

namespace WebCore {

struct RangeResponseGeneratorDataTaskData : public CanMakeWeakPtr<RangeResponseGeneratorDataTaskData> {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    RangeResponseGeneratorDataTaskData(ParsedRequestRange&& range)
        : range(WTFMove(range))
        , nextByteToGiveBufferIndex(range.begin) { }

    ParsedRequestRange range;
    size_t nextByteToGiveBufferIndex { 0 };
    enum class ResponseState : uint8_t { NotSynthesizedYet, WaitingForSession, SessionCalledCompletionHandler } responseState { ResponseState::NotSynthesizedYet };
};

struct RangeResponseGenerator::Data {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    // The RangeResponseGenerator is used with a GuaranteedSerialFunctionDispatcher which can do thread hoping over time.
    // The ResourceResponse contains WTF::Strings which must first be copied via isolatedCopy().
    Data(const ResourceResponse& response, PlatformMediaResource& resource)
        : originalResponse(ResourceResponse::fromCrossThreadData(response.crossThreadData()))
        , resource(&resource) { }

    virtual ~Data()
    {
        shutdownResource();
    }

    void shutdownResource()
    {
        if (resource) {
            resource->shutdown();
            resource = nullptr;
        }
    }
    UncheckedKeyHashMap<RetainPtr<WebCoreNSURLSessionDataTask>, std::unique_ptr<RangeResponseGeneratorDataTaskData>> taskData;
    SharedBufferBuilder buffer;
    ResourceResponse originalResponse;
    enum class SuccessfullyFinishedLoading : bool { No, Yes } successfullyFinishedLoading { SuccessfullyFinishedLoading::No };
    RefPtr<PlatformMediaResource> resource;
};

RangeResponseGenerator::RangeResponseGenerator(GuaranteedSerialFunctionDispatcher& targetDispatcher)
    : m_targetDispatcher(targetDispatcher)
{
}

RangeResponseGenerator::~RangeResponseGenerator() = default;

UncheckedKeyHashMap<String, std::unique_ptr<RangeResponseGenerator::Data>>& RangeResponseGenerator::map()
{
    assertIsCurrent(m_targetDispatcher.get());
    IGNORE_CLANG_WARNINGS_BEGIN("thread-safety-reference-return")
    return m_map;
    IGNORE_CLANG_WARNINGS_END
}

static ResourceResponse synthesizedResponseForRange(const ResourceResponse& originalResponse, const ParsedRequestRange& parsedRequestRange, std::optional<size_t> totalContentLength)
{
    auto begin = parsedRequestRange.begin;
    auto end = parsedRequestRange.end;

    auto newContentRange = makeString("bytes "_s, begin, '-', end, '/', (totalContentLength ? makeString(*totalContentLength) : "*"_s));
    auto newContentLength = makeString(end - begin + 1);

    ResourceResponse newResponse = originalResponse;
    newResponse.setHTTPHeaderField(HTTPHeaderName::ContentRange, newContentRange);
    newResponse.setHTTPHeaderField(HTTPHeaderName::ContentLength, newContentLength);
    newResponse.setHTTPStatusCode(httpStatus206PartialContent);
    
    // Values from setHTTPStatusCode and setHTTPHeaderField are not reflected in the newly generated response without this.
    newResponse.initNSURLResponse();

    return newResponse;
}

void RangeResponseGenerator::removeTask(WebCoreNSURLSessionDataTask *task)
{
    auto url = task.originalRequest.URL;
    // UncheckedKeyHashMap::get() crashes if a null String is passed.
    if (!url)
        return;
    auto* data = map().get(url.absoluteString);
    if (!data)
        return;
    data->taskData.remove(task);
}

void RangeResponseGenerator::giveResponseToTaskIfBytesInRangeReceived(WebCoreNSURLSessionDataTask *task, const ParsedRequestRange& range, std::optional<size_t> expectedContentLength, const Data& data)
{
    assertIsCurrent(m_targetDispatcher);
    auto bufferSize = data.buffer.size();
    if (bufferSize < range.begin)
        return;

    auto buffer = data.buffer.get();
    auto* taskData = data.taskData.get(task);
    if (!taskData)
        return;

    auto giveBytesToTask = [task = retainPtr(task), buffer, bufferSize, taskData = WeakPtr { *taskData }, weakGenerator = ThreadSafeWeakPtr { *this }, targetQueue = m_targetDispatcher] {
        assertIsCurrent(targetQueue);
        if ([task state] != NSURLSessionTaskStateRunning)
            return;
        if (!taskData)
            return;
        auto& range = taskData->range;
        auto& byteIndex = taskData->nextByteToGiveBufferIndex;
        while (true) {
            if (byteIndex >= bufferSize)
                break;
            auto bufferView = buffer->getSomeData(byteIndex);
            if (!bufferView.size() || byteIndex > range.end)
                break;

            size_t bytesFromThisViewToDeliver = std::min(bufferView.size(), range.end - byteIndex + 1);
            byteIndex += bytesFromThisViewToDeliver;
            [task resource:nullptr receivedData:SharedBufferDataView(bufferView, bytesFromThisViewToDeliver).createSharedBuffer()->createNSData()];
        }
        if (byteIndex >= range.end) {
            [task resourceFinished:nullptr metrics:NetworkLoadMetrics { }];
            // This can be called while we are currently iterating data.taskData in giveResponseToTasksWithFinishedRanges,
            // as such we can't remove the task from the hash table yet. Queue a task to process deletion.
            targetQueue->dispatch([weakGenerator, task] {
                if (RefPtr strongGenerator = weakGenerator.get())
                    strongGenerator->removeTask(task.get());
            });
        }
    };

    switch (taskData->responseState) {
    case RangeResponseGeneratorDataTaskData::ResponseState::NotSynthesizedYet: {
        auto response = synthesizedResponseForRange(data.originalResponse, range, expectedContentLength);
        [task resource:nullptr receivedResponse:response completionHandler:[giveBytesToTask = WTFMove(giveBytesToTask), taskData = WeakPtr { taskData }, task = retainPtr(task)] (WebCore::ShouldContinuePolicyCheck shouldContinue) mutable {
            if (taskData)
                taskData->responseState = RangeResponseGeneratorDataTaskData::ResponseState::SessionCalledCompletionHandler;
            if (shouldContinue == ShouldContinuePolicyCheck::Yes)
                giveBytesToTask();
            else
                [task cancel];
        }];
        taskData->responseState = RangeResponseGeneratorDataTaskData::ResponseState::WaitingForSession;
        break;
    }
    case RangeResponseGeneratorDataTaskData::ResponseState::WaitingForSession:
        break;
    case RangeResponseGeneratorDataTaskData::ResponseState::SessionCalledCompletionHandler:
        giveBytesToTask();
        break;
    }
}

std::optional<size_t> RangeResponseGenerator::expectedContentLengthFromData(const Data& data)
{
    if (data.successfullyFinishedLoading == Data::SuccessfullyFinishedLoading::Yes)
        return data.buffer.size();

    // FIXME: ResourceResponseBase::expectedContentLength() should return std::optional<size_t> instead of us doing this check here.
    auto expectedContentLength = data.originalResponse.expectedContentLength();
    if (expectedContentLength == NSURLResponseUnknownLength)
        return std::nullopt;
    return expectedContentLength;
}

void RangeResponseGenerator::giveResponseToTasksWithFinishedRanges(Data& data)
{
    assertIsCurrent(m_targetDispatcher);
    auto expectedContentLength = expectedContentLengthFromData(data);

    for (auto& pair : data.taskData)
        giveResponseToTaskIfBytesInRangeReceived(pair.key.get(), pair.value->range, expectedContentLength, data);
}

bool RangeResponseGenerator::willHandleRequest(WebCoreNSURLSessionDataTask *task, NSURLRequest *request)
{
    assertIsCurrent(m_targetDispatcher);

    if (!request.URL)
        return false;
    auto* data = map().get(request.URL.absoluteString);
    if (!data)
        return false;

    auto range = ParsedRequestRange::parse([request valueForHTTPHeaderField:@"Range"]);
    if (!range)
        return false;

    auto expectedContentLength = expectedContentLengthFromData(*data);
    data->taskData.add(task, makeUnique<RangeResponseGeneratorDataTaskData>(WTFMove(*range)));
    giveResponseToTaskIfBytesInRangeReceived(task, *range, expectedContentLength, *data);

    return true;
}

class RangeResponseGenerator::MediaResourceClient : public PlatformMediaResourceClient {
public:
    MediaResourceClient(RangeResponseGenerator& generator, URL&& url)
        : m_generator(generator)
        , m_urlString(WTFMove(url).string()) { }
private:

    // These methods should have been called before changing the client to this.
    void responseReceived(PlatformMediaResource&, const ResourceResponse&, CompletionHandler<void(ShouldContinuePolicyCheck)>&& completionHandler) final
    {
        ASSERT_NOT_REACHED();
        completionHandler(ShouldContinuePolicyCheck::No);
    }
    void redirectReceived(PlatformMediaResource&, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) final
    {
        RELEASE_ASSERT_NOT_REACHED();
        completionHandler({ });
    }
    void dataSent(PlatformMediaResource&, unsigned long long, unsigned long long) final
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
    void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) final
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool shouldCacheResponse(PlatformMediaResource&, const ResourceResponse&) final
    {
        return false;
    }

    void dataReceived(PlatformMediaResource&, const SharedBuffer& buffer) final
    {
        RefPtr generator = m_generator.get();
        if (!generator)
            return;
        auto* data = generator->map().get(m_urlString);
        if (!data)
            return;
        data->buffer.append(buffer);
        generator->giveResponseToTasksWithFinishedRanges(*data);
    }

    void loadFailed(PlatformMediaResource&, const ResourceError& error) final
    {
        RefPtr generator = m_generator.get();
        if (!generator)
            return;
        auto data = generator->map().take(m_urlString);
        if (!data)
            return;
        for (auto& task : data->taskData.keys())
            [task resource:nullptr loadFailedWithError:error];
    }

    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final
    {
        RefPtr generator = m_generator.get();
        if (!generator)
            return;
        auto* data = generator->map().get(m_urlString);
        if (!data)
            return;
        data->successfullyFinishedLoading = Data::SuccessfullyFinishedLoading::Yes;
        data->shutdownResource();
        generator->giveResponseToTasksWithFinishedRanges(*data);
    }

    ThreadSafeWeakPtr<RangeResponseGenerator> m_generator;
    const String m_urlString;
};

bool RangeResponseGenerator::willSynthesizeRangeResponses(WebCoreNSURLSessionDataTask *task, PlatformMediaResource& resource, const ResourceResponse& response)
{
    assertIsCurrent(m_targetDispatcher.get());
    NSURLRequest *originalRequest = task.originalRequest;
    if (!originalRequest.URL)
        return false;
    if (response.httpStatusCode() != httpStatus200OK)
        return false;
    if (!response.httpHeaderField(HTTPHeaderName::ContentRange).isEmpty())
        return false;

    auto parsedRequestRange = ParsedRequestRange::parse([originalRequest valueForHTTPHeaderField:@"Range"]);
    if (!parsedRequestRange)
        return false;

    resource.setClient(adoptRef(new MediaResourceClient(*this, originalRequest.URL)));

    m_map.ensure(originalRequest.URL.absoluteString, [&] {
        return makeUnique<Data>(response, resource);
    }).iterator->value->taskData.add(task, makeUnique<RangeResponseGeneratorDataTaskData>(WTFMove(*parsedRequestRange)));

    return true;
}

} // namespace WebCore
