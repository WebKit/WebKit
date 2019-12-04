/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All Rights Reserved.
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

#include "JSObject.h"
#include "RegExp.h"
#include "ThrowScope.h"
#include "TypeError.h"

namespace JSC {
    
class RegExpObject final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesGetPropertyNames;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        static_assert(!CellType::needsDestruction, "");
        return &vm.regExpObjectSpace;
    }

    static constexpr uintptr_t lastIndexIsNotWritableFlag = 1;

    static RegExpObject* create(VM& vm, Structure* structure, RegExp* regExp)
    {
        RegExpObject* object = new (NotNull, allocateCell<RegExpObject>(vm.heap)) RegExpObject(vm, structure, regExp);
        object->finishCreation(vm);
        return object;
    }

    static RegExpObject* create(VM& vm, Structure* structure, RegExp* regExp, JSValue lastIndex)
    {
        auto* object = create(vm, structure, regExp);
        object->m_lastIndex.set(vm, object, lastIndex);
        return object;
    }

    void setRegExp(VM& vm, RegExp* regExp)
    {
        uintptr_t result = (m_regExpAndLastIndexIsNotWritableFlag & lastIndexIsNotWritableFlag) | bitwise_cast<uintptr_t>(regExp);
        m_regExpAndLastIndexIsNotWritableFlag = result;
        vm.heap.writeBarrier(this, regExp);
    }

    RegExp* regExp() const
    {
        return bitwise_cast<RegExp*>(m_regExpAndLastIndexIsNotWritableFlag & (~lastIndexIsNotWritableFlag));
    }

    bool setLastIndex(JSGlobalObject* globalObject, size_t lastIndex)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (LIKELY(lastIndexIsWritable())) {
            m_lastIndex.setWithoutWriteBarrier(jsNumber(lastIndex));
            return true;
        }
        throwTypeError(globalObject, scope, ReadonlyPropertyWriteError);
        return false;
    }
    bool setLastIndex(JSGlobalObject* globalObject, JSValue lastIndex, bool shouldThrow)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (LIKELY(lastIndexIsWritable())) {
            m_lastIndex.set(vm, this, lastIndex);
            return true;
        }
        return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);
    }
    JSValue getLastIndex() const
    {
        return m_lastIndex.get();
    }

    bool test(JSGlobalObject* globalObject, JSString* string) { return !!match(globalObject, string); }
    bool testInline(JSGlobalObject* globalObject, JSString* string) { return !!matchInline(globalObject, string); }
    JSValue exec(JSGlobalObject*, JSString*);
    JSValue execInline(JSGlobalObject*, JSString*);
    MatchResult match(JSGlobalObject*, JSString*);
    JSValue matchGlobal(JSGlobalObject*, JSString*);

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(RegExpObjectType, StructureFlags), info());
    }

    static ptrdiff_t offsetOfRegExpAndLastIndexIsNotWritableFlag()
    {
        return OBJECT_OFFSETOF(RegExpObject, m_regExpAndLastIndexIsNotWritableFlag);
    }

    static ptrdiff_t offsetOfLastIndex()
    {
        return OBJECT_OFFSETOF(RegExpObject, m_lastIndex);
    }

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, !inlineCapacity);
        return sizeof(RegExpObject);
    }

protected:
    JS_EXPORT_PRIVATE RegExpObject(VM&, Structure*, RegExp*);
    JS_EXPORT_PRIVATE void finishCreation(VM&);

    static void visitChildren(JSCell*, SlotVisitor&);

    bool lastIndexIsWritable() const
    {
        return !(m_regExpAndLastIndexIsNotWritableFlag & lastIndexIsNotWritableFlag);
    }

    void setLastIndexIsNotWritable()
    {
        m_regExpAndLastIndexIsNotWritableFlag = (m_regExpAndLastIndexIsNotWritableFlag | lastIndexIsNotWritableFlag);
    }

    JS_EXPORT_PRIVATE static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName);
    JS_EXPORT_PRIVATE static void getOwnNonIndexPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static void getGenericPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);
    JS_EXPORT_PRIVATE static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

private:
    MatchResult matchInline(JSGlobalObject*, JSString*);

    uintptr_t m_regExpAndLastIndexIsNotWritableFlag { 0 };
    WriteBarrier<Unknown> m_lastIndex;
};

} // namespace JSC
