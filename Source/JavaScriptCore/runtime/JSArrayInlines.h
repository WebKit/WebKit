/*
 *  Copyright (C) 2016 Apple Inc. All rights reserved.
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

inline bool JSArray::canFastCopy(VM& vm, JSArray* otherArray)
{
    if (otherArray == this)
        return false;
    if (hasAnyArrayStorage(indexingType()) || hasAnyArrayStorage(otherArray->indexingType()))
        return false;
    // FIXME: We should have a watchpoint for indexed properties on Array.prototype and Object.prototype
    // instead of walking the prototype chain. https://bugs.webkit.org/show_bug.cgi?id=155592
    if (structure(vm)->holesMustForwardToPrototype(vm, this)
        || otherArray->structure(vm)->holesMustForwardToPrototype(vm, otherArray))
        return false;
    return true;
}

inline bool JSArray::canDoFastIndexedAccess(VM& vm)
{
    JSGlobalObject* globalObject = this->globalObject();
    if (!globalObject->isArrayPrototypeIndexedAccessFastAndNonObservable())
        return false;

    Structure* structure = this->structure(vm);
    // This is the fast case. Many arrays will be an original array.
    if (globalObject->isOriginalArrayStructure(structure))
        return true;

    if (structure->mayInterceptIndexedAccesses())
        return false;

    if (getPrototypeDirect(vm) != globalObject->arrayPrototype())
        return false;

    return true;
}

ALWAYS_INLINE double toLength(ExecState* exec, JSObject* obj)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (LIKELY(isJSArray(obj)))
        return jsCast<JSArray*>(obj)->length();

    JSValue lengthValue = obj->get(exec, vm.propertyNames->length);
    RETURN_IF_EXCEPTION(scope, PNaN);
    RELEASE_AND_RETURN(scope, lengthValue.toLength(exec));
}

ALWAYS_INLINE void JSArray::pushInline(ExecState* exec, JSValue value)
{
    VM& vm = exec->vm();
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
        push(exec, value);
        return;
    }

    case ArrayWithInt32: {
        if (!value.isInt32()) {
            convertInt32ForValue(vm, value);
            scope.release();
            push(exec, value);
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
            methodTable(vm)->putByIndex(this, exec, length, value, true);
            if (!scope.exception())
                throwException(exec, scope, createRangeError(exec, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        scope.release();
        putByIndexBeyondVectorLengthWithoutAttributes<Int32Shape>(exec, length, value);
        return;
    }

    case ArrayWithContiguous: {
        unsigned length = butterfly->publicLength();
        ASSERT(length <= butterfly->vectorLength());
        if (length < butterfly->vectorLength()) {
            butterfly->contiguous().at(this, length).set(vm, this, value);
            butterfly->setPublicLength(length + 1);
            return;
        }

        if (UNLIKELY(length > MAX_ARRAY_INDEX)) {
            methodTable(vm)->putByIndex(this, exec, length, value, true);
            if (!scope.exception())
                throwException(exec, scope, createRangeError(exec, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        scope.release();
        putByIndexBeyondVectorLengthWithoutAttributes<ContiguousShape>(exec, length, value);
        return;
    }

    case ArrayWithDouble: {
        if (!value.isNumber()) {
            convertDoubleToContiguous(vm);
            scope.release();
            push(exec, value);
            return;
        }
        double valueAsDouble = value.asNumber();
        if (valueAsDouble != valueAsDouble) {
            convertDoubleToContiguous(vm);
            scope.release();
            push(exec, value);
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
            methodTable(vm)->putByIndex(this, exec, length, value, true);
            if (!scope.exception())
                throwException(exec, scope, createRangeError(exec, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        scope.release();
        putByIndexBeyondVectorLengthWithoutAttributes<DoubleShape>(exec, length, value);
        return;
    }

    case ArrayWithSlowPutArrayStorage: {
        unsigned oldLength = length();
        bool putResult = false;
        if (attemptToInterceptPutByIndexOnHole(exec, oldLength, value, true, putResult)) {
            if (!scope.exception() && oldLength < 0xFFFFFFFFu) {
                scope.release();
                setLength(exec, oldLength + 1, true);
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
            methodTable(vm)->putByIndex(this, exec, storage->length(), value, true);
            // Per ES5.1 15.4.4.7 step 6 & 15.4.5.1 step 3.d.
            if (!scope.exception())
                throwException(exec, scope, createRangeError(exec, LengthExceededTheMaximumArrayLengthError));
            return;
        }

        // Handled the same as putIndex.
        scope.release();
        putByIndexBeyondVectorLengthWithArrayStorage(exec, storage->length(), value, true, storage);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace JSC
