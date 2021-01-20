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
#include "IntlSegmentIterator.h"

#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <unicode/ucurr.h>
#include <unicode/uloc.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlSegmentIterator::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlSegmentIterator) };

IntlSegmentIterator* IntlSegmentIterator::create(VM& vm, Structure* structure, std::unique_ptr<UBreakIterator, UBreakIteratorDeleter>&& segmenter, Box<Vector<UChar>> buffer, JSString* string, IntlSegmenter::Granularity granularity)
{
    auto* object = new (NotNull, allocateCell<IntlSegmentIterator>(vm.heap)) IntlSegmentIterator(vm, structure, WTFMove(segmenter), WTFMove(buffer), granularity);
    object->finishCreation(vm, string);
    return object;
}

Structure* IntlSegmentIterator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlSegmentIterator::IntlSegmentIterator(VM& vm, Structure* structure, std::unique_ptr<UBreakIterator, UBreakIteratorDeleter>&& segmenter, Box<Vector<UChar>>&& buffer, IntlSegmenter::Granularity granularity)
    : Base(vm, structure)
    , m_segmenter(WTFMove(segmenter))
    , m_buffer(WTFMove(buffer))
    , m_granularity(granularity)
{
}

void IntlSegmentIterator::finishCreation(VM& vm, JSString* string)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    m_string.set(vm, this, string);
}

void IntlSegmentIterator::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<IntlSegmentIterator*>(cell);
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_string);
}

JSObject* IntlSegmentIterator::next(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t startIndex = ubrk_current(m_segmenter.get());
    int32_t endIndex = ubrk_next(m_segmenter.get());
    if (endIndex == UBRK_DONE)
        return createIteratorResultObject(globalObject, jsUndefined(), true);
    JSObject* object = IntlSegmenter::createSegmentDataObject(globalObject, m_string.get(), startIndex, endIndex, *m_segmenter, m_granularity);
    RETURN_IF_EXCEPTION(scope, { });
    return createIteratorResultObject(globalObject, object, false);
}

} // namespace JSC
