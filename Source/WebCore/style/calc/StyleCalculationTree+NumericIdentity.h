/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "StyleCalculationTree.h"

namespace WebCore {
namespace Style {
namespace Calculation {

// MARK: - NumericIdentity

enum class NumericIdentity : uint8_t {
    Number,
    Percentage,
    Dimension,
};
constexpr uint8_t numberOfNumericIdentityTypes = static_cast<uint8_t>(NumericIdentity::Dimension) + 1;

constexpr NumericIdentity toNumericIdentity(const Number&)
{
    return NumericIdentity::Number;
}

constexpr NumericIdentity toNumericIdentity(const Percentage&)
{
    return NumericIdentity::Percentage;
}

constexpr NumericIdentity toNumericIdentity(const Dimension&)
{
    return NumericIdentity::Dimension;
}

constexpr bool isLength(NumericIdentity identity)
{
    return identity == NumericIdentity::Dimension;
}

inline bool isNumeric(const Child& root)
{
    return WTF::switchOn(root,
        []<Numeric T>(const T&) { return true; },
        [](const auto&) { return false; }
    );
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
