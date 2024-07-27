/*
 * Copyright (C) 2024 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
#include "JSGenericTypedArrayView.h"

#include <wtf/text/Base64.h>

namespace JSC {

std::tuple<FromBase64ShouldThrowError, size_t, Vector<uint8_t>> fromBase64(StringView string, size_t maxLength, Alphabet alphabet, LastChunkHandling lastChunkHandling)
{
    size_t read = 0;
    size_t length = static_cast<size_t>(string.length());

    Vector<uint8_t> bytes;
    bytes.reserveInitialCapacity(std::min(length, maxLength));

    if (!maxLength)
        return std::make_tuple(FromBase64ShouldThrowError::No, read, WTFMove(bytes));

    UChar chunk[4] = { 0, 0, 0, 0 };
    size_t chunkLength = 0;

    for (size_t i = 0; i < length;) {
        UChar c = string[i++];

        if (isASCIIWhitespace(c))
            continue;

        if (c == '=') {
            if (chunkLength < 2)
                return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

            while (i < length && isASCIIWhitespace(string[i]))
                ++i;

            if (chunkLength == 2) {
                if (i == length) {
                    if (lastChunkHandling == LastChunkHandling::StopBeforePartial)
                        return std::make_tuple(FromBase64ShouldThrowError::No, read, WTFMove(bytes));

                    return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));
                }

                if (string[i] == '=') {
                    do {
                        ++i;
                    } while (i < length && isASCIIWhitespace(string[i]));
                }
            }

            if (i < length)
                return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

            for (size_t j = chunkLength; j < 4; ++j)
                chunk[j] = 'A';

            auto decoded = base64Decode(StringView(std::span(chunk, 4)));
            if (!decoded)
                return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

            if (chunkLength == 2 || chunkLength == 3) {
                if (lastChunkHandling == LastChunkHandling::Strict && decoded->at(chunkLength - 1))
                    return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

                decoded->shrink(chunkLength - 1);
            }

            bytes.appendVector(WTFMove(*decoded));
            return std::make_tuple(FromBase64ShouldThrowError::No, length, WTFMove(bytes));
        }

        if (alphabet == Alphabet::Base64URL) {
            if (c == '+' || c == '/')
                return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

            if (c == '-')
                c = '+';
            else if (c == '_')
                c = '/';
        }

        if (!StringView("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"_s).contains(c))
            return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

        auto remaining = maxLength - bytes.size();
        if ((remaining == 1 && chunkLength == 2) || (remaining == 2 && chunkLength == 3))
            return std::make_tuple(FromBase64ShouldThrowError::No, read, WTFMove(bytes));

        chunk[chunkLength++] = c;
        if (chunkLength != 4)
            continue;

        auto decoded = base64Decode(StringView(std::span(chunk, chunkLength)));
        ASSERT(decoded);
        if (!decoded)
            return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

        read = i;

        bytes.appendVector(WTFMove(*decoded));
        if (bytes.size() == maxLength)
            return std::make_tuple(FromBase64ShouldThrowError::No, read, WTFMove(bytes));

        for (size_t j = 0; j < 4; ++j)
            chunk[j] = 0;
        chunkLength = 0;
    }

    if (chunkLength) {
        if (lastChunkHandling == LastChunkHandling::StopBeforePartial)
            return std::make_tuple(FromBase64ShouldThrowError::No, read, WTFMove(bytes));

        if (lastChunkHandling == LastChunkHandling::Strict || chunkLength == 1)
            return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

        for (size_t j = chunkLength; j < 4; ++j)
            chunk[j] = 'A';

        auto decoded = base64Decode(StringView(std::span(chunk, chunkLength)));
        ASSERT(decoded);
        if (!decoded)
            return std::make_tuple(FromBase64ShouldThrowError::Yes, read, WTFMove(bytes));

        if (chunkLength == 2 || chunkLength == 3)
            decoded->shrink(chunkLength - 1);

        bytes.appendVector(WTFMove(*decoded));
    }

    return std::make_tuple(FromBase64ShouldThrowError::No, length, WTFMove(bytes));
}

} // namespace JSC
