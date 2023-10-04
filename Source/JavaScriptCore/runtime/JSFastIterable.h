/*
 *  Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "JSArray.h"
#include "JSGlobalObject.h"

namespace JSC {

class JSFastIterable : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr bool needsDestruction = true;

    unsigned length();
    JSValue getIndex(JSGlobalObject*, unsigned index);

protected:
    JSFastIterable(VM& vm, Structure* structure, Butterfly* butterfly = nullptr)
        : Base(vm, structure, butterfly)
    {
    }

#if ASSERT_ENABLED
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(!this->structure()->hasInlineStorage());
        ASSERT(classInfo());
    }
#endif
};

template<typename ImplementationClass, typename PtrTraits = RawPtrTraits<ImplementationClass>>
class JSWrapper : public JSFastIterable {
public:
    using Base = JSFastIterable;
    using JSWrapped = ImplementationClass;

    ImplementationClass& wrapped() const { return m_wrapped; }
    static ptrdiff_t offsetOfWrapped() { return OBJECT_OFFSETOF(JSWrapped, m_wrapped); }
    constexpr static bool hasCustomPtrTraits() { return !std::is_same_v<PtrTraits, RawPtrTraits<ImplementationClass>>; };

protected:
    JSWrapper(Structure* structure, JSGlobalObject& globalObject, Ref<ImplementationClass>&& impl)
        : Base(globalObject.vm(), structure)
        , m_wrapped(WTFMove(impl)) { }

private:
    Ref<ImplementationClass, PtrTraits> m_wrapped;
};

inline JSFastIterable* asFastIterable(JSCell* cell)
{
    ASSERT(cell->inherits<JSFastIterable>());
    return jsCast<JSFastIterable*>(cell);
}

inline JSFastIterable* asFastIterable(JSValue value)
{
    return asFastIterable(value.asCell());
}

inline bool isFastIterable(JSCell* cell)
{
    return cell->type() == FastIterableType;
}

inline bool canIterateFast(JSCell* cell)
{
    return isFastIterable(cell) || isJSArray(cell);
}

inline bool canIterateFast(JSValue v)
{
    return v.isCell() && canIterateFast(v.asCell());
}

} // namespace JSC
