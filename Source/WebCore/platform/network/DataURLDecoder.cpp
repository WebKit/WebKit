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
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include <wtf/MainThread.h>
#include <wtf/Optional.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/Base64.h>

namespace WebCore {
namespace DataURLDecoder {

static WorkQueue& decodeQueue()
{
    static auto& queue = WorkQueue::create("org.webkit.DataURLDecoder", WorkQueue::Type::Serial, WorkQueue::QOS::UserInitiated).leakRef();
    return queue;
}

static Result parseMediaType(const String& mediaType)
{
    if (Optional<ParsedContentType> parsedContentType = ParsedContentType::create(mediaType))
        return { parsedContentType->mimeType(), parsedContentType->charset(), parsedContentType->serialize(), nullptr };
    return { "text/plain"_s, "US-ASCII"_s, "text/plain;charset=US-ASCII"_s, nullptr };
}

struct DecodeTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DecodeTask(const String& urlString, const ScheduleContext& scheduleContext, DecodeCompletionHandler&& completionHandler)
        : urlString(urlString.isolatedCopy())
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

        if (urlString.find(',') == notFound)
            return false;

        const char dataString[] = "data:";
        ASSERT(urlString.startsWith(dataString));

        size_t headerStart = strlen(dataString);
        size_t headerEnd = urlString.find(',', headerStart);
        size_t encodedDataStart = headerEnd == notFound ? headerEnd : headerEnd + 1;

        encodedData = StringView(urlString).substring(encodedDataStart);
        auto header = StringView(urlString).substring(headerStart, headerEnd - headerStart);
        
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

        result = parseMediaType(mediaType);
        return true;
    }

    const String urlString;
    StringView encodedData;
    bool isBase64 { false };
    const ScheduleContext scheduleContext;
    const DecodeCompletionHandler completionHandler;

    Result result;
};

static std::unique_ptr<DecodeTask> createDecodeTask(const URL& url, const ScheduleContext& scheduleContext, DecodeCompletionHandler&& completionHandler)
{
    return makeUnique<DecodeTask>(
        url.string(),
        scheduleContext,
        WTFMove(completionHandler)
    );
}

static void decodeBase64(DecodeTask& task, Mode mode)
{
    Vector<char> buffer;
    if (mode == Mode::ForgivingBase64) {
        auto unescapedString = decodeURLEscapeSequences(task.encodedData.toStringWithoutCopying());
        if (!base64Decode(unescapedString, buffer, Base64ValidatePadding | Base64IgnoreSpacesAndNewLines | Base64DiscardVerticalTab))
            return;
    } else {
        // First try base64url.
        if (!base64URLDecode(task.encodedData.toStringWithoutCopying(), buffer)) {
            // Didn't work, try unescaping and decoding as base64.
            auto unescapedString = decodeURLEscapeSequences(task.encodedData.toStringWithoutCopying());
            if (!base64Decode(unescapedString, buffer, Base64IgnoreSpacesAndNewLines | Base64DiscardVerticalTab))
                return;
        }
    }
    buffer.shrinkToFit();
    task.result.data = SharedBuffer::create(WTFMove(buffer));
}

static void decodeEscaped(DecodeTask& task)
{
    TextEncoding encodingFromCharset(task.result.charset);
    auto& encoding = encodingFromCharset.isValid() ? encodingFromCharset : UTF8Encoding();
    auto buffer = decodeURLEscapeSequencesAsData(task.encodedData, encoding);

    buffer.shrinkToFit();
    task.result.data = SharedBuffer::create(WTFMove(buffer));
}

void decode(const URL& url, const ScheduleContext& scheduleContext, Mode mode, DecodeCompletionHandler&& completionHandler)
{
    ASSERT(url.protocolIsData());

    decodeQueue().dispatch([decodeTask = createDecodeTask(url, scheduleContext, WTFMove(completionHandler)), mode]() mutable {
        if (decodeTask->process()) {
            if (decodeTask->isBase64)
                decodeBase64(*decodeTask, mode);
            else
                decodeEscaped(*decodeTask);
        }

#if USE(COCOA_EVENT_LOOP)
        auto scheduledPairs = decodeTask->scheduleContext.scheduledPairs;
#endif

        auto callCompletionHandler = [decodeTask = WTFMove(decodeTask)] {
            if (!decodeTask->result.data) {
                decodeTask->completionHandler({ });
                return;
            }
            decodeTask->completionHandler(WTFMove(decodeTask->result));
        };

#if USE(COCOA_EVENT_LOOP)
        RunLoop::dispatch(scheduledPairs, WTFMove(callCompletionHandler));
#else
        RunLoop::main().dispatch(WTFMove(callCompletionHandler));
#endif
    });
}

}
}
