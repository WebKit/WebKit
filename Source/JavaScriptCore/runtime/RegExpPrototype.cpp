/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007-2008, 2016 Apple Inc. All Rights Reserved.
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

#include "ArrayPrototype.h"
#include "BuiltinNames.h"
#include "Error.h"
#include "JSArray.h"
#include "JSCBuiltins.h"
#include "JSCJSValue.h"
#include "JSFunction.h"
#include "JSObject.h"
#include "JSString.h"
#include "JSStringBuilder.h"
#include "Lexer.h"
#include "ObjectPrototype.h"
#include "JSCInlines.h"
#include "RegExpObject.h"
#include "RegExp.h"
#include "RegExpCache.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "StringObject.h"
#include "StringRecursionChecker.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL regExpProtoFuncTest(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoFuncExec(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoFuncCompile(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoFuncToString(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoGetterGlobal(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoGetterIgnoreCase(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoGetterMultiline(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoGetterSticky(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoGetterUnicode(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoGetterSource(ExecState*);
static EncodedJSValue JSC_HOST_CALL regExpProtoGetterFlags(ExecState*);

const ClassInfo RegExpPrototype::s_info = { "Object", &Base::s_info, 0, CREATE_METHOD_TABLE(RegExpPrototype) };

RegExpPrototype::RegExpPrototype(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void RegExpPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->compile, regExpProtoFuncCompile, DontEnum, 2);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->exec, regExpProtoFuncExec, DontEnum, 1, RegExpExecIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->test, regExpProtoFuncTest, DontEnum, 1, RegExpTestIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, regExpProtoFuncToString, DontEnum, 0);
    JSC_NATIVE_GETTER(vm.propertyNames->global, regExpProtoGetterGlobal, DontEnum | Accessor);
    JSC_NATIVE_GETTER(vm.propertyNames->ignoreCase, regExpProtoGetterIgnoreCase, DontEnum | Accessor);
    JSC_NATIVE_GETTER(vm.propertyNames->multiline, regExpProtoGetterMultiline, DontEnum | Accessor);
    JSC_NATIVE_GETTER(vm.propertyNames->sticky, regExpProtoGetterSticky, DontEnum | Accessor);
    JSC_NATIVE_GETTER(vm.propertyNames->unicode, regExpProtoGetterUnicode, DontEnum | Accessor);
    JSC_NATIVE_GETTER(vm.propertyNames->source, regExpProtoGetterSource, DontEnum | Accessor);
    JSC_NATIVE_GETTER(vm.propertyNames->flags, regExpProtoGetterFlags, DontEnum | Accessor);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->matchSymbol, regExpPrototypeMatchCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->searchSymbol, regExpPrototypeSearchCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->splitSymbol, regExpPrototypeSplitCodeGenerator, DontEnum);

    m_emptyRegExp.set(vm, this, RegExp::create(vm, "", NoFlags));
}

void RegExpPrototype::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    RegExpPrototype* thisObject = jsCast<RegExpPrototype*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    
    visitor.append(&thisObject->m_emptyRegExp);
}

// ------------------------------ Functions ---------------------------

EncodedJSValue JSC_HOST_CALL regExpProtoFuncTest(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.inherits(RegExpObject::info()))
        return throwVMTypeError(exec);
    JSString* string = exec->argument(0).toStringOrNull(exec);
    if (!string)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(asRegExpObject(thisValue)->test(exec, exec->lexicalGlobalObject(), string)));
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncExec(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.inherits(RegExpObject::info()))
        return throwVMTypeError(exec, "Builtin RegExp exec can only be called on a RegExp object");
    JSString* string = exec->argument(0).toStringOrNull(exec);
    if (!string)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(asRegExpObject(thisValue)->exec(exec, exec->lexicalGlobalObject(), string));
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncMatchFast(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.inherits(RegExpObject::info()))
        return throwVMTypeError(exec);
    JSString* string = exec->argument(0).toStringOrNull(exec);
    if (!string)
        return JSValue::encode(jsUndefined());
    if (!asRegExpObject(thisValue)->regExp()->global())
        return JSValue::encode(asRegExpObject(thisValue)->exec(exec, exec->lexicalGlobalObject(), string));
    return JSValue::encode(asRegExpObject(thisValue)->matchGlobal(exec, exec->lexicalGlobalObject(), string));
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncCompile(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.inherits(RegExpObject::info()))
        return throwVMTypeError(exec);

    RegExp* regExp;
    JSValue arg0 = exec->argument(0);
    JSValue arg1 = exec->argument(1);
    
    if (arg0.inherits(RegExpObject::info())) {
        if (!arg1.isUndefined())
            return throwVMError(exec, createTypeError(exec, ASCIILiteral("Cannot supply flags when constructing one RegExp from another.")));
        regExp = asRegExpObject(arg0)->regExp();
    } else {
        String pattern = !exec->argumentCount() ? emptyString() : arg0.toString(exec)->value(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        RegExpFlags flags = NoFlags;
        if (!arg1.isUndefined()) {
            flags = regExpFlags(arg1.toString(exec)->value(exec));
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
            if (flags == InvalidFlags)
                return throwVMError(exec, createSyntaxError(exec, ASCIILiteral("Invalid flags supplied to RegExp constructor.")));
        }
        regExp = RegExp::create(exec->vm(), pattern, flags);
    }

    if (!regExp->isValid())
        return throwVMError(exec, createSyntaxError(exec, regExp->errorMessage()));

    asRegExpObject(thisValue)->setRegExp(exec->vm(), regExp);
    asRegExpObject(thisValue)->setLastIndex(exec, 0);
    return JSValue::encode(jsUndefined());
}

typedef std::array<char, 5 + 1> FlagsString; // 5 different flags and a null character terminator.

static inline FlagsString flagsString(ExecState* exec, JSObject* regexp)
{
    FlagsString string;

    VM& vm = exec->vm();

    JSValue globalValue = regexp->get(exec, exec->propertyNames().global);
    if (vm.exception())
        return string;
    JSValue ignoreCaseValue = regexp->get(exec, exec->propertyNames().ignoreCase);
    if (vm.exception())
        return string;
    JSValue multilineValue = regexp->get(exec, exec->propertyNames().multiline);
    if (vm.exception())
        return string;
    JSValue unicodeValue = regexp->get(exec, exec->propertyNames().unicode);
    if (vm.exception())
        return string;
    JSValue stickyValue = regexp->get(exec, exec->propertyNames().sticky);
    if (vm.exception())
        return string;

    unsigned index = 0;
    if (globalValue.toBoolean(exec))
        string[index++] = 'g';
    if (ignoreCaseValue.toBoolean(exec))
        string[index++] = 'i';
    if (multilineValue.toBoolean(exec))
        string[index++] = 'm';
    if (unicodeValue.toBoolean(exec))
        string[index++] = 'u';
    if (stickyValue.toBoolean(exec))
        string[index++] = 'y';
    ASSERT(index < string.size());
    string[index] = 0;
    return string;
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncToString(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(exec);

    JSObject* thisObject = asObject(thisValue);

    StringRecursionChecker checker(exec, thisObject);
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    VM& vm = exec->vm();
    JSValue sourceValue = thisObject->get(exec, vm.propertyNames->source);
    if (vm.exception())
        return JSValue::encode(jsUndefined());
    String source = sourceValue.toString(exec)->value(exec);
    if (vm.exception())
        return JSValue::encode(jsUndefined());

    JSValue flagsValue = thisObject->get(exec, vm.propertyNames->flags);
    if (vm.exception())
        return JSValue::encode(jsUndefined());
    String flags = flagsValue.toString(exec)->value(exec);
    if (vm.exception())
        return JSValue::encode(jsUndefined());

    return JSValue::encode(jsMakeNontrivialString(exec, '/', source, '/', flags));
}

EncodedJSValue JSC_HOST_CALL regExpProtoGetterGlobal(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (UNLIKELY(!thisValue.inherits(RegExpObject::info()))) {
        if (thisValue.inherits(RegExpPrototype::info()))
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(exec, ASCIILiteral("The RegExp.prototype.global getter can only be called on a RegExp object"));
    }

    return JSValue::encode(jsBoolean(asRegExpObject(thisValue)->regExp()->global()));
}

EncodedJSValue JSC_HOST_CALL regExpProtoGetterIgnoreCase(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (UNLIKELY(!thisValue.inherits(RegExpObject::info()))) {
        if (thisValue.inherits(RegExpPrototype::info()))
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(exec, ASCIILiteral("The RegExp.prototype.ignoreCase getter can only be called on a RegExp object"));
    }

    return JSValue::encode(jsBoolean(asRegExpObject(thisValue)->regExp()->ignoreCase()));
}

EncodedJSValue JSC_HOST_CALL regExpProtoGetterMultiline(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (UNLIKELY(!thisValue.inherits(RegExpObject::info()))) {
        if (thisValue.inherits(RegExpPrototype::info()))
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(exec, ASCIILiteral("The RegExp.prototype.multiline getter can only be called on a RegExp object"));
    }

    return JSValue::encode(jsBoolean(asRegExpObject(thisValue)->regExp()->multiline()));
}

EncodedJSValue JSC_HOST_CALL regExpProtoGetterSticky(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (UNLIKELY(!thisValue.inherits(RegExpObject::info()))) {
        if (thisValue.inherits(RegExpPrototype::info()))
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(exec, ASCIILiteral("The RegExp.prototype.sticky getter can only be called on a RegExp object"));
    }
    
    return JSValue::encode(jsBoolean(asRegExpObject(thisValue)->regExp()->sticky()));
}

EncodedJSValue JSC_HOST_CALL regExpProtoGetterUnicode(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (UNLIKELY(!thisValue.inherits(RegExpObject::info()))) {
        if (thisValue.inherits(RegExpPrototype::info()))
            return JSValue::encode(jsUndefined());
        return throwVMTypeError(exec, ASCIILiteral("The RegExp.prototype.unicode getter can only be called on a RegExp object"));
    }
    
    return JSValue::encode(jsBoolean(asRegExpObject(thisValue)->regExp()->unicode()));
}

EncodedJSValue JSC_HOST_CALL regExpProtoGetterFlags(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (UNLIKELY(!thisValue.isObject()))
        return throwVMTypeError(exec, ASCIILiteral("The RegExp.prototype.flags getter can only be called on an object"));

    auto flags = flagsString(exec, asObject(thisValue));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    return JSValue::encode(jsString(exec, flags.data()));
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
static inline JSValue regExpProtoGetterSourceInternal(ExecState* exec, const String& pattern, const CharacterType* characters, unsigned length)
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

EncodedJSValue JSC_HOST_CALL regExpProtoGetterSource(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (UNLIKELY(!thisValue.inherits(RegExpObject::info()))) {
        if (thisValue.inherits(RegExpPrototype::info()))
            return JSValue::encode(jsString(exec, ASCIILiteral("(?:)")));
        return throwVMTypeError(exec, ASCIILiteral("The RegExp.prototype.source getter can only be called on a RegExp object"));
    }

    String pattern = asRegExpObject(thisValue)->regExp()->pattern();
    if (pattern.is8Bit())
        return JSValue::encode(regExpProtoGetterSourceInternal(exec, pattern, pattern.characters8(), pattern.length()));
    return JSValue::encode(regExpProtoGetterSourceInternal(exec, pattern, pattern.characters16(), pattern.length()));
}

EncodedJSValue JSC_HOST_CALL regExpProtoFuncSearchFast(ExecState* exec)
{
    VM& vm = exec->vm();
    JSValue thisValue = exec->thisValue();
    RegExp* regExp = asRegExpObject(thisValue)->regExp();

    JSString* string = exec->uncheckedArgument(0).toString(exec);
    String s = string->value(exec);
    if (vm.exception())
        return JSValue::encode(jsUndefined());

    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    MatchResult result = regExpConstructor->performMatch(vm, regExp, string, s, 0);
    return JSValue::encode(result ? jsNumber(result.start) : jsNumber(-1));
}

static inline unsigned advanceStringIndex(String str, unsigned strSize, unsigned index, bool isUnicode)
{
    if (!isUnicode)
        return ++index;
    return RegExpObject::advanceStringUnicode(str, strSize, index);
}

// ES 21.2.5.11 RegExp.prototype[@@split](string, limit)
EncodedJSValue JSC_HOST_CALL regExpProtoFuncSplitFast(ExecState* exec)
{
    VM& vm = exec->vm();

    // 1. [handled by JS builtin] Let rx be the this value.
    // 2. [handled by JS builtin] If Type(rx) is not Object, throw a TypeError exception.
    JSValue thisValue = exec->thisValue();
    RegExp* regexp = asRegExpObject(thisValue)->regExp();

    // 3. [handled by JS builtin] Let S be ? ToString(string).
    String input = exec->argument(0).toString(exec)->value(exec);
    if (vm.exception())
        return JSValue::encode(jsUndefined());
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
    JSArray* result = constructEmptyArray(exec, 0);
    unsigned resultLength = 0;

    // 13. If limit is undefined, let lim be 2^32-1; else let lim be ? ToUint32(limit).
    JSValue limitValue = exec->argument(1);
    unsigned limit = limitValue.isUndefined() ? 0xFFFFFFFFu : limitValue.toUInt32(exec);

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
        if (!regexp->match(vm, input, 0))
            result->putDirectIndex(exec, 0, jsStringWithReuse(exec, thisValue, input));
        return JSValue::encode(result);
    }

    // 18. Let q = p.
    unsigned matchPosition = position;
    // 19. Repeat, while q < size
    bool regExpIsSticky = regexp->sticky();
    bool regExpIsUnicode = regexp->unicode();
    while (matchPosition < inputSize) {
        Vector<int, 32> ovector;

        // a. Perform ? Set(splitter, "lastIndex", q, true).
        // b. Let z be ? RegExpExec(splitter, S).
        int mpos = regexp->match(vm, input, matchPosition, ovector);

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
        {
            unsigned numberOfCaptures = regexp->numSubpatterns();
            unsigned newResultLength = resultLength + numberOfCaptures + 1;
            if (newResultLength < numberOfCaptures || newResultLength >= MAX_STORAGE_VECTOR_INDEX) {
                // Let's consider what's best for users here. We're about to increase the length of
                // the split array beyond the maximum length that we can support efficiently. This
                // will cause us to use a HashMap for the new entries after this point. That's going
                // to result in a very long running time of this function and very large memory
                // usage. In my experiments, JSC will sit spinning for minutes after getting here and
                // it was using >4GB of memory and eventually grew to 8GB. It kept running without
                // finishing until I killed it. That's probably not what the user wanted. The user,
                // or the program that the user is running, probably made a mistake by calling this
                // method in such a way that it resulted in such an obnoxious array. Therefore, to
                // protect ourselves, we bail at this point.
                throwOutOfMemoryError(exec);
                return JSValue::encode(jsUndefined());
            }

            // 1. Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through q (exclusive).
            // 2. Perform ! CreateDataProperty(A, ! ToString(lengthA), T).
            result->putDirectIndex(exec, resultLength, jsSubstring(exec, thisValue, input, position, matchPosition - position));

            // 3. Let lengthA be lengthA + 1.
            // 4. If lengthA = lim, return A.
            if (++resultLength == limit)
                return JSValue::encode(result);

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
                result->putDirectIndex(exec, resultLength, sub < 0 ? jsUndefined() : jsSubstring(exec, thisValue, input, sub, ovector[i * 2 + 1] - sub));

                // c. Let i be i + 1.
                // d. Let lengthA be lengthA + 1.
                // e. If lengthA = lim, return A.
                if (++resultLength == limit)
                    return JSValue::encode(result);
            }

            // 10. Let q be p.
            matchPosition = position;
        }
    }

    // 20. Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through size (exclusive).
    // 21. Perform ! CreateDataProperty(A, ! ToString(lengthA), T).
    result->putDirectIndex(exec, resultLength++, jsSubstring(exec, thisValue, input, position, inputSize - position));

    // 22. Return A.
    return JSValue::encode(result);
}

} // namespace JSC
