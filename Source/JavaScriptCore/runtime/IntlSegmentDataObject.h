/*
 * Copyright (C) 2026 Anthropic PBC.
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

#include "IntlSegmenter.h"
#include "JSGlobalObject.h"
#include "ObjectConstructor.h"

namespace JSC {

static constexpr PropertyOffset segmentDataObjectSegmentPropertyOffset = 0;
static constexpr PropertyOffset segmentDataObjectIndexPropertyOffset = 1;
static constexpr PropertyOffset segmentDataObjectInputPropertyOffset = 2;
static constexpr PropertyOffset segmentDataObjectIsWordLikePropertyOffset = 3;

Structure* createSegmentDataObjectStructure(VM&, JSGlobalObject&);
Structure* createSegmentDataObjectWithIsWordLikeStructure(VM&, JSGlobalObject&);

ALWAYS_INLINE JSObject* createSegmentDataObject(JSGlobalObject* globalObject, JSString* string, int32_t startIndex, int32_t endIndex, UBreakIterator& segmenter, IntlSegmenter::Granularity granularity)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* segment = jsSubstring(globalObject, string, startIndex, endIndex - startIndex);
    RETURN_IF_EXCEPTION(scope, nullptr);

    Structure* structure = (granularity == IntlSegmenter::Granularity::Word)
        ? globalObject->segmentDataObjectWithIsWordLikeStructure()
        : globalObject->segmentDataObjectStructure();

    JSObject* result = constructEmptyObject(vm, structure);
    result->putDirectOffset(vm, segmentDataObjectSegmentPropertyOffset, segment);
    result->putDirectOffset(vm, segmentDataObjectIndexPropertyOffset, jsNumber(startIndex));
    result->putDirectOffset(vm, segmentDataObjectInputPropertyOffset, string);

    if (granularity == IntlSegmenter::Granularity::Word) {
        int32_t ruleStatus = ubrk_getRuleStatus(&segmenter);
        result->putDirectOffset(vm, segmentDataObjectIsWordLikePropertyOffset,
            jsBoolean(!(ruleStatus >= UBRK_WORD_NONE && ruleStatus <= UBRK_WORD_NONE_LIMIT)));
    }

    return result;
}

} // namespace JSC
