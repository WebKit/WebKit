/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2012, 2016 Apple Inc. All Rights Reserved.
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

#include "ButterflyInlines.h"
#include "CopiedSpaceInlines.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "Lookup.h"
#include "JSCInlines.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObjectInlines.h"
#include "RegExpPrototype.h"
#include <wtf/text/StringBuilder.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(RegExpObject);

const ClassInfo RegExpObject::s_info = { "RegExp", &Base::s_info, nullptr, CREATE_METHOD_TABLE(RegExpObject) };

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
    ASSERT(inherits(info()));
}

void RegExpObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    RegExpObject* thisObject = jsCast<RegExpObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_regExp);
    visitor.append(&thisObject->m_lastIndex);
}

bool RegExpObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().lastIndex) {
        RegExpObject* regExp = asRegExpObject(object);
        unsigned attributes = regExp->m_lastIndexIsWritable ? DontDelete | DontEnum : DontDelete | DontEnum | ReadOnly;
        slot.setValue(regExp, attributes, regExp->getLastIndex());
        return true;
    }
    return Base::getOwnPropertySlot(object, exec, propertyName, slot);
}

bool RegExpObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    if (propertyName == exec->propertyNames().lastIndex)
        return false;
    return Base::deleteProperty(cell, exec, propertyName);
}

void RegExpObject::getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (mode.includeDontEnumProperties())
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getOwnNonIndexPropertyNames(object, exec, propertyNames, mode);
}

void RegExpObject::getPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (mode.includeDontEnumProperties())
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getPropertyNames(object, exec, propertyNames, mode);
}

void RegExpObject::getGenericPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (mode.includeDontEnumProperties())
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getGenericPropertyNames(object, exec, propertyNames, mode);
}

bool RegExpObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    if (propertyName == exec->propertyNames().lastIndex) {
        RegExpObject* regExp = asRegExpObject(object);
        if (descriptor.configurablePresent() && descriptor.configurable())
            return reject(exec, shouldThrow, "Attempting to change configurable attribute of unconfigurable property.");
        if (descriptor.enumerablePresent() && descriptor.enumerable())
            return reject(exec, shouldThrow, "Attempting to change enumerable attribute of unconfigurable property.");
        if (descriptor.isAccessorDescriptor())
            return reject(exec, shouldThrow, UnconfigurablePropertyChangeAccessMechanismError);
        if (!regExp->m_lastIndexIsWritable) {
            if (descriptor.writablePresent() && descriptor.writable())
                return reject(exec, shouldThrow, "Attempting to change writable attribute of unconfigurable property.");
            if (!sameValue(exec, regExp->getLastIndex(), descriptor.value()))
                return reject(exec, shouldThrow, "Attempting to change value of a readonly property.");
            return true;
        }
        if (descriptor.value())
            regExp->setLastIndex(exec, descriptor.value(), false);
        if (descriptor.writablePresent() && !descriptor.writable())
            regExp->m_lastIndexIsWritable = false;
        return true;
    }

    return Base::defineOwnProperty(object, exec, propertyName, descriptor, shouldThrow);
}

static bool regExpObjectSetLastIndexStrict(ExecState* exec, EncodedJSValue thisValue, EncodedJSValue value)
{
    return asRegExpObject(JSValue::decode(thisValue))->setLastIndex(exec, JSValue::decode(value), true);
}

static bool regExpObjectSetLastIndexNonStrict(ExecState* exec, EncodedJSValue thisValue, EncodedJSValue value)
{
    return asRegExpObject(JSValue::decode(thisValue))->setLastIndex(exec, JSValue::decode(value), false);
}

bool RegExpObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    RegExpObject* thisObject = jsCast<RegExpObject*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return ordinarySetSlow(exec, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());

    if (propertyName == exec->propertyNames().lastIndex) {
        bool result = asRegExpObject(cell)->setLastIndex(exec, value, slot.isStrictMode());
        slot.setCustomValue(asRegExpObject(cell), slot.isStrictMode()
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
    RegExp* regExp = this->regExp();

    ASSERT(regExp->global());

    VM* vm = &globalObject->vm();

    setLastIndex(exec, 0);
    if (exec->hadException())
        return jsUndefined();

    String s = string->value(exec);
    RegExpConstructor* regExpConstructor = globalObject->regExpConstructor();
    MatchResult result = regExpConstructor->performMatch(*vm, regExp, string, s, 0);

    // return array of matches
    MarkedArgumentBuffer list;
    // We defend ourselves from crazy.
    const size_t maximumReasonableMatchSize = 1000000000;

    if (regExp->unicode()) {
        unsigned stringLength = s.length();
        while (result) {
            if (list.size() > maximumReasonableMatchSize) {
                throwOutOfMemoryError(exec);
                return jsUndefined();
            }

            size_t end = result.end;
            size_t length = end - result.start;
            list.append(jsSubstring(exec, s, result.start, length));
            if (!length)
                end = advanceStringUnicode(s, stringLength, end);
            result = regExpConstructor->performMatch(*vm, regExp, string, s, end);
        }
    } else {
        while (result) {
            if (list.size() > maximumReasonableMatchSize) {
                throwOutOfMemoryError(exec);
                return jsUndefined();
            }

            size_t end = result.end;
            size_t length = end - result.start;
            list.append(jsSubstring(exec, s, result.start, length));
            if (!length)
                ++end;
            result = regExpConstructor->performMatch(*vm, regExp, string, s, end);
        }
    }

    if (list.isEmpty()) {
        // if there are no matches at all, it's important to return
        // Null instead of an empty array, because this matches
        // other browsers and because Null is a false value.
        return jsNull();
    }

    return constructArray(exec, static_cast<ArrayAllocationProfile*>(0), list);
}

} // namespace JSC
