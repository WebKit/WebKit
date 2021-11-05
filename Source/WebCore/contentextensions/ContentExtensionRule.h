/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionActions.h"
#include "ResourceLoadInfo.h"
#include <wtf/Hasher.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace ContentExtensions {

// A ContentExtensionRule is the smallest unit in a ContentExtension.
//
// It is composed of a trigger and an action. The trigger defines on what kind of content this extension should apply.
// The action defines what to perform on that content.

struct Trigger {
    String urlFilter;
    bool urlFilterIsCaseSensitive { false };
    bool topURLConditionIsCaseSensitive { false };
    ResourceFlags flags { 0 };
    Vector<String> conditions;
    enum class ConditionType {
        None,
        IfDomain,
        UnlessDomain,
        IfTopURL,
        UnlessTopURL,
    } conditionType { ConditionType::None };

    WEBCORE_EXPORT Trigger isolatedCopy() const;
    
    ~Trigger()
    {
        ASSERT(conditions.isEmpty() == (conditionType == ConditionType::None));
        if (topURLConditionIsCaseSensitive)
            ASSERT(conditionType == ConditionType::IfTopURL || conditionType == ConditionType::UnlessTopURL);
    }

    bool isEmpty() const
    {
        return urlFilter.isEmpty()
            && !urlFilterIsCaseSensitive
            && !topURLConditionIsCaseSensitive
            && !flags
            && conditions.isEmpty()
            && conditionType == ConditionType::None;
    }

    bool operator==(const Trigger& other) const
    {
        return urlFilter == other.urlFilter
            && urlFilterIsCaseSensitive == other.urlFilterIsCaseSensitive
            && topURLConditionIsCaseSensitive == other.topURLConditionIsCaseSensitive
            && flags == other.flags
            && conditions == other.conditions
            && conditionType == other.conditionType;
    }
};

struct TriggerHash {
    static unsigned hash(const Trigger& trigger)
    {
        return computeHash(trigger.urlFilterIsCaseSensitive, trigger.urlFilter, trigger.flags, trigger.conditions, trigger.conditionType);
    }
    static bool equal(const Trigger& a, const Trigger& b)
    {
        return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct TriggerHashTraits : public WTF::CustomHashTraits<Trigger> {
    static constexpr bool emptyValueIsZero = false;
    static constexpr bool hasIsEmptyValueFunction = true;

    static void constructDeletedValue(Trigger& trigger)
    {
        new (NotNull, std::addressof(trigger.urlFilter)) String(WTF::HashTableDeletedValue);
    }

    static bool isDeletedValue(const Trigger& trigger)
    {
        return trigger.urlFilter.isHashTableDeletedValue();
    }

    static Trigger emptyValue()
    {
        return Trigger();
    }

    static bool isEmptyValue(const Trigger& trigger)
    {
        return trigger.isEmpty();
    }
};

struct Action {
    Action(ActionData&& data)
        : m_data(WTFMove(data)) { }

    bool operator==(const Action& other) const { return m_data == other.m_data; }
    bool operator!=(const Action& other) const { return !(*this == other); }

    const ActionData& data() const { return m_data; }

    WEBCORE_EXPORT Action isolatedCopy() const;

private:
    const ActionData m_data;
};

struct DeserializedAction : public Action {
    static DeserializedAction deserialize(const SerializedActionByte* actions, const uint32_t actionsLength, uint32_t location);
    static size_t serializedLength(const SerializedActionByte* actions, const uint32_t actionsLength, uint32_t location);

    uint32_t actionID() const { return m_actionID; }

private:
    DeserializedAction(uint32_t actionID, ActionData&& data)
        : Action(WTFMove(data))
        , m_actionID(actionID) { }

    const uint32_t m_actionID;
};

class ContentExtensionRule {
public:
    WEBCORE_EXPORT ContentExtensionRule(Trigger&&, Action&&);

    const Trigger& trigger() const { return m_trigger; }
    const Action& action() const { return m_action; }

    ContentExtensionRule isolatedCopy() const
    {
        return { m_trigger.isolatedCopy(), m_action.isolatedCopy() };
    }
    bool operator==(const ContentExtensionRule& other) const
    {
        return m_trigger == other.m_trigger && m_action == other.m_action;
    }

private:
    const Trigger m_trigger;
    const Action m_action;
};

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
