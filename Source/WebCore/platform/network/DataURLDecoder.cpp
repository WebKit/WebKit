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
#include "URL.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
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
    const String urlString;
    const StringView encodedData;
    const bool isBase64;
    const DecodeCompletionHandler completionHandler;

    Result result;
};

static Result parseMediaType(const String& mediaType)
{
    auto mimeType = extractMIMETypeFromMediaType(mediaType);
    auto charset = extractCharsetFromMediaType(mediaType);

    // https://tools.ietf.org/html/rfc2397
    // If <mediatype> is omitted, it defaults to text/plain;charset=US-ASCII. As a shorthand,
    // "text/plain" can be omitted but the charset parameter supplied.
    if (mimeType.isEmpty()) {
        mimeType = ASCIILiteral("text/plain");
        if (charset.isEmpty())
            charset = ASCIILiteral("US-ASCII");
    }
    return { mimeType, charset, nullptr };
}

static std::unique_ptr<DecodeTask> createDecodeTask(const URL& url, DecodeCompletionHandler completionHandler)
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

    return std::make_unique<DecodeTask>(DecodeTask {
        WTF::move(urlString),
        WTF::move(encodedData),
        isBase64,
        WTF::move(completionHandler),
        parseMediaType(mediaType)
    });
}

static void decodeBase64(DecodeTask& task)
{
    Vector<char> buffer;
    // First try base64url.
    if (!base64URLDecode(task.encodedData.toStringWithoutCopying(), buffer)) {
        // Didn't work, try unescaping and decoding as base64.
        auto unescapedString = decodeURLEscapeSequences(task.encodedData.toStringWithoutCopying());
        if (!base64Decode(unescapedString, buffer, Base64IgnoreWhitespace))
            return;
    }
    buffer.shrinkToFit();
    task.result.data = SharedBuffer::adoptVector(buffer);
}

static void decodeEscaped(DecodeTask& task)
{
    TextEncoding encodingFromCharset(task.result.charset);
    auto& encoding = encodingFromCharset.isValid() ? encodingFromCharset : UTF8Encoding();
    auto buffer = decodeURLEscapeSequencesAsData(task.encodedData, encoding);

    buffer.shrinkToFit();
    task.result.data = SharedBuffer::adoptVector(buffer);
}

void decode(const URL& url, DecodeCompletionHandler completionHandler)
{
    ASSERT(url.protocolIsData());

    auto decodeTask = createDecodeTask(url, WTF::move(completionHandler));

    auto* decodeTaskPtr = decodeTask.release();
    decodeQueue().dispatch([decodeTaskPtr] {
        auto& decodeTask = *decodeTaskPtr;

        if (decodeTask.isBase64)
            decodeBase64(decodeTask);
        else
            decodeEscaped(decodeTask);

        callOnMainThread([decodeTaskPtr] {
            std::unique_ptr<DecodeTask> decodeTask(decodeTaskPtr);
            if (!decodeTask->result.data) {
                decodeTask->completionHandler({ });
                return;
            }
            decodeTask->completionHandler(WTF::move(decodeTask->result));
        });
    });
}

}
}
