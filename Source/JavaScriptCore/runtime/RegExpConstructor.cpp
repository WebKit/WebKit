/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All Rights Reserved.
 *  Copyright (C) 2009 Torch Mobile, Inc.
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
#include "RegExpConstructor.h"

#include "ArrayPrototype.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSString.h"
#include "Lookup.h"
#include "ObjectPrototype.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "RegExp.h"
#include "RegExpCache.h"
#include "UStringConcatenate.h"
#include <wtf/PassOwnPtr.h>

namespace JSC {

static JSValue regExpConstructorInput(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorMultiline(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorLastMatch(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorLastParen(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorLeftContext(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorRightContext(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar1(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar2(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar3(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar4(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar5(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar6(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar7(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar8(ExecState*, JSValue, const Identifier&);
static JSValue regExpConstructorDollar9(ExecState*, JSValue, const Identifier&);

static void setRegExpConstructorInput(ExecState*, JSObject*, JSValue);
static void setRegExpConstructorMultiline(ExecState*, JSObject*, JSValue);

} // namespace JSC

#include "RegExpConstructor.lut.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpConstructor);

const ClassInfo RegExpConstructor::s_info = { "Function", &InternalFunction::s_info, 0, ExecState::regExpConstructorTable, CREATE_METHOD_TABLE(RegExpConstructor) };

const ClassInfo RegExpMatchesArray::s_info = {"Array", &JSArray::s_info, 0, 0, CREATE_METHOD_TABLE(RegExpMatchesArray)};

/* Source for RegExpConstructor.lut.h
@begin regExpConstructorTable
    input           regExpConstructorInput          None
    $_              regExpConstructorInput          DontEnum
    multiline       regExpConstructorMultiline      None
    $*              regExpConstructorMultiline      DontEnum
    lastMatch       regExpConstructorLastMatch      DontDelete|ReadOnly
    $&              regExpConstructorLastMatch      DontDelete|ReadOnly|DontEnum
    lastParen       regExpConstructorLastParen      DontDelete|ReadOnly
    $+              regExpConstructorLastParen      DontDelete|ReadOnly|DontEnum
    leftContext     regExpConstructorLeftContext    DontDelete|ReadOnly
    $`              regExpConstructorLeftContext    DontDelete|ReadOnly|DontEnum
    rightContext    regExpConstructorRightContext   DontDelete|ReadOnly
    $'              regExpConstructorRightContext   DontDelete|ReadOnly|DontEnum
    $1              regExpConstructorDollar1        DontDelete|ReadOnly
    $2              regExpConstructorDollar2        DontDelete|ReadOnly
    $3              regExpConstructorDollar3        DontDelete|ReadOnly
    $4              regExpConstructorDollar4        DontDelete|ReadOnly
    $5              regExpConstructorDollar5        DontDelete|ReadOnly
    $6              regExpConstructorDollar6        DontDelete|ReadOnly
    $7              regExpConstructorDollar7        DontDelete|ReadOnly
    $8              regExpConstructorDollar8        DontDelete|ReadOnly
    $9              regExpConstructorDollar9        DontDelete|ReadOnly
@end
*/

RegExpResult& RegExpResult::operator=(const RegExpConstructorPrivate& rhs)
{
    this->input = rhs.input;
    this->ovector = rhs.lastOvector();
    this->lastNumSubPatterns = rhs.lastNumSubPatterns;
    
    return *this;            
}


RegExpConstructor::RegExpConstructor(JSGlobalObject* globalObject, Structure* structure)
    : InternalFunction(globalObject, structure)
{
}

void RegExpConstructor::finishCreation(ExecState* exec, RegExpPrototype* regExpPrototype)
{
    Base::finishCreation(exec->globalData(), Identifier(exec, "RegExp"));
    ASSERT(inherits(&s_info));

    // ECMA 15.10.5.1 RegExp.prototype
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().prototype, regExpPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().length, jsNumber(2), ReadOnly | DontDelete | DontEnum);
}

void RegExpConstructor::destroy(JSCell* cell)
{
    jsCast<RegExpConstructor*>(cell)->RegExpConstructor::~RegExpConstructor();
}

RegExpMatchesArray::RegExpMatchesArray(ExecState* exec)
    : JSArray(exec->globalData(), exec->lexicalGlobalObject()->regExpMatchesArrayStructure())
    , m_didFillArrayInstance(false)
{
}

void RegExpMatchesArray::finishCreation(JSGlobalData& globalData, const RegExpConstructorPrivate& data)
{
    Base::finishCreation(globalData, data.lastNumSubPatterns + 1);
    m_regExpResult = data;
}

void RegExpMatchesArray::destroy(JSCell* cell)
{
    jsCast<RegExpMatchesArray*>(cell)->RegExpMatchesArray::~RegExpMatchesArray();
}

void RegExpMatchesArray::fillArrayInstance(ExecState* exec)
{
    unsigned lastNumSubpatterns = m_regExpResult.lastNumSubPatterns;

    for (unsigned i = 0; i <= lastNumSubpatterns; ++i) {
        int start = m_regExpResult.ovector[2 * i];
        if (start >= 0)
            JSArray::putByIndex(this, exec, i, jsSubstring(exec, m_regExpResult.input, start, m_regExpResult.ovector[2 * i + 1] - start));
        else
            JSArray::putByIndex(this, exec, i, jsUndefined());
    }

    PutPropertySlot slot;
    JSArray::put(this, exec, exec->propertyNames().index, jsNumber(m_regExpResult.ovector[0]), slot);
    JSArray::put(this, exec, exec->propertyNames().input, jsString(exec, m_regExpResult.input), slot);

    m_didFillArrayInstance = true;
}

JSObject* RegExpConstructor::arrayOfMatches(ExecState* exec) const
{
    return RegExpMatchesArray::create(exec, d);
}

JSValue RegExpConstructor::getBackref(ExecState* exec, unsigned i) const
{
    if (!d.lastOvector().isEmpty() && i <= d.lastNumSubPatterns) {
        int start = d.lastOvector()[2 * i];
        if (start >= 0)
            return jsSubstring(exec, d.lastInput, start, d.lastOvector()[2 * i + 1] - start);
    }
    return jsEmptyString(exec);
}

JSValue RegExpConstructor::getLastParen(ExecState* exec) const
{
    unsigned i = d.lastNumSubPatterns;
    if (i > 0) {
        ASSERT(!d.lastOvector().isEmpty());
        int start = d.lastOvector()[2 * i];
        if (start >= 0)
            return jsSubstring(exec, d.lastInput, start, d.lastOvector()[2 * i + 1] - start);
    }
    return jsEmptyString(exec);
}

JSValue RegExpConstructor::getLeftContext(ExecState* exec) const
{
    if (!d.lastOvector().isEmpty())
        return jsSubstring(exec, d.lastInput, 0, d.lastOvector()[0]);
    return jsEmptyString(exec);
}

JSValue RegExpConstructor::getRightContext(ExecState* exec) const
{
    if (!d.lastOvector().isEmpty())
        return jsSubstring(exec, d.lastInput, d.lastOvector()[1], d.lastInput.length() - d.lastOvector()[1]);
    return jsEmptyString(exec);
}
    
bool RegExpConstructor::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<RegExpConstructor, InternalFunction>(exec, ExecState::regExpConstructorTable(exec), jsCast<RegExpConstructor*>(cell), propertyName, slot);
}

bool RegExpConstructor::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    return getStaticValueDescriptor<RegExpConstructor, InternalFunction>(exec, ExecState::regExpConstructorTable(exec), jsCast<RegExpConstructor*>(object), propertyName, descriptor);
}

JSValue regExpConstructorDollar1(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 1);
}

JSValue regExpConstructorDollar2(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 2);
}

JSValue regExpConstructorDollar3(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 3);
}

JSValue regExpConstructorDollar4(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 4);
}

JSValue regExpConstructorDollar5(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 5);
}

JSValue regExpConstructorDollar6(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 6);
}

JSValue regExpConstructorDollar7(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 7);
}

JSValue regExpConstructorDollar8(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 8);
}

JSValue regExpConstructorDollar9(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 9);
}

JSValue regExpConstructorInput(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return jsString(exec, asRegExpConstructor(slotBase)->input());
}

JSValue regExpConstructorMultiline(ExecState*, JSValue slotBase, const Identifier&)
{
    return jsBoolean(asRegExpConstructor(slotBase)->multiline());
}

JSValue regExpConstructorLastMatch(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getBackref(exec, 0);
}

JSValue regExpConstructorLastParen(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getLastParen(exec);
}

JSValue regExpConstructorLeftContext(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getLeftContext(exec);
}

JSValue regExpConstructorRightContext(ExecState* exec, JSValue slotBase, const Identifier&)
{
    return asRegExpConstructor(slotBase)->getRightContext(exec);
}

void RegExpConstructor::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    lookupPut<RegExpConstructor, InternalFunction>(exec, propertyName, value, ExecState::regExpConstructorTable(exec), jsCast<RegExpConstructor*>(cell), slot);
}

void setRegExpConstructorInput(ExecState* exec, JSObject* baseObject, JSValue value)
{
    asRegExpConstructor(baseObject)->setInput(value.toString(exec)->value(exec));
}

void setRegExpConstructorMultiline(ExecState* exec, JSObject* baseObject, JSValue value)
{
    asRegExpConstructor(baseObject)->setMultiline(value.toBoolean(exec));
}

// ECMA 15.10.4
JSObject* constructRegExp(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args, bool callAsConstructor)
{
    JSValue arg0 = args.at(0);
    JSValue arg1 = args.at(1);

    if (arg0.inherits(&RegExpObject::s_info)) {
        if (!arg1.isUndefined())
            return throwError(exec, createTypeError(exec, "Cannot supply flags when constructing one RegExp from another."));
        // If called as a function, this just returns the first argument (see 15.10.3.1).
        if (callAsConstructor) {
            RegExp* regExp = static_cast<RegExpObject*>(asObject(arg0))->regExp();
            return RegExpObject::create(exec, globalObject, globalObject->regExpStructure(), regExp);
        }
        return asObject(arg0);
    }

    UString pattern = arg0.isUndefined() ? UString("") : arg0.toString(exec)->value(exec);
    if (exec->hadException())
        return 0;

    RegExpFlags flags = NoFlags;
    if (!arg1.isUndefined()) {
        flags = regExpFlags(arg1.toString(exec)->value(exec));
        if (exec->hadException())
            return 0;
        if (flags == InvalidFlags)
            return throwError(exec, createSyntaxError(exec, "Invalid flags supplied to RegExp constructor."));
    }

    RegExp* regExp = RegExp::create(exec->globalData(), pattern, flags);
    if (!regExp->isValid())
        return throwError(exec, createSyntaxError(exec, regExp->errorMessage()));
    return RegExpObject::create(exec, exec->lexicalGlobalObject(), globalObject->regExpStructure(), regExp);
}

static EncodedJSValue JSC_HOST_CALL constructWithRegExpConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructRegExp(exec, asInternalFunction(exec->callee())->globalObject(), args, true));
}

ConstructType RegExpConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithRegExpConstructor;
    return ConstructTypeHost;
}

// ECMA 15.10.3
static EncodedJSValue JSC_HOST_CALL callRegExpConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructRegExp(exec, asInternalFunction(exec->callee())->globalObject(), args));
}

CallType RegExpConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callRegExpConstructor;
    return CallTypeHost;
}

void RegExpConstructor::setInput(const UString& input)
{
    d.input = input;
}

const UString& RegExpConstructor::input() const
{
    // Can detect a distinct initial state that is invisible to JavaScript, by checking for null
    // state (since jsString turns null strings to empty strings).
    return d.input;
}

void RegExpConstructor::setMultiline(bool multiline)
{
    d.multiline = multiline;
}

bool RegExpConstructor::multiline() const
{
    return d.multiline;
}

} // namespace JSC
