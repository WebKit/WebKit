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
#include <WebCore/QuirkTable.h>
#include <algorithm>
#include <span>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct QuirkBehaviorReplacement {
    QuirkBehavior target;
    QuirkBehavior replacement;

    friend bool operator==(const QuirkBehaviorReplacement&, const QuirkBehaviorReplacement&) = default;
};

struct QuirkOverrides {
    QuirkBitSet disabledBehaviors;
    Vector<QuirkBehavior> removedBehaviors;
    Vector<QuirkBehaviorReplacement> replacedBehaviors;
    RefPtr<const QuirkOverlay> overlay;

    bool isEmpty() const { return disabledBehaviors.isEmpty() && removedBehaviors.isEmpty() && replacedBehaviors.isEmpty() && !overlay; }

    friend bool operator==(const QuirkOverrides&, const QuirkOverrides&) = default;
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

    inline const Vector<QuirkBehavior>& behaviors() const LIFETIME_BOUND
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

    inline void applyDisabledBehaviors(const QuirkBitSet& disabledBehaviors)
    {
        m_maskedFlags.merge(disabledBehaviors);
        disabledBehaviors.forEachSetBit([&](size_t index) {
            removeBehaviorsMatching(static_cast<QuirkBehaviorID>(index));
        });
    }

    inline void removeBehavior(const QuirkBehavior& target)
    {
        if (m_behaviors.removeFirst(target))
            refreshBehaviorFlag(target.id);
    }

    inline void replaceBehavior(const QuirkBehavior& target, const QuirkBehavior& replacement)
    {
        auto index = m_behaviors.find(target);
        if (index == notFound)
            return;
        if (m_maskedFlags.get(static_cast<size_t>(replacement.id)) || m_behaviors.contains(replacement))
            m_behaviors.removeAt(index);
        else
            m_behaviors[index] = replacement;
        refreshBehaviorFlag(target.id);
        refreshBehaviorFlag(replacement.id);
    }

    inline void applyEdits(const QuirkOverrides& overrides)
    {
        for (auto& target : overrides.removedBehaviors)
            removeBehavior(target);
        for (auto& [target, replacement] : overrides.replacedBehaviors)
            replaceBehavior(target, replacement);
    }

    inline void setEnabled(QuirkBehaviorID id, bool state)
    {
        if (state)
            addBehavior(QuirkBehavior { .id = id });
        else
            removeBehaviorsMatching(id);
    }

    inline void addBehavior(const QuirkBehavior& behavior)
    {
        if (m_maskedFlags.get(static_cast<size_t>(behavior.id)))
            return;
        m_behaviorFlags.set(static_cast<size_t>(behavior.id), true);
        if (!m_behaviors.contains(behavior))
            m_behaviors.append(behavior);
    }

    inline void removeBehaviorsMatching(QuirkBehaviorID id)
    {
        m_behaviorFlags.set(static_cast<size_t>(id), false);
        m_behaviors.removeAllMatching([&](const auto& behavior) { return behavior.id == id; });
    }

    void merge(const QuirksData& other)
    {
        for (auto& behavior : other.m_behaviors)
            addBehavior(behavior);
    }

private:
    inline void refreshBehaviorFlag(QuirkBehaviorID id)
    {
        m_behaviorFlags.set(static_cast<size_t>(id), std::ranges::any_of(m_behaviors, [&](const auto& behavior) { return behavior.id == id; }));
    }

    QuirkBitSet m_behaviorFlags;
    QuirkBitSet m_maskedFlags;
    Vector<QuirkBehavior> m_behaviors;
};

} // namespace WebCore
