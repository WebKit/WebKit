/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
#include "RegExpPrototype.h"

#include "CachedCallInlines.h"
#include "InterpreterInlines.h"
#include "IntegrityInlines.h"
#include "JSArray.h"
#include "JSCJSValue.h"
#include "JSGlobalObject.h"
#include "JSRegExpStringIterator.h"
#include "JSStringInlines.h"
#include "ObjectConstructor.h"
#include "RegExpConstructor.h"
#include "StringPrototypeInlines.h"
#include "VMEntryScopeInlines.h"
#include "RegExpObject.h"
#include "RegExpObjectInlines.h"
#include "RegExpPrototypeInlines.h"
#include "StringRecursionChecker.h"
#include "YarrFlags.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringCommon.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncExec);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncCompile);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncToString);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterGlobal);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterHasIndices);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterIgnoreCase);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterMultiline);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterDotAll);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterSticky);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterUnicode);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterUnicodeSets);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterSource);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterFlags);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncTest);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncSearch);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncReplace);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncMatch);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncMatchAll);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncSplit);

const ClassInfo RegExpPrototype::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RegExpPrototype) };

RegExpPrototype::RegExpPrototype(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void RegExpPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->compile, regExpProtoFuncCompile, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->exec, regExpProtoFuncExec, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, RegExpExecIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, regExpProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->global, regExpProtoGetterGlobal, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpGlobalIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->dotAll, regExpProtoGetterDotAll, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpDotAllIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->hasIndices, regExpProtoGetterHasIndices, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpHasIndicesIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->ignoreCase, regExpProtoGetterIgnoreCase, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpIgnoreCaseIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->multiline, regExpProtoGetterMultiline, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpMultilineIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->sticky, regExpProtoGetterSticky, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpStickyIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->unicode, regExpProtoGetterUnicode, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpUnicodeIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->unicodeSets, regExpProtoGetterUnicodeSets, PropertyAttribute::DontEnum | PropertyAttribute::Accessor, RegExpUnicodeSetsIntrinsic);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->source, regExpProtoGetterSource, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->flags, regExpProtoGetterFlags, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSFunction* matchFunction = JSFunction::create(vm, globalObject, 1, "[Symbol.match]"_s, regExpProtoFuncMatch, ImplementationVisibility::Public, RegExpMatchIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->matchSymbol, matchFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSFunction* matchAllFunction = JSFunction::create(vm, globalObject, 1, "[Symbol.matchAll]"_s, regExpProtoFuncMatchAll, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->matchAllSymbol, matchAllFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSFunction* replaceFunction = JSFunction::create(vm, globalObject, 2, "[Symbol.replace]"_s, regExpProtoFuncReplace, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->replaceSymbol, replaceFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSFunction* searchFunction = JSFunction::create(vm, globalObject, 1, "[Symbol.search]"_s, regExpProtoFuncSearch, ImplementationVisibility::Public, RegExpSearchIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->searchSymbol, searchFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSFunction* splitFunction = JSFunction::create(vm, globalObject, 2, "[Symbol.split]"_s, regExpProtoFuncSplit, ImplementationVisibility::Public, RegExpSplitIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->splitSymbol, splitFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->test, regExpProtoFuncTest, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, RegExpTestIntrinsic);
}

// ------------------------------ Functions ---------------------------

JSValue regExpExec(JSGlobalObject* globalObject, JSValue thisValue, JSString* str)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(thisValue.isObject());

    JSObject* thisObject = asObject(thisValue);
    JSValue regExpExec = thisObject->get(globalObject, vm.propertyNames->exec);
    RETURN_IF_EXCEPTION(scope, { });
    JSFunction* regExpBuiltinExec = globalObject->regExpProtoExecFunction();

    JSValue match;
    if (regExpExec != regExpBuiltinExec && regExpExec.isCallable()) [[unlikely]] {
        auto callData = JSC::getCallDataInline(regExpExec);
        ASSERT(callData.type != CallData::Type::None);
        if (callData.type == CallData::Type::JS) [[likely]] {
            CachedCall cachedCall(globalObject, uncheckedDowncast<JSFunction>(regExpExec), 1);
            RETURN_IF_EXCEPTION(scope, { });
            match = cachedCall.callWithArguments(globalObject, thisValue, str);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            MarkedArgumentBuffer args;
            args.append(str);
            ASSERT(!args.hasOverflowed());
            match = call(globalObject, regExpExec, callData, thisValue, args);
            RETURN_IF_EXCEPTION(scope, { });
        }
        if (!match.isNull() && !match.isObject()) {
            throwTypeError(globalObject, scope, "The result of RegExp exec must be null or an object"_s);
            return { };
        }
    } else {
        auto callData = JSC::getCallDataInline(regExpBuiltinExec);
        MarkedArgumentBuffer args;
        args.append(str);
        ASSERT(!args.hasOverflowed());
        match = call(globalObject, regExpBuiltinExec, callData, thisValue, args);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return match;
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncTest, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "RegExp.prototype.test requires that |this| be an Object"_s);
    JSObject* thisObject = asObject(thisValue);

    JSString* str = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (regExpExecWatchpointIsValid(vm, thisObject)) [[likely]] {
        auto* regExp = dynamicDowncast<RegExpObject>(thisValue);
        if (!regExp) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "Builtin RegExp exec can only be called on a RegExp object"_s);
        auto strValue = str->value(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!strValue->isNull() && regExp->getLastIndex().isNumber()) [[likely]]
            RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(regExp->test(globalObject, str))));
    }

    JSValue match = regExpExec(globalObject, thisValue, str);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsBoolean(!match.isNull()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncExec, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Builtin RegExp exec can only be called on a RegExp object"_s);
    JSString* string = callFrame->argument(0).toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !string);
    if (!string)
        return JSValue::encode(jsUndefined());
    RELEASE_AND_RETURN(scope, JSValue::encode(regexp->exec(globalObject, string)));
}

JSValue regExpMatchFast(JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* string)
{
    if (!regExpObject->regExp()->global())
        return regExpObject->exec(globalObject, string);
    return regExpObject->matchGlobal(globalObject, string);
}

// https://tc39.es/ecma262/#sec-regexp.prototype-%symbol.match%
static JSValue regExpMatchSlow(JSGlobalObject* globalObject, JSObject* thisObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 4. Let flags be ? ToString(? Get(regexp, "flags")).
    JSValue flagsValue = thisObject->get(globalObject, vm.propertyNames->flags);
    RETURN_IF_EXCEPTION(scope, { });
    String flags = flagsValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // 5. If flags does not contain "g", return ? RegExpExec(regexp, string).
    if (!flags.contains('g'))
        RELEASE_AND_RETURN(scope, regExpExec(globalObject, thisObject, string));

    // 6. If flags contains "u" or flags contains "v", let fullUnicode be true; else let fullUnicode be false.
    bool fullUnicode = flags.contains('u') || flags.contains('v');

    // 7. Perform ? Set(regexp, "lastIndex", +0𝔽, true).
    PutPropertySlot lastIndexSlot(thisObject, true);
    thisObject->methodTable()->put(thisObject, globalObject, vm.propertyNames->lastIndex, jsNumber(0), lastIndexSlot);
    RETURN_IF_EXCEPTION(scope, { });

    // 8. Let array be ! ArrayCreate(0).
    JSArray* resultArray = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    auto stringValue = string->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    unsigned stringLength = stringValue->length();

    // 9. Let matchCount be 0.
    uint64_t matchCount = 0;

    // 10. Repeat,
    while (true) {
        // 10.a. Let result be ? RegExpExec(regexp, string).
        JSValue result = regExpExec(globalObject, thisObject, string);
        RETURN_IF_EXCEPTION(scope, { });

        // 10.b. If result is null, then
        if (result.isNull()) {
            // 10.b.i. If matchCount = 0, return null.
            if (!matchCount)
                return jsNull();
            // 10.b.ii. Return array.
            return resultArray;
        }

        // 10.c. Let matchString be ? ToString(? Get(result, "0")).
        JSValue matchValue = asObject(result)->get(globalObject, static_cast<unsigned>(0));
        RETURN_IF_EXCEPTION(scope, { });
        JSString* matchString = matchValue.toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        // 10.d. Perform ! CreateDataPropertyOrThrow(array, ! ToString(𝔽(matchCount)), matchString).
        resultArray->putDirectIndex(globalObject, matchCount, matchString);
        RETURN_IF_EXCEPTION(scope, { });

        // 10.e. If matchString is the empty String, then
        if (!matchString->length()) {
            // 10.e.i. Let thisIndex be ℝ(? ToLength(? Get(regexp, "lastIndex"))).
            JSValue lastIndexValue = thisObject->get(globalObject, vm.propertyNames->lastIndex);
            RETURN_IF_EXCEPTION(scope, { });
            uint64_t thisIndex = lastIndexValue.toLength(globalObject);
            RETURN_IF_EXCEPTION(scope, { });

            // 10.e.ii. Let nextIndex be AdvanceStringIndex(string, thisIndex, fullUnicode).
            uint64_t nextIndex = advanceStringIndex(stringValue, stringLength, thisIndex, fullUnicode);

            // 10.e.iii. Perform ? Set(regexp, "lastIndex", 𝔽(nextIndex), true).
            PutPropertySlot slot(thisObject, true);
            thisObject->methodTable()->put(thisObject, globalObject, vm.propertyNames->lastIndex, jsNumber(nextIndex), slot);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // 10.f. Set matchCount to matchCount + 1.
        ++matchCount;
    }
}

// https://tc39.es/ecma262/#sec-regexp.prototype-%symbol.match%
JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncMatch, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let regexp be the this value.
    // 2. If regexp is not an Object, throw a TypeError exception.
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "RegExp.prototype.@@match requires that |this| be an Object"_s);
    JSObject* thisObject = asObject(thisValue);

    // 3. Set string to ? ToString(string).
    JSString* string = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // Fast path: receiver is a primordial RegExpObject with no observable side effects.
    auto* regExpObject = dynamicDowncast<RegExpObject>(thisObject);
    if (regExpObject && regExpObject->isSymbolMatchFastAndNonObservable()) [[likely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(regExpMatchFast(globalObject, regExpObject, string)));

    RELEASE_AND_RETURN(scope, JSValue::encode(regExpMatchSlow(globalObject, thisObject, string)));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncCompile, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* thisRegExp = dynamicDowncast<RegExpObject>(thisValue);
    if (!thisRegExp) [[unlikely]]
        return throwVMTypeError(globalObject, scope);

    if (thisRegExp->realm() != globalObject)
        return throwVMTypeError(globalObject, scope, "RegExp.prototype.compile function's Realm must be the same to |this| RegExp object"_s);

    if (!thisRegExp->areLegacyFeaturesEnabled())
        return throwVMTypeError(globalObject, scope, "|this| RegExp object's legacy features are not enabled"_s);

    RegExp* regExp;
    JSValue arg0 = callFrame->argument(0);
    JSValue arg1 = callFrame->argument(1);
    
    if (auto* regExpObject = dynamicDowncast<RegExpObject>(arg0)) {
        if (!arg1.isUndefined())
            return throwVMTypeError(globalObject, scope, "Cannot supply flags when constructing one RegExp from another."_s);
        regExp = regExpObject->regExp();
    } else {
        String pattern = arg0.isUndefined() ? emptyString() : arg0.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        auto flags = arg1.isUndefined() ? std::make_optional(OptionSet<Yarr::Flags> { }) : Yarr::parseFlags(arg1.toWTFString(globalObject));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (!flags)
            return throwVMError(globalObject, scope, createSyntaxError(globalObject, "Invalid flags supplied to RegExp constructor."_s));

        regExp = RegExp::create(vm, pattern, flags.value());
    }

    if (!regExp->isValid())
        return throwVMError(globalObject, scope, regExp->errorToThrow(globalObject));

    globalObject->regExpRecompiledWatchpointSet().fireAll(vm, "RegExp is recompiled");

    thisRegExp->setRegExp(vm, regExp);
    scope.release();
    thisRegExp->setLastIndex(globalObject, 0);
    return JSValue::encode(thisRegExp);
}

static inline Yarr::FlagsString flagsString(JSGlobalObject* globalObject, JSObject* regexp)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    OptionSet<Yarr::Flags> flags;

#define JSC_RETRIEVE_REGEXP_FLAG(key, name, lowerCaseName, index) \
    JSValue lowerCaseName##Value = regexp->get(globalObject, vm.propertyNames->lowerCaseName); \
    RETURN_IF_EXCEPTION(scope, { }); \
    if (lowerCaseName##Value.toBoolean(globalObject)) \
        flags.add(Yarr::Flags::name);

    JSC_REGEXP_FLAGS(JSC_RETRIEVE_REGEXP_FLAG)

#undef JSC_RETRIEVE_REGEXP_FLAG

    return Yarr::flagsString(flags);
}

static ALWAYS_INLINE bool regExpFlagsWatchpointIsValid(VM& vm, RegExpObject* regExpObject)
{
    JSGlobalObject* globalObject = regExpObject->realmMayBeNull();
    if (!globalObject)
        return false;

    if (globalObject->regExpPrototype() != regExpObject->getPrototypeDirect())
        return false;

    if (globalObject->regExpPrimordialPropertiesWatchpointSet().state() != IsWatched)
        return false;

    if (!regExpObject->hasCustomProperties())
        return true;

#define JSC_CHECK_REGEXP_FLAG_PROPERTY(key, name, lowerCaseName, index) \
    if (regExpObject->getDirectOffset(vm, vm.propertyNames->lowerCaseName) != invalidOffset) \
        return false;
    JSC_REGEXP_FLAGS(JSC_CHECK_REGEXP_FLAG_PROPERTY)
#undef JSC_CHECK_REGEXP_FLAG_PROPERTY

    return true;
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope);

    JSObject* thisObject = asObject(thisValue);
    Integrity::auditStructureID(thisObject->structureID());

    StringRecursionChecker checker(globalObject, thisObject);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    JSValue sourceValue = thisObject->get(globalObject, vm.propertyNames->source);
    RETURN_IF_EXCEPTION(scope, { });
    String source = sourceValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue flagsValue = thisObject->get(globalObject, vm.propertyNames->flags);
    RETURN_IF_EXCEPTION(scope, { });
    String flags = flagsValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, '/', source, '/', flags)));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterGlobal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.global getter can only be called on a RegExp object"_s);
    }

    return JSValue::encode(jsBoolean(regexp->regExp()->global()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterHasIndices, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.hasIndices getter can only be called on a RegExp object"_s);
    }

    return JSValue::encode(jsBoolean(regexp->regExp()->hasIndices()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterIgnoreCase, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.ignoreCase getter can only be called on a RegExp object"_s);
    }

    return JSValue::encode(jsBoolean(regexp->regExp()->ignoreCase()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterMultiline, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.multiline getter can only be called on a RegExp object"_s);
    }

    return JSValue::encode(jsBoolean(regexp->regExp()->multiline()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterDotAll, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.dotAll getter can only be called on a RegExp object"_s);
    }
    
    return JSValue::encode(jsBoolean(regexp->regExp()->dotAll()));
}
    
JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterSticky, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.sticky getter can only be called on a RegExp object"_s);
    }
    
    return JSValue::encode(jsBoolean(regexp->regExp()->sticky()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterUnicode, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.unicode getter can only be called on a RegExp object"_s);
    }
    
    return JSValue::encode(jsBoolean(regexp->regExp()->unicode()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterUnicodeSets, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.unicodeSets getter can only be called on a RegExp object"_s);
    }

    return JSValue::encode(jsBoolean(regexp->regExp()->unicodeSets()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterFlags, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.flags getter can only be called on an object"_s);

    if (auto* regExpObject = dynamicDowncast<RegExpObject>(thisValue); regExpObject && regExpFlagsWatchpointIsValid(vm, regExpObject)) [[likely]]
        return JSValue::encode(jsString(vm, String::fromLatin1(Yarr::flagsString(regExpObject->regExp()->flags()).data())));

    auto flags = flagsString(globalObject, asObject(thisValue));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(jsString(vm, String::fromLatin1(flags.data())));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterSource, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = dynamicDowncast<RegExpObject>(thisValue);
    if (!regexp) [[unlikely]] {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsNontrivialString(vm, "(?:)"_s));
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.source getter can only be called on a RegExp object"_s);
    }

    return JSValue::encode(jsString(vm, regexp->regExp()->escapedPattern()));
}

JSValue regExpSearchFast(JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto strView = string->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    scope.release();
    MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExpObject->regExp(), string, strView, 0);
    return result ? jsNumber(result.start) : jsNumber(-1);
}

JSValue regExpSearchGeneric(JSGlobalObject* globalObject, JSObject* thisObject, JSString* str)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (regExpExecWatchpointIsValid(vm, thisObject)) [[likely]] {
        auto* regExp = dynamicDowncast<RegExpObject>(thisObject);
        if (!regExp) [[unlikely]] {
            throwTypeError(globalObject, scope, "Builtin RegExp exec can only be called on a RegExp object"_s);
            return { };
        }
        if (regExp->lastIndexIsWritable() && regExp->getLastIndex().isNumber()) [[likely]]
            RELEASE_AND_RETURN(scope, regExpSearchFast(globalObject, regExp, str));
    }

    auto previousLastIndex = thisObject->get(globalObject, vm.propertyNames->lastIndex);
    RETURN_IF_EXCEPTION(scope, { });

    bool isPreviousLastIndexZero = sameValue(globalObject, previousLastIndex, jsNumber(0));
    RETURN_IF_EXCEPTION(scope, { });
    if (!isPreviousLastIndexZero) {
        PutPropertySlot slot(thisObject, true);
        thisObject->methodTable()->put(thisObject, globalObject, vm.propertyNames->lastIndex, jsNumber(0), slot);
        RETURN_IF_EXCEPTION(scope, { });
    }

    JSValue match = regExpExec(globalObject, thisObject, str);
    RETURN_IF_EXCEPTION(scope, { });

    auto currentLastIndex = thisObject->get(globalObject, vm.propertyNames->lastIndex);
    RETURN_IF_EXCEPTION(scope, { });
    bool isCurrentAndPreviousLastIndexSame = sameValue(globalObject, currentLastIndex, previousLastIndex);
    RETURN_IF_EXCEPTION(scope, { });
    if (!isCurrentAndPreviousLastIndexSame) {
        PutPropertySlot slot(thisObject, true);
        thisObject->methodTable()->put(thisObject, globalObject, vm.propertyNames->lastIndex, previousLastIndex, slot);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (match.isNull())
        return jsNumber(-1);

    RELEASE_AND_RETURN(scope, match.get(globalObject, vm.propertyNames->index));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncSearch, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "RegExp.prototype.@@search requires that |this| be an Object"_s);
    JSObject* thisObject = asObject(thisValue);

    JSString* str = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(regExpSearchGeneric(globalObject, thisObject, str)));
}

enum SplitControl {
    ContinueSplit,
    AbortSplit
};

template<typename ControlFunc, typename PushFunc>
void genericSplit(
    JSGlobalObject* globalObject, RegExp* regexp, JSString* inputString, StringView input, unsigned inputSize, unsigned& position,
    unsigned& matchPosition, bool regExpIsSticky, bool regExpIsUnicode,
    const ControlFunc& control, const PushFunc& push)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    while (matchPosition < inputSize) {
        {
            auto result = control();
            RETURN_IF_EXCEPTION(scope, void());
            if (result == AbortSplit)
                return;
        }
        
        int* ovector;
        
        // a. Perform ? Set(splitter, "lastIndex", q, true).
        // b. Let z be ? RegExpExec(splitter, S).
        MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regexp, inputString, input, matchPosition, &ovector);
        int mpos = result.start;
        RETURN_IF_EXCEPTION(scope, void());

        // c. If z is null, let q be AdvanceStringIndex(S, q, unicodeMatching).
        if (mpos < 0) {
            if (!regExpIsSticky)
                break;
            matchPosition = advanceStringIndex(input, inputSize, matchPosition, regExpIsUnicode);
            continue;
        }
        if (static_cast<unsigned>(mpos) >= inputSize) {
            // The spec redoes the RegExpExec starting at the next character of the input.
            // But in our case, mpos < 0 means that the native regexp already searched all permutations
            // and know that we won't be able to find a match for the separator even if we redo the
            // RegExpExec starting at the next character of the input. So, just bail.
            break;
        }

        // d. Else, z is not null
        //    i. Let e be ? ToLength(? Get(splitter, "lastIndex")).
        //   ii. Let e be min(e, size).
        matchPosition = mpos;
        unsigned matchEnd = ovector[1];

        //  iii. If e = p, let q be AdvanceStringIndex(S, q, unicodeMatching).
        if (matchEnd == position) {
            matchPosition = advanceStringIndex(input, inputSize, matchPosition, regExpIsUnicode);
            continue;
        }
        // if matchEnd == 0 then position should also be zero and thus matchEnd should equal position.
        ASSERT(matchEnd);

        //   iv. Else e != p,
        unsigned numberOfCaptures = regexp->numSubpatterns();
        
        // 1. Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through q (exclusive).
        // 2. Perform ! CreateDataProperty(A, ! ToString(lengthA), T).
        {
            auto result = push(true, position, matchPosition - position);
            RETURN_IF_EXCEPTION(scope, void());
            if (result == AbortSplit)
                return;
        }
        
        // 5. Let p be e.
        position = matchEnd;
        
        // 6. Let numberOfCaptures be ? ToLength(? Get(z, "length")).
        // 7. Let numberOfCaptures be max(numberOfCaptures-1, 0).
        // 8. Let i be 1.
        // 9. Repeat, while i <= numberOfCaptures,
        for (unsigned i = 1; i <= numberOfCaptures; ++i) {
            // a. Let nextCapture be ? Get(z, ! ToString(i)).
            // b. Perform ! CreateDataProperty(A, ! ToString(lengthA), nextCapture).
            int sub = ovector[i * 2];
            int subEnd = ovector[i * 2 + 1];
            auto result = push(sub >= 0 && subEnd >= sub, sub, subEnd - sub);
            RETURN_IF_EXCEPTION(scope, void());
            if (result == AbortSplit)
                return;
        }
        
        // 10. Let q be p.
        matchPosition = position;
    }
}

// Fast path used by RegExp.prototype[Symbol.split] and String.prototype.split when the
// receiver is a primordial RegExpObject. Skips the species construct, custom flags, and
// custom exec — caller must guarantee non-observability via isSymbolSplitFastAndNonObservable().
// ES 22.2.5.13 RegExp.prototype[@@split](string, limit)
JSCell* regExpSplitFast(JSGlobalObject* globalObject, RegExpObject* regexpObject, JSString* inputString, unsigned limit)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regexp = regexpObject->regExp();

    auto input = inputString->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(!input->isNull());

    // Steps 1-10 are handled by the caller / inlined: the primordial species path effectively
    // constructs a sticky version of |regexpObject|. We pattern-match against the underlying
    // RegExp directly (no need to rebuild the splitter), since flags are not user-overridden.

    // 11. Let array be ! ArrayCreate(0).
    // 12. Let lengthA be 0.
    // 13. If limit is undefined, let lim be 2**32 - 1; else let lim be ℝ(? ToUint32(limit)).
    unsigned resultLength = 0;
    unsigned inputSize = input->length();
    unsigned position = 0;

    // 14. If lim = 0, return array.
    if (!limit)
        RELEASE_AND_RETURN(scope, constructEmptyArray(globalObject, nullptr));

    // 15. If string is the empty String, then
    if (input->isEmpty()) {
        // 15.a. Let matchResult be ? RegExpExec(splitter, string).
        JSArray* result = constructEmptyArray(globalObject, nullptr);
        RETURN_IF_EXCEPTION(scope, { });
        auto matchResult = globalObject->regExpGlobalData().performMatch(globalObject, regexp, inputString, input, 0);
        RETURN_IF_EXCEPTION(scope, { });

        // 15.b. If matchResult is not null, return array.
        if (matchResult)
            return result;

        // 15.c. Perform ! CreateDataPropertyOrThrow(array, "0", string).
        result->putDirectIndex(globalObject, 0, inputString);
        RETURN_IF_EXCEPTION(scope, { });

        // 15.d. Return array.
        return result;
    }

    // Fast path for newline splitting pattern: \r\n?|\n
    if (regexp->specificPattern() == Yarr::SpecificPattern::Newlines) {
        JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 1);
        if (!result) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }

        unsigned resultLength = 0;
        MatchResult lastMatchResult = MatchResult::failed();

        auto processSplit = [&](auto span) {
            while (position < inputSize && resultLength < limit) {
                auto newlinePos = WTF::findNextNewline(span, position);
                if (newlinePos.position == WTF::notFound)
                    break;

                // Record before pushing so limit-abort still reports the last match (matches genericSplit behavior).
                lastMatchResult = MatchResult(newlinePos.position, newlinePos.position + newlinePos.length);

                result->putDirectIndex(globalObject, resultLength++, jsSubstringOfResolved(vm, inputString, position, newlinePos.position - position));
                RETURN_IF_EXCEPTION(scope, AbortSplit);

                if (resultLength >= limit)
                    break;

                position = newlinePos.position + newlinePos.length;
            }
            return ContinueSplit;
        };

        if (input->is8Bit())
            processSplit(input->span8());
        else
            processSplit(input->span16());
        RETURN_IF_EXCEPTION(scope, { });

        if (lastMatchResult)
            globalObject->regExpGlobalData().recordMatch(vm, globalObject, regexp, inputString, lastMatchResult, false);

        if (resultLength >= limit)
            return result;

        result->putDirectIndex(globalObject, resultLength++, jsSubstringOfResolved(vm, inputString, position, inputSize - position));
        RETURN_IF_EXCEPTION(scope, { });

        return result;
    }

    // 16. Let size be the length of string.
    // 17. Let lastMatchEnd be 0.
    // 18. Let searchIndex be lastMatchEnd.
    // 19. Repeat, while searchIndex < size,
    unsigned matchPosition = position;
    bool regExpIsSticky = regexp->sticky();
    bool regExpIsUnicode = regexp->eitherUnicode();

    unsigned maxSizeForDirectPath = 100000;
    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 1);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    genericSplit(
        globalObject, regexp, inputString, input, inputSize, position, matchPosition, regExpIsSticky, regExpIsUnicode,
        [&] () -> SplitControl {
            if (resultLength >= maxSizeForDirectPath)
                return AbortSplit;
            return ContinueSplit;
        },
        [&] (bool isDefined, unsigned start, unsigned length) -> SplitControl {
            result->putDirectIndex(globalObject, resultLength++, isDefined ? jsSubstringOfResolved(vm, inputString, start, length) : jsUndefined());
            RETURN_IF_EXCEPTION(scope, AbortSplit);
            if (resultLength >= limit)
                return AbortSplit;
            return ContinueSplit;
        });
    RETURN_IF_EXCEPTION(scope, { });

    if (resultLength >= limit)
        return result;

    if (resultLength < maxSizeForDirectPath) {
        // 20. Let substring be the substring of string from lastMatchEnd to size.
        // 21. Perform ! CreateDataPropertyOrThrow(array, ! ToString(𝔽(lengthA)), substring).
        scope.release();
        result->putDirectIndex(globalObject, resultLength, jsSubstringOfResolved(vm, inputString, position, inputSize - position));

        // 22. Return array.
        return result;
    }

    // Now do a dry run to see how big things get. Give up if they get absurd.
    unsigned savedPosition = position;
    unsigned savedMatchPosition = matchPosition;
    unsigned dryRunCount = 0;
    genericSplit(
        globalObject, regexp, inputString, input, inputSize, position, matchPosition, regExpIsSticky, regExpIsUnicode,
        [&] () -> SplitControl {
            if (resultLength + dryRunCount > MAX_STORAGE_VECTOR_LENGTH)
                return AbortSplit;
            return ContinueSplit;
        },
        [&] (bool, unsigned, unsigned) -> SplitControl {
            dryRunCount++;
            if (resultLength + dryRunCount >= limit)
                return AbortSplit;
            return ContinueSplit;
        });
    RETURN_IF_EXCEPTION(scope, { });
    
    if (resultLength + dryRunCount > MAX_STORAGE_VECTOR_LENGTH) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    
    // OK, we know that if we finish the split, we won't have to OOM.
    position = savedPosition;
    matchPosition = savedMatchPosition;
    
    genericSplit(
        globalObject, regexp, inputString, input, inputSize, position, matchPosition, regExpIsSticky, regExpIsUnicode,
        [&] () -> SplitControl {
            return ContinueSplit;
        },
        [&] (bool isDefined, unsigned start, unsigned length) -> SplitControl {
            result->putDirectIndex(globalObject, resultLength++, isDefined ? jsSubstringOfResolved(vm, inputString, start, length) : jsUndefined());
            RETURN_IF_EXCEPTION(scope, AbortSplit);
            if (resultLength >= limit)
                return AbortSplit;
            return ContinueSplit;
        });
    RETURN_IF_EXCEPTION(scope, { });

    if (resultLength >= limit)
        return result;

    // 20. Let substring be the substring of string from lastMatchEnd to size.
    // 21. Perform ! CreateDataPropertyOrThrow(array, ! ToString(𝔽(lengthA)), substring).
    scope.release();
    result->putDirectIndex(globalObject, resultLength, jsSubstringOfResolved(vm, inputString, position, inputSize - position));

    // 22. Return array.
    return result;
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncSplit, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // https://tc39.es/ecma262/#sec-regexp.prototype-%25symbol.split%25
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let regexp be the this value.
    // 2. If regexp is not an Object, throw a TypeError exception.
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "RegExp.prototype.@@split requires that |this| be an Object"_s);
    JSObject* thisObject = asObject(thisValue);

    // 3. Set string to ? ToString(string).
    JSString* string = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue limitValue = callFrame->argument(1);

    auto* regExpObject = dynamicDowncast<RegExpObject>(thisObject);
    if (regExpObject && regExpObject->isSymbolSplitFastAndNonObservable() && (limitValue.isUndefined() || limitValue.isNumber())) [[likely]] {
        unsigned limit = 0xFFFFFFFFu;
        if (!limitValue.isUndefined()) {
            limit = limitValue.toUInt32(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
        }
        RELEASE_AND_RETURN(scope, JSValue::encode(regExpSplitFast(globalObject, regExpObject, string, limit)));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(regExpSplitSlow(globalObject, thisObject, string, limitValue)));
}

// https://tc39.es/ecma262/#sec-regexp.prototype-%symbol.split%
// Spec steps 4-22 — invoked after the C++ fast path declines, either because the
// receiver isn't a primordial RegExpObject or because some watchpoint guarding the
// fast path has been invalidated.
JSValue regExpSplitSlow(JSGlobalObject* globalObject, JSObject* thisObject, JSString* string, JSValue limitValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = thisObject;

    // 4. Let speciesCtor be ? SpeciesConstructor(regexp, %RegExp%).
    JSObject* speciesConstructor;
    {
        JSValue constructorValue = thisObject->get(globalObject, vm.propertyNames->constructor);
        RETURN_IF_EXCEPTION(scope, { });
        if (constructorValue.isUndefined())
            speciesConstructor = globalObject->regExpConstructor();
        else {
            if (!constructorValue.isObject()) [[unlikely]] {
                throwTypeError(globalObject, scope, "|this|.constructor is not an Object or undefined"_s);
                return { };
            }
            JSValue speciesValue = asObject(constructorValue)->get(globalObject, vm.propertyNames->speciesSymbol);
            RETURN_IF_EXCEPTION(scope, { });
            if (speciesValue.isUndefinedOrNull())
                speciesConstructor = globalObject->regExpConstructor();
            else {
                if (!speciesValue.isConstructor()) [[unlikely]] {
                    throwTypeError(globalObject, scope, "|this|.constructor[Symbol.species] is not a constructor"_s);
                    return { };
                }
                speciesConstructor = asObject(speciesValue);
            }
        }
    }

    // 5. Let flags be ? ToString(? Get(regexp, "flags")).
    JSValue flagsValue = thisObject->get(globalObject, vm.propertyNames->flags);
    RETURN_IF_EXCEPTION(scope, { });
    String flags = flagsValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // 6. If flags contains "u" or flags contains "v", let unicodeMatching be true.
    // 7. Else, let unicodeMatching be false.
    bool unicodeMatching = flags.contains('u') || flags.contains('v');

    // 8. If flags contains "y", let newFlags be flags.
    // 9. Else, let newFlags be the string-concatenation of flags and "y".
    String newFlags = flags.contains('y') ? flags : tryMakeString(flags, 'y');
    if (newFlags.isNull()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    // 10. Let splitter be ? Construct(speciesCtor, « regexp, newFlags »).
    MarkedArgumentBuffer constructorArgs;
    constructorArgs.append(thisValue);
    constructorArgs.append(jsString(vm, newFlags));
    ASSERT(!constructorArgs.hasOverflowed());
    auto constructData = JSC::getConstructDataInline(speciesConstructor);
    JSObject* splitter = construct(globalObject, speciesConstructor, constructData, constructorArgs);
    RETURN_IF_EXCEPTION(scope, { });

    // After Construct, re-check whether the splitter is a primordial RegExpObject with non-observable
    // side effects so RegExp subclasses (whose species is %RegExp%) still take the fast path.
    if (auto* splitterRegExp = dynamicDowncast<RegExpObject>(splitter); splitterRegExp && splitterRegExp->isSymbolSplitFastAndNonObservable() && (limitValue.isUndefined() || limitValue.isNumber())) {
        unsigned limit = 0xFFFFFFFFu;
        if (!limitValue.isUndefined()) {
            limit = limitValue.toUInt32(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
        }
        RELEASE_AND_RETURN(scope, regExpSplitFast(globalObject, splitterRegExp, string, limit));
    }

    // 11. Let array be ! ArrayCreate(0).
    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });
    uint64_t lengthA = 0;

    // 12. Let lengthA be 0.
    // 13. If limit is undefined, let lim be 2**32 - 1; else let lim be ℝ(? ToUint32(limit)).
    uint32_t lim = 0xFFFFFFFFu;
    if (!limitValue.isUndefined()) {
        lim = limitValue.toUInt32(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // 14. If lim = 0, return array.
    if (!lim)
        return result;

    // 15. If string is the empty String, then
    auto stringView = string->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    unsigned size = stringView->length();
    if (!size) {
        // 15.a. Let matchResult be ? RegExpExec(splitter, string).
        JSValue matchResult = regExpExec(globalObject, splitter, string);
        RETURN_IF_EXCEPTION(scope, { });

        // 15.b. If matchResult is not null, return array.
        if (!matchResult.isNull())
            return result;

        // 15.c. Perform ! CreateDataPropertyOrThrow(array, "0", string).
        result->putDirectIndex(globalObject, 0, string);
        RETURN_IF_EXCEPTION(scope, { });

        // 15.d. Return array.
        return result;
    }

    // 16. Let size be the length of string.
    // 17. Let lastMatchEnd be 0.
    uint64_t lastMatchEnd = 0;
    // 18. Let searchIndex be lastMatchEnd.
    uint64_t searchIndex = 0;
    // 19. Repeat, while searchIndex < size,
    while (searchIndex < size) {
        // 19.a. Perform ? Set(splitter, "lastIndex", 𝔽(searchIndex), true).
        PutPropertySlot lastIndexSlot(splitter, true);
        splitter->methodTable()->put(splitter, globalObject, vm.propertyNames->lastIndex, jsNumber(searchIndex), lastIndexSlot);
        RETURN_IF_EXCEPTION(scope, { });

        // 19.b. Let matchResult be ? RegExpExec(splitter, string).
        JSValue matchResult = regExpExec(globalObject, splitter, string);
        RETURN_IF_EXCEPTION(scope, { });

        // 19.c. If matchResult is null, then
        if (matchResult.isNull()) {
            // 19.c.i. Set searchIndex to AdvanceStringIndex(string, searchIndex, unicodeMatching).
            searchIndex = advanceStringIndex(stringView, size, searchIndex, unicodeMatching);
            continue;
        }
        // 19.d. Else,
        // 19.d.i. Let matchEnd be ℝ(? ToLength(? Get(splitter, "lastIndex"))).
        JSValue lastIndexValue = splitter->get(globalObject, vm.propertyNames->lastIndex);
        RETURN_IF_EXCEPTION(scope, { });
        uint64_t matchEnd = lastIndexValue.toLength(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        // 19.d.ii. Set matchEnd to min(matchEnd, size).
        matchEnd = std::min<uint64_t>(matchEnd, size);

        // 19.d.iii. If matchEnd = lastMatchEnd, then
        if (matchEnd == lastMatchEnd) {
            // 19.d.iii.1. Set searchIndex to AdvanceStringIndex(string, searchIndex, unicodeMatching).
            searchIndex = advanceStringIndex(stringView, size, searchIndex, unicodeMatching);
            continue;
        }
        // 19.d.iv. Else,
        // 19.d.iv.1. Let substring be the substring of string from lastMatchEnd to searchIndex.
        auto* substring = jsSubstring(globalObject, string, static_cast<unsigned>(lastMatchEnd), static_cast<unsigned>(searchIndex - lastMatchEnd));
        RETURN_IF_EXCEPTION(scope, { });

        // 19.d.iv.2. Perform ! CreateDataPropertyOrThrow(array, ! ToString(𝔽(lengthA)), substring).
        result->putDirectIndex(globalObject, lengthA, substring);
        RETURN_IF_EXCEPTION(scope, { });

        // 19.d.iv.3. Set lengthA to lengthA + 1.
        ++lengthA;

        // 19.d.iv.4. If lengthA = lim, return array.
        if (lengthA == lim)
            return result;

        // 19.d.iv.5. Set lastMatchEnd to matchEnd.
        lastMatchEnd = matchEnd;

        // 19.d.iv.6. Let numberOfCaptures be ? LengthOfArrayLike(matchResult).
        JSObject* matchObject = asObject(matchResult);
        JSValue lengthValue = matchObject->get(globalObject, vm.propertyNames->length);
        RETURN_IF_EXCEPTION(scope, { });

        uint64_t numberOfCaptures = lengthValue.toLength(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        // 19.d.iv.7. Set numberOfCaptures to max(numberOfCaptures - 1, 0).
        numberOfCaptures = numberOfCaptures > 1 ? numberOfCaptures - 1 : 0;

        // 19.d.iv.8. Let captureIndex be 1.
        // 19.d.iv.9. Repeat, while captureIndex ≤ numberOfCaptures,
        for (uint64_t i = 1; i <= numberOfCaptures; ++i) {
            // 19.d.iv.9.a. Let nextCapture be ? Get(matchResult, ! ToString(𝔽(captureIndex))).
            JSValue nextCapture = matchObject->get(globalObject, static_cast<uint64_t>(i));
            RETURN_IF_EXCEPTION(scope, { });

            // 19.d.iv.9.b. Perform ! CreateDataPropertyOrThrow(array, ! ToString(𝔽(lengthA)), nextCapture).
            result->putDirectIndex(globalObject, lengthA, nextCapture);
            RETURN_IF_EXCEPTION(scope, { });

            // 19.d.iv.9.c. Set captureIndex to captureIndex + 1.
            // 19.d.iv.9.d. Set lengthA to lengthA + 1.
            ++lengthA;

            // 19.d.iv.9.e. If lengthA = lim, return array.
            if (lengthA == lim)
                return result;
        }
        // 19.d.iv.10. Set searchIndex to lastMatchEnd.
        searchIndex = lastMatchEnd;
    }

    // 20. Let substring be the substring of string from lastMatchEnd to size.
    auto* substring = jsSubstring(globalObject, string, static_cast<unsigned>(lastMatchEnd), static_cast<unsigned>(size - lastMatchEnd));
    RETURN_IF_EXCEPTION(scope, { });

    // 21. Perform ! CreateDataPropertyOrThrow(array, ! ToString(𝔽(lengthA)), substring).
    result->putDirectIndex(globalObject, lengthA, substring);
    RETURN_IF_EXCEPTION(scope, { });

    // 22. Return array.
    return result;
}

// https://tc39.es/ecma262/#sec-getsubstitution
static inline String getSubstitution(JSGlobalObject* globalObject, const String& matched, const String& str, unsigned position, const Vector<String, 16>& captures, JSObject* namedCaptures, const String& replacement)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t start = replacement.find('$');
    if (start == notFound)
        return replacement;

    size_t matchLength = matched.length();
    size_t stringLength = str.length();
    size_t tailPos = position + matchLength;
    size_t nCaptures = captures.size();
    size_t replacementLength = replacement.length();
    StringBuilder result(OverflowPolicy::RecordOverflow); // overflow should gracefully throw an exception, not crash
    size_t lastStart = 0;

    for (; start != notFound; lastStart = start, start = replacement.find('$', lastStart)) {
        if (start > lastStart)
            result.append(StringView(replacement).substring(lastStart, start - lastStart));

        ++start;
        if (start >= replacementLength) {
            result.append('$');
            lastStart = start;
            break;
        }

        char16_t ch = replacement[start];
        switch (ch) {
        case '$':
            result.append('$');
            ++start;
            break;
        case '&':
            result.append(matched);
            ++start;
            break;
        case '`':
            if (position > 0)
                result.append(StringView(str).substring(0, position));
            ++start;
            break;
        case '\'':
            if (tailPos < stringLength)
                result.append(StringView(str).substring(tailPos));
            ++start;
            break;
        case '<': {
            if (namedCaptures) {
                unsigned groupNameStartIndex = start + 1;
                size_t groupNameEndIndex = replacement.find('>', groupNameStartIndex);
                if (groupNameEndIndex != notFound) {
                    String groupName = replacement.substring(groupNameStartIndex, groupNameEndIndex - groupNameStartIndex);
                    JSValue capture = namedCaptures->get(globalObject, Identifier::fromString(vm, groupName));
                    RETURN_IF_EXCEPTION(scope, String());
                    if (!capture.isUndefined()) {
                        String captureString = capture.toWTFString(globalObject);
                        RETURN_IF_EXCEPTION(scope, String());
                        result.append(captureString);
                    }
                    start = groupNameEndIndex + 1;
                    break;
                }
            }
            result.append("$<"_s);
            ++start;
            break;
        }
        default:
            if (isASCIIDigit(ch)) {
                unsigned originalStart = start - 1;
                ++start;

                unsigned n = ch - '0';
                if (n > nCaptures) {
                    result.append(StringView(replacement).substring(originalStart, start - originalStart));
                    break;
                }

                if (start < replacementLength) {
                    char16_t nextCh = replacement[start];
                    if (isASCIIDigit(nextCh)) {
                        unsigned nn = 10 * n + nextCh - '0';
                        if (nn <= nCaptures) {
                            n = nn;
                            ++start;
                        }
                    }
                }

                if (!n) {
                    result.append(StringView(replacement).substring(originalStart, start - originalStart));
                    break;
                }

                const String& capture = captures[n - 1];
                if (!capture.isNull())
                    result.append(capture);
            } else
                result.append('$');
            break;
        }
    }

    if (lastStart < replacementLength)
        result.append(StringView(replacement).substring(lastStart));

    if (result.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    return result.toString();
}

JSValue regExpReplaceGeneric(JSGlobalObject* globalObject, JSObject* thisObject, JSString* string, JSValue replaceValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String str = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned stringLength = str.length();

    // 4. Let lengthS be the number of code unit elements in S.
    // 5. Let functionalReplace be IsCallable(replaceValue).
    auto callData = JSC::getCallData(replaceValue);
    bool functionalReplace = callData.type != CallData::Type::None;

    // 6. If functionalReplace is false, then
    //    a. Set replaceValue to ? ToString(replaceValue).
    String replacementString;
    if (!functionalReplace) {
        replacementString = replaceValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // 7. Let flags be ? ToString(? Get(rx, "flags")).
    JSValue flagsValue = thisObject->get(globalObject, vm.propertyNames->flags);
    RETURN_IF_EXCEPTION(scope, { });
    String flags = flagsValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // 8. If flags contains "g", let global be true. Else, let global be false.
    bool global = flags.contains('g');

    // 9. If global is true, then
    //    a. If flags contains "u" or "v", let fullUnicode be true. Else, let fullUnicode be false.
    //    b. Perform ? Set(rx, "lastIndex", +0F, true).
    bool fullUnicode = false;
    if (global) {
        fullUnicode = flags.contains('u') || flags.contains('v');
        PutPropertySlot slot(thisObject, true);
        thisObject->methodTable()->put(thisObject, globalObject, vm.propertyNames->lastIndex, jsNumber(0), slot);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // 10. Let results be a new empty List.
    MarkedArgumentBuffer results;

    // 11. Let done be false.
    // 12. Repeat, while done is false,
    while (true) {
        // a. Let result be ? RegExpExec(rx, S).
        JSValue result = regExpExec(globalObject, thisObject, string);
        RETURN_IF_EXCEPTION(scope, { });

        // b. If result is null, then
        //    i. Set done to true.
        if (result.isNull())
            break;

        // c. Else,
        //    i. Append result to results.
        results.append(result);
        if (results.hasOverflowed()) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }

        //    ii. If global is false, then
        //        1. Set done to true.
        if (!global)
            break;

        //    iii. Else,
        //         1. Let matchStr be ? ToString(? Get(result, "0")).
        JSObject* resultObject = asObject(result);
        JSValue matchValue = resultObject->get(globalObject, static_cast<unsigned>(0));
        RETURN_IF_EXCEPTION(scope, { });
        String matchStr = matchValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        //         2. If matchStr is the empty String, then
        //            a. Let thisIndex be R(? ToLength(? Get(rx, "lastIndex"))).
        //            b. If flags contains "u" or flags contains "v", let fullUnicode be true; otherwise let fullUnicode be false.
        //            c. Let nextIndex be AdvanceStringIndex(S, thisIndex, fullUnicode).
        //            d. Perform ? Set(rx, "lastIndex", F(nextIndex), true).
        if (matchStr.isEmpty()) {
            JSValue lastIndexValue = thisObject->get(globalObject, vm.propertyNames->lastIndex);
            RETURN_IF_EXCEPTION(scope, { });
            uint64_t thisIndex = lastIndexValue.toLength(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            uint64_t nextIndex = advanceStringIndex(str, stringLength, thisIndex, fullUnicode);
            PutPropertySlot slot(thisObject, true);
            thisObject->methodTable()->put(thisObject, globalObject, vm.propertyNames->lastIndex, jsNumber(nextIndex), slot);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    // 13. Let accumulatedResult be the empty String.
    StringBuilder accumulatedResult(OverflowPolicy::RecordOverflow);

    // 14. Let nextSourcePosition be 0.
    unsigned nextSourcePosition = 0;

    // 15. For each element result of results, do
    for (unsigned i = 0; i < results.size(); ++i) {
        JSObject* result = asObject(results.at(i));

        // a. Let resultLength be ? LengthOfArrayLike(result).
        JSValue lengthValue = result->get(globalObject, vm.propertyNames->length);
        RETURN_IF_EXCEPTION(scope, { });
        uint64_t resultLength = lengthValue.toLength(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        // b. Let nCaptures be max(resultLength - 1, 0).
        unsigned nCaptures = resultLength > 1 ? resultLength - 1 : 0;

        // c. Let matched be ? ToString(? Get(result, "0")).
        JSValue matchedValue = result->get(globalObject, static_cast<unsigned>(0));
        RETURN_IF_EXCEPTION(scope, { });
        String matched = matchedValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        unsigned matchLength = matched.length();

        // d. Let matchLength be the number of code units in matched.
        // e. Let position be ? ToIntegerOrInfinity(? Get(result, "index")).
        JSValue positionValue = result->get(globalObject, vm.propertyNames->index);
        RETURN_IF_EXCEPTION(scope, { });
        double positionDouble = positionValue.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        // f. Set position to the result of clamping position between 0 and lengthS.
        unsigned position = static_cast<unsigned>(std::clamp<double>(positionDouble, 0, stringLength));

        // g. Let captures be a new empty List.
        Vector<String, 16> captures;
        if (!captures.tryReserveCapacity(nCaptures)) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }

        // h. Let n be 1.
        // i. Repeat, while n ≤ nCaptures,
        for (unsigned n = 1; n <= nCaptures; ++n) {
            // i. Let capN be ? Get(result, ! ToString(F(n))).
            JSValue capN = result->get(globalObject, n);
            RETURN_IF_EXCEPTION(scope, { });

            // ii. If capN is not undefined, then
            //     1. Set capN to ? ToString(capN).
            if (!capN.isUndefined()) {
                String capString = capN.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                captures.constructAndAppend(WTF::move(capString));
            } else
                captures.constructAndAppend(String());
        }

        // j. Let namedCaptures be ? Get(result, "groups").
        JSValue namedCapturesValue = result->get(globalObject, vm.propertyNames->groups);
        RETURN_IF_EXCEPTION(scope, { });

        String replacement;

        // j. If functionalReplace is true, then
        if (functionalReplace) {
            // i. Let replacerArgs be the list-concatenation of « matched », captures, and « F(position), S ».
            MarkedArgumentBuffer replacerArgs;
            replacerArgs.append(jsString(vm, matched));
            for (unsigned n = 0; n < captures.size(); ++n) {
                const String& capture = captures[n];
                if (!capture.isNull())
                    replacerArgs.append(jsString(vm, capture));
                else
                    replacerArgs.append(jsUndefined());
            }
            replacerArgs.append(jsNumber(position));
            replacerArgs.append(string);

            // ii. If namedCaptures is not undefined, then
            //     1. Append namedCaptures to replacerArgs.
            if (!namedCapturesValue.isUndefined())
                replacerArgs.append(namedCapturesValue);

            if (replacerArgs.hasOverflowed()) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return { };
            }

            // iii. Let replacementValue be ? Call(replaceValue, undefined, replacerArgs).
            JSValue replValue = call(globalObject, replaceValue, callData, jsUndefined(), replacerArgs);
            RETURN_IF_EXCEPTION(scope, { });

            // iv. Let replacementString be ? ToString(replacementValue).
            replacement = replValue.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            // k. Else,
            //    i. If namedCaptures is not undefined, then
            //       1. Set namedCaptures to ? ToObject(namedCaptures).
            JSObject* namedCaptures = nullptr;
            if (!namedCapturesValue.isUndefined()) {
                namedCaptures = namedCapturesValue.toObject(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
            }

            //    ii. Let replacementString be ? GetSubstitution(matched, S, position, captures, namedCaptures, replaceValue).
            replacement = getSubstitution(globalObject, matched, str, position, captures, namedCaptures, replacementString);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // m. If position ≥ nextSourcePosition, then
        if (position >= nextSourcePosition) {
            // i. NOTE: position should not normally move backwards. If it does, it is an indication of an
            //    ill-behaving RegExp subclass or use of an access triggered side-effect to change the global
            //    flag or other characteristics of rx. In such cases, the corresponding substitution is ignored.
            // ii. Set accumulatedResult to the string-concatenation of accumulatedResult,
            //     the substring of S from nextSourcePosition to position, and replacementString.
            accumulatedResult.append(StringView(str).substring(nextSourcePosition, position - nextSourcePosition));
            accumulatedResult.append(replacement);

            //    iii. Set nextSourcePosition to position + matchLength.
            nextSourcePosition = position + matchLength;
        }
    }

    // 16. If nextSourcePosition ≥ lengthS, return accumulatedResult.
    if (nextSourcePosition >= stringLength) {
        if (accumulatedResult.hasOverflowed()) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        return jsString(vm, accumulatedResult.toString());
    }

    // 17. Return the string-concatenation of accumulatedResult and the substring of S from nextSourcePosition.
    accumulatedResult.append(StringView(str).substring(nextSourcePosition));
    if (accumulatedResult.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    return jsString(vm, accumulatedResult.toString());
}

// 22.2.6.11 RegExp.prototype [ %Symbol.replace% ] ( string, replaceValue )
// https://tc39.es/ecma262/#sec-regexp.prototype-%25symbol.replace%25
JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncReplace, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "RegExp.prototype.@@replace requires that |this| be an Object"_s);
    JSObject* thisObject = asObject(thisValue);

    JSString* string = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(regExpReplaceGeneric(globalObject, thisObject, string, callFrame->argument(1))));
}

// https://tc39.es/ecma262/#sec-regexp.prototype-%symbol.matchall%
JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncMatchAll, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "RegExp.prototype.@@matchAll requires that |this| be an Object"_s);
    JSObject* thisObject = asObject(thisValue);

    JSString* string = callFrame->argument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto* regExpObject = dynamicDowncast<RegExpObject>(thisObject);
    if (regExpObject && regExpObject->isSymbolMatchAllFastAndNonObservable()) [[likely]] {
        RegExp* regExp = regExpObject->regExp();

        bool global = regExp->global();
        bool fullUnicode = regExp->eitherUnicode();

        double lastIndexDouble = regExpObject->getLastIndex().asNumber();
        size_t lastIndex = lastIndexDouble > 0 ? static_cast<size_t>(std::min(lastIndexDouble, maxSafeInteger())) : 0;

        Structure* structure = globalObject->regExpStructure();
        RegExpObject* matcher = RegExpObject::create(vm, structure, regExp);
        matcher->setLastIndex(globalObject, lastIndex);
        RETURN_IF_EXCEPTION(scope, { });

        auto* iterator = JSRegExpStringIterator::createWithInitialValues(vm, globalObject->regExpStringIteratorStructure());
        iterator->setRegExp(vm, matcher);
        iterator->setString(vm, string);
        iterator->setFlags(global, fullUnicode);

        return JSValue::encode(iterator);
    }

    JSValue constructorValue = thisObject->get(globalObject, vm.propertyNames->constructor);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* constructor;
    if (constructorValue.isUndefined())
        constructor = globalObject->regExpConstructor();
    else {
        if (!constructorValue.isObject()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "|this|.constructor is not an Object or undefined"_s);

        JSValue speciesValue = asObject(constructorValue)->get(globalObject, vm.propertyNames->speciesSymbol);
        RETURN_IF_EXCEPTION(scope, { });

        if (speciesValue.isUndefinedOrNull())
            constructor = globalObject->regExpConstructor();
        else {
            if (!speciesValue.isConstructor()) [[unlikely]]
                return throwVMTypeError(globalObject, scope, "|this|.constructor[Symbol.species] is not a constructor"_s);
            constructor = asObject(speciesValue);
        }
    }

    JSValue flagsValue = thisObject->get(globalObject, vm.propertyNames->flags);
    RETURN_IF_EXCEPTION(scope, { });
    String flags = flagsValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    MarkedArgumentBuffer constructorArgs;
    constructorArgs.append(thisValue);
    constructorArgs.append(jsString(vm, flags));
    ASSERT(!constructorArgs.hasOverflowed());

    auto constructData = JSC::getConstructDataInline(constructor);
    JSObject* matcher = construct(globalObject, constructor, constructData, constructorArgs);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue lastIndexValue = thisObject->get(globalObject, vm.propertyNames->lastIndex);
    RETURN_IF_EXCEPTION(scope, { });
    uint64_t lastIndex = lastIndexValue.toLength(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    PutPropertySlot slot(matcher, true);
    matcher->methodTable()->put(matcher, globalObject, vm.propertyNames->lastIndex, jsNumber(lastIndex), slot);
    RETURN_IF_EXCEPTION(scope, { });

    bool global = flags.contains('g');
    bool fullUnicode = flags.contains('u') || flags.contains('v');
    auto* regExpStringIterator = JSRegExpStringIterator::createWithInitialValues(vm, globalObject->regExpStringIteratorStructure());

    regExpStringIterator->setRegExp(vm, matcher);
    regExpStringIterator->setString(vm, string);
    regExpStringIterator->setFlags(global, fullUnicode);

    return JSValue::encode(regExpStringIterator);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
