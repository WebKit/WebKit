/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <legacy-position> = left | right | center
// https://drafts.csswg.org/css-align/#valdef-justify-items-legacy
using LegacyPosition = Variant<CSS::Keyword::Left, CSS::Keyword::Right, CSS::Keyword::Center>;

// Used by alignment types in bitfields.
enum class LegacyPositionKind : uint8_t {
    None,
    Left,
    Right,
    Center
};

constexpr LegacyPositionKind computeKind(std::optional<LegacyPosition> position)
{
    if (!position)
        return LegacyPositionKind::None;
    return WTF::switchOn(*position,
        [](CSS::Keyword::Left) { return LegacyPositionKind::Left; },
        [](CSS::Keyword::Right) { return LegacyPositionKind::Right; },
        [](CSS::Keyword::Center) { return LegacyPositionKind::Center; }
    );
}

constexpr decltype(auto) visitLegacyPosition(auto&& primaryKeyword, LegacyPositionKind kind, auto&& visitor)
{
    switch (kind) {
    case LegacyPositionKind::None:
        // `legacy` on its own computes to `normal`.
        // https://drafts.csswg.org/css-align/#valdef-justify-items-legacy
        return visitor(CSS::Keyword::Normal { });
    case LegacyPositionKind::Left:
        return visitor(SpaceSeparatedTuple { primaryKeyword, CSS::Keyword::Left { } });
    case LegacyPositionKind::Right:
        return visitor(SpaceSeparatedTuple { primaryKeyword, CSS::Keyword::Right { } });
    case LegacyPositionKind::Center:
        return visitor(SpaceSeparatedTuple { primaryKeyword, CSS::Keyword::Center { } });
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace Style
} // namespace WebCore
