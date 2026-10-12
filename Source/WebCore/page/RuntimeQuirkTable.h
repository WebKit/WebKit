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

#include <WebCore/QuirkMatchPattern.h>
#include <WebCore/QuirkTable.h>
#include <WebCore/QuirksData.h>
#include <optional>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct RuntimeQuirk {
    Vector<QuirkMatchPattern> matches;
    Vector<QuirkMatchPattern> embeddedMatches;
    Vector<QuirkMatchPattern> excludeMatches;
    String queryContains;
    String fragmentContains;
    std::optional<URLEnvironment> environment;

    Vector<RuntimeQuirkBehavior> behaviors;

    WEBCORE_EXPORT static RuntimeQuirk from(const Quirk&);

    WEBCORE_EXPORT bool appliesTo(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument) const;
    void apply(QuirksData&) const;

    friend bool operator==(const RuntimeQuirk&, const RuntimeQuirk&) = default;
};

struct RuntimeQuirkTable {
    Vector<RuntimeQuirk> quirks;
};

enum class QuirkTableParseErrorKind : uint8_t {
    InvalidDocument,
    InvalidRow,
    InvalidBehavior,
    UnknownField,
    MissingField,
    DisallowedField,
    MissingMatches,
    InvalidString,
    InvalidStringArray,
    InvalidMatchPattern,
    InvalidEnvironment,
    InvalidAvailableExpression,
    InvalidBehaviors,
    UnknownBehavior,
    DuplicateBehavior,
    InvalidBugs,
    DuplicateBug,
};

struct QuirkTableParseError {
    struct Location {
        std::optional<size_t> row;
        String rowPattern;
        std::optional<size_t> behavior;
        std::optional<QuirkBehaviorID> behaviorID;
    };

    Location location;
    QuirkTableParseErrorKind kind;
    String field;
    String value;

    WEBCORE_EXPORT String description() const;
};

struct QuirkTableParseResult {
    RuntimeQuirkTable table;
    Vector<QuirkTableParseError> errors;
};

WEBCORE_EXPORT QuirkTableParseResult parseQuirkTable(StringView json);

} // namespace WebCore
