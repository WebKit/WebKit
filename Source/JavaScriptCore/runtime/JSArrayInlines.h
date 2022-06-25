/*
 *  Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "ArrayPrototype.h"
#include "ButterflyInlines.h"
#include "Error.h"
#include "JSArray.h"
#include "JSCellInlines.h"
#include "Structure.h"

namespace JSC {

inline IndexingType JSArray::mergeIndexingTypeForCopying(IndexingType other)
{
    IndexingType type = indexingType();
    if (!(type & IsArray && other & IsArray))
        return NonArray;

    if (hasAnyArrayStorage(type) || hasAnyArrayStorage(other))
        return NonArray;

    if (type == ArrayWithUndecided)
        return other;

    if (other == ArrayWithUndecided)
        return type;

    // We can memcpy an Int32 and a Contiguous into a Contiguous array since
    // both share the same memory layout for Int32 numbers.
    if ((type == ArrayWithInt32 || type == ArrayWithContiguous)
        && (other == ArrayWithInt32 || other == ArrayWithContiguous)) {
        if (other == ArrayWithContiguous)
            return other;
        return type;
    }

    if (type != other)
        return NonArray;

    return type;
}

inline bool JSArray::canFastCopy(JSArray* otherArray)
{
    if (otherArray == this)
        return false;
    if (hasAnyArrayStorage(indexingType()) || hasAnyArrayStorage(otherArray->indexingType()))
        return false;
    // FIXME: We should have a watchpoint for indexed properties on Array.prototype and Object.prototype
    // instead of walking the prototype chain. https://bugs.webkit.org/show_bug.cgi?id=155592
    if (structure()->holesMustForwardToPrototype(this)
        || otherArray->structure()->holesMustForwardToPrototype(otherArray))
        return false;
    return true;
}

inline bool JSArray::canDoFastIndexedAccess()
{
    JSGlobalObject* globalObject = this->globalObject();
    if (!globalObject->arrayPrototypeChainIsSane())
        return false;

    Structure* structure = this->structure();
    // This is the fast case. Many arrays will be an original array.
    if (globalObject->isOriginalArrayStructure(structure))
        return true;

    if (structure->mayInterceptIndexedAccesses())
        return false;

    if (getPrototypeDirect() != globalObject->arrayPrototype())
        return false;

    return true;
}

ALWAYS_INLINE double toLength(JSGlobalObject* globalObject, JSObject* obj)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (LIKELY(isJSArray(obj)))
        return jsCast<JSArray*>(obj)->length();

    JSValue lengthValue = obj->get(globalObject, vm.propertyNames->length);
    RETURN_IF_EXCEPTION(scope, PNaN);
    RELEASE_AND_RETURN(scope, lengthValue.toLength(globalObject));
}

ALWAYS_INLINE void JSArray::pushInline(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ensureWritable(vm);

    Butterfly* butterfly = this->butterfly();

    switch (indexingMode()) {
    case ArrayClass: {
        createInitialUndecided(vm, 0);
        FALLTHROUGH;
    }

    case ArrayWithUndecided: {
        convertUndecidedForValue(vm, value);
        scope.release();
        push(globalObject, value);
        return;
    }

    case ArrayWithInt32: {
        if (!value.isInt32()) {
            convertInt32ForValue(vm, value);
            scope.release();
            push(globalObject, value);
            return;
        }

        unsigned length = butterfly->publicLength();
        ASSERT(length <= butterfly->vectorLength());
        if (length < butterfly->vectorLength()) {
            butterfly->contiguousInt32().at(this, length).setWithoutWriteBarrier(value);
            butterfly->setPublicLength(length + 1);
            return;
        }

        if (UNLIKELY(length > MAX_ARRAY_INDEX)) {
            methodTable()->putByIndex(this, globalObject, length, value, true);
            if (!scope.exception())
                throwException(globalObject, scope, createRangeError(globalObject, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        scope.release();
        putByIndexBeyondVectorLengthWithoutAttributes<Int32Shape>(globalObject, length, value);
        return;
    }

    case ArrayWithContiguous: {
        unsigned length = butterfly->publicLength();
        ASSERT(length <= butterfly->vectorLength());
        if (length < butterfly->vectorLength()) {
            butterfly->contiguous().at(this, length).setWithoutWriteBarrier(value);
            butterfly->setPublicLength(length + 1);
            vm.writeBarrier(this, value);
            return;
        }

        if (UNLIKELY(length > MAX_ARRAY_INDEX)) {
            methodTable()->putByIndex(this, globalObject, length, value, true);
            if (!scope.exception())
                throwException(globalObject, scope, createRangeError(globalObject, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        scope.release();
        putByIndexBeyondVectorLengthWithoutAttributes<ContiguousShape>(globalObject, length, value);
        return;
    }

    case ArrayWithDouble: {
        if (!value.isNumber()) {
            convertDoubleToContiguous(vm);
            scope.release();
            push(globalObject, value);
            return;
        }
        double valueAsDouble = value.asNumber();
        if (valueAsDouble != valueAsDouble) {
            convertDoubleToContiguous(vm);
            scope.release();
            push(globalObject, value);
            return;
        }

        unsigned length = butterfly->publicLength();
        ASSERT(length <= butterfly->vectorLength());
        if (length < butterfly->vectorLength()) {
            butterfly->contiguousDouble().at(this, length) = valueAsDouble;
            butterfly->setPublicLength(length + 1);
            return;
        }

        if (UNLIKELY(length > MAX_ARRAY_INDEX)) {
            methodTable()->putByIndex(this, globalObject, length, value, true);
            if (!scope.exception())
                throwException(globalObject, scope, createRangeError(globalObject, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        scope.release();
        putByIndexBeyondVectorLengthWithoutAttributes<DoubleShape>(globalObject, length, value);
        return;
    }

    case ArrayWithSlowPutArrayStorage: {
        unsigned oldLength = length();
        bool putResult = false;
        bool result = attemptToInterceptPutByIndexOnHole(globalObject, oldLength, value, true, putResult);
        RETURN_IF_EXCEPTION(scope, void());
        if (result) {
            if (oldLength < 0xFFFFFFFFu) {
                scope.release();
                setLength(globalObject, oldLength + 1, true);
            }
            return;
        }
        FALLTHROUGH;
    }

    case ArrayWithArrayStorage: {
        ArrayStorage* storage = butterfly->arrayStorage();

        // Fast case - push within vector, always update m_length & m_numValuesInVector.
        unsigned length = storage->length();
        if (length < storage->vectorLength()) {
            storage->m_vector[length].set(vm, this, value);
            storage->setLength(length + 1);
            ++storage->m_numValuesInVector;
            return;
        }

        // Pushing to an array of invalid length (2^31-1) stores the property, but throws a range error.
        if (UNLIKELY(storage->length() > MAX_ARRAY_INDEX)) {
            methodTable()->putByIndex(this, globalObject, storage->length(), value, true);
            // Per ES5.1 15.4.4.7 step 6 & 15.4.5.1 step 3.d.
            if (!scope.exception())
                throwException(globalObject, scope, createRangeError(globalObject, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        // Handled the same as putIndex.
        scope.release();
        putByIndexBeyondVectorLengthWithArrayStorage(globalObject, storage->length(), value, true, storage);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace JSC
