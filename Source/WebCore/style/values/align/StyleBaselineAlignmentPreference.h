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

// <baseline-position-preference> = first | last
// https://drafts.csswg.org/css-align/#baseline-alignment-preference
using BaselineAlignmentPreference = Variant<CSS::Keyword::First, CSS::Keyword::Last>;

// Used by alignment types in bitfields.
enum class BaselineAlignmentPreferenceKind : bool {
    First,
    Last
};

constexpr BaselineAlignmentPreferenceKind computeKind(std::optional<BaselineAlignmentPreference> preference)
{
    if (!preference)
        return BaselineAlignmentPreferenceKind::First;
    return WTF::switchOn(*preference,
        [](CSS::Keyword::First) { return BaselineAlignmentPreferenceKind::First; },
        [](CSS::Keyword::Last) { return BaselineAlignmentPreferenceKind::Last; }
    );
}

constexpr decltype(auto) visitBaselineAlignmentPreference(auto&& primaryKeyword, BaselineAlignmentPreferenceKind kind, auto&& visitor)
{
    switch (kind) {
    case BaselineAlignmentPreferenceKind::First:
        return visitor(primaryKeyword);
    case BaselineAlignmentPreferenceKind::Last:
        return visitor(SpaceSeparatedTuple { CSS::Keyword::Last { }, primaryKeyword });
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace Style
} // namespace WebCore
