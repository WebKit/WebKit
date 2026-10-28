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
#include <WebCore/QuirkBehaviors.h>
#include <optional>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class QuirksData;

WEBCORE_EXPORT std::optional<QuirkBehaviorID> quirkBehaviorIDFromName(StringView);

struct Quirk {
    Vector<QuirkMatchPattern> matches;
    Vector<QuirkMatchPattern> embeddedMatches;
    Vector<QuirkMatchPattern> excludeMatches;
    String queryContains;
    String fragmentContains;
    std::optional<URLEnvironment> environment;

    Vector<QuirkBehavior> behaviors;

    WEBCORE_EXPORT bool appliesTo(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument) const;
    void apply(QuirksData&) const;

    friend bool operator==(const Quirk&, const Quirk&) = default;
};

// The contents of QuirkTable.json, parsed at runtime; what quirkTable() and setQuirkTable() hold.
struct QuirkTable {
    Vector<Quirk> quirks;
};

class QuirkOverlay : public RefCounted<QuirkOverlay> {
public:
    static Ref<QuirkOverlay> create(QuirkTable&& table) { return adoptRef(*new QuirkOverlay(WTF::move(table))); }

    const QuirkTable& table() const LIFETIME_BOUND { return m_table; }

private:
    explicit QuirkOverlay(QuirkTable&& table)
        : m_table(WTF::move(table))
    {
    }

    QuirkTable m_table;
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
    QuirkTable table;
    Vector<QuirkTableParseError> errors;
};

WEBCORE_EXPORT QuirkTableParseResult parseQuirkTable(StringView json);

WEBCORE_EXPORT String loadQuirkTableJSON();

WEBCORE_EXPORT const QuirkTable& quirkTable();

WEBCORE_EXPORT void setQuirkTable(QuirkTable&&);

struct QuirkTableOverride {
    WEBCORE_EXPORT static void install(QuirkTable&&);
    WEBCORE_EXPORT static void restore();
};

} // namespace WebCore
