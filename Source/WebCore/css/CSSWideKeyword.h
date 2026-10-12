/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSValueKeywords.h>
#include <optional>

namespace WebCore {

enum class CSSWideKeyword : uint8_t {
    Initial,
    Inherit,
    Unset,
    Revert,
    RevertLayer,
    RevertRule
};

inline bool isCSSWideKeyword(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Initial:
    case CSSValueID::Inherit:
    case CSSValueID::Unset:
    case CSSValueID::Revert:
    case CSSValueID::RevertLayer:
    case CSSValueID::RevertRule:
        return true;
    default:
        return false;
    };
}

inline std::optional<CSSWideKeyword> parseCSSWideKeyword(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueID::Initial:
        return CSSWideKeyword::Initial;
    case CSSValueID::Inherit:
        return CSSWideKeyword::Inherit;
    case CSSValueID::Unset:
        return CSSWideKeyword::Unset;
    case CSSValueID::Revert:
        return CSSWideKeyword::Revert;
    case CSSValueID::RevertLayer:
        return CSSWideKeyword::RevertLayer;
    case CSSValueID::RevertRule:
        return CSSWideKeyword::RevertRule;
    default:
        return { };
    }
}

inline CSSValueID toValueID(CSSWideKeyword keyword)
{
    switch (keyword) {
    case CSSWideKeyword::Initial:
        return CSSValueID::Initial;
    case CSSWideKeyword::Inherit:
        return CSSValueID::Inherit;
    case CSSWideKeyword::Unset:
        return CSSValueID::Unset;
    case CSSWideKeyword::Revert:
        return CSSValueID::Revert;
    case CSSWideKeyword::RevertLayer:
        return CSSValueID::RevertLayer;
    case CSSWideKeyword::RevertRule:
        return CSSValueID::RevertRule;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore
