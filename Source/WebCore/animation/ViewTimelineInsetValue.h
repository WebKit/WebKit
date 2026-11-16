/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSNumericValue.h"
#include "CSSOMKeywordValue.h"
#include <optional>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;

namespace Style {
struct ViewTimelineInsetItem;
}

// https://drafts.csswg.org/scroll-animations-1/#dom-viewtimelineoptions-inset

// FIXME: This diverges from the spec to match Chrome by using the union
// `(CSSNumericValue or CSSOMKeywordish)` instead of the union
// `(CSSNumericValue or CSSOMKeywordValue)`. You don't see `CSSOMKeywordish`
// below because it is defined as the union `(DOMString or CSSOMKeywordValue)`
// which gets flattened into the union `(CSSNumericValue or DOMString or CSSOMKeywordValue)`.
// Tracked via https://github.com/w3c/csswg-drafts/issues/11477

using ViewTimelineIndividualInset = Variant<Ref<CSSNumericValue>, String, Ref<CSSOMKeywordValue>>;
using ViewTimelineInsetValue = Variant<String, Vector<ViewTimelineIndividualInset>>;

std::optional<Style::ViewTimelineInsetItem> validateViewTimelineInset(ViewTimelineInsetValue&&, const Document&);

} // namespace WebCore
