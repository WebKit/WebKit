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
#include "IntlSegmentIteratorPrototype.h"

#include "IntlSegmentIterator.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL IntlSegmentIteratorPrototypeFuncNext(JSGlobalObject*, CallFrame*);

}

#include "IntlSegmentIteratorPrototype.lut.h"

namespace JSC {

const ClassInfo IntlSegmentIteratorPrototype::s_info = { "Segment String Iterator", &Base::s_info, &segmentIteratorPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlSegmentIteratorPrototype) };

/* Source for IntlSegmentIteratorPrototype.lut.h
@begin segmentIteratorPrototypeTable
  next             IntlSegmentIteratorPrototypeFuncNext               DontEnum|Function 0
@end
*/

IntlSegmentIteratorPrototype* IntlSegmentIteratorPrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlSegmentIteratorPrototype>(vm.heap)) IntlSegmentIteratorPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlSegmentIteratorPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlSegmentIteratorPrototype::IntlSegmentIteratorPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlSegmentIteratorPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-intl-segmenter/#sec-%segmentiteratorprototype%.next
EncodedJSValue JSC_HOST_CALL IntlSegmentIteratorPrototypeFuncNext(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* segmentIterator = jsDynamicCast<IntlSegmentIterator*>(vm, callFrame->thisValue());
    if (!segmentIterator)
        return throwVMTypeError(globalObject, scope, "Intl.SegmentIterator.prototype.next called on value that's not an object initialized as a SegmentIterator"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(segmentIterator->next(globalObject)));
}

} // namespace JSC
