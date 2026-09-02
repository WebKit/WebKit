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

#pragma once

#include <span>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class SecurityOriginData;

constexpr size_t defaultWellKnownMaxResourceSize = 64 * 1024;

struct WellKnownOriginListPolicy {
    size_t maxRegistrableOriginLabels { 5 };
    size_t maxCandidates { 100 };
    size_t maxResourceSize { defaultWellKnownMaxResourceSize };
};

enum class WellKnownOriginListResult : uint8_t {
    Found,
    NotFound,
    Malformed,
};

WEBCORE_EXPORT URL wellKnownURL(StringView host, StringView path);

WEBCORE_EXPORT bool isWellKnownResponseAcceptable(int httpStatusCode, StringView mimeType);

WEBCORE_EXPORT bool isWellKnownRedirectAllowed(const URL&);

WEBCORE_EXPORT WellKnownOriginListResult findOriginInWellKnownList(const SecurityOriginData& callerOrigin, std::span<const uint8_t> resource, ASCIILiteral memberName, WellKnownOriginListPolicy&& = { });

WEBCORE_EXPORT Vector<String> parseOriginsFromWellKnownList(std::span<const uint8_t> resource, ASCIILiteral memberName, WellKnownOriginListPolicy&& = { });

} // namespace WebCore
