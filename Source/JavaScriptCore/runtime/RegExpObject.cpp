/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2018 Apple Inc. All Rights Reserved.
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

#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "Lookup.h"
#include "JSCInlines.h"
#include "RegExpConstructor.h"
#include "RegExpObjectInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(RegExpObject);

const ClassInfo RegExpObject::s_info = { "RegExp", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RegExpObject) };

RegExpObject::RegExpObject(VM& vm, Structure* structure, RegExp* regExp)
    : JSNonFinalObject(vm, structure)
    , m_regExp(vm, this, regExp)
    , m_lastIndexIsWritable(true)
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
    visitor.append(thisObject->m_regExp);
    visitor.append(thisObject->m_lastIndex);
}

bool RegExpObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    if (propertyName == vm.propertyNames->lastIndex) {
        RegExpObject* regExp = jsCast<RegExpObject*>(object);
        unsigned attributes = regExp->m_lastIndexIsWritable ? PropertyAttribute::DontDelete | PropertyAttribute::DontEnum : PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly;
        slot.setValue(regExp, attributes, regExp->getLastIndex());
        return true;
    }
    return Base::getOwnPropertySlot(object, exec, propertyName, slot);
}

bool RegExpObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    VM& vm = exec->vm();
    if (propertyName == vm.propertyNames->lastIndex)
        return false;
    return Base::deleteProperty(cell, exec, propertyName);
}

void RegExpObject::getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = exec->vm();
    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->lastIndex);
    Base::getOwnNonIndexPropertyNames(object, exec, propertyNames, mode);
}

void RegExpObject::getPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = exec->vm();
    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->lastIndex);
    Base::getPropertyNames(object, exec, propertyNames, mode);
}

void RegExpObject::getGenericPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = exec->vm();
    if (mode.includeDontEnumProperties())
        propertyNames.add(vm.propertyNames->lastIndex);
    Base::getGenericPropertyNames(object, exec, propertyNames, mode);
}

bool RegExpObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName == vm.propertyNames->lastIndex) {
        RegExpObject* regExp = jsCast<RegExpObject*>(object);
        if (descriptor.configurablePresent() && descriptor.configurable())
            return typeError(exec, scope, shouldThrow, UnconfigurablePropertyChangeConfigurabilityError);
        if (descriptor.enumerablePresent() && descriptor.enumerable())
            return typeError(exec, scope, shouldThrow, UnconfigurablePropertyChangeEnumerabilityError);
        if (descriptor.isAccessorDescriptor())
            return typeError(exec, scope, shouldThrow, UnconfigurablePropertyChangeAccessMechanismError);
        if (!regExp->m_lastIndexIsWritable) {
            if (descriptor.writablePresent() && descriptor.writable())
                return typeError(exec, scope, shouldThrow, UnconfigurablePropertyChangeWritabilityError);
            if (descriptor.value() && !sameValue(exec, regExp->getLastIndex(), descriptor.value()))
                return typeError(exec, scope, shouldThrow, ReadonlyPropertyChangeError);
            return true;
        }
        if (descriptor.value()) {
            regExp->setLastIndex(exec, descriptor.value(), false);
            RETURN_IF_EXCEPTION(scope, false);
        }
        if (descriptor.writablePresent() && !descriptor.writable())
            regExp->m_lastIndexIsWritable = false;
        return true;
    }

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, exec, propertyName, descriptor, shouldThrow));
}

static bool regExpObjectSetLastIndexStrict(ExecState* exec, EncodedJSValue thisValue, EncodedJSValue value)
{
    return jsCast<RegExpObject*>(JSValue::decode(thisValue))->setLastIndex(exec, JSValue::decode(value), true);
}

static bool regExpObjectSetLastIndexNonStrict(ExecState* exec, EncodedJSValue thisValue, EncodedJSValue value)
{
    return jsCast<RegExpObject*>(JSValue::decode(thisValue))->setLastIndex(exec, JSValue::decode(value), false);
}

bool RegExpObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    RegExpObject* thisObject = jsCast<RegExpObject*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return ordinarySetSlow(exec, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());

    if (propertyName == vm.propertyNames->lastIndex) {
        bool result = thisObject->setLastIndex(exec, value, slot.isStrictMode());
        slot.setCustomValue(thisObject, slot.isStrictMode()
            ? regExpObjectSetLastIndexStrict
            : regExpObjectSetLastIndexNonStrict);
        return result;
    }
    return Base::put(cell, exec, propertyName, value, slot);
}

JSValue RegExpObject::exec(ExecState* exec, JSGlobalObject* globalObject, JSString* string)
{
    return execInline(exec, globalObject, string);
}

// Shared implementation used by test and exec.
MatchResult RegExpObject::match(ExecState* exec, JSGlobalObject* globalObject, JSString* string)
{
    return matchInline(exec, globalObject, string);
}

JSValue RegExpObject::matchGlobal(ExecState* exec, JSGlobalObject* globalObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    RegExp* regExp = this->regExp();

    ASSERT(regExp->global());

    setLastIndex(exec, 0);
    RETURN_IF_EXCEPTION(scope, { });

    String s = string->value(exec);
    RETURN_IF_EXCEPTION(scope, { });
    RegExpConstructor* regExpConstructor = globalObject->regExpConstructor();

    ASSERT(!s.isNull());
    if (regExp->unicode()) {
        unsigned stringLength = s.length();
        RELEASE_AND_RETURN(scope, collectMatches(
            vm, exec, string, s, regExpConstructor, regExp,
            [&] (size_t end) -> size_t {
                return advanceStringUnicode(s, stringLength, end);
            }));
    }

    RELEASE_AND_RETURN(scope, collectMatches(
        vm, exec, string, s, regExpConstructor, regExp,
        [&] (size_t end) -> size_t {
            return end + 1;
        }));
}

} // namespace JSC
