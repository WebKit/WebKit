/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSCustomIdent.h>
#include <WebCore/CSSValueKeywords.h>
#include <optional>
#include <wtf/Variant.h>

namespace WebCore {

enum CSSPropertyID : uint16_t;

namespace CSSCalc {

// <random-value-sharing> = [ [ auto | <dashed-ident> ] || element-scoped ] | fixed <number [0,1]>
struct RandomSharingOptions {
    struct Auto {
        CSSPropertyID property;
        unsigned index;

        bool operator==(const Auto&) const = default;
    };
    Variant<Auto, CSS::CustomIdent> identifier;
    std::optional<CSS::Keyword::ElementScoped> elementScoped;

    bool operator==(const RandomSharingOptions&) const = default;
};

} // namespace CSSCalc
} // namespace WebCore
