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

#include <WebCore/QuirkBehaviors.h>
#include <WebCore/QuirkMatchPattern.h>
#include <WebCore/QuirksData.h>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <span>

namespace WebCore {

class QuirkBehaviorList {
public:
    constexpr QuirkBehaviorList() = default;
    consteval QuirkBehaviorList(std::initializer_list<QuirkBehavior> behaviors)
    {
        for (auto& behavior : behaviors)
            m_behaviors[m_count++] = behavior;

        removeUnavailable();
    }

    constexpr std::span<const QuirkBehavior> span() const LIFETIME_BOUND { return std::span { m_behaviors }.first(m_count); }

private:
    static constexpr size_t maxBehaviors = 12;

    constexpr void removeUnavailable()
    {
        const auto behaviors = std::span { m_behaviors }.first(m_count);
        const auto unavailable = std::ranges::remove_if(behaviors, std::not_fn(&QuirkBehavior::isAvailable));
        m_count -= unavailable.size();
    }

    std::array<QuirkBehavior, maxBehaviors> m_behaviors { };
    size_t m_count { 0 };
};

enum class IsTopDocument : bool { No, Yes };

struct Quirk {
    URLPatternList matches { };
    URLPatternList embeddedMatches { };
    URLPatternList excludeMatches { };
    ASCIILiteral queryContains { };
    ASCIILiteral fragmentContains { };
    std::optional<URLEnvironment> environment { };
    QuirkBehaviorList behaviors { };
    bool isAvailable { true };
};

WEBCORE_EXPORT QuirksData resolveSiteSpecificQuirks(const URL& topURL, const URL& documentURL, IsTopDocument);

// For callers with no Document, which therefore only see top-URL quirks.
WEBCORE_EXPORT QuirksData resolveTopURLQuirks(const URL&);

WEBCORE_EXPORT std::span<const Quirk> LEGACY_NODELETE compiledQuirks();

} // namespace WebCore
