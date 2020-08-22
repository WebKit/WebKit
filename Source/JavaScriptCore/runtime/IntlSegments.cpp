/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IntlSegments.h"

#include "IntlObjectInlines.h"
#include "IntlSegmentIterator.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <unicode/ucurr.h>
#include <unicode/uloc.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlSegments::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlSegments) };

IntlSegments* IntlSegments::create(VM& vm, Structure* structure, std::unique_ptr<UBreakIterator, UBreakIteratorDeleter>&& segmenter, Box<Vector<UChar>>&& buffer, JSString* string, IntlSegmenter::Granularity granularity)
{
    auto* object = new (NotNull, allocateCell<IntlSegments>(vm.heap)) IntlSegments(vm, structure, WTFMove(segmenter), WTFMove(buffer), granularity);
    object->finishCreation(vm, string);
    return object;
}

Structure* IntlSegments::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlSegments::IntlSegments(VM& vm, Structure* structure, std::unique_ptr<UBreakIterator, UBreakIteratorDeleter>&& segmenter, Box<Vector<UChar>>&& buffer, IntlSegmenter::Granularity granularity)
    : Base(vm, structure)
    , m_segmenter(WTFMove(segmenter))
    , m_buffer(WTFMove(buffer))
    , m_granularity(granularity)
{
}

void IntlSegments::finishCreation(VM& vm, JSString* string)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    m_string.set(vm, this, string);
}

// https://tc39.es/proposal-intl-segmenter/#sec-intl.segmenter.prototype.containing
JSValue IntlSegments::containing(JSGlobalObject* globalObject, JSValue indexValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double value = indexValue.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (value < 0 || value >= m_buffer->size())
        return jsUndefined();
    int32_t index = toInt32(value);

    // The result of ubrk_preceding is always *smaller* than offset, or UBRK_DONE. In this case, we should set scan position with `index + 1`.
    // Even if index + 1 exceeds length of string by 1, this is desirable if we want to scan the last segment.
    int32_t startIndex = ubrk_preceding(m_segmenter.get(), index + 1);
    if (startIndex == UBRK_DONE)
        startIndex = 0;
    // The result of ubrk_following is always greater than offset, or UBRK_DONE. Scan position should be `index`.
    int32_t endIndex = ubrk_following(m_segmenter.get(), index);
    if (endIndex == UBRK_DONE)
        endIndex = m_buffer->size();

    scope.release();
    return IntlSegmenter::createSegmentDataObject(globalObject, m_string.get(), startIndex, endIndex, *m_segmenter, m_granularity);
}

// https://tc39.es/proposal-intl-segmenter/#sec-%segmentsprototype%-@@iterator
JSObject* IntlSegments::createSegmentIterator(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    auto segmenter = std::unique_ptr<UBreakIterator, UBreakIteratorDeleter>(ubrk_safeClone(m_segmenter.get(), nullptr, nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize SegmentIterator"_s);
        return nullptr;
    }

    ubrk_first(segmenter.get());
    return IntlSegmentIterator::create(vm, globalObject->segmentIteratorStructure(), WTFMove(segmenter), m_buffer, m_string.get(), m_granularity);
}

void IntlSegments::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<IntlSegments*>(cell);
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_string);
}

} // namespace JSC
