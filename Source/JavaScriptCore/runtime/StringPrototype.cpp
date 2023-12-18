/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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
#include "CachedCall.h"
#include "ExecutableBaseInlines.h"
#include "FrameTracers.h"
#include "IntegrityInlines.h"
#include "InterpreterInlines.h"
#include "IntlCollator.h"
#include "IntlObjectInlines.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSStringIterator.h"
#include "ObjectConstructor.h"
#include "ParseInt.h"
#include "RegExpConstructor.h"
#include "RegExpGlobalDataInlines.h"
#include "StringPrototypeInlines.h"
#include "StringReplaceCacheInlines.h"
#include "StringSplitCacheInlines.h"
#include "SuperSampler.h"
#include "VMEntryScopeInlines.h"
#include <algorithm>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(StringPrototype);

static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncToString);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncCharAt);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncCharCodeAt);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncCodePointAt);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncIndexOf);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncLastIndexOf);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncReplaceUsingRegExp);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncReplaceUsingStringSearch);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncReplaceAllUsingStringSearch);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncSlice);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncSubstr);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncToLowerCase);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncToUpperCase);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncLocaleCompare);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncToLocaleLowerCase);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncToLocaleUpperCase);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncTrim);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncTrimStart);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncTrimEnd);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncStartsWith);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncEndsWith);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncIncludes);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncNormalize);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncIterator);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncIsWellFormed);
static JSC_DECLARE_HOST_FUNCTION(stringProtoFuncToWellFormed);

}

#include "StringPrototype.lut.h"

namespace JSC {

const ClassInfo StringPrototype::s_info = { "String"_s, &StringObject::s_info, &stringPrototypeTable, nullptr, CREATE_METHOD_TABLE(StringPrototype) };

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

void StringPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm, jsEmptyString(vm));
    ASSERT(inherits(info()));

    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, stringProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, StringPrototypeValueOfIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->valueOf, stringProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, StringPrototypeValueOfIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("charAt"_s, stringProtoFuncCharAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, CharAtIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("charCodeAt"_s, stringProtoFuncCharCodeAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, CharCodeAtIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("codePointAt"_s, stringProtoFuncCodePointAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, StringPrototypeCodePointAtIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().indexOfPublicName(), stringProtoFuncIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, StringPrototypeIndexOfIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf"_s, stringProtoFuncLastIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().replaceUsingRegExpPrivateName(), stringProtoFuncReplaceUsingRegExp, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, StringPrototypeReplaceRegExpIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().replaceUsingStringSearchPrivateName(), stringProtoFuncReplaceUsingStringSearch, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, StringPrototypeReplaceStringIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().replaceAllUsingStringSearchPrivateName(), stringProtoFuncReplaceAllUsingStringSearch, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("slice"_s, stringProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, StringPrototypeSliceIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("substr"_s, stringProtoFuncSubstr, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "substring"_s), globalObject->stringProtoSubstringFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("toLowerCase"_s, stringProtoFuncToLowerCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, StringPrototypeToLowerCaseIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toUpperCase"_s, stringProtoFuncToUpperCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("localeCompare"_s, stringProtoFuncLocaleCompare, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, StringPrototypeLocaleCompareIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toLocaleLowerCase"_s, stringProtoFuncToLocaleLowerCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toLocaleUpperCase"_s, stringProtoFuncToLocaleUpperCase, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("trim"_s, stringProtoFuncTrim, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("startsWith"_s, stringProtoFuncStartsWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("endsWith"_s, stringProtoFuncEndsWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("includes"_s, stringProtoFuncIncludes, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("normalize"_s, stringProtoFuncNormalize, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().charCodeAtPrivateName(), stringProtoFuncCharCodeAt, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, CharCodeAtIntrinsic);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().atPublicName(), stringPrototypeAtCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* trimStartFunction = JSFunction::create(vm, globalObject, 0, "trimStart"_s, stringProtoFuncTrimStart, ImplementationVisibility::Public);
    JSFunction* trimEndFunction = JSFunction::create(vm, globalObject, 0, "trimEnd"_s, stringProtoFuncTrimEnd, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimStart"_s), trimStartFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimLeft"_s), trimStartFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimEnd"_s), trimEndFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "trimRight"_s), trimEndFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* iteratorFunction = JSFunction::create(vm, globalObject, 0, "[Symbol.iterator]"_s, stringProtoFuncIterator, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, iteratorFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().substrPrivateName(), stringProtoFuncSubstr, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().endsWithPrivateName(), stringProtoFuncEndsWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);

    if (Options::useStringWellFormed()) {
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->isWellFormed, stringProtoFuncIsWellFormed, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toWellFormed, stringProtoFuncToWellFormed, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    }

    // The constructor will be added later, after StringConstructor has been built
}

StringPrototype* StringPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    StringPrototype* prototype = new (NotNull, allocateCell<StringPrototype>(vm)) StringPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

// ------------------------------ Functions --------------------------

NEVER_INLINE void substituteBackreferencesSlow(StringBuilder& result, StringView replacement, StringView source, const int* ovector, RegExp* reg, size_t i)
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
            unsigned backrefIndex = reg->subpatternIdForGroupName(replacement.substring(i + 2, nameLength), ovector);

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

static ALWAYS_INLINE JSString* jsSpliceSubstrings(JSGlobalObject* globalObject, JSString* sourceVal, const String& source, const Range<int32_t>* substringRanges, int rangeCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (rangeCount == 1) {
        int sourceSize = source.length();
        int position = substringRanges[0].begin();
        int length = substringRanges[0].distance();
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
        totalLength += substringRanges[i].distance();
    ASSERT(totalLength <= static_cast<int>(String::MaxLength));

    if (!totalLength)
        return jsEmptyString(vm);

    if (source.is8Bit()) {
        LChar* buffer;
        const LChar* sourceData = source.characters8();
        auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
        if (!impl) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        Checked<int, AssertNoOverflow> bufferPos = 0;
        for (int i = 0; i < rangeCount; i++) {
            int srcLen = substringRanges[i].distance();
            StringImpl::copyCharacters(buffer + bufferPos.value(), sourceData + substringRanges[i].begin(), srcLen);
            bufferPos += srcLen;
        }

        RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
    }

    UChar* buffer;
    const UChar* sourceData = source.characters16();

    auto impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
    if (!impl) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    Checked<int, AssertNoOverflow> bufferPos = 0;
    for (int i = 0; i < rangeCount; i++) {
        int srcLen = substringRanges[i].distance();
        StringImpl::copyCharacters(buffer + bufferPos.value(), sourceData + substringRanges[i].begin(), srcLen);
        bufferPos += srcLen;
    }

    RELEASE_AND_RETURN(scope, jsString(vm, impl.releaseNonNull()));
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

    Vector<Range<int32_t>, 16> sourceRanges;
    unsigned sourceLen = source.length();

    while (true) {
        MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (!result)
            break;

        if (lastIndex < result.start) {
            if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start)))
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
        if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen)))
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstrings(globalObject, string, source, sourceRanges.data(), sourceRanges.size()));
}

static ALWAYS_INLINE JSString* replaceUsingRegExpSearchWithCache(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& source, RegExp* regExp, JSFunction* replaceFunction)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    SuperSamplerScope superSamplerScope(true);

    ASSERT(source.length() >= Options::thresholdForStringReplaceCache());
    // Currently not caching results when named captures are specified.
    ASSERT(!regExp->hasNamedCaptures());

    unsigned sourceLen = source.length();
    unsigned cachedCount = regExp->numSubpatterns() + 2;
    unsigned argCount = cachedCount + 1;

    JSImmutableButterfly* result = nullptr;

    auto* entry = vm.stringReplaceCache.get(source, regExp);
    if (!entry) {
        // This is either a loop (if global is set) or a one-way (if not).
        // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
        size_t lastIndex = 0;
        unsigned startPosition = 0;
        MarkedArgumentBuffer results;
        while (true) {
            int* ovector;
            MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!result)
                break;

            for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
                int matchStart = ovector[i * 2];
                int matchLen = ovector[i * 2 + 1] - matchStart;

                JSValue patternValue;

                if (matchStart < 0)
                    patternValue = jsUndefined();
                else {
                    patternValue = jsSubstring(globalObject, string, matchStart, matchLen);
                    RETURN_IF_EXCEPTION(scope, { });
                }

                results.append(patternValue);
            }

            results.append(jsNumber(result.start));

            lastIndex = result.end;
            startPosition = lastIndex;

            // special case of empty match
            if (result.empty()) {
                startPosition++;
                if (startPosition > sourceLen)
                    break;
                if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                    startPosition++;
                    if (startPosition > sourceLen)
                        break;
                }
            }
        }

        // Nothing matches.
        if (results.isEmpty())
            return string;

        result = JSImmutableButterfly::tryCreateFromArgList(vm, results);
        if (UNLIKELY(!result)) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        vm.stringReplaceCache.set(source, regExp, result, globalObject->regExpGlobalData().matchResult(), globalObject->regExpGlobalData().ovector());
    } else {
        result = entry->m_result;
        auto lastMatch = entry->m_lastMatch;
        auto matchResult = entry->m_matchResult;
        globalObject->regExpGlobalData().resetResultFromCache(globalObject, regExp, string, matchResult, WTFMove(lastMatch));
    }

    // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
    unsigned length = result->length();
    unsigned items = length / cachedCount;
    size_t lastIndex = 0;
    Vector<Range<int32_t>, 16> sourceRanges;
    Vector<String, 16> replacements;

    if (UNLIKELY(!sourceRanges.tryReserveCapacity(items + 1))) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    if (UNLIKELY(!replacements.tryReserveCapacity(items))) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    replacements.grow(items);

    CachedCall cachedCall(globalObject, replaceFunction, argCount);
    RETURN_IF_EXCEPTION(scope, nullptr);
    size_t replacementIndex = 0;
    for (unsigned cursor = 0; cursor < length; cursor += cachedCount) {
        cachedCall.clearArguments();
        for (unsigned i = 0; i < cachedCount; ++i)
            cachedCall.appendArgument(result->get(cursor + i));
        cachedCall.appendArgument(string);

        int32_t start = result->get(cursor + cachedCount - 1).asInt32();
        int32_t end = start + asString(result->get(cursor))->length();

        sourceRanges.constructAndAppend(lastIndex, start);

        cachedCall.setThis(jsUndefined());
        if (UNLIKELY(cachedCall.hasOverflowedArguments())) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        JSValue jsResult = cachedCall.call();
        RETURN_IF_EXCEPTION(scope, nullptr);
        auto string = jsResult.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        replacements[replacementIndex++] = WTFMove(string);

        lastIndex = end;
    }
    ASSERT(replacementIndex == replacements.size());

    if (static_cast<unsigned>(lastIndex) < sourceLen)
        sourceRanges.constructAndAppend(lastIndex, sourceLen);
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, string, source, sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size()));
}

static ALWAYS_INLINE JSString* replaceUsingRegExpSearch(
    VM& vm, JSGlobalObject* globalObject, JSString* string, JSValue searchValue, const CallData& callData,
    String& replacementString, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String source = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned sourceLen = source.length();
    RegExpObject* regExpObject = jsCast<RegExpObject*>(searchValue);
    RegExp* regExp = regExpObject->regExp();
    bool global = regExp->global();
    bool hasNamedCaptures = regExp->hasNamedCaptures();

    if (global) {
        // ES5.1 15.5.4.10 step 8.a.
        regExpObject->setLastIndex(globalObject, 0);
        RETURN_IF_EXCEPTION(scope, nullptr);

        if (callData.type == CallData::Type::None && !replacementString.length())
            RELEASE_AND_RETURN(scope, removeUsingRegExpSearch(vm, globalObject, string, source, regExp));

        if (callData.type == CallData::Type::JS && !hasNamedCaptures && sourceLen >= Options::thresholdForStringReplaceCache())
            RELEASE_AND_RETURN(scope, replaceUsingRegExpSearchWithCache(vm, globalObject, string, source, regExp, jsCast<JSFunction*>(replaceValue)));
    }

    size_t lastIndex = 0;
    unsigned startPosition = 0;

    Vector<Range<int32_t>, 16> sourceRanges;
    Vector<String, 16> replacements;

    // This is either a loop (if global is set) or a one-way (if not).
    if (global && callData.type == CallData::Type::JS) {
        // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
        int argCount = regExp->numSubpatterns() + 1 + 2;
        if (hasNamedCaptures)
            ++argCount;
        JSFunction* func = jsCast<JSFunction*>(replaceValue);
        CachedCall cachedCall(globalObject, func, argCount);
        RETURN_IF_EXCEPTION(scope, nullptr);
        while (true) {
            int* ovector;
            MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!result)
                break;

            if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start)))
                OUT_OF_MEMORY(globalObject, scope);

            cachedCall.clearArguments();
            JSObject* groups = hasNamedCaptures ? constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure()) : nullptr;

            for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
                int matchStart = ovector[i * 2];
                int matchLen = ovector[i * 2 + 1] - matchStart;

                JSValue patternValue;

                if (matchStart < 0)
                    patternValue = jsUndefined();
                else {
                    patternValue = jsSubstring(vm, globalObject, string, matchStart, matchLen);
                    RETURN_IF_EXCEPTION(scope, nullptr);
                }

                cachedCall.appendArgument(patternValue);

                if (i && hasNamedCaptures) {
                    String groupName = regExp->getCaptureGroupNameForSubpatternId(i);
                    if (!groupName.isEmpty()) {
                        auto captureIndex = regExp->subpatternIdForGroupName(groupName, ovector);

                        if (captureIndex == i)
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), patternValue);
                        else if (captureIndex > 0) {
                            int captureStart = ovector[captureIndex * 2];
                            int captureLen = ovector[captureIndex * 2 + 1] - captureStart;
                            JSValue captureValue;
                            if (captureStart < 0)
                                captureValue = jsUndefined();
                            else {
                                captureValue = jsSubstring(vm, globalObject, string, captureStart, captureLen);
                                RETURN_IF_EXCEPTION(scope, nullptr);
                            }
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), captureValue);
                        } else
                            groups->putDirect(vm, Identifier::fromString(vm, groupName), jsUndefined());
                    }
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
                if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                    startPosition++;
                    if (startPosition > sourceLen)
                        break;
                }
            }
        }
    } else {
        do {
            int* ovector;
            MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, source, startPosition, &ovector);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (!result)
                break;

            if (callData.type != CallData::Type::None) {
                if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start)))
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
                        patternValue = jsSubstring(vm, globalObject, string, matchStart, matchLen);
                        RETURN_IF_EXCEPTION(scope, nullptr);
                    }

                    args.append(patternValue);

                    if (i && hasNamedCaptures) {
                        String groupName = regExp->getCaptureGroupNameForSubpatternId(i);
                        if (!groupName.isEmpty()) {
                            auto captureIndex = regExp->subpatternIdForGroupName(groupName, ovector);

                            if (captureIndex == i)
                                groups->putDirect(vm, Identifier::fromString(vm, groupName), patternValue);
                            else if (captureIndex > 0) {
                                int captureStart = ovector[captureIndex * 2];
                                int captureLen = ovector[captureIndex * 2 + 1] - captureStart;
                                JSValue captureValue;
                                if (captureStart < 0)
                                    captureValue = jsUndefined();
                                else {
                                    captureValue = jsSubstring(vm, globalObject, string, captureStart, captureLen);
                                    RETURN_IF_EXCEPTION(scope, nullptr);
                                }
                                groups->putDirect(vm, Identifier::fromString(vm, groupName), captureValue);
                            } else
                                groups->putDirect(vm, Identifier::fromString(vm, groupName), jsUndefined());
                        }
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

                JSValue replacement = call(globalObject, replaceValue, callData, jsUndefined(), args);
                RETURN_IF_EXCEPTION(scope, nullptr);
                String replacementString = replacement.toWTFString(globalObject);
                RETURN_IF_EXCEPTION(scope, nullptr);
                replacements.append(replacementString);
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else {
                int replLen = replacementString.length();
                if (lastIndex < result.start || replLen) {
                    if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, result.start)))
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
                if (U16_IS_LEAD(source[startPosition - 1]) && U16_IS_TRAIL(source[startPosition])) {
                    startPosition++;
                    if (startPosition > sourceLen)
                        break;
                }
            }
        } while (global);
    }

    if (!lastIndex && replacements.isEmpty())
        return string;

    if (static_cast<unsigned>(lastIndex) < sourceLen) {
        if (UNLIKELY(!sourceRanges.tryConstructAndAppend(lastIndex, sourceLen)))
            OUT_OF_MEMORY(globalObject, scope);
    }
    RELEASE_AND_RETURN(scope, jsSpliceSubstringsWithSeparators(globalObject, string, source, sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size()));
}

IGNORE_WARNINGS_BEGIN("frame-address")

JSC_DEFINE_JIT_OPERATION(operationStringProtoFuncReplaceRegExpEmptyStr, JSCell*, (JSGlobalObject* globalObject, JSString* thisValue, RegExpObject* searchValue))
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
        RELEASE_AND_RETURN(scope, removeUsingRegExpSearch(vm, globalObject, thisValue, WTFMove(source), regExp));
    }

    CallData callData;
    String replacementString = emptyString();
    RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(
        vm, globalObject, thisValue, searchValue, callData, replacementString, JSValue()));
}

JSC_DEFINE_JIT_OPERATION(operationStringProtoFuncReplaceRegExpString, JSCell*, (JSGlobalObject* globalObject, JSString* thisValue, RegExpObject* searchValue, JSString* replaceString))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    CallData callData;
    String replacementString = replaceString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(
        vm, globalObject, thisValue, searchValue, callData, replacementString, replaceString));
}

static ALWAYS_INLINE JSString* replaceUsingRegExpSearch(VM& vm, JSGlobalObject* globalObject, JSString* string, JSValue searchValue, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String replacementString;
    auto callData = JSC::getCallData(replaceValue);
    if (callData.type == CallData::Type::None) {
        replacementString = replaceValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(
        vm, globalObject, string, searchValue, callData, replacementString, replaceValue));
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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncRepeatCharacter, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

ALWAYS_INLINE JSString* replace(VM& vm, JSGlobalObject* globalObject, JSValue thisValue, JSValue searchValue, JSValue replaceValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!checkObjectCoercible(thisValue)) {
        throwVMTypeError(globalObject, scope);
        return nullptr;
    }
    JSString* string = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (searchValue.inherits<RegExpObject>())
        RELEASE_AND_RETURN(scope, replaceUsingRegExpSearch(vm, globalObject, string, searchValue, replaceValue));

    String thisString = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String searchString = searchValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, replaceUsingStringSearch(vm, globalObject, string, WTFMove(thisString), WTFMove(searchString), replaceValue, StringReplaceMode::Single));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncReplaceUsingRegExp, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = callFrame->thisValue().toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue searchValue = callFrame->argument(0);
    if (!searchValue.inherits<RegExpObject>())
        return JSValue::encode(jsUndefined());

    RELEASE_AND_RETURN(scope, JSValue::encode(replaceUsingRegExpSearch(vm, globalObject, string, searchValue, callFrame->argument(1))));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncReplaceUsingStringSearch, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = asString(callFrame->thisValue());

    String thisString = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    String searchString = asString(callFrame->uncheckedArgument(0))->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(replaceUsingStringSearch(vm, globalObject, string, WTFMove(thisString), WTFMove(searchString), callFrame->uncheckedArgument(1), StringReplaceMode::Single)));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncReplaceAllUsingStringSearch, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* string = asString(callFrame->thisValue());

    String thisString = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    String searchString = asString(callFrame->uncheckedArgument(0))->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(replaceUsingStringSearch(vm, globalObject, string, WTFMove(thisString), WTFMove(searchString), callFrame->uncheckedArgument(1), StringReplaceMode::Global)));
}

JSC_DEFINE_JIT_OPERATION(operationStringProtoFuncReplaceGeneric, JSCell*, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue searchValue, EncodedJSValue replaceValue))
{
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    
    return replace(vm, globalObject, JSValue::decode(thisValue), JSValue::decode(searchValue), JSValue::decode(replaceValue));
}

IGNORE_WARNINGS_END

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    // Also used for valueOf.

    if (thisValue.isString()) {
        Integrity::auditStructureID(thisValue.asCell()->structureID());
        return JSValue::encode(thisValue);
    }

    auto* stringObject = jsDynamicCast<StringObject*>(thisValue);
    if (!stringObject)
        return throwVMTypeError(globalObject, scope);

    Integrity::auditStructureID(stringObject->structureID());
    return JSValue::encode(stringObject->internalValue());
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncCharAt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    auto* thisString = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto viewWithString = thisString->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    StringView view = viewWithString.view;
    JSValue a0 = callFrame->argument(0);
    if (a0.isUInt32()) {
        uint32_t i = a0.asUInt32();
        if (i < view.length())
            return JSValue::encode(jsSingleCharacterString(vm, view[i]));
        return JSValue::encode(jsEmptyString(vm));
    }
    double dpos = a0.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (dpos >= 0 && dpos < view.length())
        return JSValue::encode(jsSingleCharacterString(vm, view[static_cast<unsigned>(dpos)]));
    return JSValue::encode(jsEmptyString(vm));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncCharCodeAt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    auto* thisString = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto viewWithString = thisString->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    StringView view = viewWithString.view;
    JSValue a0 = callFrame->argument(0);
    if (a0.isUInt32()) {
        uint32_t i = a0.asUInt32();
        if (i < view.length())
            return JSValue::encode(jsNumber(view[i]));
        return JSValue::encode(jsNaN());
    }
    double dpos = a0.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (dpos >= 0 && dpos < view.length())
        return JSValue::encode(jsNumber(view[static_cast<int>(dpos)]));
    return JSValue::encode(jsNaN());
}

static inline char32_t codePointAt(const String& string, unsigned position, unsigned length)
{
    RELEASE_ASSERT(position < length);
    if (string.is8Bit())
        return string.characters8()[position];
    char32_t character;
    U16_NEXT(string.characters16(), position, length, character);
    return character;
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncCodePointAt, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

    double doublePosition = argument0.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (doublePosition >= 0 && doublePosition < length)
        return JSValue::encode(jsNumber(codePointAt(string, static_cast<unsigned>(doublePosition), length)));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue stringIndexOfImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
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
            double dpos = a1.toIntegerOrInfinity(globalObject);
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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return stringIndexOfImpl(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(builtinStringIndexOfInternal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->thisValue().isString());
    ASSERT(callFrame->argument(0).isString());
    ASSERT(callFrame->argument(1).isNumber() || callFrame->argument(1).isUndefined());
    return stringIndexOfImpl(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncLastIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncSlice, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
    double start = a0.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    double end = a1.isUndefined() ? length : a1.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(stringSlice<double>(globalObject, vm, string, length, start, end)));
}

// Return true in case of early return (resultLength got to limitLength).
template<typename CharacterType, typename Indice>
static ALWAYS_INLINE bool splitStringByOneCharacterImpl(Indice& result, StringImpl* string, UChar separatorCharacter, unsigned limitLength)
{
    // 12. Let q = p.
    size_t matchPosition;
    size_t position = 0;
    unsigned stringLength = string->length();
    const CharacterType* characters = string->characters<CharacterType>();
    // 13. Repeat, while q != s
    //   a. Call SplitMatch(S, q, R) and let z be its MatchResult result.
    //   b. If z is failure, then let q = q+1.
    //   c. Else, z is not failure
    while ((matchPosition = WTF::find(characters, stringLength, separatorCharacter, position)) != notFound) {
        // 1. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
        //    through q (exclusive).
        // 2. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(lengthA),
        //    Property Descriptor {[[Value]]: T, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
        result.append(matchPosition);
        // 3. Increment lengthA by 1.
        // 4. If lengthA == lim, return A.
        if (result.size() == limitLength)
            return true;

        // 5. Let p = e.
        // 8. Let q = p.
        position = matchPosition + 1;
    }
    return false;
}

// ES 21.1.3.17 String.prototype.split(separator, limit)
JSC_DEFINE_HOST_FUNCTION(stringProtoFuncSplitFast, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    ASSERT(checkObjectCoercible(thisValue));

    // 3. Let S be the result of calling ToString, giving it the this value as its argument.
    // 7. Let s be the number of characters in S.
    JSString* thisString = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    String input = thisString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(!input.isNull());

    // 6. If limit is undefined, let lim = 2^32-1; else let lim = ToUint32(limit).
    JSValue limitValue = callFrame->uncheckedArgument(1);
    unsigned limit = 0xFFFFFFFFu;
    if (!limitValue.isUndefined()) {
        limit = limitValue.toUInt32(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // 9. If separator is a RegExp object (its [[Class]] is "RegExp"), let R = separator;
    //    otherwise let R = ToString(separator).
    JSValue separatorValue = callFrame->uncheckedArgument(0);
    String separator = separatorValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    unsigned separatorLength = separator.length();

    // 10. If lim == 0, return A.
    if (!limit)
        RELEASE_AND_RETURN(scope, JSValue::encode(constructEmptyArray(globalObject, nullptr)));

    // 11. If separator is undefined, then
    if (separatorValue.isUndefined()) {
        // a. Call the [[DefineOwnProperty]] internal method of A with arguments "0",
        MarkedArgumentBuffer result;
        result.appendWithCrashOnOverflow(jsStringWithReuse(globalObject, thisString, input));
        RETURN_IF_EXCEPTION(scope, { });
        // b. Return A.
        RELEASE_AND_RETURN(scope, JSValue::encode(constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), result)));
    }

    if (LIKELY(limit == 0xFFFFFFFFu && !globalObject->isHavingABadTime())) {
        if (auto* immutableButterfly = vm.stringSplitCache.get(input, separator)) {
            Structure* arrayStructure = globalObject->originalArrayStructureForIndexingType(CopyOnWriteArrayWithContiguous);
            return JSValue::encode(JSArray::createWithButterfly(vm, nullptr, arrayStructure, immutableButterfly->toButterfly()));
        }
    }

    auto& result = vm.stringSplitIndice;
    result.shrink(0);

    auto cacheAndCreateArray = [&]() -> JSArray* {
        if (result.isEmpty())
            return constructEmptyArray(globalObject, nullptr);

        if (LIKELY(limit == 0xFFFFFFFFu && !globalObject->isHavingABadTime() && result.size() < MIN_SPARSE_ARRAY_INDEX)) {
            auto* newButterfly = JSImmutableButterfly::create(vm, CopyOnWriteArrayWithContiguous, result.size());
            unsigned start = 0;
            for (unsigned i = 0, size = result.size(); i < size; ++i) {
                unsigned end = result[i];
                auto* string = jsSubstring(globalObject, thisString, start, end - start);
                RETURN_IF_EXCEPTION(scope, { });
                newButterfly->setIndex(vm, i, string);
                start = end + separatorLength;
            }
            vm.stringSplitCache.set(input, separator, newButterfly);
            Structure* arrayStructure = globalObject->originalArrayStructureForIndexingType(CopyOnWriteArrayWithContiguous);
            return JSArray::createWithButterfly(vm, nullptr, arrayStructure, newButterfly->toButterfly());
        }

        auto* array = constructEmptyArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), result.size());
        RETURN_IF_EXCEPTION(scope, { });
        unsigned start = 0;
        for (unsigned i = 0, size = result.size(); i < size; ++i) {
            unsigned end = result[i];
            auto* string = jsSubstring(globalObject, thisString, start, end - start);
            RETURN_IF_EXCEPTION(scope, { });
            array->putDirectIndex(globalObject, i, string);
            RETURN_IF_EXCEPTION(scope, { });
            start = end + separatorLength;
        }
        return array;
    };

    // 12. If s == 0, then
    if (input.isEmpty()) {
        // a. Let z be SplitMatch(S, 0, R) where S is input, R is separator.
        // b. If z is not false, return A.
        // c. Call CreateDataProperty(A, "0", S).
        // d. Return A.
        scope.release();
        if (!separator.isEmpty())
            result.append(input.length());
        return JSValue::encode(cacheAndCreateArray());
    }

    // Optimized case for splitting on the empty string.
    if (!separatorLength) {
        unsigned resultSize = std::min(limit, input.length());
        // Zero limt/input length handled in steps 9/11 respectively, above.
        ASSERT(resultSize);

        if (LIKELY(limit == 0xFFFFFFFFu && !globalObject->isHavingABadTime() && resultSize < MIN_SPARSE_ARRAY_INDEX)) {
            auto* newButterfly = JSImmutableButterfly::create(vm, CopyOnWriteArrayWithContiguous, resultSize);
            for (unsigned i = 0; i < resultSize; ++i)
                newButterfly->setIndex(vm, i, jsSingleCharacterString(vm, input[i]));
            vm.stringSplitCache.set(input, separator, newButterfly);
            Structure* arrayStructure = globalObject->originalArrayStructureForIndexingType(CopyOnWriteArrayWithContiguous);
            return JSValue::encode(JSArray::createWithButterfly(vm, nullptr, arrayStructure, newButterfly->toButterfly()));
        }

        auto* array = constructEmptyArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), resultSize);
        RETURN_IF_EXCEPTION(scope, { });
        for (unsigned i = 0; i < resultSize; ++i) {
            array->putDirectIndex(globalObject, i, jsSingleCharacterString(vm, input[i]));
            RETURN_IF_EXCEPTION(scope, { });
        }
        return JSValue::encode(array);
    }

    // 3 cases:
    // -separator length == 1, 8 bits
    // -separator length == 1, 16 bits
    // -separator length > 1
    StringImpl* stringImpl = input.impl();
    StringImpl* separatorImpl = separator.impl();

    if (separatorLength == 1) {
        UChar separatorCharacter;
        if (separatorImpl->is8Bit())
            separatorCharacter = separatorImpl->characters8()[0];
        else
            separatorCharacter = separatorImpl->characters16()[0];

        if (stringImpl->is8Bit()) {
            if (splitStringByOneCharacterImpl<LChar>(result, stringImpl, separatorCharacter, limit))
                RELEASE_AND_RETURN(scope, JSValue::encode(cacheAndCreateArray()));
        } else {
            if (splitStringByOneCharacterImpl<UChar>(result, stringImpl, separatorCharacter, limit))
                RELEASE_AND_RETURN(scope, JSValue::encode(cacheAndCreateArray()));
        }
    } else {
        // 13. Let q = p.
        size_t matchPosition;
        // 14. Repeat, while q != s
        //   a. let e be SplitMatch(S, q, R).
        //   b. If e is failure, then let q = q+1.
        //   c. Else, e is an integer index <= s.
        size_t position = 0;
        while ((matchPosition = stringImpl->find(separatorImpl, position)) != notFound) {
            // 1. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
            //    through q (exclusive).
            // 2. Call CreateDataProperty(A, ToString(lengthA), T).
            result.append(matchPosition);
            // 3. Increment lengthA by 1.
            // 4. If lengthA == lim, return A.
            if (result.size() == limit)
                RELEASE_AND_RETURN(scope, JSValue::encode(cacheAndCreateArray()));

            // 5. Let p = e.
            // 6. Let q = p.
            position = matchPosition + separatorLength;
        }
    }

    // 15. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
    //     through s (exclusive).
    // 16. Call CreateDataProperty(A, ToString(lengthA), T).
    result.append(input.length());
    RELEASE_AND_RETURN(scope, JSValue::encode(cacheAndCreateArray()));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncSubstr, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);
    unsigned len;
    JSString* jsString = nullptr;
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

    double start = a0.toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    double length = a1.isUndefined() ? len : a1.toIntegerOrInfinity(globalObject);
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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncSubstring, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    JSString* jsString = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue a0 = callFrame->argument(0);
    JSValue a1 = callFrame->argument(1);

    if (LIKELY(a0.isInt32() && (a1.isUndefined() || a1.isInt32())))
        RELEASE_AND_RETURN(scope, JSValue::encode(stringSubstring(globalObject, jsString, a0.asInt32(), a1.isUndefined() ? std::nullopt : std::optional<int32_t>(a1.asInt32()))));

    int len = jsString->length();
    RELEASE_ASSERT(len >= 0);

    double start = a0.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    double end;
    if (!(start >= 0)) // check for negative values or NaN
        start = 0;
    else if (start > len)
        start = len;
    if (a1.isUndefined())
        end = len;
    else {
        end = a1.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncToLowerCase, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, WTFMove(lowercasedString))));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncToUpperCase, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, WTFMove(uppercasedString))));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncLocaleCompare, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

    JSValue locales = callFrame->argument(1);
    JSValue options = callFrame->argument(2);
    IntlCollator* collator = nullptr;
    if (locales.isUndefined() && options.isUndefined())
        collator = globalObject->defaultCollator();
    else {
        collator = IntlCollator::create(vm, globalObject->collatorStructure());
        collator->initializeCollator(globalObject, locales, options);
    }
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(collator->compareStrings(globalObject, string, that))));
}

enum class CaseConversionMode {
    Upper,
    Lower,
};
template<CaseConversionMode mode>
static EncodedJSValue toLocaleCase(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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

    JSValue localeValue = callFrame->argument(0);

    // Optimization for empty strings.
    if (s.isEmpty() && localeValue.isUndefined())
        return JSValue::encode(sVal);

    // 4. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, localeValue);

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
    // Note 1: As of Unicode 5.1, the availableLocales list contains the elements "az", "el", "lt", and "tr".
    // 11. Let locale be BestAvailableLocale(availableLocales, noExtensionsLocale).
    String locale = bestAvailableLocale(noExtensionsLocale, [](const String& candidate) {
        if (candidate.length() != 2)
            return false;
        switch (computeTwoCharacters16Code(candidate)) {
        case computeTwoCharacters16Code("az"_s):
        case computeTwoCharacters16Code("el"_s):
        case computeTwoCharacters16Code("lt"_s):
        case computeTwoCharacters16Code("tr"_s):
            return true;
        default:
            return false;
        }
    });

    // 12. If locale is undefined, let locale be "und".
    if (locale.isNull())
        locale = "und"_s;

    // Delegate the following steps to icu u_strToLower or u_strToUpper.
    // 13. Let cpList be a List containing in order the code points of S as defined in ES2015, 6.1.4, starting at the first element of S.
    // 14. For each code point c in cpList, if the Unicode Character Database provides a lower(/upper) case equivalent of c that is either language insensitive or for the language locale, then replace c in cpList with that/those equivalent code point(s).
    // 15. Let cuList be a new List.
    // 16. For each code point c in cpList, in order, append to cuList the elements of the UTF-16 Encoding (defined in ES2015, 6.1.4) of c.
    // 17. Let L be a String whose elements are, in order, the elements of cuList.

    // Most strings lower/upper case will be the same size as original, so try that first.
    Vector<UChar> buffer;
    buffer.reserveInitialCapacity(s.length());
    auto convertCase = mode == CaseConversionMode::Lower ? u_strToLower : u_strToUpper;
    auto status = callBufferProducingFunction(convertCase, buffer, StringView { s }.upconvertedCharacters().get(), s.length(), locale.utf8().data());
    if (U_FAILURE(status))
        return throwVMTypeError(globalObject, scope, String::fromLatin1(u_errorName(status)));

    // 18. Return L.
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, String { WTFMove(buffer) })));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncToLocaleLowerCase, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // 13.1.2 String.prototype.toLocaleLowerCase ([locales])
    // http://ecma-international.org/publications/standards/Ecma-402.htm
    return toLocaleCase<CaseConversionMode::Lower>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncToLocaleUpperCase, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // 13.1.3 String.prototype.toLocaleUpperCase ([locales])
    // http://ecma-international.org/publications/standards/Ecma-402.htm
    // This function interprets a string value as a sequence of code points, as described in ES2015, 6.1.4. This function behaves in exactly the same way as String.prototype.toLocaleLowerCase, except that characters are mapped to their uppercase equivalents as specified in the Unicode character database.
    return toLocaleCase<CaseConversionMode::Upper>(globalObject, callFrame);
}

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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncTrim, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue();
    return JSValue::encode(trimString(globalObject, thisValue, TrimStart | TrimEnd));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncTrimStart, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue();
    return JSValue::encode(trimString(globalObject, thisValue, TrimStart));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncTrimEnd, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncStartsWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
        return throwVMTypeError(globalObject, scope, "Argument to String.prototype.startsWith cannot be a RegExp"_s);

    String searchString = a0.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue positionArg = callFrame->argument(1);
    unsigned length = stringToSearchIn.length();
    unsigned start;
    if (positionArg.isInt32())
        start = std::min(clampTo<unsigned>(positionArg.asInt32()), length);
    else {
        start = clampAndTruncateToUnsigned(positionArg.toIntegerOrInfinity(globalObject), 0, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.hasInfixStartingAt(searchString, start)));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncEndsWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
        return throwVMTypeError(globalObject, scope, "Argument to String.prototype.endsWith cannot be a RegExp"_s);

    String searchString = a0.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue endPositionArg = callFrame->argument(1);
    unsigned length = stringToSearchIn.length();
    unsigned end;
    if (endPositionArg.isUndefined())
        end = length;
    else if (endPositionArg.isInt32())
        end = std::min(clampTo<unsigned>(endPositionArg.asInt32()), length);
    else {
        end = clampAndTruncateToUnsigned(endPositionArg.toIntegerOrInfinity(globalObject), 0, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.hasInfixEndingAt(searchString, end)));
}

static EncodedJSValue stringIncludesImpl(JSGlobalObject* globalObject, VM& vm, String stringToSearchIn, String searchString, JSValue positionArg)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto length = stringToSearchIn.length();
    unsigned start;
    if (positionArg.isInt32())
        start = std::min(clampTo<unsigned>(positionArg.asInt32()), length);
    else {
        start = clampAndTruncateToUnsigned(positionArg.toIntegerOrInfinity(globalObject), 0, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.find(searchString, start) != notFound));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncIncludes, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
        return throwVMTypeError(globalObject, scope, "Argument to String.prototype.includes cannot be a RegExp"_s);

    String searchString = a0.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue positionArg = callFrame->argument(1);

    RELEASE_AND_RETURN(scope, stringIncludesImpl(globalObject, vm, stringToSearchIn, searchString, positionArg));
}

JSC_DEFINE_HOST_FUNCTION(builtinStringIncludesInternal, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncIterator, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
    ASSERT(needsToGrowToProduceBuffer(status));

    UChar* buffer;
    auto result = StringImpl::tryCreateUninitialized(normalizedStringLength, buffer);
    if (!result)
        return throwOutOfMemoryError(globalObject, scope);

    status = U_ZERO_ERROR;
    unorm2_normalize(normalizer, characters, view.length(), buffer, normalizedStringLength, &status);
    ASSERT(U_SUCCESS(status));

    RELEASE_AND_RETURN(scope, jsString(vm, result.releaseNonNull()));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncNormalize, (JSGlobalObject* globalObject, CallFrame* callFrame))
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

        if (formString == "NFC"_s)
            form = NormalizationForm::NFC;
        else if (formString == "NFD"_s)
            form = NormalizationForm::NFD;
        else if (formString == "NFKC"_s)
            form = NormalizationForm::NFKC;
        else if (formString == "NFKD"_s)
            form = NormalizationForm::NFKD;
        else
            return throwVMRangeError(globalObject, scope, "argument does not match any normalization form"_s);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(normalize(globalObject, string, form)));
}

static inline std::optional<unsigned> illFormedIndex(const UChar* characters, unsigned length)
{
    for (unsigned index = 0; index < length; ++index) {
        UChar character = characters[index];
        if (!U16_IS_SURROGATE(character))
            continue;

        if (U16_IS_SURROGATE_TRAIL(character))
            return index;

        ASSERT(U16_IS_SURROGATE_LEAD(character));
        if ((index + 1) == length)
            return index;
        UChar nextCharacter = characters[index + 1];

        if (!U16_IS_SURROGATE(nextCharacter))
            return index;

        if (!U16_IS_SURROGATE_TRAIL(nextCharacter))
            return index;

        ++index; // Increment additionally.
    }
    return std::nullopt;
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncIsWellFormed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    // Latin-1 characters do not have surrogates.
    if (thisValue.isString() && asString(thisValue)->is8Bit())
        return JSValue::encode(jsBoolean(true));

    String string = thisValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (string.is8Bit())
        return JSValue::encode(jsBoolean(true));
    return JSValue::encode(jsBoolean(!illFormedIndex(string.characters16(), string.length())));
}

JSC_DEFINE_HOST_FUNCTION(stringProtoFuncToWellFormed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(globalObject, scope);

    // Latin-1 characters do not have surrogates.
    if (thisValue.isString() && asString(thisValue)->is8Bit())
        return JSValue::encode(thisValue);

    JSString* stringValue = thisValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (stringValue->is8Bit())
        return JSValue::encode(stringValue);

    String string = stringValue->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (string.is8Bit())
        return JSValue::encode(stringValue);

    const UChar* characters = string.characters16();
    unsigned length = string.length();
    auto firstIllFormedIndex = illFormedIndex(characters, length);
    if (!firstIllFormedIndex)
        return JSValue::encode(stringValue);

    Vector<UChar> buffer;
    buffer.reserveInitialCapacity(length);
    buffer.append(characters, firstIllFormedIndex.value());
    for (unsigned index = firstIllFormedIndex.value(); index < length; ++index) {
        UChar character = characters[index];

        if (!U16_IS_SURROGATE(character)) {
            buffer.append(character);
            continue;
        }

        if (U16_IS_SURROGATE_TRAIL(character)) {
            buffer.append(replacementCharacter);
            continue;
        }

        ASSERT(U16_IS_SURROGATE_LEAD(character));
        if ((index + 1) == length) {
            buffer.append(replacementCharacter);
            continue;
        }
        UChar nextCharacter = characters[index + 1];

        if (!U16_IS_SURROGATE(nextCharacter)) {
            buffer.append(replacementCharacter);
            continue;
        }

        if (!U16_IS_SURROGATE_TRAIL(nextCharacter)) {
            buffer.append(replacementCharacter);
            continue;
        }

        buffer.append(character);
        buffer.append(nextCharacter);
        index += 1;
    }

    return JSValue::encode(jsString(vm, String::adopt(WTFMove(buffer))));
}

} // namespace JSC
