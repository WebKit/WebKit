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

#include <WebCore/QuirkBehaviorID.h>
#include <array>
#include <span>
#include <wtf/Assertions.h>
#include <wtf/OptionSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

class URLPatternList {
public:
    constexpr URLPatternList() = default;

    constexpr URLPatternList(ASCIILiteral pattern)
        : m_single(pattern)
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!pattern.isNull());
    }

    template<size_t size> constexpr URLPatternList(const std::array<ASCIILiteral, size>& patterns LIFETIME_BOUND)
        : m_multiple(patterns)
    {
        static_assert(size, "A URL pattern list must name at least one pattern.");
    }

    constexpr bool isEmpty() const { return m_single.isNull() && m_multiple.empty(); }

    constexpr std::span<const ASCIILiteral> span() const LIFETIME_BOUND
    {
        if (!m_multiple.empty())
            return m_multiple;
        if (m_single.isNull())
            return { };
        return singleElementSpan(m_single);
    }

private:
    ASCIILiteral m_single;
    std::span<const ASCIILiteral> m_multiple;
};

struct QuirkParameters {
    ASCIILiteral script { };
    ASCIILiteral userAgent { };
    ASCIILiteral chromeCompatibilityVersion { };
    std::span<const ASCIILiteral> cookieNames { };

    static consteval QuirkParameters fromScript(ASCIILiteral script)
    {
        return QuirkParameters {
            .script = script
        };
    }

    static consteval QuirkParameters fromUserAgent(ASCIILiteral userAgent)
    {
        return QuirkParameters {
            .userAgent = userAgent
        };
    }

    static consteval QuirkParameters fromChromeCompatibilityVersion(ASCIILiteral chromeCompatibilityVersion)
    {
        return QuirkParameters {
            .chromeCompatibilityVersion = chromeCompatibilityVersion
        };
    }

    static consteval QuirkParameters fromCookieNames(std::span<const ASCIILiteral> cookieNames)
    {
        return QuirkParameters {
            .cookieNames = cookieNames
        };
    }
};

enum class QuirkParametersNeeded : uint8_t {
    NeedsScript = 1 << 0,
    NeedsUserAgent = 1 << 1,
    NeedsChromeCompatibilityVersion = 1 << 2,
    NeedsCookieNames = 1 << 3,
};

enum class QuirkConditionsSupported : uint8_t {
    ElementSelector = 1 << 0,
    SecondaryURL = 1 << 1,
    DocumentSelector = 1 << 2,
};

namespace QuirkBehaviorConditions {
struct ElementMatchesSelector {
    ASCIILiteral selector;
};

struct SecondaryURLMatches {
    URLPatternList patterns;
};

struct DocumentHasElementMatching {
    ASCIILiteral selector;
};

constexpr ElementMatchesSelector elementMatchesSelector(ASCIILiteral selector)
{
    return ElementMatchesSelector { selector };
}

constexpr SecondaryURLMatches secondaryURLMatches(URLPatternList patterns)
{
    return SecondaryURLMatches { patterns };
}

constexpr DocumentHasElementMatching documentHasElementMatching(ASCIILiteral selector)
{
    return DocumentHasElementMatching { selector };
}

} // namespace QuirkBehaviorConditions

struct QuirkConditions {
    std::optional<ASCIILiteral> elementSelector { std::nullopt };
    URLPatternList secondaryURL { };
    std::optional<ASCIILiteral> documentSelector { std::nullopt };
};

struct QuirkBehavior {
    QuirkBehaviorID id;
    bool isAvailable { false };
    OptionSet<QuirkParametersNeeded> quirkParametersNeeded { };
    OptionSet<QuirkConditionsSupported> quirkConditionsSupported { };
    OptionSet<QuirkConditionsSupported> quirkConditionsNeeded { };
    QuirkConditions conditions { };
    std::optional<QuirkParameters> parameters { std::nullopt };

    consteval QuirkBehavior operator()(QuirkParameters params) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!quirkParametersNeeded.isEmpty());
        auto copy = *this;
        copy.parameters = params;
        return copy;
    }

    template<typename... Conditions> consteval QuirkBehavior when(Conditions... conditions) const
    {
        static_assert(sizeof...(conditions), "when() must name at least one condition");
        auto copy = *this;
        (applyCondition(copy, conditions), ...);
        return copy;
    }

    consteval void applyCondition(QuirkBehavior& behavior, QuirkBehaviorConditions::ElementMatchesSelector elementMatchesSelector) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::ElementSelector));
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!behavior.conditions.elementSelector);
        behavior.conditions.elementSelector = elementMatchesSelector.selector;
    }

    consteval void applyCondition(QuirkBehavior& behavior, QuirkBehaviorConditions::SecondaryURLMatches secondaryURLMatches) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::SecondaryURL));
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.conditions.secondaryURL.isEmpty());
        behavior.conditions.secondaryURL = secondaryURLMatches.patterns;
    }

    consteval void applyCondition(QuirkBehavior& behavior, QuirkBehaviorConditions::DocumentHasElementMatching documentHasElementMatching) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::DocumentSelector));
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!behavior.conditions.documentSelector);
        behavior.conditions.documentSelector = documentHasElementMatching.selector;
    }
};

} // namespace WebCore
