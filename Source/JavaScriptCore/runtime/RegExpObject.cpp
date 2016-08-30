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

template<typename FixEndFunc>
JSValue collectMatches(VM& vm, ExecState* exec, JSString* string, const String& s, RegExpConstructor* constructor, RegExp* regExp, const FixEndFunc& fixEnd)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    MatchResult result = constructor->performMatch(vm, regExp, string, s, 0);
    if (!result)
        return jsNull();
    
    static unsigned maxSizeForDirectPath = 100000;
    
    JSArray* array = constructEmptyArray(exec, nullptr);
    if (UNLIKELY(vm.exception()))
        return jsUndefined();

    auto iterate = [&] () {
        size_t end = result.end;
        size_t length = end - result.start;
        array->push(exec, JSRopeString::createSubstringOfResolved(vm, string, result.start, length));
        if (!length)
            end = fixEnd(end);
        result = constructor->performMatch(vm, regExp, string, s, end);
    };
    
    do {
        if (array->length() >= maxSizeForDirectPath) {
            // First do a throw-away match to see how many matches we'll get.
            unsigned matchCount = 0;
            MatchResult savedResult = result;
            do {
                if (array->length() + matchCount >= MAX_STORAGE_VECTOR_LENGTH) {
                    throwOutOfMemoryError(exec, scope);
                    return jsUndefined();
                }
                
                size_t end = result.end;
                matchCount++;
                if (result.empty())
                    end = fixEnd(end);
                
                // Using RegExpConstructor::performMatch() instead of calling RegExp::match()
                // directly is a surprising but profitable choice: it means that when we do OOM, we
                // will leave the cached result in the state it ought to have had just before the
                // OOM! On the other hand, if this loop concludes that the result is small enough,
                // then the iterate() loop below will overwrite the cached result anyway.
                result = constructor->performMatch(vm, regExp, string, s, end);
            } while (result);
            
            // OK, we have a sensible number of matches. Now we can create them for reals.
            result = savedResult;
            do
                iterate();
            while (result);
            
            return array;
        }
        
        iterate();
    } while (result);
    
    return array;
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
    
    if (regExp->unicode()) {
        unsigned stringLength = s.length();
        return collectMatches(
            *vm, exec, string, s, regExpConstructor, regExp,
            [&] (size_t end) -> size_t {
                return advanceStringUnicode(s, stringLength, end);
            });
    }
    
    return collectMatches(
        *vm, exec, string, s, regExpConstructor, regExp,
        [&] (size_t end) -> size_t {
            return end + 1;
        });
}

} // namespace JSC
