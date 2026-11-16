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

#include <optional>

namespace WebCore {
namespace Style {
namespace Calculation {

struct Child;
struct Tree;

// Replaces every `size` keyword with `insertion`. Fails if the result would be too large.
// Most nodes a calc-size() calculation may grow to. Nesting multiplies the calculation rather than
// deepening it, so depth alone does not bound it.
// https://github.com/w3c/csswg-drafts/issues/10369
constexpr size_t maximumCalcSizeNodeCount = 1024;

std::optional<Tree> substituteSize(const Tree&, const Child& insertion);

// Replaces every percentage P with `size * P / 100`, to keep a percentage basis interpolating
// linearly rather than quadratically.
Tree dePercentify(const Tree&);

bool containsSize(const Tree&);
bool containsPercentage(const Tree&);

} // namespace Calculation
} // namespace Style
} // namespace WebCore
