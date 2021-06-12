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

#include "DecodeEscapeSequences.h"
#include "HTTPParsers.h"
#include "ParsedContentType.h"
#include "TextEncoding.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/Base64.h>

#if PLATFORM(COCOA)
#include "VersionChecks.h"
#endif

namespace WebCore {
namespace DataURLDecoder {

static bool shouldRemoveFragmentIdentifier(const String& mediaType)
{
#if PLATFORM(COCOA)
    if (!linkedOnOrAfter(SDKVersion::FirstWithDataURLFragmentRemoval))
        return false;

    // HLS uses # in the middle of the manifests.
    return !equalLettersIgnoringASCIICase(mediaType, "video/mpegurl")
        && !equalLettersIgnoringASCIICase(mediaType, "audio/mpegurl")
        && !equalLettersIgnoringASCIICase(mediaType, "application/x-mpegurl")
        && !equalLettersIgnoringASCIICase(mediaType, "vnd.apple.mpegurl");
#else
    UNUSED_PARAM(mediaType);
    return true;
#endif
}

static WorkQueue& decodeQueue()
{
    static auto& queue = WorkQueue::create("org.webkit.DataURLDecoder", WorkQueue::Type::Serial, WorkQueue::QOS::UserInitiated).leakRef();
    return queue;
}

static Result parseMediaType(const String& mediaType)
{
    if (std::optional<ParsedContentType> parsedContentType = ParsedContentType::create(mediaType))
        return { parsedContentType->mimeType(), parsedContentType->charset(), parsedContentType->serialize(), { } };
    return { "text/plain"_s, "US-ASCII"_s, "text/plain;charset=US-ASCII"_s, { } };
}

struct DecodeTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DecodeTask(const URL& url, const ScheduleContext& scheduleContext, DecodeCompletionHandler&& completionHandler)
        : url(url.isolatedCopy())
        , scheduleContext(scheduleContext)
        , completionHandler(WTFMove(completionHandler))
    {
    }

    bool process()
    {
        // Syntax:
        //  url := data:<header>,<encodedData>
        //  header := [<mediatype>][;base64]
        //  mediatype := [<mimetype>][;charset=<charsettype>]

        const char dataString[] = "data:";
        ASSERT(url.string().startsWith(dataString));

        size_t headerStart = strlen(dataString);
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
        formatType = stripLeadingAndTrailingHTTPSpaces(formatType);

        isBase64 = equalLettersIgnoringASCIICase(formatType, "base64");

        // If header does not end with "base64", mediaType should be the whole header.
        auto mediaType = (isBase64 ? header.substring(0, mediaTypeEnd) : header).toString();
        mediaType = stripLeadingAndTrailingHTTPSpaces(mediaType);
        if (mediaType.startsWith(';'))
            mediaType.insert("text/plain", 0);

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
    DecodeCompletionHandler completionHandler;

    Result result;
};

static std::unique_ptr<DecodeTask> createDecodeTask(const URL& url, const ScheduleContext& scheduleContext, DecodeCompletionHandler&& completionHandler)
{
    return makeUnique<DecodeTask>(
        url,
        scheduleContext,
        WTFMove(completionHandler)
    );
}

static std::optional<Vector<uint8_t>> decodeBase64(const DecodeTask& task, Mode mode)
{
    switch (mode) {
    case Mode::ForgivingBase64:
        return base64Decode(decodeURLEscapeSequences(task.encodedData), { Base64DecodeOptions::ValidatePadding, Base64DecodeOptions::IgnoreSpacesAndNewLines, Base64DecodeOptions::DiscardVerticalTab});

    case Mode::Legacy:
        // First try base64url.
        if (auto decodedData = base64URLDecode(task.encodedData))
            return decodedData;
        // Didn't work, try unescaping and decoding as base64.
        return base64Decode(decodeURLEscapeSequences(task.encodedData), { Base64DecodeOptions::IgnoreSpacesAndNewLines, Base64DecodeOptions::DiscardVerticalTab });
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static Vector<uint8_t> decodeEscaped(const DecodeTask& task)
{
    TextEncoding encodingFromCharset(task.result.charset);
    auto& encoding = encodingFromCharset.isValid() ? encodingFromCharset : UTF8Encoding();
    return decodeURLEscapeSequencesAsData(task.encodedData, encoding);
}

static std::optional<Result> decodeSynchronously(DecodeTask& task, Mode mode)
{
    if (!task.process())
        return std::nullopt;

    if (task.isBase64) {
        auto decodedData = decodeBase64(task, mode);
        if (!decodedData)
            return std::nullopt;
        task.result.data = WTFMove(*decodedData);
    } else
        task.result.data = decodeEscaped(task);

    task.result.data.shrinkToFit();
    return WTFMove(task.result);
}

void decode(const URL& url, const ScheduleContext& scheduleContext, Mode mode, DecodeCompletionHandler&& completionHandler)
{
    ASSERT(url.protocolIsData());

    decodeQueue().dispatch([decodeTask = createDecodeTask(url, scheduleContext, WTFMove(completionHandler)), mode]() mutable {
        auto result = decodeSynchronously(*decodeTask, mode);

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

std::optional<Result> decode(const URL& url, Mode mode)
{
    ASSERT(url.protocolIsData());

    auto task = createDecodeTask(url, { }, nullptr);
    return decodeSynchronously(*task, mode);
}

} // namespace DataURLDecoder
} // namespace WebCore
