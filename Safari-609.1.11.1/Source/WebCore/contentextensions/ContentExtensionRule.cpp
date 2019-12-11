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

#include "config.h"
#include "ContentExtensionRule.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {

namespace ContentExtensions {

ContentExtensionRule::ContentExtensionRule(Trigger&& trigger, Action&& action)
    : m_trigger(WTFMove(trigger))
    , m_action(WTFMove(action))
{
    ASSERT(!m_trigger.urlFilter.isEmpty());
}

static String deserializeString(const SerializedActionByte* actions, const uint32_t actionsLength, uint32_t beginIndex)
{
    uint32_t prefixLength = sizeof(uint32_t) + sizeof(bool);
    uint32_t stringStartIndex = beginIndex + prefixLength;
    RELEASE_ASSERT(actionsLength >= stringStartIndex);
    uint32_t stringLength = *reinterpret_cast<const uint32_t*>(&actions[beginIndex]);
    bool wideCharacters = actions[beginIndex + sizeof(uint32_t)];

    if (wideCharacters) {
        RELEASE_ASSERT(actionsLength >= stringStartIndex + stringLength * sizeof(UChar));
        return String(reinterpret_cast<const UChar*>(&actions[stringStartIndex]), stringLength);
    }
    RELEASE_ASSERT(actionsLength >= stringStartIndex + stringLength * sizeof(LChar));
    return String(reinterpret_cast<const LChar*>(&actions[stringStartIndex]), stringLength);
}

Action Action::deserialize(const SerializedActionByte* actions, const uint32_t actionsLength, uint32_t location)
{
    RELEASE_ASSERT(location < actionsLength);
    auto actionType = static_cast<ActionType>(actions[location]);
    switch (actionType) {
    case ActionType::BlockCookies:
    case ActionType::BlockLoad:
    case ActionType::IgnorePreviousRules:
    case ActionType::MakeHTTPS:
        return Action(actionType, location);
    case ActionType::CSSDisplayNoneSelector:
    case ActionType::Notify:
        return Action(actionType, deserializeString(actions, actionsLength, location + sizeof(ActionType)), location);
    }
    RELEASE_ASSERT_NOT_REACHED();
}
    
ActionType Action::deserializeType(const SerializedActionByte* actions, const uint32_t actionsLength, uint32_t location)
{
    RELEASE_ASSERT(location < actionsLength);
    ActionType type = static_cast<ActionType>(actions[location]);
    switch (type) {
    case ActionType::BlockCookies:
    case ActionType::BlockLoad:
    case ActionType::Notify:
    case ActionType::IgnorePreviousRules:
    case ActionType::CSSDisplayNoneSelector:
    case ActionType::MakeHTTPS:
        return type;
    }
    RELEASE_ASSERT_NOT_REACHED();
}
    
uint32_t Action::serializedLength(const SerializedActionByte* actions, const uint32_t actionsLength, uint32_t location)
{
    RELEASE_ASSERT(location < actionsLength);
    switch (static_cast<ActionType>(actions[location])) {
    case ActionType::BlockCookies:
    case ActionType::BlockLoad:
    case ActionType::IgnorePreviousRules:
    case ActionType::MakeHTTPS:
        return sizeof(ActionType);
    case ActionType::Notify:
    case ActionType::CSSDisplayNoneSelector: {
        uint32_t prefixLength = sizeof(ActionType) + sizeof(uint32_t) + sizeof(bool);
        uint32_t stringStartIndex = location + prefixLength;
        RELEASE_ASSERT(actionsLength >= stringStartIndex);
        uint32_t stringLength = *reinterpret_cast<const unsigned*>(&actions[location + sizeof(ActionType)]);
        bool wideCharacters = actions[location + sizeof(ActionType) + sizeof(uint32_t)];
        
        if (wideCharacters)
            return prefixLength + stringLength * sizeof(UChar);
        return prefixLength + stringLength * sizeof(LChar);
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Trigger Trigger::isolatedCopy() const
{
    return {
        urlFilter.isolatedCopy(),
        urlFilterIsCaseSensitive,
        topURLConditionIsCaseSensitive,
        flags,
        conditions.isolatedCopy(),
        conditionType
    };
}

Action Action::isolatedCopy() const
{
    if (hasStringArgument(m_type)) {
        return {
            m_type,
            m_stringArgument.isolatedCopy(),
            m_actionID
        };
    } else {
        return {
            m_type,
            m_actionID
        };
    }
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
