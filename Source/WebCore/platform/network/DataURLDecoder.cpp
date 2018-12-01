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
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/Base64.h>

namespace WebCore {
namespace DataURLDecoder {

static WorkQueue& decodeQueue()
{
    static auto& queue = WorkQueue::create("org.webkit.DataURLDecoder").leakRef();
    return queue;
}

struct DecodeTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DecodeTask(const String& urlString, StringView&& encodedData, bool isBase64, const ScheduleContext& scheduleContext, DecodeCompletionHandler&& completionHandler, Result&& result)
        : urlString(urlString.isolatedCopy())
        , encodedData(WTFMove(encodedData))
        , isBase64(isBase64)
        , scheduleContext(scheduleContext)
        , completionHandler(WTFMove(completionHandler))
        , result(WTFMove(result))
    {
    }

    const String urlString;
    const StringView encodedData;
    const bool isBase64;
    const ScheduleContext scheduleContext;
    const DecodeCompletionHandler completionHandler;

    Result result;
};

#if HAVE(RUNLOOP_TIMER)

class DecodingResultDispatcher : public ThreadSafeRefCounted<DecodingResultDispatcher> {
public:
    static void dispatch(std::unique_ptr<DecodeTask> decodeTask)
    {
        Ref<DecodingResultDispatcher> dispatcher = adoptRef(*new DecodingResultDispatcher(WTFMove(decodeTask)));
        dispatcher->startTimer();
    }

private:
    DecodingResultDispatcher(std::unique_ptr<DecodeTask> decodeTask)
        : m_timer(*this, &DecodingResultDispatcher::timerFired)
        , m_decodeTask(WTFMove(decodeTask))
    {
    }

    void startTimer()
    {
        // Keep alive until the timer has fired.
        ref();

        auto scheduledPairs = m_decodeTask->scheduleContext.scheduledPairs;
        m_timer.startOneShot(0_s);
        m_timer.schedule(scheduledPairs);
    }

    void timerFired()
    {
        if (m_decodeTask->result.data)
            m_decodeTask->completionHandler(WTFMove(m_decodeTask->result));
        else
            m_decodeTask->completionHandler({ });

        // Ensure DecodeTask gets deleted in the main thread.
        m_decodeTask = nullptr;

        deref();
    }

    RunLoopTimer<DecodingResultDispatcher> m_timer;
    std::unique_ptr<DecodeTask> m_decodeTask;
};

#endif // HAVE(RUNLOOP_TIMER)

static Result parseMediaType(const String& mediaType)
{
    auto mimeType = extractMIMETypeFromMediaType(mediaType);
    auto charset = extractCharsetFromMediaType(mediaType);

    // https://tools.ietf.org/html/rfc2397
    // If <mediatype> is omitted, it defaults to text/plain;charset=US-ASCII. As a shorthand,
    // "text/plain" can be omitted but the charset parameter supplied.
    if (mimeType.isEmpty()) {
        mimeType = "text/plain"_s;
        if (charset.isEmpty())
            charset = "US-ASCII"_s;
    }
    return { mimeType, charset, !mediaType.isEmpty() ? mediaType : "text/plain;charset=US-ASCII", nullptr };
}

static std::unique_ptr<DecodeTask> createDecodeTask(const URL& url, const ScheduleContext& scheduleContext, DecodeCompletionHandler&& completionHandler)
{
    const char dataString[] = "data:";
    const char base64String[] = ";base64";

    auto urlString = url.string();
    ASSERT(urlString.startsWith(dataString));

    size_t headerEnd = urlString.find(',', strlen(dataString));
    size_t encodedDataStart = headerEnd == notFound ? headerEnd : headerEnd + 1;

    auto encodedData = StringView(urlString).substring(encodedDataStart);
    auto header = StringView(urlString).substring(strlen(dataString), headerEnd - strlen(dataString));
    bool isBase64 = header.endsWithIgnoringASCIICase(StringView(base64String));
    auto mediaType = (isBase64 ? header.substring(0, header.length() - strlen(base64String)) : header).toString();

    return std::make_unique<DecodeTask>(
        urlString,
        WTFMove(encodedData),
        isBase64,
        scheduleContext,
        WTFMove(completionHandler),
        parseMediaType(mediaType)
    );
}

static void decodeBase64(DecodeTask& task)
{
    Vector<char> buffer;
    // First try base64url.
    if (!base64URLDecode(task.encodedData.toStringWithoutCopying(), buffer)) {
        // Didn't work, try unescaping and decoding as base64.
        auto unescapedString = decodeURLEscapeSequences(task.encodedData.toStringWithoutCopying());
        if (!base64Decode(unescapedString, buffer, Base64IgnoreSpacesAndNewLines))
            return;
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

void decode(const URL& url, const ScheduleContext& scheduleContext, DecodeCompletionHandler&& completionHandler)
{
    ASSERT(url.protocolIsData());

    decodeQueue().dispatch([decodeTask = createDecodeTask(url, scheduleContext, WTFMove(completionHandler))]() mutable {
        if (decodeTask->isBase64)
            decodeBase64(*decodeTask);
        else
            decodeEscaped(*decodeTask);

#if HAVE(RUNLOOP_TIMER)
        DecodingResultDispatcher::dispatch(WTFMove(decodeTask));
#else
        callOnMainThread([decodeTask = WTFMove(decodeTask)] {
            if (!decodeTask->result.data) {
                decodeTask->completionHandler({ });
                return;
            }
            decodeTask->completionHandler(WTFMove(decodeTask->result));
        });
#endif
    });
}

}
}
