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

#include "ButterflyInlines.h"
#include "CopiedSpaceInlines.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "Lexer.h"
#include "Lookup.h"
#include "JSCInlines.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpPrototype.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

static EncodedJSValue regExpObjectGlobal(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpObjectIgnoreCase(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpObjectMultiline(ExecState*, JSObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpObjectSource(ExecState*, JSObject*, EncodedJSValue, PropertyName);

} // namespace JSC

#include "RegExpObject.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(RegExpObject);

const ClassInfo RegExpObject::s_info = { "RegExp", &Base::s_info, &regExpTable, CREATE_METHOD_TABLE(RegExpObject) };

/* Source for RegExpObject.lut.h
@begin regExpTable
    global        regExpObjectGlobal       DontDelete|ReadOnly|DontEnum
    ignoreCase    regExpObjectIgnoreCase   DontDelete|ReadOnly|DontEnum
    multiline     regExpObjectMultiline    DontDelete|ReadOnly|DontEnum
    source        regExpObjectSource       DontDelete|ReadOnly|DontEnum
@end
*/

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
    return getStaticValueSlot<RegExpObject, JSObject>(exec, regExpTable, jsCast<RegExpObject*>(object), propertyName, slot);
}

bool RegExpObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    if (propertyName == exec->propertyNames().lastIndex)
        return false;
    return Base::deleteProperty(cell, exec, propertyName);
}

void RegExpObject::getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (shouldIncludeDontEnumProperties(mode))
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getOwnNonIndexPropertyNames(object, exec, propertyNames, mode);
}

void RegExpObject::getPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (shouldIncludeDontEnumProperties(mode))
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getPropertyNames(object, exec, propertyNames, mode);
}

void RegExpObject::getGenericPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (shouldIncludeDontEnumProperties(mode))
        propertyNames.add(exec->propertyNames().lastIndex);
    Base::getGenericPropertyNames(object, exec, propertyNames, mode);
}

static bool reject(ExecState* exec, bool throwException, const char* message)
{
    if (throwException)
        throwTypeError(exec, ASCIILiteral(message));
    return false;
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

EncodedJSValue regExpObjectGlobal(ExecState*, JSObject* slotBase, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsBoolean(asRegExpObject(slotBase)->regExp()->global()));
}

EncodedJSValue regExpObjectIgnoreCase(ExecState*, JSObject* slotBase, EncodedJSValue, PropertyName)
{
    return JSValue::encode(jsBoolean(asRegExpObject(slotBase)->regExp()->ignoreCase()));
}
 
EncodedJSValue regExpObjectMultiline(ExecState*, JSObject* slotBase, EncodedJSValue, PropertyName)
{            
    return JSValue::encode(jsBoolean(asRegExpObject(slotBase)->regExp()->multiline()));
}

template <typename CharacterType>
static inline void appendLineTerminatorEscape(StringBuilder&, CharacterType);

template <>
inline void appendLineTerminatorEscape<LChar>(StringBuilder& builder, LChar lineTerminator)
{
    if (lineTerminator == '\n')
        builder.append('n');
    else
        builder.append('r');
}

template <>
inline void appendLineTerminatorEscape<UChar>(StringBuilder& builder, UChar lineTerminator)
{
    if (lineTerminator == '\n')
        builder.append('n');
    else if (lineTerminator == '\r')
        builder.append('r');
    else if (lineTerminator == 0x2028)
        builder.appendLiteral("u2028");
    else
        builder.appendLiteral("u2029");
}

template <typename CharacterType>
static inline JSValue regExpObjectSourceInternal(ExecState* exec, String pattern, const CharacterType* characters, unsigned length)
{
    bool previousCharacterWasBackslash = false;
    bool inBrackets = false;
    bool shouldEscape = false;

    // 15.10.6.4 specifies that RegExp.prototype.toString must return '/' + source + '/',
    // and also states that the result must be a valid RegularExpressionLiteral. '//' is
    // not a valid RegularExpressionLiteral (since it is a single line comment), and hence
    // source cannot ever validly be "". If the source is empty, return a different Pattern
    // that would match the same thing.
    if (!length)
        return jsNontrivialString(exec, ASCIILiteral("(?:)"));

    // early return for strings that don't contain a forwards slash and LineTerminator
    for (unsigned i = 0; i < length; ++i) {
        CharacterType ch = characters[i];
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

        if (Lexer<CharacterType>::isLineTerminator(ch)) {
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
    StringBuilder result;
    for (unsigned i = 0; i < length; ++i) {
        CharacterType ch = characters[i];
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
        if (Lexer<CharacterType>::isLineTerminator(ch)) {
            if (!previousCharacterWasBackslash)
                result.append('\\');

            appendLineTerminatorEscape<CharacterType>(result, ch);
        } else
            result.append(ch);

        if (previousCharacterWasBackslash)
            previousCharacterWasBackslash = false;
        else
            previousCharacterWasBackslash = ch == '\\';
    }

    return jsString(exec, result.toString());
}

    
    
EncodedJSValue regExpObjectSource(ExecState* exec, JSObject* slotBase, EncodedJSValue, PropertyName)
{
    String pattern = asRegExpObject(slotBase)->regExp()->pattern();
    if (pattern.is8Bit())
        return JSValue::encode(regExpObjectSourceInternal(exec, pattern, pattern.characters8(), pattern.length()));
    return JSValue::encode(regExpObjectSourceInternal(exec, pattern, pattern.characters16(), pattern.length()));
}

static void regExpObjectSetLastIndexStrict(ExecState* exec, JSObject* slotBase, EncodedJSValue, EncodedJSValue value)
{
    asRegExpObject(slotBase)->setLastIndex(exec, JSValue::decode(value), true);
}

static void regExpObjectSetLastIndexNonStrict(ExecState* exec, JSObject* slotBase, EncodedJSValue, EncodedJSValue value)
{
    asRegExpObject(slotBase)->setLastIndex(exec, JSValue::decode(value), false);
}

void RegExpObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    if (propertyName == exec->propertyNames().lastIndex) {
        asRegExpObject(cell)->setLastIndex(exec, value, slot.isStrictMode());
        slot.setCustomProperty(asRegExpObject(cell), slot.isStrictMode()
            ? regExpObjectSetLastIndexStrict
            : regExpObjectSetLastIndexNonStrict);
        return;
    }
    Base::put(cell, exec, propertyName, value, slot);
}

JSValue RegExpObject::exec(ExecState* exec, JSString* string)
{
    if (MatchResult result = match(exec, string))
        return createRegExpMatchesArray(exec, string, regExp(), result);
    return jsNull();
}

// Shared implementation used by test and exec.
MatchResult RegExpObject::match(ExecState* exec, JSString* string)
{
    RegExp* regExp = this->regExp();
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    String input = string->value(exec);
    VM& vm = exec->vm();
    if (!regExp->global())
        return regExpConstructor->performMatch(vm, regExp, string, input, 0);

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

    MatchResult result = regExpConstructor->performMatch(vm, regExp, string, input, lastIndex);
    setLastIndex(exec, result.end);
    return result;
}

} // namespace JSC
