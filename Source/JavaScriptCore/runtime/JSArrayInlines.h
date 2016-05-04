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

#ifndef JSArrayInlines_h
#define JSArrayInlines_h

#include "JSArray.h"
#include "JSCellInlines.h"
#include "Structure.h"

namespace JSC {

IndexingType JSArray::memCopyWithIndexingType(IndexingType other)
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

bool JSArray::canFastCopy(VM& vm, JSArray* otherArray)
{
    if (hasAnyArrayStorage(this->indexingType()) || hasAnyArrayStorage(otherArray->indexingType()))
        return false;
    // FIXME: We should have a watchpoint for indexed properties on Array.prototype and Object.prototype
    // instead of walking the prototype chain. https://bugs.webkit.org/show_bug.cgi?id=155592
    if (otherArray->structure(vm)->holesMustForwardToPrototype(vm)
        || otherArray->structure(vm)->holesMustForwardToPrototype(vm))
        return false;
    return true;
}

} // namespace JSC

#endif /* JSArrayInlines_h */
