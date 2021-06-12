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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ParsedRequestRange.h"

#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

std::optional<ParsedRequestRange> ParsedRequestRange::parse(StringView input)
{
    // https://tools.ietf.org/html/rfc7233#section-2.1 but assuming there will always be a begin and an end or parsing will fail
    if (!input.startsWith(StringView("bytes="_s)))
        return std::nullopt;

    size_t begin { 0 };
    size_t end { 0 };
    size_t rangeBeginPosition = 6;
    size_t dashPosition = input.find('-', rangeBeginPosition);
    if (dashPosition == notFound)
        return std::nullopt;

    auto beginString = input.substring(rangeBeginPosition, dashPosition - rangeBeginPosition);
    auto optionalBegin = parseInteger<uint64_t>(beginString);
    if (!optionalBegin)
        return std::nullopt;
    begin = *optionalBegin;

    auto endString = input.substring(dashPosition + 1);
    auto optionalEnd = parseInteger<uint64_t>(endString);
    if (!optionalEnd)
        return std::nullopt;
    end = *optionalEnd;

    if (begin > end)
        return std::nullopt;

    return {{ begin, end }};
}

}
