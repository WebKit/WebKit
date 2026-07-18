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

#include <WebCore/StylePrimitiveNumericOrKeyword.h>
#include <concepts>

namespace WebCore {
namespace Style {

// Compile-time analog of the `isIntrinsicOrStretch()` predicate shared by the sizing
// types (PreferredSize, MinimumSize, MaximumSize, FlexBasis). It matches exactly the
// keywords those types classify as intrinsic-or-stretch, for use as a constraint on
// `WTF::switchOn` visitor lambdas.
template<typename T>
concept IsIntrinsicOrStretchSizeKeyword = std::same_as<T, CSS::Keyword::MinContent>
    || std::same_as<T, CSS::Keyword::MaxContent>
    || std::same_as<T, CSS::Keyword::FitContent>
    || std::same_as<T, CSS::Keyword::Stretch>
    || std::same_as<T, CSS::Keyword::WebkitFillAvailable>;

} // namespace Style
} // namespace WebCore
