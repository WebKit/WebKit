/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All Rights Reserved.
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

#include "IntegrityInlines.h"
#include "JSArray.h"
#include "JSCBuiltins.h"
#include "JSCJSValue.h"
#include "JSGlobalObject.h"
#include "JSStringInlines.h"
#include "RegExpObject.h"
#include "RegExpObjectInlines.h"
#include "StringRecursionChecker.h"
#include "YarrFlags.h"
#include <wtf/text/StringBuilder.h>

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
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterSource);
static JSC_DECLARE_HOST_FUNCTION(regExpProtoGetterFlags);

const ClassInfo RegExpPrototype::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RegExpPrototype) };

RegExpPrototype::RegExpPrototype(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void RegExpPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->compile, regExpProtoFuncCompile, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->exec, regExpProtoFuncExec, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, RegExpExecIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, regExpProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->global, regExpProtoGetterGlobal, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->dotAll, regExpProtoGetterDotAll, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->hasIndices, regExpProtoGetterHasIndices, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->ignoreCase, regExpProtoGetterIgnoreCase, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->multiline, regExpProtoGetterMultiline, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->sticky, regExpProtoGetterSticky, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->unicode, regExpProtoGetterUnicode, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->source, regExpProtoGetterSource, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->flags, regExpProtoGetterFlags, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->matchSymbol, regExpPrototypeMatchCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->matchAllSymbol, regExpPrototypeMatchAllCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->replaceSymbol, regExpPrototypeReplaceCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->searchSymbol, regExpPrototypeSearchCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->splitSymbol, regExpPrototypeSplitCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->test, regExpPrototypeTestCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// ------------------------------ Functions ---------------------------

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncTestFast, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp))
        return throwVMTypeError(globalObject, scope);
    JSString* string = callFrame->argument(0).toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !string);
    if (!string)
        return JSValue::encode(jsUndefined());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(regexp->test(globalObject, string))));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncExec, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp))
        return throwVMTypeError(globalObject, scope, "Builtin RegExp exec can only be called on a RegExp object");
    JSString* string = callFrame->argument(0).toStringOrNull(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !string);
    if (!string)
        return JSValue::encode(jsUndefined());
    RELEASE_AND_RETURN(scope, JSValue::encode(regexp->exec(globalObject, string)));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncMatchFast, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    RegExpObject* thisObject = jsCast<RegExpObject*>(callFrame->thisValue());
    JSString* string = jsCast<JSString*>(callFrame->uncheckedArgument(0));
    if (!thisObject->regExp()->global())
        return JSValue::encode(thisObject->exec(globalObject, string));
    return JSValue::encode(thisObject->matchGlobal(globalObject, string));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncCompile, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* thisRegExp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!thisRegExp))
        return throwVMTypeError(globalObject, scope);

    RegExp* regExp;
    JSValue arg0 = callFrame->argument(0);
    JSValue arg1 = callFrame->argument(1);
    
    if (auto* regExpObject = jsDynamicCast<RegExpObject*>(vm, arg0)) {
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

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope);

    JSObject* thisObject = asObject(thisValue);
    Integrity::auditStructureID(vm, thisObject->structureID());

    StringRecursionChecker checker(globalObject, thisObject);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    JSValue sourceValue = thisObject->get(globalObject, vm.propertyNames->source);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String source = sourceValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue flagsValue = thisObject->get(globalObject, vm.propertyNames->flags);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String flags = flagsValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, '/', source, '/', flags)));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterGlobal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
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
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
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
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
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
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
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
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
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
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
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
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.unicode getter can only be called on a RegExp object"_s);
    }
    
    return JSValue::encode(jsBoolean(regexp->regExp()->unicode()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterFlags, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (UNLIKELY(!thisValue.isObject()))
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.flags getter can only be called on an object"_s);

    auto flags = flagsString(globalObject, asObject(thisValue));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(jsString(vm, flags.data()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoGetterSource, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, thisValue);
    if (UNLIKELY(!regexp)) {
        if (thisValue == globalObject->regExpPrototype())
            return JSValue::encode(jsNontrivialString(vm, "(?:)"_s));
        return throwVMTypeError(globalObject, scope, "The RegExp.prototype.source getter can only be called on a RegExp object"_s);
    }

    return JSValue::encode(jsString(vm, regexp->regExp()->escapedPattern()));
}

JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncSearchFast, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    RegExp* regExp = jsCast<RegExpObject*>(thisValue)->regExp();

    JSString* string = callFrame->uncheckedArgument(0).toString(globalObject);
    String s = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, s, 0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(result ? jsNumber(result.start) : jsNumber(-1));
}

static inline unsigned advanceStringIndex(String str, unsigned strSize, unsigned index, bool isUnicode)
{
    if (!isUnicode)
        return ++index;
    return advanceStringUnicode(str, strSize, index);
}

enum SplitControl {
    ContinueSplit,
    AbortSplit
};

template<typename ControlFunc, typename PushFunc>
void genericSplit(
    JSGlobalObject* globalObject, RegExp* regexp, const String& input, unsigned inputSize, unsigned& position,
    unsigned& matchPosition, bool regExpIsSticky, bool regExpIsUnicode,
    const ControlFunc& control, const PushFunc& push)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Vector<int> ovector;
        
    while (matchPosition < inputSize) {
        {
            auto result = control();
            RETURN_IF_EXCEPTION(scope, void());
            if (result == AbortSplit)
                return;
        }
        
        ovector.shrink(0);
        
        // a. Perform ? Set(splitter, "lastIndex", q, true).
        // b. Let z be ? RegExpExec(splitter, S).
        int mpos = regexp->match(globalObject, input, matchPosition, ovector);
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
            auto result = push(sub >= 0, sub, ovector[i * 2 + 1] - sub);
            RETURN_IF_EXCEPTION(scope, void());
            if (result == AbortSplit)
                return;
        }
        
        // 10. Let q be p.
        matchPosition = position;
    }
}

// ES 21.2.5.11 RegExp.prototype[@@split](string, limit)
JSC_DEFINE_HOST_FUNCTION(regExpProtoFuncSplitFast, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. [handled by JS builtin] Let rx be the this value.
    // 2. [handled by JS builtin] If Type(rx) is not Object, throw a TypeError exception.
    JSValue thisValue = callFrame->thisValue();
    RegExp* regexp = jsCast<RegExpObject*>(thisValue)->regExp();

    // 3. [handled by JS builtin] Let S be ? ToString(string).
    JSString* inputString = callFrame->argument(0).toString(globalObject);
    String input = inputString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    ASSERT(!input.isNull());

    // 4. [handled by JS builtin] Let C be ? SpeciesConstructor(rx, %RegExp%).
    // 5. [handled by JS builtin] Let flags be ? ToString(? Get(rx, "flags")).
    // 6. [handled by JS builtin] If flags contains "u", let unicodeMatching be true.
    // 7. [handled by JS builtin] Else, let unicodeMatching be false.
    // 8. [handled by JS builtin] If flags contains "y", let newFlags be flags.
    // 9. [handled by JS builtin] Else, let newFlags be the string that is the concatenation of flags and "y".
    // 10. [handled by JS builtin] Let splitter be ? Construct(C, « rx, newFlags »).

    // 11. Let A be ArrayCreate(0).
    // 12. Let lengthA be 0.
    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    unsigned resultLength = 0;

    // 13. If limit is undefined, let lim be 2^32-1; else let lim be ? ToUint32(limit).
    JSValue limitValue = callFrame->argument(1);
    unsigned limit = limitValue.isUndefined() ? 0xFFFFFFFFu : limitValue.toUInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 14. Let size be the number of elements in S.
    unsigned inputSize = input.length();

    // 15. Let p = 0.
    unsigned position = 0;

    // 16. If lim == 0, return A.
    if (!limit)
        return JSValue::encode(result);

    // 17. If size == 0, then
    if (input.isEmpty()) {
        // a. Let z be ? RegExpExec(splitter, S).
        // b. If z is not null, return A.
        // c. Perform ! CreateDataProperty(A, "0", S).
        // d. Return A.
        auto matchResult = regexp->match(globalObject, input, 0);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (!matchResult) {
            result->putDirectIndex(globalObject, 0, inputString);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        }
        return JSValue::encode(result);
    }

    // 18. Let q = p.
    unsigned matchPosition = position;
    // 19. Repeat, while q < size
    bool regExpIsSticky = regexp->sticky();
    bool regExpIsUnicode = regexp->unicode();
    
    unsigned maxSizeForDirectPath = 100000;
    
    genericSplit(
        globalObject, regexp, input, inputSize, position, matchPosition, regExpIsSticky, regExpIsUnicode,
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
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (resultLength >= limit)
        return JSValue::encode(result);
    if (resultLength < maxSizeForDirectPath) {
        // 20. Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through size (exclusive).
        // 21. Perform ! CreateDataProperty(A, ! ToString(lengthA), T).
        scope.release();
        result->putDirectIndex(globalObject, resultLength, jsSubstringOfResolved(vm, inputString, position, inputSize - position));
        
        // 22. Return A.
        return JSValue::encode(result);
    }
    
    // Now do a dry run to see how big things get. Give up if they get absurd.
    unsigned savedPosition = position;
    unsigned savedMatchPosition = matchPosition;
    unsigned dryRunCount = 0;
    genericSplit(
        globalObject, regexp, input, inputSize, position, matchPosition, regExpIsSticky, regExpIsUnicode,
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
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    if (resultLength + dryRunCount > MAX_STORAGE_VECTOR_LENGTH) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }
    
    // OK, we know that if we finish the split, we won't have to OOM.
    position = savedPosition;
    matchPosition = savedMatchPosition;
    
    genericSplit(
        globalObject, regexp, input, inputSize, position, matchPosition, regExpIsSticky, regExpIsUnicode,
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
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (resultLength >= limit)
        return JSValue::encode(result);
    
    // 20. Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through size (exclusive).
    // 21. Perform ! CreateDataProperty(A, ! ToString(lengthA), T).
    scope.release();
    result->putDirectIndex(globalObject, resultLength, jsSubstringOfResolved(vm, inputString, position, inputSize - position));
    // 22. Return A.
    return JSValue::encode(result);
}

} // namespace JSC
