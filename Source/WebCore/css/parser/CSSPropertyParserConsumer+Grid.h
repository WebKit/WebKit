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

struct CSSParserContext;
struct NamedGridAreaMap;

enum CSSValueID : uint16_t;

namespace CSSPropertyParserHelpers {

// https://drafts.csswg.org/css-grid/

enum class AllowEmpty : bool { No, Yes };
enum TrackListType : uint8_t { GridTemplate, GridTemplateNoRepeat, GridAuto };

bool isGridBreadthIdent(CSSValueID);
bool parseGridTemplateAreasRow(StringView gridRowNames, NamedGridAreaMap&, const size_t rowCount, size_t& columnCount);
RefPtr<CSSGridLineNamesValue> consumeGridLineNames(CSSParserTokenRange&, const CSSParserContext&, AllowEmpty = AllowEmpty::No);
RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeGridTrackSize(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeGridTrackList(CSSParserTokenRange&, const CSSParserContext&, TrackListType);
RefPtr<CSSValue> consumeGridTemplatesRowsOrColumns(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeGridAutoFlow(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeMasonryAutoFlow(CSSParserTokenRange&, const CSSParserContext&);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
