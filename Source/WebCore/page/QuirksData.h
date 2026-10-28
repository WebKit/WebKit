/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include <algorithm>
#include <optional>
#include <span>
#include <wtf/StdLibExtras.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// Indices into the current quirk table: valid for the table that produced them, so never persist them
// without a table version or content hash.
struct QuirkTablePosition {
    size_t row;
    size_t behavior;

    friend bool operator==(const QuirkTablePosition&, const QuirkTablePosition&) = default;
};

struct QuirkPlatformSource {
    QuirkBehaviorID id;

    friend bool operator==(const QuirkPlatformSource&, const QuirkPlatformSource&) = default;
};

using QuirkBehaviorSource = Variant<QuirkTablePosition, QuirkPlatformSource>;

struct QuirkParameterOverride {
    QuirkBehaviorSource targetSource;

    std::optional<String> script { };
    std::optional<String> userAgent { };
    std::optional<String> chromeCompatibilityVersion { };
    std::optional<Vector<String>> cookieNames { };

    friend bool operator==(const QuirkParameterOverride&, const QuirkParameterOverride&) = default;
};

struct QuirkOverrides {
    QuirkBitSet disabledBehaviors;
    Vector<QuirkBehavior> enabledBehaviors;
    Vector<QuirkParameterOverride> parameterOverrides;

    bool isEmpty() const { return disabledBehaviors.isEmpty() && enabledBehaviors.isEmpty() && parameterOverrides.isEmpty(); }

    void upsertParameterOverride(QuirkParameterOverride&& edit)
    {
        for (auto& existing : parameterOverrides) {
            if (existing.targetSource == edit.targetSource) {
                existing = WTF::move(edit);
                return;
            }
        }
        parameterOverrides.append(WTF::move(edit));
    }

    friend bool operator==(const QuirkOverrides&, const QuirkOverrides&) = default;
};

struct ActiveQuirkBehavior : QuirkBehavior {
    Vector<QuirkBehaviorSource, 1> sources;

    void addSource(const QuirkBehaviorSource& source)
    {
        if (!sources.contains(source))
            sources.append(source);
    }
};

class QuirksData {
public:
    inline bool isBehaviorEnabled(const QuirkBehaviorID& id) const
    {
        return m_behaviorFlags.get(static_cast<size_t>(id));
    }

    inline bool hasBehaviors() const
    {
        return !m_behaviorFlags.isEmpty();
    }

    inline bool hasSameBehaviorFlags(const QuirksData& other) const
    {
        return m_behaviorFlags == other.m_behaviorFlags;
    }

    inline const Vector<ActiveQuirkBehavior>& behaviors() const LIFETIME_BOUND
    {
        return m_behaviors;
    }

    inline Vector<QuirkBehavior> behaviorsMatching(QuirkBehaviorID id) const
    {
        return WTF::compactMap(m_behaviors, [&](const auto& behavior) -> std::optional<QuirkBehavior> {
            if (behavior.id == id)
                return behavior;
            return std::nullopt;
        });
    }

    inline bool behaviorAppliesToURL(QuirkBehaviorID id, const URL& url) const
    {
        if (!isBehaviorEnabled(id))
            return false;

        URLMatchContext context { url };
        return std::ranges::any_of(m_behaviors, [&](const auto& behavior) {
            return behavior.id == id && behavior.secondaryURLConditionMatches(context);
        });
    }

    inline bool hasBehaviorFrom(const QuirkBehaviorSource& source) const
    {
        return std::ranges::any_of(m_behaviors, [&](const auto& behavior) {
            return behavior.sources.contains(source);
        });
    }

    inline void applyDisabledBehaviors(const QuirkOverrides& overrides)
    {
        m_maskedFlags.merge(overrides.disabledBehaviors);
        overrides.disabledBehaviors.forEachSetBit([&](size_t index) {
            removeBehaviorsMatching(static_cast<QuirkBehaviorID>(index));
        });
    }

    inline void applyEnabledBehaviorsAndParameterOverrides(const QuirkOverrides& overrides)
    {
        for (auto& behavior : overrides.enabledBehaviors)
            addBehavior(behavior);

        for (auto& parameterOverride : overrides.parameterOverrides) {
            for (auto& behavior : m_behaviors) {
                if (!behavior.sources.contains(parameterOverride.targetSource))
                    continue;
                if (parameterOverride.script)
                    behavior.script = *parameterOverride.script;
                if (parameterOverride.userAgent)
                    behavior.userAgent = *parameterOverride.userAgent;
                if (parameterOverride.chromeCompatibilityVersion)
                    behavior.chromeCompatibilityVersion = *parameterOverride.chromeCompatibilityVersion;
                if (parameterOverride.cookieNames)
                    behavior.cookieNames = *parameterOverride.cookieNames;
            }
        }
    }

    inline void setEnabled(QuirkBehaviorID id, bool state)
    {
        if (state)
            addBehavior(QuirkBehavior { .id = id }, QuirkPlatformSource { id });
        else
            removeBehaviorsMatching(id);
    }

    inline void addBehavior(const QuirkBehavior& behavior, std::optional<QuirkBehaviorSource> source = std::nullopt)
    {
        ActiveQuirkBehavior active { behavior, { } };
        if (source)
            active.addSource(*source);
        addActiveBehavior(WTF::move(active));
    }

    inline void removeBehaviorsMatching(QuirkBehaviorID id)
    {
        m_behaviorFlags.set(static_cast<size_t>(id), false);
        m_behaviors.removeAllMatching([&](const auto& behavior) { return behavior.id == id; });
    }

    void merge(const QuirksData& other)
    {
        for (auto& behavior : other.m_behaviors)
            addActiveBehavior(behavior);
    }

private:
    QuirkBitSet m_behaviorFlags;
    QuirkBitSet m_maskedFlags;
    Vector<ActiveQuirkBehavior> m_behaviors;

    // An equal behavior that is already present gains the sources instead of being added twice.
    inline void addActiveBehavior(const ActiveQuirkBehavior& incoming)
    {
        if (m_maskedFlags.get(static_cast<size_t>(incoming.id)))
            return;
        m_behaviorFlags.set(static_cast<size_t>(incoming.id), true);

        auto existing = std::ranges::find_if(m_behaviors, [&](const auto& behavior) {
            return static_cast<const QuirkBehavior&>(behavior) == static_cast<const QuirkBehavior&>(incoming);
        });
        if (existing == m_behaviors.end()) {
            m_behaviors.append(incoming);
            return;
        }
        for (auto& source : incoming.sources)
            existing->addSource(source);
    }
};

} // namespace WebCore
