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
#include "IntlSegmentsPrototype.h"

#include "IntlSegments.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL IntlSegmentsPrototypeFuncContaining(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlSegmentsPrototypeFuncIterator(JSGlobalObject*, CallFrame*);

}

#include "IntlSegmentsPrototype.lut.h"

namespace JSC {

const ClassInfo IntlSegmentsPrototype::s_info = { "%Segments%", &Base::s_info, &segmentsPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlSegmentsPrototype) };

/* Source for IntlSegmentsPrototype.lut.h
@begin segmentsPrototypeTable
  containing       IntlSegmentsPrototypeFuncContaining         DontEnum|Function 1
@end
*/

IntlSegmentsPrototype* IntlSegmentsPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlSegmentsPrototype>(vm.heap)) IntlSegmentsPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* IntlSegmentsPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlSegmentsPrototype::IntlSegmentsPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlSegmentsPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->iteratorSymbol, IntlSegmentsPrototypeFuncIterator, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
}

// https://tc39.es/proposal-intl-segmenter/#sec-%segmentsprototype%.containing
EncodedJSValue JSC_HOST_CALL IntlSegmentsPrototypeFuncContaining(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* segments = jsDynamicCast<IntlSegments*>(vm, callFrame->thisValue());
    if (!segments)
        return throwVMTypeError(globalObject, scope, "%Segments.prototype%.containing called on value that's not an object initialized as a Segments"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(segments->containing(globalObject, callFrame->argument(0))));
}

// https://tc39.es/proposal-intl-segmenter/#sec-%segmentsprototype%-@@iterator
EncodedJSValue JSC_HOST_CALL IntlSegmentsPrototypeFuncIterator(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* segments = jsDynamicCast<IntlSegments*>(vm, callFrame->thisValue());
    if (!segments)
        return throwVMTypeError(globalObject, scope, "%Segments.prototype%[@@iterator] called on value that's not an object initialized as a Segments"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(segments->createSegmentIterator(globalObject)));
}

} // namespace JSC
