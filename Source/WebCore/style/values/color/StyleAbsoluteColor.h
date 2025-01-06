/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSAbsoluteColor.h"
#include "CSSAbsoluteColorResolver.h"
#include "StyleColorResolutionState.h"
#include "StyleResolvedColor.h"

namespace WebCore {
namespace Style {

// NOTE: There is no Style::AbsoluteColor<D> type. Style conversion of CSS::AbsoluteColor<D> yields a Style::ResolvedColor.

template<typename D> Color toStyleColor(const CSS::AbsoluteColor<D>& unresolved, ColorResolutionState& state)
{
    ColorResolutionStateNester nester { state };

    auto resolver = CSS::AbsoluteColorResolver<D> {
        .components = unresolved.components,
        .nestingLevel = state.nestingLevel
    };

    return Color { ResolvedColor { resolve(WTFMove(resolver), state.conversionData) } };
}

} // namespace Style
} // namespace WebCore
