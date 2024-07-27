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

#include "config.h"
#include "DataURLDecoder.h"

#include "HTTPParsers.h"
#include "ParsedContentType.h"
#include <pal/text/DecodeEscapeSequences.h>
#include <pal/text/TextEncoding.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {
namespace DataURLDecoder {

static bool shouldRemoveFragmentIdentifier(const String& mediaType)
{
#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DataURLFragmentRemoval))
        return false;

    // HLS uses # in the middle of the manifests.
    return !equalLettersIgnoringASCIICase(mediaType, "video/mpegurl"_s)
        && !equalLettersIgnoringASCIICase(mediaType, "audio/mpegurl"_s)
        && !equalLettersIgnoringASCIICase(mediaType, "application/x-mpegurl"_s)
        && !equalLettersIgnoringASCIICase(mediaType, "vnd.apple.mpegurl"_s);
#else
    UNUSED_PARAM(mediaType);
    return true;
#endif
}

static WorkQueue& decodeQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("org.webkit.DataURLDecoder"_s, WorkQueue::QOS::UserInitiated));
    return queue.get();
}

static Result parseMediaType(const String& mediaType)
{
    if (std::optional<ParsedContentType> parsedContentType = ParsedContentType::create(mediaType))
        return { parsedContentType->mimeType().isolatedCopy(), parsedContentType->charset().isolatedCopy(), parsedContentType->serialize().isolatedCopy(), { } };
    return { "text/plain"_s, "US-ASCII"_s, "text/plain;charset=US-ASCII"_s, { } };
}

struct DecodeTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DecodeTask(const URL& url, const ScheduleContext& scheduleContext, ShouldValidatePadding shouldValidatePadding, DecodeCompletionHandler&& completionHandler)
        : url(url.isolatedCopy())
        , scheduleContext(scheduleContext)
        , shouldValidatePadding(shouldValidatePadding)
        , completionHandler(WTFMove(completionHandler))
    {
    }

    bool process()
    {
        // Syntax:
        //  url := data:<header>,<encodedData>
        //  header := [<mediatype>][;base64]
        //  mediatype := [<mimetype>][;charset=<charsettype>]

        constexpr auto dataString = "data:"_s;
        ASSERT(url.string().startsWith(dataString));

        size_t headerStart = dataString.length();
        size_t headerEnd = url.string().find(',', headerStart);
        if (headerEnd == notFound)
            return false;
        if (size_t fragmentInHeader = url.string().reverseFind('#', headerEnd); fragmentInHeader != notFound)
            return false;
        size_t encodedDataStart = headerEnd == notFound ? headerEnd : headerEnd + 1;

        auto header = StringView(url.string()).substring(headerStart, headerEnd - headerStart);
        
        // There might one or two semicolons in the header, find the last one.
        size_t mediaTypeEnd = header.reverseFind(';');
        mediaTypeEnd = mediaTypeEnd == notFound ? header.length() : mediaTypeEnd;

        // formatTypeStart might be at the begining of "base64" or "charset=...".
        size_t formatTypeStart = mediaTypeEnd + 1;
        auto formatType = header.substring(formatTypeStart, header.length() - formatTypeStart);
        formatType = formatType.trim(isASCIIWhitespaceWithoutFF<UChar>);

        isBase64 = equalLettersIgnoringASCIICase(formatType, "base64"_s);

        // If header does not end with "base64", mediaType should be the whole header.
        auto mediaType = (isBase64 ? header.left(mediaTypeEnd) : header).toString();
        mediaType = mediaType.trim(isASCIIWhitespaceWithoutFF<UChar>);
        if (mediaType.startsWith(';'))
            mediaType = makeString("text/plain"_s, mediaType);

        if (shouldRemoveFragmentIdentifier(mediaType))
            url.removeFragmentIdentifier();

        encodedData = StringView(url.string()).substring(encodedDataStart);

        result = parseMediaType(mediaType);
        return true;
    }

    URL url;
    StringView encodedData;
    bool isBase64 { false };
    const ScheduleContext scheduleContext;
    ShouldValidatePadding shouldValidatePadding;
    DecodeCompletionHandler completionHandler;

    Result result;
};

static std::unique_ptr<DecodeTask> createDecodeTask(const URL& url, const ScheduleContext& scheduleContext, ShouldValidatePadding shouldValidatePadding, DecodeCompletionHandler&& completionHandler)
{
    return makeUnique<DecodeTask>(
        url,
        scheduleContext,
        shouldValidatePadding,
        WTFMove(completionHandler)
    );
}

static std::optional<Result> decodeSynchronously(DecodeTask& task)
{
    if (!task.process())
        return std::nullopt;

    if (task.isBase64) {
        OptionSet<Base64DecodeOption> options = { Base64DecodeOption::IgnoreWhitespace };
        if (task.shouldValidatePadding == ShouldValidatePadding::Yes)
            options.add(Base64DecodeOption::ValidatePadding);
        auto decodedData = base64Decode(PAL::decodeURLEscapeSequences(task.encodedData), options);
        if (!decodedData)
            return std::nullopt;
        task.result.data = WTFMove(*decodedData);
    } else
        task.result.data = PAL::decodeURLEscapeSequencesAsData(task.encodedData);

    task.result.data.shrinkToFit();
    return WTFMove(task.result);
}

void decode(const URL& url, const ScheduleContext& scheduleContext, ShouldValidatePadding shouldValidatePadding, DecodeCompletionHandler&& completionHandler)
{
    ASSERT(url.protocolIsData());

    decodeQueue().dispatch([decodeTask = createDecodeTask(url, scheduleContext, shouldValidatePadding, WTFMove(completionHandler))]() mutable {
        auto result = decodeSynchronously(*decodeTask);

#if USE(COCOA_EVENT_LOOP)
        auto scheduledPairs = decodeTask->scheduleContext.scheduledPairs;
#endif

        auto callCompletionHandler = [result = WTFMove(result), completionHandler = WTFMove(decodeTask->completionHandler)]() mutable {
            completionHandler(WTFMove(result));
        };

#if USE(COCOA_EVENT_LOOP)
        RunLoop::dispatch(scheduledPairs, WTFMove(callCompletionHandler));
#else
        RunLoop::main().dispatch(WTFMove(callCompletionHandler));
#endif
    });
}

std::optional<Result> decode(const URL& url, ShouldValidatePadding shouldValidatePadding)
{
    ASSERT(url.protocolIsData());

    auto task = createDecodeTask(url, { }, shouldValidatePadding, nullptr);
    return decodeSynchronously(*task);
}

} // namespace DataURLDecoder
} // namespace WebCore
