/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#include <wtf/CrossThreadCopier.h>

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore::ContentExtensions {

ContentExtensionRule::ContentExtensionRule(Trigger&& trigger, Action&& action)
    : m_trigger(WTFMove(trigger))
    , m_action(WTFMove(action))
{
    ASSERT(!m_trigger.urlFilter.isEmpty());
}

template<size_t index, typename... Types>
struct VariantDeserializerHelper {
    using VariantType = typename std::variant_alternative<index, std::variant<Types...>>::type;
    static std::variant<Types...> deserialize(std::span<const uint8_t> span, size_t i)
    {
        if (i == index)
            return VariantType::deserialize(span);
        return VariantDeserializerHelper<index - 1, Types...>::deserialize(span, i);
    }
    static size_t serializedLength(std::span<const uint8_t> span, size_t i)
    {
        if (i == index)
            return VariantType::serializedLength(span);
        return VariantDeserializerHelper<index - 1, Types...>::serializedLength(span, i);
    }
};

template<typename... Types>
struct VariantDeserializerHelper<0, Types...> {
    using VariantType = typename std::variant_alternative<0, std::variant<Types...>>::type;
    static std::variant<Types...> deserialize(std::span<const uint8_t> span, size_t i)
    {
        ASSERT_UNUSED(i, !i);
        return VariantType::deserialize(span);
    }
    static size_t serializedLength(std::span<const uint8_t> span, size_t i)
    {
        ASSERT_UNUSED(i, !i);
        return VariantType::serializedLength(span);
    }
};

template<typename T> struct VariantDeserializer;
template<typename... Types> struct VariantDeserializer<std::variant<Types...>> {
    static std::variant<Types...> deserialize(std::span<const uint8_t> span, size_t i)
    {
        return VariantDeserializerHelper<sizeof...(Types) - 1, Types...>::deserialize(span, i);
    }
    static size_t serializedLength(std::span<const uint8_t> span, size_t i)
    {
        return VariantDeserializerHelper<sizeof...(Types) - 1, Types...>::serializedLength(span, i);
    }
};

DeserializedAction DeserializedAction::deserialize(std::span<const uint8_t> serializedActions, uint32_t location)
{
    RELEASE_ASSERT(location < serializedActions.size());
    return { location, VariantDeserializer<ActionData>::deserialize(serializedActions.subspan(location + 1), serializedActions[location]) };
}

size_t DeserializedAction::serializedLength(std::span<const uint8_t> serializedActions, uint32_t location)
{
    RELEASE_ASSERT(location < serializedActions.size());
    return 1 + VariantDeserializer<ActionData>::serializedLength(serializedActions.subspan(location + 1), serializedActions[location]);
}

Trigger Trigger::isolatedCopy() const &
{
    return { urlFilter.isolatedCopy(), urlFilterIsCaseSensitive, topURLFilterIsCaseSensitive, frameURLFilterIsCaseSensitive, flags, crossThreadCopy(conditions) };
}

Trigger Trigger::isolatedCopy() &&
{
    return { WTFMove(urlFilter).isolatedCopy(), urlFilterIsCaseSensitive, topURLFilterIsCaseSensitive, frameURLFilterIsCaseSensitive, flags, crossThreadCopy(WTFMove(conditions)) };
}

Action Action::isolatedCopy() const &
{
    return { crossThreadCopy(m_data) };
}

Action Action::isolatedCopy() &&
{
    return { crossThreadCopy(WTFMove(m_data)) };
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
