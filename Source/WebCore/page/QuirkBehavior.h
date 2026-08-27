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

#include <WebCore/QuirkMatch.h>
#include <WebCore/QuirkNames.h>
#include <concepts>
#include <optional>
#include <wtf/Assertions.h>
#include <wtf/EnumTraits.h>
#include <wtf/Variant.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringView.h>

namespace WebCore {

namespace ParameterizedQuirk {

struct EvaluateScriptBeforeRunningScript {
    static constexpr auto name = "EvaluateScriptBeforeRunningScript"_s;

    ASCIILiteral script;
    std::optional<QuirkMatch> gate;

    bool appliesTo(const URL& scriptURL) const { return !gate || gate->matches(scriptURL); }

    static EvaluateScriptBeforeRunningScript create(ASCIILiteral script, std::optional<QuirkMatch> gate = std::nullopt)
    {
        RELEASE_ASSERT(!script.isNull());
        return { script, gate };
    }
};

} // namespace ParameterizedQuirk

template<typename T> concept IsParameterizedQuirk = requires {
    { T::name } -> std::convertible_to<ASCIILiteral>;
};

using QuirkBehavior = Variant<
    SiteSpecificQuirk,
    ParameterizedQuirk::EvaluateScriptBeforeRunningScript
>;

inline StringView quirkBehaviorName(const QuirkBehavior& behavior)
{
    return WTF::switchOn(behavior,
        [](SiteSpecificQuirk quirk) -> StringView { return WTF::enumName(quirk); },
        [](const IsParameterizedQuirk auto& payload) -> StringView { return payload.name; });
}

} // namespace WebCore
