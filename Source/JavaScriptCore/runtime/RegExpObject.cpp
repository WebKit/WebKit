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

#include "config.h"
#include "RegExpObject.h"

#include "RegExpObjectInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(RegExpObject);

const ClassInfo RegExpObject::s_info = { "RegExp", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RegExpObject) };

JSC_DECLARE_CUSTOM_SETTER(regExpObjectSetLastIndexStrict);
JSC_DECLARE_CUSTOM_SETTER(regExpObjectSetLastIndexNonStrict);

RegExpObject::RegExpObject(VM& vm, Structure* structure, RegExp* regExp)
    : JSNonFinalObject(vm, structure)
    , m_regExpAndLastIndexIsNotWritableFlag(bitwise_cast<uintptr_t>(regExp)) // lastIndexIsNotWritableFlag is not set.
{
    m_lastIndex.setWithoutWriteBarrier(jsNumber(0));
}

void RegExpObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    ASSERT(type() == RegExpObjectType);
}

void RegExpObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    RegExpObject* thisObject = jsCast<RegExpObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.appendUnbarriered(thisObject->regExp());
    visitor.append(thisObject->m_lastIndex);
}

bool RegExpObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    if (propertyName == vm.propertyNames->lastIndex) {
        RegExpObject* regExp = jsCast<RegExpObject*>(object);
        unsigned attributes = regExp->lastIndexIsWritable() ? PropertyAttribute::DontDelete | PropertyAttribute::DontEnum : PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly;
        slot.setValue(regExp, attributes, regExp->getLastIndex());
        return true;
    }
    return Base::getOwnPropertySlot(object, globalObject, propertyName, slot);
}

bool RegExpObject::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    if (propertyName == vm.propertyNames->lastIndex)
        return false;
    return Base::deleteProperty(cell, globalObject, propertyName, slot);
}

void RegExpObject::getOwnNonIndexPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->lastIndex);
    Base::getOwnNonIndexPropertyNames(object, globalObject, propertyNames, mode);
}

void RegExpObject::getPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->lastIndex);
    Base::getPropertyNames(object, globalObject, propertyNames, mode);
}

void RegExpObject::getGenericPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->lastIndex);
    Base::getGenericPropertyNames(object, globalObject, propertyNames, mode);
}

bool RegExpObject::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName == vm.propertyNames->lastIndex) {
        RegExpObject* regExp = jsCast<RegExpObject*>(object);
        if (descriptor.configurablePresent() && descriptor.configurable())
            return typeError(globalObject, scope, shouldThrow, UnconfigurablePropertyChangeConfigurabilityError);
        if (descriptor.enumerablePresent() && descriptor.enumerable())
            return typeError(globalObject, scope, shouldThrow, UnconfigurablePropertyChangeEnumerabilityError);
        if (descriptor.isAccessorDescriptor())
            return typeError(globalObject, scope, shouldThrow, UnconfigurablePropertyChangeAccessMechanismError);
        if (!regExp->lastIndexIsWritable()) {
            if (descriptor.writablePresent() && descriptor.writable())
                return typeError(globalObject, scope, shouldThrow, UnconfigurablePropertyChangeWritabilityError);
            if (descriptor.value()) {
                bool isSame = sameValue(globalObject, regExp->getLastIndex(), descriptor.value());
                RETURN_IF_EXCEPTION(scope, false);
                if (!isSame)
                    return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyChangeError);
            }
            return true;
        }
        if (descriptor.value()) {
            regExp->setLastIndex(globalObject, descriptor.value(), false);
            RETURN_IF_EXCEPTION(scope, false);
        }
        if (descriptor.writablePresent() && !descriptor.writable())
            regExp->setLastIndexIsNotWritable();
        return true;
    }

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, propertyName, descriptor, shouldThrow));
}

JSC_DEFINE_CUSTOM_SETTER(regExpObjectSetLastIndexStrict, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value))
{
    return jsCast<RegExpObject*>(JSValue::decode(thisValue))->setLastIndex(globalObject, JSValue::decode(value), true);
}

JSC_DEFINE_CUSTOM_SETTER(regExpObjectSetLastIndexNonStrict, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value))
{
    return jsCast<RegExpObject*>(JSValue::decode(thisValue))->setLastIndex(globalObject, JSValue::decode(value), false);
}

bool RegExpObject::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    RegExpObject* thisObject = jsCast<RegExpObject*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());

    if (propertyName == vm.propertyNames->lastIndex) {
        bool result = thisObject->setLastIndex(globalObject, value, slot.isStrictMode());
        slot.setCustomValue(thisObject, slot.isStrictMode()
            ? regExpObjectSetLastIndexStrict
            : regExpObjectSetLastIndexNonStrict);
        return result;
    }
    return Base::put(cell, globalObject, propertyName, value, slot);
}

String RegExpObject::toStringName(const JSObject*, JSGlobalObject*)
{
    return "RegExp"_s;
}

JSValue RegExpObject::exec(JSGlobalObject* globalObject, JSString* string)
{
    return execInline(globalObject, string);
}

// Shared implementation used by test and exec.
MatchResult RegExpObject::match(JSGlobalObject* globalObject, JSString* string)
{
    return matchInline(globalObject, string);
}

JSValue RegExpObject::matchGlobal(JSGlobalObject* globalObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    RegExp* regExp = this->regExp();

    ASSERT(regExp->global());

    setLastIndex(globalObject, 0);
    RETURN_IF_EXCEPTION(scope, { });

    String s = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(!s.isNull());
    if (regExp->unicode()) {
        unsigned stringLength = s.length();
        RELEASE_AND_RETURN(scope, collectMatches(
            vm, globalObject, string, s, regExp,
            [&] (size_t end) -> size_t {
                return advanceStringUnicode(s, stringLength, end);
            }));
    }

    RELEASE_AND_RETURN(scope, collectMatches(
        vm, globalObject, string, s, regExp,
        [&] (size_t end) -> size_t {
            return end + 1;
        }));
}

} // namespace JSC
