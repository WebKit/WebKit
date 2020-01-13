/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2009 Torch Mobile, Inc.
 *  Copyright (C) 2015 Jordan Harband (ljharb@gmail.com)
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
#include "StringPrototype.h"

#include "BuiltinNames.h"
#include "ButterflyInlines.h"
#include "CachedCall.h"
#include "Error.h"
#include "FrameTracers.h"
#include "InterpreterInlines.h"
#include "IntlCollator.h"
#include "IntlObject.h"
#include "JITCodeInlines.h"
#include "JSArray.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSStringIterator.h"
#include "Lookup.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "ParseInt.h"
#include "PropertyNameArray.h"
#include "RegExpCache.h"
#include "RegExpConstructor.h"
#include "RegExpGlobalDataInlines.h"
#include "StringPrototypeInlines.h"
#include "SuperSampler.h"
#include <algorithm>
#include <unicode/uconfig.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>
#include <wtf/unicode/Collator.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(StringPrototype);

EncodedJSValue JSC_HOST_CALL stringProtoFuncToString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncCharAt(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncCharCodeAt(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncCodePointAt(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncIndexOf(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncLastIndexOf(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncReplaceUsingRegExp(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncReplaceUsingStringSearch(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncReplaceAllUsingStringSearch(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSlice(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstr(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstring(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToLowerCase(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToUpperCase(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncLocaleCompare(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleLowerCase(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleUpperCase(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncTrim(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimStart(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimEnd(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncStartsWith(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncEndsWith(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncIncludes(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncNormalize(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncIterator(JSGlobalObject*, CallFrame*);

}

#include "StringPrototype.lut.h"

namespace JSC {

const ClassInfo StringPrototype::s_info = { "String", &StringObject::s_info, &stringPrototypeTable, nullptr, CREATE_METHOD_TABLE(StringPrototype) };

/* Source for StringConstructor.lut.h
@begin stringPrototypeTable
    concat        JSBuiltin    DontEnum|Function 1
    match         JSBuiltin    DontEnum|Function 1
    matchAll      JSBuiltin    DontEnum|Function 1
    padStart      JSBuiltin    DontEnum|Function 1
    padEnd        JSBuiltin    DontEnum|Function 1
    repeat        JSBuiltin    DontEnum|Function 1
    replace       JSBuiltin    DontEnum|Function 2
    replaceAll    JSBuiltin    DontEnum|Function 2
    search        JSBuiltin    DontEnum|Function 1
    split         JSBuiltin    DontEnum|Function 1
    anchor        JSBuiltin    DontEnum|Function 1
    big           JSBuiltin    DontEnum|Function 0
    bold          JSBuiltin    DontEnum|Function 0
    blink         JSBuiltin    DontEnum|Function 0
    fixed         JSBuiltin    DontEnum|Function 0
    fontcolor     JSBuiltin    DontEnum|Function 1
    fontsize      JSBuiltin    DontEnum|Function 1
    italics       JSBuiltin    DontEnum|Function 0
    link          JSBuiltin    DontEnum|Function 1
    small         JSBuiltin    DontEnum|Function 0
    strike        JSBuiltin    DontEnum|Function 0
    sub           JSBuiltin    DontEnum|Function 0
    sup           JSBuiltin    DontEnum|Function 0
@end
*/

// ECMA 15.5.4
StringPrototype::StringPrototype(VM& vm, Structure* structure)
    : StringObject(vm, structure)
{
}

void StringPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject, JSString* nameAndMessage)
{
    Base::finishCreation(vm, nameAndMessage);
    ASSERT(inherits(vm, info()));

    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, stringProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, StringPrototypeValueOfIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->valueOf, stringProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, StringPrototypeValueOfIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("charAt", stringProtoFuncCharAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, CharAtIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("charCodeAt", stringProtoFuncCharCodeAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, CharCodeAtIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("codePointAt", stringProtoFuncCodePointAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, StringPrototypeCodePointAtIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("indexOf", stringProtoFuncIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf", stringProtoFuncLastIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().replaceUsingRegExpPrivateName(), stringProtoFuncReplaceUsingRegExp, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, StringPrototypeReplaceRegExpIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().replaceUsingStringSearchPrivateName(), stringProtoFuncReplaceUsingStringSearch, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().replaceAllUsingStringSearchPrivateName(), stringProtoFuncReplaceAllUsingStringSearch, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("slice", stringProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, StringPrototypeSliceIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("substr", stringProtoFuncSubstr, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("substring", stringProtoFuncSubstring, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("toLowerCase", stringProtoFuncToLowerCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, StringPrototypeToLowerCaseIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toUpperCase", stringProtoFuncToUpperCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("localeCompare", stringProtoFuncLocaleCompare, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
#if ENABLE(INTL)
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toLocaleLowerCase", stringProtoFuncToLocaleLowerCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toLocaleUpperCase", stringProtoFuncToLocaleUpperCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
#else
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toLocaleLowerCase", stringProtoFuncToLowerCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toLocaleUpperCase", stringProtoFuncToUpperCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
#endif
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("trim", stringProtoFuncTrim, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("startsWith", stringProtoFuncStartsWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("endsWith", stringProtoFuncEndsWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("includes", stringProtoFuncIncludes, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("normalize", stringProtoFuncNormalize, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().charCodeAtPrivateName(), stringProtoFuncCharCodeAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, CharCodeAtIntrinsic);

    JSFunction* trimStartFunction = JSFunction::create(vm, globalObject, 0, "trimStart"_s, stringProtoFuncTrimStart, NoIntrinsic);
    JSFunction* trimEndFunction = JSFunction::create(vm, globalObject, 0, "trimEnd"_s, stringProtoFuncTrimEnd, NoIntrinsic);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimStart"), trimStartFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimLeft"), trimStartFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimEnd"), trimEndFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimRight"), trimEndFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* iteratorFunction = JSFunction::create(vm, globalObject, 0, "[Symbol.iterator]"_s, stringProtoFuncIterator, NoIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, iteratorFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    // The constructor will be added later, after StringConstructor has been built
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

StringPrototype* StringPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSString* empty = jsEmptyString(vm);
    StringPrototype* prototype = new (NotNull, allocateCell<StringPrototype>(vm.heap)) StringPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject, empty);
    return prototype;
}

// ------------------------------ Functions --------------------------

static NEVER_INLINE void substituteBackreferencesSlow(StringBuilder& result, StringView replacement, StringView source, const int* ovector, RegExp* reg, size_t i)
{
    bool hasNamedCaptures = reg && reg->hasNamedCaptures();
    int offset = 0;
    do {
        if (i + 1 == replacement.length())
            break;

        UChar ref = replacement[i + 1];
        if (ref == '$') {
            // "$$" -> "$"
            ++i;
            result.append(replacement.substring(offset, i - offset));
            offset = i + 1;
            continue;
        }

        int backrefStart;
        int backrefLength;
        int advance = 0;
        if (ref == '&') {
            backrefStart = ovector[0];
            backrefLength = ovector[1] - backrefStart;
        } else if (ref == '`') {
            backrefStart = 0;
            backrefLength = ovector[0];
        } else if (ref == '\'') {
            backrefStart = ovector[1];
            backrefLength = source.length() - backrefStart;
        } else if (reg && ref == '<') {
            // Named back reference
            if (!hasNamedCaptures)
                continue;

            size_t closingBracket = replacement.find('>', i + 2);
            if (closingBracket == WTF::notFound)
                continue;

            unsigned nameLength = closingBracket - i - 2;
            unsigned backrefIndex = reg->subpatternForName(replacement.substring(i + 2, nameLength).toString());

            if (!backrefIndex || backrefIndex > reg->numSubpatterns()) {
                backrefStart = 0;
                backrefLength = 0;
            } else {
                backrefStart = ovector[2 * backrefIndex];
                backrefLength = ovector[2 * backrefIndex + 1] - backrefStart;
            }
            advance = nameLength + 1;
        } else if (reg && isASCIIDigit(ref)) {
            // 1- and 2-digit back references are allowed
            unsigned backrefIndex = ref - '0';
            if (backrefIndex > reg->numSubpatterns())
                continue;
            if (replacement.length() > i + 2) {
                ref = replacement[i + 2];
                if (isASCIIDigit(ref)) {
                    backrefIndex = 10 * backrefIndex + ref - '0';
                    if (backrefIndex > reg->numSubpatterns())
                        backrefIndex = backrefIndex / 10;   // Fall back to the 1-digit reference
                    else
                        advance = 1;
                }
            }
            if (!backrefIndex)
                continue;
            backrefStart = ovector[2 * backrefIndex];
            backrefLength = ovector[2 * backrefIndex + 1] - backrefStart;
        } else
            continue;

        if (i - offset)
            result.append(replacement.substring(offset, i - offset));
        i += 1 + advance;
        offset = i + 1;
        if (backrefStart >= 0)
            result.append(source.substring(backrefStart, backrefLength));
    } while ((i = replacement.find('$', i + 1)) != notFound);

    if (replacement.length() - offset)
        result.append(replacement.substring(offset));
}

inline void substituteBackreferencesInline(StringBuilder& result, const String& replacement, StringView source, const int* ovector, RegExp* reg)
{
    size_t i = replacement.find('$');
    if (UNLIKELY(i != notFound))
        return substituteBackreferencesSlow(result, replacement, source, ovector, reg, i);

    result.append(replacement);
}

void substituteBackreferences(StringBuilder& result, const String& replacement, StringView source, const int* ovector, RegExp* reg)
{
    substituteBackreferencesInline(result, replacement, source, ovector, reg);
}

struct StringRange {
    StringRange(int pos, int len)
        : position(pos)
        , length(len)
    {
    }

    StringRange()
    {
    }

    int position;
    int length;
};

static ALWAYS_INLINE JSString* jsSpliceSubstrings(JSGlobalObject* globalObject, JSString* sourceVal, const String& source, const StringRange* substringRanges, int rangeCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (rangeCount == 1) {
        int sourceSize = source.length();
        int position = substringRanges[0].position;
        int length = substringRanges[0].length;
        if (position <= 0 && length >= sourceSize)
            return sourceVal;
        // We could call String::substringSharingImpl(), but this would result in redundant checks.
        RELEASE_AND_RETURN(scope, jsString(vm, StringImpl::createSubstringSharingImpl(*source.impl(), std::max(0, position), std::min(sourceSize, length))));
    }

    // We know that the sum of substringRanges lengths cannot exceed length of
    // source because the substringRanges were computed from the source string
    // in removeUsingRegExpSearch(). Hence, totalLength cannot exceed
    // String::MaxLength, and therefore, cannot overflow.
    Checked<int, AssertNoOverflow> totalLength = 0;
    for (int i = 0; i < rangeCount; i++)
        totalLength += substringRanges[i].length;
    ASSERT(totalLength <= String::MaxLength);

    if (!totalLength)
        return jsEmptyString(vm);

    if (source.is8Bit()) {
        LChar* buffer;
        const LChar* sourceData = source.characters8();
        auto impl = StringImpl::tryCreateUninitialized(totalLength.unsafeGet(), buffer);
        if (!impl) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        Checked<int, AssertNoOverflow> bufferPos = 0;
        for (int i = 0; i < rangeCount; i++) {
            if (int srcLen = substringRanges[i].length) {
                StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), sourceData + substringRanges[i].position, srcLen);
                bufferPos += srcLen;
            }
        }

        RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(impl)));
    }

    UChar* buffer;
    const UChar* sourceData = source.characters16();

    auto impl = StringImpl::tryCreateUninitialized(totalLength.unsafeGet(), buffer);
    if (!impl) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    Checked<int, AssertNoOverflow> bufferPos = 0;
    for (int i = 0; i < rangeCount; i++) {
        if (int srcLen = substringRanges[i].length) {
            StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), sourceData + substringRanges[i].position, srcLen);
            bufferPos += srcLen;
        }
    }

    RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(impl)));
}

static ALWAYS_INLINE JSString* jsSpliceSubstringsWithSeparators(JSGlobalObject* globalObject, JSString* sourceVal, const String& source, const StringRange* substringRanges, int rangeCount, const String* separators, int separatorCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (rangeCount == 1 && separatorCount == 0) {
        int sourceSize = source.length();
        int position = substringRanges[0].position;
        int length = substringRanges[0].length;
        if (position <= 0 && length >= sourceSize)
            return sourceVal;
        // We could call String::substringSharingImpl(), but this would result in redundant checks.
        RELEASE_AND_RETURN(scope, jsString(vm, StringImpl::createSubstringSharingImpl(*source.impl(), std::max(0, position), std::min(sourceSize, length))));
    }

    if (rangeCount == 2 && separatorCount == 1) {
        String leftPart(StringImpl::createSubstringSharingImpl(*source.impl(), substringRanges[0].position, substringRanges[0].length));
        String rightPart(StringImpl::createSubstringSharingImpl(*source.impl(), substringRanges[1].position, substringRanges[1].length));
        RELEASE_AND_RETURN(scope, jsString(globalObject, leftPart, separators[0], rightPart));
    }

    Checked<int, RecordOverflow> totalLength = 0;
    bool allSeparators8Bit = true;
    for (int i = 0; i < rangeCount; i++)
        totalLength += substringRanges[i].length;
    for (int i = 0; i < separatorCount; i++) {
        totalLength += separators[i].length();
        if (separators[i].length() && !separators[i].is8Bit())
            allSeparators8Bit = false;
    }
    if (totalLength.hasOverflowed()) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    if (!totalLength)
        return jsEmptyString(vm);

    if (source.is8Bit() && allSeparators8Bit) {
        LChar* buffer;
        const LChar* sourceData = source.characters8();

        auto impl = StringImpl::tryCreateUninitialized(totalLength.unsafeGet(), buffer);
        if (!impl) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        int maxCount = std::max(rangeCount, separatorCount);
        Checked<int, AssertNoOverflow> bufferPos = 0;
        for (int i = 0; i < maxCount; i++) {
            if (i < rangeCount) {
                if (int srcLen = substringRanges[i].length) {
                    StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), sourceData + substringRanges[i].position, srcLen);
                    bufferPos += srcLen;
                }
            }
            if (i < separatorCount) {
                if (int sepLen = separators[i].length()) {
                    StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), separators[i].characters8(), sepLen);
                    bufferPos += sepLen;
                }
            }
        }        

        RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(impl)));
    }

    UChar* buffer;
    auto impl = StringImpl::tryCreateUninitialized(totalLength.unsafeGet(), buffer);
    if (!impl) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    int maxCount = std::max(rangeCount, separatorCount);
    Checked<int, AssertNoOverflow> bufferPos = 0;
    for (int i = 0; i < maxCount; i++) {
        if (i < rangeCount) {
            if (int srcLen = substringRanges[i].length) {
                if (source.is8Bit())
                    StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), source.characters8() + substringRanges[i].position, srcLen);
                else
                    StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), source.characters16() + substringRanges[i].position, srcLen);
                bufferPos += srcLen;
            }
        }
        if (i < separatorCount) {
            if (int sepLen = separators[i].length()) {
                if (separators[i].is8Bit())
                    StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), separators[i].characters8(), sepLen);
                else
                    StringImpl::copyCharacters(buffer + bufferPos.unsafeGet(), separators[i].characters16(), sepLen);
                bufferPos += sepLen;
            }
        }
    }

    RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(impl)));
}

#define OUT_OF_MEMORY(exec__, scope__) \
    do { \
        throwOutOfMemoryError(exec__, scope__); \
        return nullptr; \
    } while (false)

static ALWAYS_INLINE JSString* removeUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    SuperSamplerScope superSamplerScope(false);
    
    size_t lastIndex = 0;
    unsigned startPosition = 0;

    Vector<StringRange, 16> sourceRanges;
    unsigned sourceLen = source.length();

    while (true) {
        MatchResult result = globalObject->regExpGlobalData().performMatch(vm, globalObject, regExp, string, source, startPosition);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!result)
            break;

        if (lastIndex < result.start) {
            if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start - lastIndex)))
                OUT_OF_MEMORY(globalObject, scope);
        }
        lastIndex = result.end;
        startPosition = lastIndex;

        // special case of empty match
        if (result.empty()) {
            startPosition++;
            if (startPosition > sourceLen)
                break;
        }
    }

    if (!lastIndex)
        return string;

    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen - lastIndex)))
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstrings(globalObject, string, source, sourceRanges.data(), sourceRanges.size()));
}

static ALWAYS_INLINE JSString* replaceUsingRegExpSearch(
    VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSString* string, JSValue searchValue, CallData& callData,
    CallType callType, String& replacementString, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String source = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    unsigned sourceLen = source.length();
    RETURN_IF_EXCEPTION(scope, nullptr);
    RegExpObject* regExpObject = jsCast<RegExpObject*>(searchValue);
    RegExp* regExp = regExpObject->regExp();
    bool global = regExp->global();
    bool hasNamedCaptures = regExp->hasNamedCaptures();

    if (global) {
        // ES5.1 15.5.4.10 step 8.a.
        regExpObject->setLastIndex(globalObject, 0);
        RETURN_IF_EXCEPTION(scope, nullptr);

        if (callType == CallType::None && !replacementString.length()) 
            RELEASE_AND_RETURN(scope, removeUsingRegExpSearch(vm, globalObject, string, source, regExp));
    }

    size_t lastIndex = 0;
    unsigned startPosition = 0;

    Vector<StringRange, 16> sourceRanges;
    Vector<String, 16> replacements;

    // This is either a loop (if global is set) or a one-way (if not).
    if (global && callType == CallType::JS) {
        // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
        int argCount = regExp->numSubpatterns() + 1 + 2;
        if (hasNamedCaptures)
            ++argCount;
        JSFunction* func = jsCast<JSFunction*>(replaceValue);
        CachedCall cachedCall(globalObject, callFrame, func, argCount);
        RETURN_IF_EXCEPTION(scope, nullptr);
        while (true) {
            int* ovector;
            MatchResult result = globalObject->regExpGlobalData().performMatch(vm, globalObject, regExp, string, source, startPosition, &ovector);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!result)
                break;

            if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start - lastIndex)))
                OUT_OF_MEMORY(globalObject, scope);

            cachedCall.clearArguments();
            JSObject* groups = hasNamedCaptures ? constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure()) : nullptr;

            for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
                int matchStart = ovector[i * 2];
                int matchLen = ovector[i * 2 + 1] - matchStart;

                JSValue patternValue;

                if (matchStart < 0)
                    patternValue = jsUndefined();
                else
                    patternValue = jsSubstring(vm, source, matchStart, matchLen);

                cachedCall.appendArgument(patternValue);

                if (i && hasNamedCaptures) {
                    String groupName = regExp->getCaptureGroupName(i);
                    if (!groupName.isEmpty())
                        groups->putDirect(vm, Identifier::fromString(vm, groupName), patternValue);
                }
            }

            cachedCall.appendArgument(jsNumber(result.start));
            cachedCall.appendArgument(string);
            if (hasNamedCaptures)
                cachedCall.appendArgument(groups);

            cachedCall.setThis(jsUndefined());
            if (UNLIKELY(cachedCall.hasOverflowedArguments())) {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }

            JSValue jsResult = cachedCall.call();
            RETURN_IF_EXCEPTION(scope, nullptr);
            replacements.append(jsResult.toWTFString(globalObject));
            RETURN_IF_EXCEPTION(scope, nullptr);

            lastIndex = result.end;
            startPosition = lastIndex;

            // special case of empty match
            if (result.empty()) {
                startPosition++;
                if (startPosition > sourceLen)
                    break;
            }
        }
    } else {
        do {
            int* ovector;
            MatchResult result = globalObject->regExpGlobalData().performMatch(vm, globalObject, regExp, string, source, startPosition, &ovector);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!result)
                break;

            if (callType != CallType::None) {
                if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start - lastIndex)))
                    OUT_OF_MEMORY(globalObject, scope);

                MarkedArgumentBuffer args;
                JSObject* groups = hasNamedCaptures ? constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure()) : nullptr;

                for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
                    int matchStart = ovector[i * 2];
                    int matchLen = ovector[i * 2 + 1] - matchStart;

                    JSValue patternValue;

                    if (matchStart < 0)
                        patternValue = jsUndefined();
                    else {
                        patternValue = jsSubstring(vm, source, matchStart, matchLen);
                        RETURN_IF_EXCEPTION(scope, nullptr);
                    }

                    args.append(patternValue);

                    if (i && hasNamedCaptures) {
                        String groupName = regExp->getCaptureGroupName(i);
                        if (!groupName.isEmpty())
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), patternValue);
                    }
                }

                args.append(jsNumber(result.start));
                args.append(string);
                if (hasNamedCaptures)
                    args.append(groups);
                if (UNLIKELY(args.hasOverflowed())) {
                    throwOutOfMemoryError(globalObject, scope);
                    return nullptr;
                }

                JSValue replacement = call(globalObject, replaceValue, callType, callData, jsUndefined(), args);
                RETURN_IF_EXCEPTION(scope, nullptr);
                String replacementString = replacement.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
                replacements.append(replacementString);
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else {
                int replLen = replacementString.length();
                if (lastIndex < result.start || replLen) {
                    if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start - lastIndex)))
                        OUT_OF_MEMORY(globalObject, scope);

                    if (replLen) {
                        StringBuilder replacement(StringBuilder::OverflowHandler::RecordOverflow);
                        substituteBackreferences(replacement, replacementString, source, ovector, regExp);
                        if (UNLIKELY(replacement.hasOverflowed()))
                            OUT_OF_MEMORY(globalObject, scope);
                        replacements.append(replacement.toString());
                    } else
                        replacements.append(String());
                }
            }

            lastIndex = result.end;
            startPosition = lastIndex;

            // special case of empty match
            if (result.empty()) {
                startPosition++;
                if (startPosition > sourceLen)
                    break;
            }
        } while (global);
    }

    if (!lastIndex && replacements.isEmpty())
        return string;

    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen - lastIndex)))
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, string, source, sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size()));
}

IGNORE_WARNINGS_BEGIN("frame-address")

JSCell* JIT_OPERATION operationStringProtoFuncReplaceRegExpEmptyStr(JSGlobalObject* globalObject, JSString* thisValue, RegExpObject* searchValue)
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regExp = searchValue->regExp();
    if (regExp->global()) {
        // ES5.1 15.5.4.10 step 8.a.
        searchValue->setLastIndex(globalObject, 0);
        RETURN_IF_EXCEPTION(scope, nullptr);
        String source = thisValue->value(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        RELEASE_AND_RETURN(scope, removeUsingRegExpSearch(vm, globalObject, thisValue, source, regExp));
    }

    CallData callData;
    String replacementString = emptyString();
    RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(
        vm, globalObject, callFrame, thisValue, searchValue, callData, CallType::None, replacementString, JSValue()));
}

JSCell* JIT_OPERATION operationStringProtoFuncReplaceRegExpString(JSGlobalObject* globalObject, JSString* thisValue, RegExpObject* searchValue, JSString* replaceString)
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CallData callData;
    String replacementString = replaceString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(
        vm, globalObject, callFrame, thisValue, searchValue, callData, CallType::None, replacementString, replaceString));
}

static ALWAYS_INLINE JSString* replaceUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSString* string, JSValue searchValue, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String replacementString;
    CallData callData;
    CallType callType = getCallData(vm, replaceValue, callData);
    if (callType == CallType::None) {
        replacementString = replaceValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(
        vm, globalObject, callFrame, string, searchValue, callData, callType, replacementString, replaceValue));
}

enum class ReplaceMode : bool { Single, Global };

static ALWAYS_INLINE JSString* replaceUsingStringSearch(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSString* jsString, JSValue searchValue, JSValue replaceValue, ReplaceMode mode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = jsString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    String searchString = searchValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    CallData callData;
    CallType callType = getCallData(vm, replaceValue, callData);
    Optional<CachedCall> cachedCall;
    String replaceString;
    if (callType == CallType::None) {
        replaceString = replaceValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    } else if (callType == CallType::JS) {
        cachedCall.emplace(globalObject, callFrame, jsCast<JSFunction*>(replaceValue), 3);
        RETURN_IF_EXCEPTION(scope, nullptr);
        cachedCall->setThis(jsUndefined());
    }

    size_t matchStart = string.find(searchString);
    if (matchStart == notFound)
        return jsString;

    size_t endOfLastMatch = 0;
    size_t searchStringLength = searchString.length();
    Vector<StringRange, 16> sourceRanges;
    Vector<String, 16> replacements;
    do {
        if (callType != CallType::None) {
            JSValue replacement;
            if (cachedCall) {
                auto* substring = jsSubstring(vm, string, matchStart, searchStringLength);
                RETURN_IF_EXCEPTION(scope, nullptr);
                cachedCall->clearArguments();
                cachedCall->appendArgument(substring);
                cachedCall->appendArgument(jsNumber(matchStart));
                cachedCall->appendArgument(jsString);
                ASSERT(!cachedCall->hasOverflowedArguments());
                replacement = cachedCall->call();
            } else {
                MarkedArgumentBuffer args;
                auto* substring = jsSubstring(vm, string, matchStart, searchString.impl()->length());
                RETURN_IF_EXCEPTION(scope, nullptr);
                args.append(substring);
                args.append(jsNumber(matchStart));
                args.append(jsString);
                ASSERT(!args.hasOverflowed());
                replacement = call(globalObject, replaceValue, callType, callData, jsUndefined(), args);
            }
            RETURN_IF_EXCEPTION(scope, nullptr);
            replaceString = replacement.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }

        if (UNLIKELY(!sourceRanges.tryConstructAndAppend(endOfLastMatch, matchStart - endOfLastMatch)))
            OUT_OF_MEMORY(globalObject, scope);

        size_t matchEnd = matchStart + searchStringLength;
        if (callType != CallType::None)
            replacements.append(replaceString);
        else {
            StringBuilder replacement(StringBuilder::OverflowHandler::RecordOverflow);
            int ovector[2] = { static_cast<int>(matchStart),  static_cast<int>(matchEnd) };
            substituteBackreferences(replacement, replaceString, string, ovector, nullptr);
            if (UNLIKELY(replacement.hasOverflowed()))
                OUT_OF_MEMORY(globalObject, scope);
            replacements.append(replacement.toString());
        }

        endOfLastMatch = matchEnd;
        if (mode == ReplaceMode::Single)
            break;
        matchStart = string.find(searchString, !searchStringLength ? endOfLastMatch + 1 : endOfLastMatch);
    } while (matchStart != notFound);

    if (UNLIKELY(!sourceRanges.tryConstructAndAppend(endOfLastMatch, string.length() - endOfLastMatch)))
        OUT_OF_MEMORY(globalObject, scope);
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, jsString, string, sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size()));
}

static inline bool checkObjectCoercible(JSValue thisValue)
{
    if (thisValue.isString())
        return true;

    if (thisValue.isUndefinedOrNull())
        return false;

    if (thisValue.isObject() && asObject(thisValue)->isEnvironment())
        return false;

    return true;
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncRepeatCharacter(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // For a string which length is single, instead of creating ropes,
    // allocating a sequential buffer and fill with the repeated string for efficiency.
    ASSERT(callFrame->argumentCount() == 2);

    ASSERT(callFrame->uncheckedArgument(0).isString());
    JSString* string = asString(callFrame->uncheckedArgument(0));
    ASSERT(string->length() == 1);

    JSValue repeatCountValue = callFrame->uncheckedArgument(1);
    RELEASE_ASSERT(repeatCountValue.isNumber());
    int32_t repeatCount;
    double value = repeatCountValue.asNumber();
    if (value > JSString::MaxLength)
        return JSValue::encode(throwOutOfMemoryError(globalObject, scope));
    repeatCount = static_cast<int32_t>(value);
    ASSERT(repeatCount >= 0);
    ASSERT(!repeatCountValue.isDouble() || repeatCountValue.asDouble() == repeatCount);

    auto viewWithString = string->viewWithUnderlyingString(globalObject);
    StringView view = viewWithString.view;
    ASSERT(view.length() == 1);
    scope.assertNoException();
    UChar character = view[0];
    scope.release();
    if (isLatin1(character))
        return JSValue::encode(repeatCharacter(globalObject, static_cast<LChar>(character), repeatCount));
    return JSValue::encode(repeatCharacter(globalObject, character, repeatCount));
}

ALWAYS_INLINE JSString* replace(
    VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSString* string, JSValue searchValue, JSValue replaceValue)
{
    if (searchValue.inherits<RegExpObject>(vm))
        return replaceUsingRegExpSearch(vm, globalObject, callFrame, string, searchValue, replaceValue);
    return replaceUsingStringSearch(vm, globalObject, callFrame, string, searchValue, replaceValue, ReplaceMode::Single);
}

ALWAYS_INLINE JSString* replace(
    VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue, JSValue searchValue, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!checkObjectCoercible(thisValue)) {
        throwVMTypeError(globalObject, scope);
        return nullptr;
    }
    JSString* string = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, replace(vm, globalObject, callFrame, string, searchValue, replaceValue));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncReplaceUsingRegExp(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = callFrame->thisValue().toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue searchValue = callFrame->argument(0);
    if (!searchValue.inherits<RegExpObject>(vm))
        return JSValue::encode(jsUndefined());

    RELEASE_AND_RETURN(scope, JSValue::encode(replaceUsingRegExpSearch(vm, globalObject, callFrame, string, searchValue, callFrame->argument(1))));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncReplaceUsingStringSearch(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = callFrame->thisValue().toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(replaceUsingStringSearch(vm, globalObject, callFrame, string, callFrame->argument(0), callFrame->argument(1), ReplaceMode::Single)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncReplaceAllUsingStringSearch(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = callFrame->thisValue().toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(replaceUsingStringSearch(vm, globalObject, callFrame, string, callFrame->argument(0), callFrame->argument(1), ReplaceMode::Global)));
}

JSCell* JIT_OPERATION operationStringProtoFuncReplaceGeneric(JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue searchValue, EncodedJSValue replaceValue)
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return replace(
        vm, globalObject, callFrame, JSValue::decode(thisValue), JSValue::decode(searchValue),
        JSValue::decode(replaceValue));
}

IGNORE_WARNINGS_END

EncodedJSValue JSC_HOST_CALL stringProtoFuncToString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    // Also used for valueOf.

    if (thisValue.isString())
        return JSValue::encode(thisValue);

    auto* stringObject = jsDynamicCast<StringObject*>(vm, thisValue);
    if (stringObject)
        return JSValue::encode(stringObject->internalValue());

    return throwVMTypeError(globalObject, scope);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncCharAt(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    auto viewWithString = thisValue.toString(globalObject)->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    StringView view = viewWithString.view;
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSValue a0 = callFrame->argument(0);
    if (a0.isUInt32()) {
        uint32_t i = a0.asUInt32();
        if (i < view.length())
            return JSValue::encode(jsSingleCharacterString(vm, view[i]));
        return JSValue::encode(jsEmptyString(vm));
    }
    double dpos = a0.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (dpos >= 0 && dpos < view.length())
        return JSValue::encode(jsSingleCharacterString(vm, view[static_cast<unsigned>(dpos)]));
    return JSValue::encode(jsEmptyString(vm));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncCharCodeAt(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    auto viewWithString = thisValue.toString(globalObject)->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    StringView view = viewWithString.view;
    JSValue a0 = callFrame->argument(0);
    if (a0.isUInt32()) {
        uint32_t i = a0.asUInt32();
        if (i < view.length())
            return JSValue::encode(jsNumber(view[i]));
        return JSValue::encode(jsNaN());
    }
    double dpos = a0.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (dpos >= 0 && dpos < view.length())
        return JSValue::encode(jsNumber(view[static_cast<int>(dpos)]));
    return JSValue::encode(jsNaN());
}

static inline UChar32 codePointAt(const String& string, unsigned position, unsigned length)
{
    RELEASE_ASSERT(position < length);
    if (string.is8Bit())
        return string.characters8()[position];
    UChar32 character;
    U16_NEXT(string.characters16(), position, length, character);
    return character;
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncCodePointAt(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    String string = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    unsigned length = string.length();

    JSValue argument0 = callFrame->argument(0);
    if (argument0.isUInt32()) {
        unsigned position = argument0.asUInt32();
        if (position < length)
            return JSValue::encode(jsNumber(codePointAt(string, position, length)));
        return JSValue::encode(jsUndefined());
    }

    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    double doublePosition = argument0.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (doublePosition >= 0 && doublePosition < length)
        return JSValue::encode(jsNumber(codePointAt(string, static_cast<unsigned>(doublePosition), length)));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncIndexOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    JSValue a0 = callFrame->argument(0);
    JSValue a1 = callFrame->argument(1);

    JSString* thisJSString = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSString* otherJSString = a0.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned pos = 0;
    if (!a1.isUndefined()) {
        int len = thisJSString->length();
        RELEASE_ASSERT(len >= 0);
        if (a1.isUInt32())
            pos = std::min<uint32_t>(a1.asUInt32(), len);
        else {
            double dpos = a1.toInteger(globalObject);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (dpos < 0)
                dpos = 0;
            else if (dpos > len)
                dpos = len;
            pos = static_cast<unsigned>(dpos);
        }
    }

    if (thisJSString->length() < otherJSString->length() + pos)
        return JSValue::encode(jsNumber(-1));

    auto thisViewWithString = thisJSString->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto otherViewWithString = otherJSString->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t result = thisViewWithString.view.find(otherViewWithString.view, pos);
    if (result == notFound)
        return JSValue::encode(jsNumber(-1));
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncLastIndexOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    JSValue a0 = callFrame->argument(0);
    JSValue a1 = callFrame->argument(1);

    JSString* thisJSString = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    unsigned len = thisJSString->length();
    JSString* otherJSString = a0.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    double dpos = a1.toIntegerPreserveNaN(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    unsigned startPosition;
    if (dpos < 0)
        startPosition = 0;
    else if (!(dpos <= len)) // true for NaN
        startPosition = len;
    else
        startPosition = static_cast<unsigned>(dpos);

    if (len < otherJSString->length())
        return JSValue::encode(jsNumber(-1));

    String thisString = thisJSString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String otherString = otherJSString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t result;
    if (!startPosition)
        result = thisString.startsWith(otherString) ? 0 : notFound;
    else
        result = thisString.reverseFind(otherString, startPosition);
    if (result == notFound)
        return JSValue::encode(jsNumber(-1));
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSlice(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    JSString* string = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue a0 = callFrame->argument(0);
    JSValue a1 = callFrame->argument(1);

    int length = string->length();
    RELEASE_ASSERT(length >= 0);

    // The arg processing is very much like ArrayProtoFunc::Slice
    double start = a0.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    double end = a1.isUndefined() ? length : a1.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(stringSlice(globalObject, vm, string, length, start, end)));
}

// Return true in case of early return (resultLength got to limitLength).
template<typename CharacterType>
static ALWAYS_INLINE bool splitStringByOneCharacterImpl(JSGlobalObject* globalObject, JSArray* result, JSValue originalValue, const String& input, StringImpl* string, UChar separatorCharacter, size_t& position, unsigned& resultLength, unsigned limitLength)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12. Let q = p.
    size_t matchPosition;
    const CharacterType* characters = string->characters<CharacterType>();
    // 13. Repeat, while q != s
    //   a. Call SplitMatch(S, q, R) and let z be its MatchResult result.
    //   b. If z is failure, then let q = q+1.
    //   c. Else, z is not failure
    while ((matchPosition = WTF::find(characters, string->length(), separatorCharacter, position)) != notFound) {
        // 1. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
        //    through q (exclusive).
        // 2. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(lengthA),
        //    Property Descriptor {[[Value]]: T, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        auto* substring = jsSubstring(globalObject, originalValue, input, position, matchPosition - position);
        RETURN_IF_EXCEPTION(scope, false);
        result->putDirectIndex(globalObject, resultLength, substring);
        RETURN_IF_EXCEPTION(scope, false);
        // 3. Increment lengthA by 1.
        // 4. If lengthA == lim, return A.
        if (++resultLength == limitLength)
            return true;

        // 5. Let p = e.
        // 8. Let q = p.
        position = matchPosition + 1;
    }
    return false;
}

// ES 21.1.3.17 String.prototype.split(separator, limit)
EncodedJSValue JSC_HOST_CALL stringProtoFuncSplitFast(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    ASSERT(checkObjectCoercible(thisValue));

    // 3. Let S be the result of calling ToString, giving it the this value as its argument.
    // 7. Let s be the number of characters in S.
    String input = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    ASSERT(!input.isNull());

    // 4. Let A be a new array created as if by the expression new Array()
    //    where Array is the standard built-in constructor with that name.
    JSArray* result = constructEmptyArray(globalObject, 0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 5. Let lengthA be 0.
    unsigned resultLength = 0;

    // 6. If limit is undefined, let lim = 2^32-1; else let lim = ToUint32(limit).
    JSValue limitValue = callFrame->uncheckedArgument(1);
    unsigned limit = limitValue.isUndefined() ? 0xFFFFFFFFu : limitValue.toUInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 8. Let p = 0.
    size_t position = 0;

    // 9. If separator is a RegExp object (its [[Class]] is "RegExp"), let R = separator;
    //    otherwise let R = ToString(separator).
    JSValue separatorValue = callFrame->uncheckedArgument(0);
    String separator = separatorValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 10. If lim == 0, return A.
    if (!limit)
        return JSValue::encode(result);

    // 11. If separator is undefined, then
    if (separatorValue.isUndefined()) {
        // a. Call the [[DefineOwnProperty]] internal method of A with arguments "0",
        scope.release();
        result->putDirectIndex(globalObject, 0, jsStringWithReuse(globalObject, thisValue, input));
        // b. Return A.
        return JSValue::encode(result);
    }

    // 12. If s == 0, then
    if (input.isEmpty()) {
        // a. Let z be SplitMatch(S, 0, R) where S is input, R is separator.
        // b. If z is not false, return A.
        // c. Call CreateDataProperty(A, "0", S).
        // d. Return A.
        if (!separator.isEmpty()) {
            scope.release();
            result->putDirectIndex(globalObject, 0, jsStringWithReuse(globalObject, thisValue, input));
        }
        return JSValue::encode(result);
    }

    // Optimized case for splitting on the empty string.
    if (separator.isEmpty()) {
        limit = std::min(limit, input.length());
        // Zero limt/input length handled in steps 9/11 respectively, above.
        ASSERT(limit);

        do {
            result->putDirectIndex(globalObject, position, jsSingleCharacterString(vm, input[position]));
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        } while (++position < limit);

        return JSValue::encode(result);
    }

    // 3 cases:
    // -separator length == 1, 8 bits
    // -separator length == 1, 16 bits
    // -separator length > 1
    StringImpl* stringImpl = input.impl();
    StringImpl* separatorImpl = separator.impl();
    size_t separatorLength = separatorImpl->length();

    if (separatorLength == 1) {
        UChar separatorCharacter;
        if (separatorImpl->is8Bit())
            separatorCharacter = separatorImpl->characters8()[0];
        else
            separatorCharacter = separatorImpl->characters16()[0];

        if (stringImpl->is8Bit()) {
            if (splitStringByOneCharacterImpl<LChar>(globalObject, result, thisValue, input, stringImpl, separatorCharacter, position, resultLength, limit)) 
                RELEASE_AND_RETURN(scope, JSValue::encode(result));
        } else {
            if (splitStringByOneCharacterImpl<UChar>(globalObject, result, thisValue, input, stringImpl, separatorCharacter, position, resultLength, limit)) 
                RELEASE_AND_RETURN(scope, JSValue::encode(result));
        }
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    } else {
        // 13. Let q = p.
        size_t matchPosition;
        // 14. Repeat, while q != s
        //   a. let e be SplitMatch(S, q, R).
        //   b. If e is failure, then let q = q+1.
        //   c. Else, e is an integer index <= s.
        while ((matchPosition = stringImpl->find(separatorImpl, position)) != notFound) {
            // 1. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
            //    through q (exclusive).
            // 2. Call CreateDataProperty(A, ToString(lengthA), T).
            auto* substring = jsSubstring(globalObject, thisValue, input, position, matchPosition - position);
            RETURN_IF_EXCEPTION(scope, { });
            result->putDirectIndex(globalObject, resultLength, substring);
            RETURN_IF_EXCEPTION(scope, { });
            // 3. Increment lengthA by 1.
            // 4. If lengthA == lim, return A.
            if (++resultLength == limit)
                return JSValue::encode(result);

            // 5. Let p = e.
            // 6. Let q = p.
            position = matchPosition + separator.length();
        }
    }

    // 15. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
    //     through s (exclusive).
    // 16. Call CreateDataProperty(A, ToString(lengthA), T).
    auto* substring = jsSubstring(globalObject, thisValue, input, position, input.length() - position);
    RETURN_IF_EXCEPTION(scope, { });
    scope.release();
    result->putDirectIndex(globalObject, resultLength++, substring);

    // 17. Return A.
    return JSValue::encode(result);
}

static EncodedJSValue stringProtoFuncSubstrImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    unsigned len;
    JSString* jsString = 0;
    String uString;
    if (thisValue.isString()) {
        jsString = asString(thisValue);
        len = jsString->length();
    } else {
        uString = thisValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        len = uString.length();
    }

    JSValue a0 = callFrame->argument(0);
    JSValue a1 = callFrame->argument(1);

    double start = a0.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    double length = a1.isUndefined() ? len : a1.toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (start >= len || length <= 0)
        return JSValue::encode(jsEmptyString(vm));
    if (start < 0) {
        start += len;
        if (start < 0)
            start = 0;
    }
    if (start + length > len)
        length = len - start;
    unsigned substringStart = static_cast<unsigned>(start);
    unsigned substringLength = static_cast<unsigned>(length);
    scope.release();
    if (jsString)
        return JSValue::encode(jsSubstring(globalObject, jsString, substringStart, substringLength));
    return JSValue::encode(jsSubstring(vm, uString, substringStart, substringLength));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstr(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return stringProtoFuncSubstrImpl(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL builtinStringSubstrInternal(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // @substrInternal should not have any observable side effects (e.g. it should not call
    // GetMethod(..., @@toPrimitive) on the thisValue).

    // It is ok to use the default stringProtoFuncSubstr as the implementation of
    // @substrInternal because @substrInternal will only be called by builtins, which will
    // guarantee that we only pass it a string thisValue. As a result, stringProtoFuncSubstr
    // will not need to call toString() on the thisValue, and there will be no observable
    // side-effects.
    ASSERT(callFrame->thisValue().isString());
    return stringProtoFuncSubstrImpl(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstring(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    JSString* jsString = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue a0 = callFrame->argument(0);
    JSValue a1 = callFrame->argument(1);
    int len = jsString->length();
    RELEASE_ASSERT(len >= 0);

    double start = a0.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    double end;
    if (!(start >= 0)) // check for negative values or NaN
        start = 0;
    else if (start > len)
        start = len;
    if (a1.isUndefined())
        end = len;
    else { 
        end = a1.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (!(end >= 0)) // check for negative values or NaN
            end = 0;
        else if (end > len)
            end = len;
    }
    if (start > end) {
        double temp = end;
        end = start;
        start = temp;
    }
    unsigned substringStart = static_cast<unsigned>(start);
    unsigned substringLength = static_cast<unsigned>(end) - substringStart;
    RELEASE_AND_RETURN(scope, JSValue::encode(jsSubstring(globalObject, jsString, substringStart, substringLength)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToLowerCase(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    JSString* sVal = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String s = sVal->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String lowercasedString = s.convertToLowercaseWithoutLocale();
    if (lowercasedString.impl() == s.impl())
        return JSValue::encode(sVal);
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, lowercasedString)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToUpperCase(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    JSString* sVal = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String s = sVal->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String uppercasedString = s.convertToUppercaseWithoutLocale();
    if (uppercasedString.impl() == s.impl())
        return JSValue::encode(sVal);
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, uppercasedString)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncLocaleCompare(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // 13.1.1 String.prototype.localeCompare (that [, locales [, options ]]) (ECMA-402 2.0)
    // http://ecma-international.org/publications/standards/Ecma-402.htm

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let O be RequireObjectCoercible(this value).
    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope, "String.prototype.localeCompare requires that |this| not be null or undefined"_s);

    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    String string = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 4. Let That be ToString(that).
    // 5. ReturnIfAbrupt(That).
    JSValue thatValue = callFrame->argument(0);
    String that = thatValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if ENABLE(INTL)
    JSValue locales = callFrame->argument(1);
    JSValue options = callFrame->argument(2);
    IntlCollator* collator = nullptr;
    if (locales.isUndefined() && options.isUndefined())
        collator = globalObject->defaultCollator();
    else {
        collator = IntlCollator::create(vm, globalObject->collatorStructure());
        collator->initializeCollator(globalObject, locales, options);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(collator->compareStrings(globalObject, string, that)));
#else
    return JSValue::encode(jsNumber(Collator().collate(string, that)));
#endif
}

#if ENABLE(INTL)
enum class CaseConversionMode {
    Upper,
    Lower,
};
template<CaseConversionMode mode>
static EncodedJSValue toLocaleCase(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto convertCase = [&] (auto&&... args) {
        if (mode == CaseConversionMode::Lower)
            return u_strToLower(std::forward<decltype(args)>(args)...);
        return u_strToUpper(std::forward<decltype(args)>(args)...);
    };

    // 1. Let O be RequireObjectCoercible(this value).
    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    // 2. Let S be ToString(O).
    JSString* sVal = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    String s = sVal->value(globalObject);

    // 3. ReturnIfAbrupt(S).
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // Optimization for empty strings.
    if (s.isEmpty())
        return JSValue::encode(sVal);

    // 4. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));

    // 5. ReturnIfAbrupt(requestedLocales).
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 6. Let len be the number of elements in requestedLocales.
    size_t len = requestedLocales.size();

    // 7. If len > 0, then
    // a. Let requestedLocale be the first element of requestedLocales.
    // 8. Else
    // a. Let requestedLocale be DefaultLocale().
    String requestedLocale = len > 0 ? requestedLocales.first() : defaultLocale(globalObject);

    // 9. Let noExtensionsLocale be the String value that is requestedLocale with all Unicode locale extension sequences (6.2.1) removed.
    String noExtensionsLocale = removeUnicodeLocaleExtension(requestedLocale);

    // 10. Let availableLocales be a List with the language tags of the languages for which the Unicode character database contains language sensitive case mappings.
    // Note 1: As of Unicode 5.1, the availableLocales list contains the elements "az", "lt", and "tr".
    const HashSet<String> availableLocales({ "az"_s, "lt"_s, "tr"_s });

    // 11. Let locale be BestAvailableLocale(availableLocales, noExtensionsLocale).
    String locale = bestAvailableLocale(availableLocales, noExtensionsLocale);

    // 12. If locale is undefined, let locale be "und".
    if (locale.isNull())
        locale = "und"_s;

    CString utf8LocaleBuffer = locale.utf8();
    const StringView view(s);
    const int32_t viewLength = view.length();

    // Delegate the following steps to icu u_strToLower or u_strToUpper.
    // 13. Let cpList be a List containing in order the code points of S as defined in ES2015, 6.1.4, starting at the first element of S.
    // 14. For each code point c in cpList, if the Unicode Character Database provides a lower(/upper) case equivalent of c that is either language insensitive or for the language locale, then replace c in cpList with that/those equivalent code point(s).
    // 15. Let cuList be a new List.
    // 16. For each code point c in cpList, in order, append to cuList the elements of the UTF-16 Encoding (defined in ES2015, 6.1.4) of c.
    // 17. Let L be a String whose elements are, in order, the elements of cuList.

    // Most strings lower/upper case will be the same size as original, so try that first.
    UErrorCode error(U_ZERO_ERROR);
    Vector<UChar> buffer(viewLength);
    String lower;
    const int32_t resultLength = convertCase(buffer.data(), viewLength, view.upconvertedCharacters(), viewLength, utf8LocaleBuffer.data(), &error);
    if (U_SUCCESS(error))
        lower = String(buffer.data(), resultLength);
    else if (error == U_BUFFER_OVERFLOW_ERROR) {
        // Converted case needs more space than original. Try again.
        UErrorCode error(U_ZERO_ERROR);
        Vector<UChar> buffer(resultLength);
        convertCase(buffer.data(), resultLength, view.upconvertedCharacters(), viewLength, utf8LocaleBuffer.data(), &error);
        if (U_FAILURE(error))
            return throwVMTypeError(globalObject, scope, u_errorName(error));
        lower = String(buffer.data(), resultLength);
    } else
        return throwVMTypeError(globalObject, scope, u_errorName(error));

    // 18. Return L.
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, lower)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleLowerCase(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // 13.1.2 String.prototype.toLocaleLowerCase ([locales])
    // http://ecma-international.org/publications/standards/Ecma-402.htm
    return toLocaleCase<CaseConversionMode::Lower>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleUpperCase(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // 13.1.3 String.prototype.toLocaleUpperCase ([locales])
    // http://ecma-international.org/publications/standards/Ecma-402.htm
    // This function interprets a string value as a sequence of code points, as described in ES2015, 6.1.4. This function behaves in exactly the same way as String.prototype.toLocaleLowerCase, except that characters are mapped to their uppercase equivalents as specified in the Unicode character database.
    return toLocaleCase<CaseConversionMode::Upper>(globalObject,callFrame);
}
#endif // ENABLE(INTL)

enum {
    TrimStart = 1,
    TrimEnd = 2
};

static inline JSValue trimString(JSGlobalObject* globalObject, JSValue thisValue, int trimKind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!checkObjectCoercible(thisValue))
        return throwTypeError(globalObject, scope);
    String str = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned left = 0;
    if (trimKind & TrimStart) {
        while (left < str.length() && isStrWhiteSpace(str[left]))
            left++;
    }
    unsigned right = str.length();
    if (trimKind & TrimEnd) {
        while (right > left && isStrWhiteSpace(str[right - 1]))
            right--;
    }

    // Don't gc allocate a new string if we don't have to.
    if (left == 0 && right == str.length() && thisValue.isString())
        return thisValue;

    RELEASE_AND_RETURN(scope, jsString(vm, str.substringSharingImpl(left, right - left)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncTrim(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue thisValue = callFrame->thisValue();
    return JSValue::encode(trimString(globalObject, thisValue, TrimStart | TrimEnd));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimStart(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue thisValue = callFrame->thisValue();
    return JSValue::encode(trimString(globalObject, thisValue, TrimStart));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimEnd(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue thisValue = callFrame->thisValue();
    return JSValue::encode(trimString(globalObject, thisValue, TrimEnd));
}

static inline unsigned clampAndTruncateToUnsigned(double value, unsigned min, unsigned max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return static_cast<unsigned>(value);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncStartsWith(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    String stringToSearchIn = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue a0 = callFrame->argument(0);
    bool isRegularExpression = isRegExp(vm, globalObject, a0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (isRegularExpression)
        return throwVMTypeError(globalObject, scope, "Argument to String.prototype.startsWith cannot be a RegExp");

    String searchString = a0.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue positionArg = callFrame->argument(1);
    unsigned start = 0;
    if (positionArg.isInt32())
        start = std::max(0, positionArg.asInt32());
    else {
        unsigned length = stringToSearchIn.length();
        start = clampAndTruncateToUnsigned(positionArg.toInteger(globalObject), 0, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.hasInfixStartingAt(searchString, start)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncEndsWith(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    String stringToSearchIn = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue a0 = callFrame->argument(0);
    bool isRegularExpression = isRegExp(vm, globalObject, a0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (isRegularExpression)
        return throwVMTypeError(globalObject, scope, "Argument to String.prototype.endsWith cannot be a RegExp");

    String searchString = a0.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned length = stringToSearchIn.length();

    JSValue endPositionArg = callFrame->argument(1);
    unsigned end = length;
    if (endPositionArg.isInt32())
        end = std::max(0, endPositionArg.asInt32());
    else if (!endPositionArg.isUndefined()) {
        end = clampAndTruncateToUnsigned(endPositionArg.toInteger(globalObject), 0, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.hasInfixEndingAt(searchString, std::min(end, length))));
}

static EncodedJSValue stringIncludesImpl(JSGlobalObject* globalObject, VM& vm, String stringToSearchIn, String searchString, JSValue positionArg)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned start = 0;
    if (positionArg.isInt32())
        start = std::max(0, positionArg.asInt32());
    else {
        unsigned length = stringToSearchIn.length();
        start = clampAndTruncateToUnsigned(positionArg.toInteger(globalObject), 0, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.find(searchString, start) != notFound));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncIncludes(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    String stringToSearchIn = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue a0 = callFrame->argument(0);
    bool isRegularExpression = isRegExp(vm, globalObject, a0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (isRegularExpression)
        return throwVMTypeError(globalObject, scope, "Argument to String.prototype.includes cannot be a RegExp");

    String searchString = a0.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue positionArg = callFrame->argument(1);

    RELEASE_AND_RETURN(scope, stringIncludesImpl(globalObject, vm, stringToSearchIn, searchString, positionArg));
}

EncodedJSValue JSC_HOST_CALL builtinStringIncludesInternal(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    ASSERT(checkObjectCoercible(thisValue));

    String stringToSearchIn = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue a0 = callFrame->uncheckedArgument(0);
    String searchString = a0.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue positionArg = callFrame->argument(1);

    RELEASE_AND_RETURN(scope, stringIncludesImpl(globalObject, vm, stringToSearchIn, searchString, positionArg));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncIterator(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    JSString* string = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(JSStringIterator::create(vm, globalObject->stringIteratorStructure(), string));
}

enum class NormalizationForm { NFC, NFD, NFKC, NFKD };

static constexpr bool normalizationAffects8Bit(NormalizationForm form)
{
    switch (form) {
    case NormalizationForm::NFC:
        return false;
    case NormalizationForm::NFD:
        return true;
    case NormalizationForm::NFKC:
        return false;
    case NormalizationForm::NFKD:
        return true;
    default:
        ASSERT_NOT_REACHED();
    }
    return true;
}

static const UNormalizer2* normalizer(NormalizationForm form)
{
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer = nullptr;
    switch (form) {
    case NormalizationForm::NFC:
        normalizer = unorm2_getNFCInstance(&status);
        break;
    case NormalizationForm::NFD:
        normalizer = unorm2_getNFDInstance(&status);
        break;
    case NormalizationForm::NFKC:
        normalizer = unorm2_getNFKCInstance(&status);
        break;
    case NormalizationForm::NFKD:
        normalizer = unorm2_getNFKDInstance(&status);
        break;
    }
    ASSERT(normalizer);
    ASSERT(U_SUCCESS(status));
    return normalizer;
}

static JSValue normalize(JSGlobalObject* globalObject, JSString* string, NormalizationForm form)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto viewWithString = string->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    StringView view = viewWithString.view;
    if (view.is8Bit() && (!normalizationAffects8Bit(form) || charactersAreAllASCII(view.characters8(), view.length())))
        RELEASE_AND_RETURN(scope, string);

    const UNormalizer2* normalizer = JSC::normalizer(form);

    // Since ICU does not offer functions that can perform normalization or check for
    // normalization with input that is Latin-1, we need to upconvert to UTF-16 at this point.
    auto characters = view.upconvertedCharacters();

    UErrorCode status = U_ZERO_ERROR;
    UBool isNormalized = unorm2_isNormalized(normalizer, characters, view.length(), &status);
    ASSERT(U_SUCCESS(status));
    if (isNormalized)
        RELEASE_AND_RETURN(scope, string);

    int32_t normalizedStringLength = unorm2_normalize(normalizer, characters, view.length(), nullptr, 0, &status);
    ASSERT(status == U_BUFFER_OVERFLOW_ERROR);

    UChar* buffer;
    auto result = StringImpl::tryCreateUninitialized(normalizedStringLength, buffer);
    if (!result)
        return throwOutOfMemoryError(globalObject, scope);

    status = U_ZERO_ERROR;
    unorm2_normalize(normalizer, characters, view.length(), buffer, normalizedStringLength, &status);
    ASSERT(U_SUCCESS(status));

    RELEASE_AND_RETURN(scope, jsString(vm, WTFMove(result)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncNormalize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    JSString* string = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto form = NormalizationForm::NFC;
    JSValue formValue = callFrame->argument(0);
    if (!formValue.isUndefined()) {
        String formString = formValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (formString == "NFC")
            form = NormalizationForm::NFC;
        else if (formString == "NFD")
            form = NormalizationForm::NFD;
        else if (formString == "NFKC")
            form = NormalizationForm::NFKC;
        else if (formString == "NFKD")
            form = NormalizationForm::NFKD;
        else
            return throwVMRangeError(globalObject, scope, "argument does not match any normalization form"_s);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(normalize(globalObject, string, form)));
}

} // namespace JSC
