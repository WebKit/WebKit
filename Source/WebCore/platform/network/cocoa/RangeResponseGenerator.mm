/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "NetworkLoadMetrics.h"
#import "ParsedRequestRange.h"
#import "PlatformMediaResourceLoader.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import "WebCoreNSURLSession.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/FastMalloc.h>
#import <wtf/text/StringBuilder.h>

namespace WebCore {

struct RangeResponseGenerator::Data {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Data(const ResourceResponse& response, PlatformMediaResource& resource)
        : buffer(SharedBuffer::create())
        , originalResponse(response)
        , resource(&resource) { }

    struct TaskData : public CanMakeWeakPtr<TaskData> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        TaskData(ParsedRequestRange&& range)
            : range(WTFMove(range))
            , nextByteToGiveBufferIndex(range.begin) { }

        ParsedRequestRange range;
        size_t nextByteToGiveBufferIndex { 0 };
        enum class ResponseState : uint8_t { NotSynthesizedYet, WaitingForSession, SessionCalledCompletionHandler } responseState { ResponseState::NotSynthesizedYet };
    };
    
    HashMap<RetainPtr<WebCoreNSURLSessionDataTask>, std::unique_ptr<TaskData>> taskData;
    Ref<SharedBuffer> buffer;
    ResourceResponse originalResponse;
    enum class SuccessfullyFinishedLoading : bool { No, Yes } successfullyFinishedLoading { SuccessfullyFinishedLoading::No };
    RefPtr<PlatformMediaResource> resource;
};

RangeResponseGenerator::RangeResponseGenerator()
{
    ASSERT(isMainThread());
}

RangeResponseGenerator::~RangeResponseGenerator()
{
    ASSERT(isMainThread());
}

static ResourceResponse synthesizedResponseForRange(const ResourceResponse& originalResponse, const ParsedRequestRange& parsedRequestRange, size_t totalContentLength)
{
    ASSERT(isMainThread());
    auto begin = parsedRequestRange.begin;
    auto end = parsedRequestRange.end;

    auto newContentRange = makeString("bytes ", begin, "-", end, "/", totalContentLength);
    auto newContentLength = makeString(end - begin + 1);

    ResourceResponse newResponse = originalResponse;
    newResponse.setHTTPHeaderField(HTTPHeaderName::ContentRange, newContentRange);
    newResponse.setHTTPHeaderField(HTTPHeaderName::ContentLength, newContentLength);
    constexpr auto partialContent = 206;
    newResponse.setHTTPStatusCode(partialContent);
    
    // Values from setHTTPStatusCode and setHTTPHeaderField are not reflected in the newly generated response without this.
    newResponse.initNSURLResponse();

    return newResponse;
}

void RangeResponseGenerator::removeTask(WebCoreNSURLSessionDataTask *task)
{
    ASSERT(isMainThread());
    auto* data = m_map.get(task.originalRequest.URL.absoluteString);
    if (!data)
        return;
    data->taskData.remove(task);
}

void RangeResponseGenerator::giveResponseToTaskIfBytesInRangeReceived(WebCoreNSURLSessionDataTask *task, const ParsedRequestRange& range, std::optional<size_t> expectedContentLength, const Data& data)
{
    ASSERT(isMainThread());
    auto buffer = data.buffer;
    auto bufferSize = buffer->size();

    // FIXME: We ought to be able to just make a range with a * after the / but AVFoundation doesn't accept such ranges.
    // Instead, we just wait until the load has completed, at which time we will know the content length from the buffer length.
    if (!expectedContentLength)
        return;

    if (bufferSize < range.begin)
        return;
    
    auto* taskData = data.taskData.get(task);
    if (!taskData)
        return;
    
    auto giveBytesToTask = [task = retainPtr(task), buffer, taskData = WeakPtr { *taskData }, generator = WeakPtr { *this }] {
        ASSERT(isMainThread());
        if ([task state] != NSURLSessionTaskStateRunning)
            return;
        if (!taskData)
            return;
        auto& range = taskData->range;
        auto& byteIndex = taskData->nextByteToGiveBufferIndex;
        while (true) {
            if (byteIndex >= buffer->size())
                break;
            auto bufferView = buffer->getSomeData(byteIndex);
            if (!bufferView.size() || byteIndex > range.end)
                break;

            size_t bytesFromThisViewToDeliver = std::min(bufferView.size(), range.end - byteIndex + 1);
            byteIndex += bytesFromThisViewToDeliver;
            [task resource:nullptr receivedData:SharedBufferDataView(bufferView, bytesFromThisViewToDeliver).createSharedBuffer()];
        }
        if (byteIndex >= range.end) {
            [task resourceFinished:nullptr metrics:NetworkLoadMetrics { }];
            callOnMainThread([generator, task] {
                if (generator)
                    generator->removeTask(task.get());
            });
        }
    };

    switch (taskData->responseState) {
    case Data::TaskData::ResponseState::NotSynthesizedYet: {
        auto response = synthesizedResponseForRange(data.originalResponse, range, *expectedContentLength);
        [task resource:nullptr receivedResponse:response completionHandler:[giveBytesToTask = WTFMove(giveBytesToTask), taskData = WeakPtr { taskData }, task = retainPtr(task)] (WebCore::ShouldContinuePolicyCheck shouldContinue) {
            if (taskData)
                taskData->responseState = Data::TaskData::ResponseState::SessionCalledCompletionHandler;
            if (shouldContinue == ShouldContinuePolicyCheck::Yes)
                giveBytesToTask();
            else
                [task cancel];
        }];
        taskData->responseState = Data::TaskData::ResponseState::WaitingForSession;
        break;
    }
    case Data::TaskData::ResponseState::WaitingForSession:
        break;
    case Data::TaskData::ResponseState::SessionCalledCompletionHandler:
        giveBytesToTask();
        break;
    }
}

std::optional<size_t> RangeResponseGenerator::expectedContentLengthFromData(const Data& data)
{
    ASSERT(isMainThread());
    if (data.successfullyFinishedLoading == Data::SuccessfullyFinishedLoading::Yes)
        return data.buffer->size();

    // FIXME: ResourceResponseBase::expectedContentLength() should return std::optional<size_t> instead of us doing this check here.
    auto expectedContentLength = data.originalResponse.expectedContentLength();
    if (expectedContentLength == NSURLResponseUnknownLength)
        return std::nullopt;
    return expectedContentLength;
}

void RangeResponseGenerator::giveResponseToTasksWithFinishedRanges(Data& data)
{
    ASSERT(isMainThread());
    auto expectedContentLength = expectedContentLengthFromData(data);

    for (auto& pair : data.taskData)
        giveResponseToTaskIfBytesInRangeReceived(pair.key.get(), pair.value->range, expectedContentLength, data);
}

bool RangeResponseGenerator::willHandleRequest(WebCoreNSURLSessionDataTask *task, NSURLRequest *request)
{
    ASSERT(isMainThread());
    auto* data = m_map.get(request.URL.absoluteString);
    if (!data)
        return false;

    auto range = ParsedRequestRange::parse([request valueForHTTPHeaderField:@"Range"]);
    if (!range)
        return false;

    auto expectedContentLength = expectedContentLengthFromData(*data);
    data->taskData.add(task, makeUnique<Data::TaskData>(WTFMove(*range)));
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
        ASSERT(isMainThread());
        return false;
    }

    void dataReceived(PlatformMediaResource&, Ref<SharedBuffer>&& buffer) final
    {
        ASSERT(isMainThread());
        if (!m_generator)
            return;
        auto* data = m_generator->m_map.get(m_urlString);
        if (!data)
            return;
        data->buffer->append(WTFMove(buffer));
        m_generator->giveResponseToTasksWithFinishedRanges(*data);
    }

    void loadFailed(PlatformMediaResource&, const ResourceError& error) final
    {
        ASSERT(isMainThread());
        if (!m_generator)
            return;
        auto data = m_generator->m_map.take(m_urlString);
        if (!data)
            return;
        for (auto& task : data->taskData.keys())
            [task resource:nullptr loadFailedWithError:error];
    }

    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final
    {
        ASSERT(isMainThread());
        RefPtr generator { m_generator.get() };
        if (!generator)
            return;
        auto* data = generator->m_map.get(m_urlString);
        if (!data)
            return;
        data->successfullyFinishedLoading = Data::SuccessfullyFinishedLoading::Yes;
        data->resource = nullptr; // This line can delete this MediaResourceClient.
        generator->giveResponseToTasksWithFinishedRanges(*data);
    }

    WeakPtr<RangeResponseGenerator> m_generator;
    const String m_urlString;
};

bool RangeResponseGenerator::willSynthesizeRangeResponses(WebCoreNSURLSessionDataTask *task, PlatformMediaResource& resource, const ResourceResponse& response)
{
    ASSERT(isMainThread());
    NSURLRequest *originalRequest = task.originalRequest;
    if (!originalRequest.URL)
        return false;
    if (response.httpStatusCode() != 200)
        return false;
    if (!response.httpHeaderField(HTTPHeaderName::ContentRange).isEmpty())
        return false;

    auto parsedRequestRange = ParsedRequestRange::parse([originalRequest valueForHTTPHeaderField:@"Range"]);
    if (!parsedRequestRange)
        return false;

    resource.setClient(adoptRef(new MediaResourceClient(*this, originalRequest.URL)));

    m_map.ensure(originalRequest.URL.absoluteString, [&] {
        return makeUnique<Data>(response, resource);
    }).iterator->value->taskData.add(task, makeUnique<Data::TaskData>(WTFMove(*parsedRequestRange)));

    return true;
}

} // namespace WebCore
