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

#include <WebCore/CSSProperty.h>
#include <wtf/Forward.h>

namespace WebCore {

class CSSValue;

namespace CSS {

struct PropertyParserState;

struct PropertyParserResult {
    Vector<CSSProperty, 256>& parsedProperties;

    void addProperty(CSSProperty&&);

    // Bottleneck where the CSSValue is added to the CSSProperty vector.
    void addProperty(PropertyParserState&, CSSPropertyID longhand, CSSPropertyID shorthand, RefPtr<CSSValue>&&, IsImportant, IsImplicit = IsImplicit::No);

    // Utility functions to make adding properties more ergonomic.
    void addPropertyForCurrentShorthand(PropertyParserState&, CSSPropertyID longhand, RefPtr<CSSValue>&&, IsImplicit = IsImplicit::No);
    void addPropertyForAllLonghandsOfShorthand(PropertyParserState&, CSSPropertyID shorthand, RefPtr<CSSValue>&&, IsImportant, IsImplicit = IsImplicit::No);
    void addPropertyForAllLonghandsOfCurrentShorthand(PropertyParserState&, RefPtr<CSSValue>&&, IsImplicit = IsImplicit::No);
};

} // namespace CSS
} // namespace WebCore
