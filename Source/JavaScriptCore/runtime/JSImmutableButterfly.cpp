/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSImmutableButterfly.h"

#include "ButterflyInlines.h"
#include "ClonedArguments.h"
#include "DirectArguments.h"
#include "ScopedArguments.h"
#include <wtf/IterationStatus.h>

namespace JSC {

const ClassInfo JSImmutableButterfly::s_info = { "Immutable Butterfly"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSImmutableButterfly) };

template<typename Visitor>
void JSImmutableButterfly::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    ASSERT_GC_OBJECT_INHERITS(cell, info());
    Base::visitChildren(cell, visitor);
    if (!hasContiguous(cell->indexingType())) {
        ASSERT(hasDouble(cell->indexingType()) || hasInt32(cell->indexingType()));
        return;
    }

    Butterfly* butterfly = jsCast<JSImmutableButterfly*>(cell)->toButterfly();
    visitor.appendValuesHidden(butterfly->contiguous().data(), butterfly->publicLength());
}

DEFINE_VISIT_CHILDREN(JSImmutableButterfly);

void JSImmutableButterfly::copyToArguments(JSGlobalObject*, JSValue* firstElementDest, unsigned offset, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if ((i + offset) < publicLength())
            firstElementDest[i] = get(i + offset);
        else
            firstElementDest[i] = jsUndefined();
    }
}

static_assert(JSImmutableButterfly::offsetOfData() == sizeof(JSImmutableButterfly), "m_header needs to be adjacent to Data");

JSImmutableButterfly* JSImmutableButterfly::createFromClonedArguments(JSGlobalObject* globalObject, ClonedArguments* arguments)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = arguments->length(globalObject); // This must be side-effect free, and it is ensured by ClonedArguments::isIteratorProtocolFastAndNonObservable.
    unsigned vectorLength = arguments->getVectorLength();
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSImmutableButterfly* result = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), length);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    if (!length)
        return result;

    IndexingType indexingType = arguments->indexingType() & IndexingShapeMask;
    if (indexingType == ContiguousShape) {
        // Since |length| is not tightly coupled with butterfly, it is possible that |length| is larger than vectorLength.
        for (unsigned i = 0; i < std::min(length, vectorLength); i++) {
            JSValue value = arguments->butterfly()->contiguous().at(arguments, i).get();
            value = !!value ? value : jsUndefined();
            result->setIndex(vm, i, value);
        }
        if (vectorLength < length) {
            for (unsigned i = vectorLength; i < length; i++)
                result->setIndex(vm, i, jsUndefined());
        }
        return result;
    }

    for (unsigned i = 0; i < length; i++) {
        JSValue value = arguments->getDirectIndex(globalObject, i);
        if (!value) {
            // When we see a hole, we assume that it's safe to assume the get would have returned undefined.
            // We may still call into this function when !globalObject->isArgumentsIteratorProtocolFastAndNonObservable(),
            // however, if we do that, we ensure we're calling in with an array with all self properties between
            // [0, length).
            value = jsUndefined();
        }
        RETURN_IF_EXCEPTION(scope, nullptr);
        result->setIndex(vm, i, value);
    }

    return result;
}

template<typename Arguments>
static ALWAYS_INLINE JSImmutableButterfly* createFromNonClonedArguments(JSGlobalObject* globalObject, Arguments* arguments)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = arguments->internalLength();

    JSImmutableButterfly* result = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), length);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    if (!length)
        return result;

    for (unsigned i = 0; i < length; ++i) {
        JSValue value = arguments->getIndexQuickly(i);
        if (!value) {
            // When we see a hole, we assume that it's safe to assume the get would have returned undefined.
            // We may still call into this function when !globalObject->isArgumentsIteratorProtocolFastAndNonObservable(),
            // however, if we do that, we ensure we're calling in with an array with all self properties between
            // [0, length).
            value = jsUndefined();
        }
        result->setIndex(vm, i, value);
    }

    return result;
}

JSImmutableButterfly* JSImmutableButterfly::createFromDirectArguments(JSGlobalObject* globalObject, DirectArguments* arguments)
{
    return createFromNonClonedArguments(globalObject, arguments);
}

JSImmutableButterfly* JSImmutableButterfly::createFromScopedArguments(JSGlobalObject* globalObject, ScopedArguments* arguments)
{
    return createFromNonClonedArguments(globalObject, arguments);
}

JSImmutableButterfly* JSImmutableButterfly::createFromString(JSGlobalObject* globalObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto holder = string->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned length = holder.view.length();
    if (holder.view.is8Bit()) {
        JSImmutableButterfly* result = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), length);
        if (UNLIKELY(!result)) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        const auto* characters = holder.view.characters8();
        for (unsigned i = 0; i < length; ++i) {
            auto* value = jsSingleCharacterString(vm, characters[i]);
            result->setIndex(vm, i, value);
        }
        return result;
    }

    auto forEachCodePointViaStringIteratorProtocol = [](const UChar* characters, unsigned length, auto func) {
        for (unsigned i = 0; i < length; ++i) {
            UChar character = characters[i];
            if (character < 0xD800 || character > 0xDBFF || (i + 1) == length) {
                if (func(i, 1) == IterationStatus::Done)
                    return;
                continue;
            }
            UChar second = characters[i + 1];
            if (second < 0xDC00 || second > 0xDFFF) {
                if (func(i, 1) == IterationStatus::Done)
                    return;
                continue;
            }

            // Construct surrogate.
            if (func(i, 2) == IterationStatus::Done)
                return;
            ++i;
        }
    };

    const auto* characters = holder.view.characters16();
    unsigned codePointLength = 0;
    forEachCodePointViaStringIteratorProtocol(characters, length, [&](unsigned, unsigned) {
        codePointLength += 1;
        return IterationStatus::Continue;
    });

    JSImmutableButterfly* result = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), codePointLength);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    unsigned resultIndex = 0;
    forEachCodePointViaStringIteratorProtocol(characters, length, [&](unsigned index, unsigned size) {
        JSString* value = nullptr;
        if (size == 1)
            value = jsSingleCharacterString(vm, characters[index]);
        else {
            ASSERT(size == 2);
            UChar string[2] = {
                characters[index],
                characters[index + 1],
            };
            value = jsNontrivialString(vm, String(string, 2));
        }

        result->setIndex(vm, resultIndex++, value);
        return IterationStatus::Continue;
    });

    return result;
}

} // namespace JSC
