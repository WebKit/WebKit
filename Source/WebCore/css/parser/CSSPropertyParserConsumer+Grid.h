/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>

namespace WebCore {

class CSSGridLineNamesValue;
class CSSParserTokenRange;
class CSSValue;

enum CSSValueID : uint16_t;

namespace CSS {
struct PropertyParserState;
using GridNamedAreaMapRow = Vector<String, 8>;
}

namespace CSSPropertyParserHelpers {

// https://drafts.csswg.org/css-grid/

enum class AllowEmpty : bool { No, Yes };
enum TrackListType : uint8_t { GridTemplate, GridTemplateNoRepeat, GridAuto };

bool isGridBreadthIdent(CSSValueID);

// Parses a single <string> token from a <'grid-template-areas'> production.
std::optional<CSS::GridNamedAreaMapRow> consumeUnresolvedGridTemplateAreasRow(CSSParserTokenRange&, CSS::PropertyParserState&);

RefPtr<CSSGridLineNamesValue> consumeGridLineNames(CSSParserTokenRange&, CSS::PropertyParserState&, AllowEmpty = AllowEmpty::No);
RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeGridTrackSize(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeGridTrackList(CSSParserTokenRange&, CSS::PropertyParserState&, TrackListType = TrackListType::GridAuto);
RefPtr<CSSValue> consumeGridTemplatesRowsOrColumns(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange&, CSS::PropertyParserState&);
RefPtr<CSSValue> consumeGridAutoFlow(CSSParserTokenRange&, CSS::PropertyParserState&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
