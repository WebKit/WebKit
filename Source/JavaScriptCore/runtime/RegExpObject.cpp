/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2012 Apple Inc. All Rights Reserved.
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
#include "Lexer.h"
#include "Lookup.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpPrototype.h"
#include "UStringBuilder.h"
#include "UStringConcatenate.h"
#include <wtf/PassOwnPtr.h>

namespace JSC {

static JSValue regExpObjectGlobal(ExecState*, JSValue, PropertyName);
static JSValue regExpObjectIgnoreCase(ExecState*, JSValue, PropertyName);
static JSValue regExpObjectMultiline(ExecState*, JSValue, PropertyName);
static JSValue regExpObjectSource(ExecState*, JSValue, PropertyName);

} // namespace JSC

#include "RegExpObject.lut.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpObject);

const ClassInfo RegExpObject::s_info = { "RegExp", &JSNonFinalObject::s_info, 0, ExecState::regExpTable, CREATE_METHOD_TABLE(RegExpObject) };

/* Source for RegExpObject.lut.h
@begin regExpTable
    global        regExpObjectGlobal       DontDelete|ReadOnly|DontEnum
    ignoreCase    regExpObjectIgnoreCase   DontDelete|ReadOnly|DontEnum
    multiline     regExpObjectMultiline    DontDelete|ReadOnly|DontEnum
    source        regExpObjectSource       DontDelete|ReadOnly|DontEnum
@end
*/

RegExpObject::RegExpObject(JSGlobalObject* globalObject, Structure* structure, RegExp* regExp)
    : JSNonFinalObject(globalObject->globalData(), structure)
    , m_regExp(globalObject->globalData(), this, regExp)
    , m_lastIndexIsWritable(true)
{
    m_lastIndex.setWithoutWriteBarrier(jsNumber(0));
}

void RegExpObject::finishCreation(JSGlobalObject* globalObject)
{
    Base::finishCreation(globalObject->globalData());
    ASSERT(inherits(&s_info));
}

void RegExpObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    RegExpObject* thisObject = jsCast<RegExpObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);
    if (thisObject->m_regExp)
        visitor.append(&thisObject->m_regExp);
    if (UNLIKELY(!thisObject->m_lastIndex.get().isInt32()))
        visitor.append(&thisObject->m_lastIndex);
}

bool RegExpObject::getOwnPropertySlot(JSCell* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().lastIndex) {
        RegExpObject* regExp = asRegExpObject(cell);
        slot.setValue(regExp, regExp->getLastIndex());
        return true;
    }
    return getStaticValueSlot<RegExpObject, JSObject>(exec, ExecState::regExpTable(exec), jsCast<RegExpObject*>(cell), propertyName, slot);
}

bool RegExpObject::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    if (propertyName == exec->propertyNames().lastIndex) {
        RegExpObject* regExp = asRegExpObject(object);
        descriptor.setDescriptor(regExp->getLastIndex(), regExp->m_lastIndexIsWritable ? DontDelete | DontEnum : DontDelete | DontEnum | ReadOnly);
        return true;
    }
    return getStaticValueDescriptor<RegExpObject, JSObject>(exec, ExecState::regExpTable(exec), jsCast<RegExpObject*>(object), propertyName, descriptor);
}

bool RegExpObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    if (propertyName == exec->propertyNames().lastIndex)
        return false;
    return Base::deleteProperty(cell, exec, propertyName);
}

void RegExpObject::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (mode == IncludeDontEnumProperties)
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getOwnPropertyNames(object, exec, propertyNames, mode);
}

void RegExpObject::getPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (mode == IncludeDontEnumProperties)
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getPropertyNames(object, exec, propertyNames, mode);
}

static bool reject(ExecState* exec, bool throwException, const char* message)
{
    if (throwException)
        throwTypeError(exec, message);
    return false;
}

bool RegExpObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, PropertyDescriptor& descriptor, bool shouldThrow)
{
    if (propertyName == exec->propertyNames().lastIndex) {
        RegExpObject* regExp = asRegExpObject(object);
        if (descriptor.configurablePresent() && descriptor.configurable())
            return reject(exec, shouldThrow, "Attempting to change configurable attribute of unconfigurable property.");
        if (descriptor.enumerablePresent() && descriptor.enumerable())
            return reject(exec, shouldThrow, "Attempting to change enumerable attribute of unconfigurable property.");
        if (descriptor.isAccessorDescriptor())
            return reject(exec, shouldThrow, "Attempting to change access mechanism for an unconfigurable property.");
        if (!regExp->m_lastIndexIsWritable) {
            if (descriptor.writablePresent() && descriptor.writable())
                return reject(exec, shouldThrow, "Attempting to change writable attribute of unconfigurable property.");
            if (!sameValue(exec, regExp->getLastIndex(), descriptor.value()))
                return reject(exec, shouldThrow, "Attempting to change value of a readonly property.");
            return true;
        }
        if (descriptor.writablePresent() && !descriptor.writable())
            regExp->m_lastIndexIsWritable = false;
        if (descriptor.value())
            regExp->setLastIndex(exec, descriptor.value(), false);
        return true;
    }

    return Base::defineOwnProperty(object, exec, propertyName, descriptor, shouldThrow);
}

JSValue regExpObjectGlobal(ExecState*, JSValue slotBase, PropertyName)
{
    return jsBoolean(asRegExpObject(slotBase)->regExp()->global());
}

JSValue regExpObjectIgnoreCase(ExecState*, JSValue slotBase, PropertyName)
{
    return jsBoolean(asRegExpObject(slotBase)->regExp()->ignoreCase());
}
 
JSValue regExpObjectMultiline(ExecState*, JSValue slotBase, PropertyName)
{            
    return jsBoolean(asRegExpObject(slotBase)->regExp()->multiline());
}

JSValue regExpObjectSource(ExecState* exec, JSValue slotBase, PropertyName)
{
    UString pattern = asRegExpObject(slotBase)->regExp()->pattern();
    unsigned length = pattern.length();
    const UChar* characters = pattern.characters();
    bool previousCharacterWasBackslash = false;
    bool inBrackets = false;
    bool shouldEscape = false;

    // 15.10.6.4 specifies that RegExp.prototype.toString must return '/' + source + '/',
    // and also states that the result must be a valid RegularExpressionLiteral. '//' is
    // not a valid RegularExpressionLiteral (since it is a single line comment), and hence
    // source cannot ever validly be "". If the source is empty, return a different Pattern
    // that would match the same thing.
    if (!length)
        return jsString(exec, "(?:)");

    // early return for strings that don't contain a forwards slash and LineTerminator
    for (unsigned i = 0; i < length; ++i) {
        UChar ch = characters[i];
        if (!previousCharacterWasBackslash) {
            if (inBrackets) {
                if (ch == ']')
                    inBrackets = false;
            } else {
                if (ch == '/') {
                    shouldEscape = true;
                    break;
                }
                if (ch == '[')
                    inBrackets = true;
            }
        }

        if (Lexer<UChar>::isLineTerminator(ch)) {
            shouldEscape = true;
            break;
        }

        if (previousCharacterWasBackslash)
            previousCharacterWasBackslash = false;
        else
            previousCharacterWasBackslash = ch == '\\';
    }

    if (!shouldEscape)
        return jsString(exec, pattern);

    previousCharacterWasBackslash = false;
    inBrackets = false;
    UStringBuilder result;
    for (unsigned i = 0; i < length; ++i) {
        UChar ch = characters[i];
        if (!previousCharacterWasBackslash) {
            if (inBrackets) {
                if (ch == ']')
                    inBrackets = false;
            } else {
                if (ch == '/')
                    result.append('\\');
                else if (ch == '[')
                    inBrackets = true;
            }
        }

        // escape LineTerminator
        if (Lexer<UChar>::isLineTerminator(ch)) {
            if (!previousCharacterWasBackslash)
                result.append('\\');

            if (ch == '\n')
                result.append('n');
            else if (ch == '\r')
                result.append('r');
            else if (ch == 0x2028)
                result.append("u2028");
            else
                result.append("u2029");
        } else
            result.append(ch);

        if (previousCharacterWasBackslash)
            previousCharacterWasBackslash = false;
        else
            previousCharacterWasBackslash = ch == '\\';
    }

    return jsString(exec, result.toUString());
}

void RegExpObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    if (propertyName == exec->propertyNames().lastIndex) {
        asRegExpObject(cell)->setLastIndex(exec, value, slot.isStrictMode());
        return;
    }
    lookupPut<RegExpObject, JSObject>(exec, propertyName, value, ExecState::regExpTable(exec), jsCast<RegExpObject*>(cell), slot);
}

JSValue RegExpObject::exec(ExecState* exec, JSString* string)
{
    if (MatchResult result = match(exec, string))
        return RegExpMatchesArray::create(exec, string, regExp(), result);
    return jsNull();
}

// Shared implementation used by test and exec.
MatchResult RegExpObject::match(ExecState* exec, JSString* string)
{
    RegExp* regExp = this->regExp();
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    UString input = string->value(exec);
    JSGlobalData& globalData = exec->globalData();
    if (!regExp->global())
        return regExpConstructor->performMatch(globalData, regExp, string, input, 0);

    JSValue jsLastIndex = getLastIndex();
    unsigned lastIndex;
    if (LIKELY(jsLastIndex.isUInt32())) {
        lastIndex = jsLastIndex.asUInt32();
        if (lastIndex > input.length()) {
            setLastIndex(exec, 0);
            return MatchResult::failed();
        }
    } else {
        double doubleLastIndex = jsLastIndex.toInteger(exec);
        if (doubleLastIndex < 0 || doubleLastIndex > input.length()) {
            setLastIndex(exec, 0);
            return MatchResult::failed();
        }
        lastIndex = static_cast<unsigned>(doubleLastIndex);
    }

    MatchResult result = regExpConstructor->performMatch(globalData, regExp, string, input, lastIndex);
    setLastIndex(exec, result.end);
    return result;
}

} // namespace JSC
