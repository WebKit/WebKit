/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "Helpers/Test.h"
#include <WebCore/WebTransportHeaderValidation.h>
#include <wtf/KeyValuePair.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using WebCore::areValidWebTransportHeaders;

static Vector<KeyValuePair<String, String>> makeHeaders(std::initializer_list<std::pair<ASCIILiteral, ASCIILiteral>> pairs)
{
    Vector<KeyValuePair<String, String>> headers;
    headers.reserveInitialCapacity(pairs.size());
    for (auto& pair : pairs)
        headers.constructAndAppend(String(pair.first), String(pair.second));
    return headers;
}

TEST(WebTransportHeaderValidation, EmptyIsValid)
{
    EXPECT_TRUE(areValidWebTransportHeaders({ }));
}

TEST(WebTransportHeaderValidation, SimpleCustomHeaderIsValid)
{
    EXPECT_TRUE(areValidWebTransportHeaders(makeHeaders({ { "x-custom"_s, "value"_s } })));
}

TEST(WebTransportHeaderValidation, RejectsMixedCaseName)
{
    EXPECT_FALSE(areValidWebTransportHeaders(makeHeaders({ { "X-Custom"_s, "value"_s } })));
}

TEST(WebTransportHeaderValidation, RejectsInvalidHTTPToken)
{
    EXPECT_FALSE(areValidWebTransportHeaders(makeHeaders({ { "bad name"_s, "value"_s } })));
}

TEST(WebTransportHeaderValidation, RejectsWhenAnyHeaderInvalid)
{
    EXPECT_FALSE(areValidWebTransportHeaders(makeHeaders({
        { "x-good"_s, "ok"_s },
        { "cookie"_s, "oops"_s },
    })));
}

TEST(WebTransportHeaderValidation, AcceptsHighByteValues)
{
    StringBuilder builder;
    for (unsigned i = 0x80; i <= 0xFF; ++i)
        builder.append(static_cast<char16_t>(i));
    Vector<KeyValuePair<String, String>> headers;
    headers.constructAndAppend("x-bytes"_s, builder.toString());
    EXPECT_TRUE(areValidWebTransportHeaders(headers));
}

TEST(WebTransportHeaderValidation, RejectsUntrimmedValue)
{
    EXPECT_FALSE(areValidWebTransportHeaders(makeHeaders({ { "x-custom"_s, " value "_s } })));
}

} // namespace TestWebKitAPI
