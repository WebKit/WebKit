/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All Rights Reserved.
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

#include "Error.h"
#include "GetterSetter.h"
#include "JSCInlines.h"
#include "RegExpGlobalDataInlines.h"
#include "RegExpPrototype.h"
#include "YarrFlags.h"

namespace JSC {

static EncodedJSValue regExpConstructorInput(JSGlobalObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorMultiline(JSGlobalObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorLastMatch(JSGlobalObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorLastParen(JSGlobalObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorLeftContext(JSGlobalObject*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorRightContext(JSGlobalObject*, EncodedJSValue, PropertyName);
template<int N>
static EncodedJSValue regExpConstructorDollar(JSGlobalObject*, EncodedJSValue, PropertyName);

static bool setRegExpConstructorInput(JSGlobalObject*, EncodedJSValue, EncodedJSValue);
static bool setRegExpConstructorMultiline(JSGlobalObject*, EncodedJSValue, EncodedJSValue);

} // namespace JSC

#include "RegExpConstructor.lut.h"

namespace JSC {

const ClassInfo RegExpConstructor::s_info = { "Function", &InternalFunction::s_info, &regExpConstructorTable, nullptr, CREATE_METHOD_TABLE(RegExpConstructor) };

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
    $1              regExpConstructorDollar<1>      DontDelete|ReadOnly
    $2              regExpConstructorDollar<2>      DontDelete|ReadOnly
    $3              regExpConstructorDollar<3>      DontDelete|ReadOnly
    $4              regExpConstructorDollar<4>      DontDelete|ReadOnly
    $5              regExpConstructorDollar<5>      DontDelete|ReadOnly
    $6              regExpConstructorDollar<6>      DontDelete|ReadOnly
    $7              regExpConstructorDollar<7>      DontDelete|ReadOnly
    $8              regExpConstructorDollar<8>      DontDelete|ReadOnly
    $9              regExpConstructorDollar<9>      DontDelete|ReadOnly
@end
*/


static EncodedJSValue JSC_HOST_CALL callRegExpConstructor(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructWithRegExpConstructor(JSGlobalObject*, CallFrame*);

RegExpConstructor::RegExpConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callRegExpConstructor, constructWithRegExpConstructor)
{
}

void RegExpConstructor::finishCreation(VM& vm, RegExpPrototype* regExpPrototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, vm.propertyNames->RegExp.string(), NameAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(vm, info()));

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, regExpPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(2), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);

    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, speciesSymbol, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

template<int N>
EncodedJSValue regExpConstructorDollar(JSGlobalObject*, EncodedJSValue thisValue, PropertyName)
{
    JSGlobalObject* globalObject = jsCast<RegExpConstructor*>(JSValue::decode(thisValue))->globalObject();
    return JSValue::encode(globalObject->regExpGlobalData().getBackref(globalObject, N));
}

EncodedJSValue regExpConstructorInput(JSGlobalObject*, EncodedJSValue thisValue, PropertyName)
{
    JSGlobalObject* globalObject = jsCast<RegExpConstructor*>(JSValue::decode(thisValue))->globalObject();
    return JSValue::encode(globalObject->regExpGlobalData().input());
}

EncodedJSValue regExpConstructorMultiline(JSGlobalObject*, EncodedJSValue thisValue, PropertyName)
{
    JSGlobalObject* globalObject = jsCast<RegExpConstructor*>(JSValue::decode(thisValue))->globalObject();
    return JSValue::encode(jsBoolean(globalObject->regExpGlobalData().multiline()));
}

EncodedJSValue regExpConstructorLastMatch(JSGlobalObject*, EncodedJSValue thisValue, PropertyName)
{
    JSGlobalObject* globalObject = jsCast<RegExpConstructor*>(JSValue::decode(thisValue))->globalObject();
    return JSValue::encode(globalObject->regExpGlobalData().getBackref(globalObject, 0));
}

EncodedJSValue regExpConstructorLastParen(JSGlobalObject*, EncodedJSValue thisValue, PropertyName)
{
    JSGlobalObject* globalObject = jsCast<RegExpConstructor*>(JSValue::decode(thisValue))->globalObject();
    return JSValue::encode(globalObject->regExpGlobalData().getLastParen(globalObject));
}

EncodedJSValue regExpConstructorLeftContext(JSGlobalObject*, EncodedJSValue thisValue, PropertyName)
{
    JSGlobalObject* globalObject = jsCast<RegExpConstructor*>(JSValue::decode(thisValue))->globalObject();
    return JSValue::encode(globalObject->regExpGlobalData().getLeftContext(globalObject));
}

EncodedJSValue regExpConstructorRightContext(JSGlobalObject*, EncodedJSValue thisValue, PropertyName)
{
    JSGlobalObject* globalObject = jsCast<RegExpConstructor*>(JSValue::decode(thisValue))->globalObject();
    return JSValue::encode(globalObject->regExpGlobalData().getRightContext(globalObject));
}

bool setRegExpConstructorInput(JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (auto constructor = jsDynamicCast<RegExpConstructor*>(vm, JSValue::decode(thisValue))) {
        auto* string = JSValue::decode(value).toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        scope.release();
        JSGlobalObject* globalObject = constructor->globalObject();
        globalObject->regExpGlobalData().setInput(globalObject, string);
        return true;
    }
    return false;
}

bool setRegExpConstructorMultiline(JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (auto constructor = jsDynamicCast<RegExpConstructor*>(vm, JSValue::decode(thisValue))) {
        bool multiline = JSValue::decode(value).toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        scope.release();
        JSGlobalObject* globalObject = constructor->globalObject();
        globalObject->regExpGlobalData().setMultiline(multiline);
        return true;
    }
    return false;
}

inline Structure* getRegExpStructure(JSGlobalObject* globalObject, JSValue newTarget)
{
    Structure* structure = globalObject->regExpStructure();
    if (newTarget != jsUndefined())
        structure = InternalFunction::createSubclassStructure(globalObject, globalObject->regExpConstructor(), newTarget, structure);
    return structure;
}

inline OptionSet<Yarr::Flags> toFlags(JSGlobalObject* globalObject, JSValue flags)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (flags.isUndefined())
        return { };
    
    auto result = Yarr::parseFlags(flags.toWTFString(globalObject));
    RETURN_IF_EXCEPTION(scope, { });
    if (!result) {
        throwSyntaxError(globalObject, scope, "Invalid flags supplied to RegExp constructor."_s);
        return { };
    }

    return result.value();
}

static JSObject* regExpCreate(JSGlobalObject* globalObject, JSValue newTarget, JSValue patternArg, JSValue flagsArg)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String pattern = patternArg.isUndefined() ? emptyString() : patternArg.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto flags = toFlags(globalObject, flagsArg);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RegExp* regExp = RegExp::create(vm, pattern, flags);
    if (UNLIKELY(!regExp->isValid())) {
        throwException(globalObject, scope, regExp->errorToThrow(globalObject));
        return nullptr;
    }

    Structure* structure = getRegExpStructure(globalObject, newTarget);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return RegExpObject::create(vm, structure, regExp);
}

JSObject* constructRegExp(JSGlobalObject* globalObject, const ArgList& args,  JSObject* callee, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue patternArg = args.at(0);
    JSValue flagsArg = args.at(1);

    bool isPatternRegExp = patternArg.inherits<RegExpObject>(vm);
    bool constructAsRegexp = isRegExp(vm, globalObject, patternArg);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (newTarget.isUndefined() && constructAsRegexp && flagsArg.isUndefined()) {
        JSValue constructor = patternArg.get(globalObject, vm.propertyNames->constructor);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (callee == constructor) {
            // We know that patternArg is a object otherwise constructAsRegexp would be false.
            return patternArg.getObject();
        }
    }

    if (isPatternRegExp) {
        RegExp* regExp = jsCast<RegExpObject*>(patternArg)->regExp();
        Structure* structure = getRegExpStructure(globalObject, newTarget);
        RETURN_IF_EXCEPTION(scope, nullptr);

        if (!flagsArg.isUndefined()) {
            auto flags = toFlags(globalObject, flagsArg);
            RETURN_IF_EXCEPTION(scope, nullptr);

            regExp = RegExp::create(vm, regExp->pattern(), flags);
            if (UNLIKELY(!regExp->isValid())) {
                throwException(globalObject, scope, regExp->errorToThrow(globalObject));
                return nullptr;
            }
        }

        return RegExpObject::create(vm, structure, regExp);
    }

    if (constructAsRegexp) {
        JSValue pattern = patternArg.get(globalObject, vm.propertyNames->source);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (flagsArg.isUndefined()) {
            flagsArg = patternArg.get(globalObject, vm.propertyNames->flags);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        patternArg = pattern;
    }

    RELEASE_AND_RETURN(scope, regExpCreate(globalObject, newTarget, patternArg, flagsArg));
}

EncodedJSValue JSC_HOST_CALL esSpecRegExpCreate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue patternArg = callFrame->argument(0);
    JSValue flagsArg = callFrame->argument(1);
    return JSValue::encode(regExpCreate(globalObject, jsUndefined(), patternArg, flagsArg));
}

EncodedJSValue JSC_HOST_CALL esSpecIsRegExp(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    return JSValue::encode(jsBoolean(isRegExp(vm, globalObject, callFrame->argument(0))));
}

static EncodedJSValue JSC_HOST_CALL constructWithRegExpConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ArgList args(callFrame);
    return JSValue::encode(constructRegExp(globalObject, args, callFrame->jsCallee(), callFrame->newTarget()));
}

static EncodedJSValue JSC_HOST_CALL callRegExpConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ArgList args(callFrame);
    return JSValue::encode(constructRegExp(globalObject, args, callFrame->jsCallee()));
}

} // namespace JSC
