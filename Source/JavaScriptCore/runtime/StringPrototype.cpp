/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#include "ButterflyInlines.h"
#include "CachedCall.h"
#include "CopiedSpaceInlines.h"
#include "Error.h"
#include "Executable.h"
#include "IntlObject.h"
#include "JSCInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSStringBuilder.h"
#include "JSStringIterator.h"
#include "Lookup.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "RegExpCache.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include <algorithm>
#include <unicode/uconfig.h>
#include <unicode/unorm.h>
#include <unicode/ustring.h>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringView.h>
#include <wtf/unicode/Collator.h>

using namespace WTF;

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(StringPrototype);

EncodedJSValue JSC_HOST_CALL stringProtoFuncToString(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncCharAt(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncCharCodeAt(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncCodePointAt(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncConcat(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncIndexOf(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncLastIndexOf(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncMatch(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncRepeat(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncReplace(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSearch(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSlice(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSplit(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstr(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstring(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToLowerCase(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToUpperCase(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncLocaleCompare(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleLowerCase(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleUpperCase(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncBig(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSmall(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncBlink(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncBold(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncFixed(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncItalics(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncStrike(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSub(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncSup(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncFontcolor(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncFontsize(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncAnchor(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncLink(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncTrim(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimLeft(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimRight(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncStartsWith(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncEndsWith(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncIncludes(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncNormalize(ExecState*);
EncodedJSValue JSC_HOST_CALL stringProtoFuncIterator(ExecState*);

const ClassInfo StringPrototype::s_info = { "String", &StringObject::s_info, 0, CREATE_METHOD_TABLE(StringPrototype) };

// ECMA 15.5.4
StringPrototype::StringPrototype(VM& vm, Structure* structure)
    : StringObject(vm, structure)
{
}

void StringPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject, JSString* nameAndMessage)
{
    Base::finishCreation(vm, nameAndMessage);
    ASSERT(inherits(info()));

    JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->toString, stringProtoFuncToString, DontEnum, 0, StringPrototypeValueOfIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->valueOf, stringProtoFuncToString, DontEnum, 0, StringPrototypeValueOfIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION("charAt", stringProtoFuncCharAt, DontEnum, 1, CharAtIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION("charCodeAt", stringProtoFuncCharCodeAt, DontEnum, 1, CharCodeAtIntrinsic);
    JSC_NATIVE_FUNCTION("codePointAt", stringProtoFuncCodePointAt, DontEnum, 1);
    JSC_NATIVE_FUNCTION("concat", stringProtoFuncConcat, DontEnum, 1);
    JSC_NATIVE_FUNCTION("indexOf", stringProtoFuncIndexOf, DontEnum, 1);
    JSC_NATIVE_FUNCTION("lastIndexOf", stringProtoFuncLastIndexOf, DontEnum, 1);
    JSC_NATIVE_FUNCTION("match", stringProtoFuncMatch, DontEnum, 1);
    JSC_NATIVE_FUNCTION("repeat", stringProtoFuncRepeat, DontEnum, 1);
    JSC_NATIVE_FUNCTION("replace", stringProtoFuncReplace, DontEnum, 2);
    JSC_NATIVE_FUNCTION("search", stringProtoFuncSearch, DontEnum, 1);
    JSC_NATIVE_FUNCTION("slice", stringProtoFuncSlice, DontEnum, 2);
    JSC_NATIVE_FUNCTION("split", stringProtoFuncSplit, DontEnum, 2);
    JSC_NATIVE_FUNCTION("substr", stringProtoFuncSubstr, DontEnum, 2);
    JSC_NATIVE_FUNCTION("substring", stringProtoFuncSubstring, DontEnum, 2);
    JSC_NATIVE_FUNCTION("toLowerCase", stringProtoFuncToLowerCase, DontEnum, 0);
    JSC_NATIVE_FUNCTION("toUpperCase", stringProtoFuncToUpperCase, DontEnum, 0);
    JSC_NATIVE_FUNCTION("localeCompare", stringProtoFuncLocaleCompare, DontEnum, 1);
#if ENABLE(INTL)
    JSC_NATIVE_FUNCTION("toLocaleLowerCase", stringProtoFuncToLocaleLowerCase, DontEnum, 0);
    JSC_NATIVE_FUNCTION("toLocaleUpperCase", stringProtoFuncToLocaleUpperCase, DontEnum, 0);
#else
    JSC_NATIVE_FUNCTION("toLocaleLowerCase", stringProtoFuncToLowerCase, DontEnum, 0);
    JSC_NATIVE_FUNCTION("toLocaleUpperCase", stringProtoFuncToUpperCase, DontEnum, 0);
#endif
    JSC_NATIVE_FUNCTION("big", stringProtoFuncBig, DontEnum, 0);
    JSC_NATIVE_FUNCTION("small", stringProtoFuncSmall, DontEnum, 0);
    JSC_NATIVE_FUNCTION("blink", stringProtoFuncBlink, DontEnum, 0);
    JSC_NATIVE_FUNCTION("bold", stringProtoFuncBold, DontEnum, 0);
    JSC_NATIVE_FUNCTION("fixed", stringProtoFuncFixed, DontEnum, 0);
    JSC_NATIVE_FUNCTION("italics", stringProtoFuncItalics, DontEnum, 0);
    JSC_NATIVE_FUNCTION("strike", stringProtoFuncStrike, DontEnum, 0);
    JSC_NATIVE_FUNCTION("sub", stringProtoFuncSub, DontEnum, 0);
    JSC_NATIVE_FUNCTION("sup", stringProtoFuncSup, DontEnum, 0);
    JSC_NATIVE_FUNCTION("fontcolor", stringProtoFuncFontcolor, DontEnum, 1);
    JSC_NATIVE_FUNCTION("fontsize", stringProtoFuncFontsize, DontEnum, 1);
    JSC_NATIVE_FUNCTION("anchor", stringProtoFuncAnchor, DontEnum, 1);
    JSC_NATIVE_FUNCTION("link", stringProtoFuncLink, DontEnum, 1);
    JSC_NATIVE_FUNCTION("trim", stringProtoFuncTrim, DontEnum, 0);
    JSC_NATIVE_FUNCTION("trimLeft", stringProtoFuncTrimLeft, DontEnum, 0);
    JSC_NATIVE_FUNCTION("trimRight", stringProtoFuncTrimRight, DontEnum, 0);
    JSC_NATIVE_FUNCTION("startsWith", stringProtoFuncStartsWith, DontEnum, 1);
    JSC_NATIVE_FUNCTION("endsWith", stringProtoFuncEndsWith, DontEnum, 1);
    JSC_NATIVE_FUNCTION("includes", stringProtoFuncIncludes, DontEnum, 1);
    JSC_NATIVE_FUNCTION("normalize", stringProtoFuncNormalize, DontEnum, 1);
    JSC_NATIVE_FUNCTION(vm.propertyNames->iteratorSymbol, stringProtoFuncIterator, DontEnum, 0);

    JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->charCodeAtPrivateName, stringProtoFuncCharCodeAt, DontEnum, 1, CharCodeAtIntrinsic);

    // The constructor will be added later, after StringConstructor has been built
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), DontDelete | ReadOnly | DontEnum);
}

StringPrototype* StringPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSString* empty = jsEmptyString(&vm);
    StringPrototype* prototype = new (NotNull, allocateCell<StringPrototype>(vm.heap)) StringPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject, empty);
    return prototype;
}

// ------------------------------ Functions --------------------------

// Helper for producing a JSString for 'string', where 'string' was been produced by
// calling ToString on 'originalValue'. In cases where 'originalValue' already was a
// string primitive we can just use this, otherwise we need to allocate a new JSString.
static inline JSString* jsStringWithReuse(ExecState* exec, JSValue originalValue, const String& string)
{
    if (originalValue.isString()) {
        ASSERT(asString(originalValue)->value(exec) == string);
        return asString(originalValue);
    }
    return jsString(exec, string);
}

// Helper that tries to use the JSString substring sharing mechanism if 'originalValue' is a JSString.
static inline JSString* jsSubstring(ExecState* exec, JSValue originalValue, const String& string, unsigned offset, unsigned length)
{
    if (originalValue.isString()) {
        ASSERT(asString(originalValue)->value(exec) == string);
        return jsSubstring(exec, asString(originalValue), offset, length);
    }
    return jsSubstring(exec, string, offset, length);
}

static NEVER_INLINE String substituteBackreferencesSlow(StringView replacement, StringView source, const int* ovector, RegExp* reg, size_t i)
{
    StringBuilder substitutedReplacement;
    int offset = 0;
    do {
        if (i + 1 == replacement.length())
            break;

        UChar ref = replacement[i + 1];
        if (ref == '$') {
            // "$$" -> "$"
            ++i;
            substitutedReplacement.append(replacement.substring(offset, i - offset));
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
        } else if (reg && ref >= '0' && ref <= '9') {
            // 1- and 2-digit back references are allowed
            unsigned backrefIndex = ref - '0';
            if (backrefIndex > reg->numSubpatterns())
                continue;
            if (replacement.length() > i + 2) {
                ref = replacement[i + 2];
                if (ref >= '0' && ref <= '9') {
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
            substitutedReplacement.append(replacement.substring(offset, i - offset));
        i += 1 + advance;
        offset = i + 1;
        if (backrefStart >= 0)
            substitutedReplacement.append(source.substring(backrefStart, backrefLength));
    } while ((i = replacement.find('$', i + 1)) != notFound);

    if (replacement.length() - offset)
        substitutedReplacement.append(replacement.substring(offset));

    return substitutedReplacement.toString();
}

static inline String substituteBackreferences(const String& replacement, StringView source, const int* ovector, RegExp* reg)
{
    size_t i = replacement.find('$');
    if (UNLIKELY(i != notFound))
        return substituteBackreferencesSlow(replacement, source, ovector, reg, i);

    return replacement;
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

static ALWAYS_INLINE JSValue jsSpliceSubstrings(ExecState* exec, JSString* sourceVal, const String& source, const StringRange* substringRanges, int rangeCount)
{
    if (rangeCount == 1) {
        int sourceSize = source.length();
        int position = substringRanges[0].position;
        int length = substringRanges[0].length;
        if (position <= 0 && length >= sourceSize)
            return sourceVal;
        // We could call String::substringSharingImpl(), but this would result in redundant checks.
        return jsString(exec, StringImpl::createSubstringSharingImpl(source.impl(), std::max(0, position), std::min(sourceSize, length)));
    }

    int totalLength = 0;
    for (int i = 0; i < rangeCount; i++)
        totalLength += substringRanges[i].length;

    if (!totalLength)
        return jsEmptyString(exec);

    if (source.is8Bit()) {
        LChar* buffer;
        const LChar* sourceData = source.characters8();
        RefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
        if (!impl)
            return throwOutOfMemoryError(exec);

        int bufferPos = 0;
        for (int i = 0; i < rangeCount; i++) {
            if (int srcLen = substringRanges[i].length) {
                StringImpl::copyChars(buffer + bufferPos, sourceData + substringRanges[i].position, srcLen);
                bufferPos += srcLen;
            }
        }

        return jsString(exec, impl.release());
    }

    UChar* buffer;
    const UChar* sourceData = source.characters16();

    RefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(totalLength, buffer);
    if (!impl)
        return throwOutOfMemoryError(exec);

    int bufferPos = 0;
    for (int i = 0; i < rangeCount; i++) {
        if (int srcLen = substringRanges[i].length) {
            StringImpl::copyChars(buffer + bufferPos, sourceData + substringRanges[i].position, srcLen);
            bufferPos += srcLen;
        }
    }

    return jsString(exec, impl.release());
}

static ALWAYS_INLINE JSValue jsSpliceSubstringsWithSeparators(ExecState* exec, JSString* sourceVal, const String& source, const StringRange* substringRanges, int rangeCount, const String* separators, int separatorCount)
{
    if (rangeCount == 1 && separatorCount == 0) {
        int sourceSize = source.length();
        int position = substringRanges[0].position;
        int length = substringRanges[0].length;
        if (position <= 0 && length >= sourceSize)
            return sourceVal;
        // We could call String::substringSharingImpl(), but this would result in redundant checks.
        return jsString(exec, StringImpl::createSubstringSharingImpl(source.impl(), std::max(0, position), std::min(sourceSize, length)));
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
    if (totalLength.hasOverflowed())
        return throwOutOfMemoryError(exec);

    if (!totalLength)
        return jsEmptyString(exec);

    if (source.is8Bit() && allSeparators8Bit) {
        LChar* buffer;
        const LChar* sourceData = source.characters8();

        RefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(totalLength.unsafeGet(), buffer);
        if (!impl)
            return throwOutOfMemoryError(exec);

        int maxCount = std::max(rangeCount, separatorCount);
        int bufferPos = 0;
        for (int i = 0; i < maxCount; i++) {
            if (i < rangeCount) {
                if (int srcLen = substringRanges[i].length) {
                    StringImpl::copyChars(buffer + bufferPos, sourceData + substringRanges[i].position, srcLen);
                    bufferPos += srcLen;
                }
            }
            if (i < separatorCount) {
                if (int sepLen = separators[i].length()) {
                    StringImpl::copyChars(buffer + bufferPos, separators[i].characters8(), sepLen);
                    bufferPos += sepLen;
                }
            }
        }        

        return jsString(exec, impl.release());
    }

    UChar* buffer;
    RefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(totalLength.unsafeGet(), buffer);
    if (!impl)
        return throwOutOfMemoryError(exec);

    int maxCount = std::max(rangeCount, separatorCount);
    int bufferPos = 0;
    for (int i = 0; i < maxCount; i++) {
        if (i < rangeCount) {
            if (int srcLen = substringRanges[i].length) {
                if (source.is8Bit())
                    StringImpl::copyChars(buffer + bufferPos, source.characters8() + substringRanges[i].position, srcLen);
                else
                    StringImpl::copyChars(buffer + bufferPos, source.characters16() + substringRanges[i].position, srcLen);
                bufferPos += srcLen;
            }
        }
        if (i < separatorCount) {
            if (int sepLen = separators[i].length()) {
                if (separators[i].is8Bit())
                    StringImpl::copyChars(buffer + bufferPos, separators[i].characters8(), sepLen);
                else
                    StringImpl::copyChars(buffer + bufferPos, separators[i].characters16(), sepLen);
                bufferPos += sepLen;
            }
        }
    }

    return jsString(exec, impl.release());
}

static NEVER_INLINE EncodedJSValue removeUsingRegExpSearch(ExecState* exec, JSString* string, const String& source, RegExp* regExp)
{
    size_t lastIndex = 0;
    unsigned startPosition = 0;

    Vector<StringRange, 16> sourceRanges;
    VM* vm = &exec->vm();
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    unsigned sourceLen = source.length();

    while (true) {
        MatchResult result = regExpConstructor->performMatch(*vm, regExp, string, source, startPosition);
        if (!result)
            break;

        if (lastIndex < result.start)
            sourceRanges.append(StringRange(lastIndex, result.start - lastIndex));

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
        return JSValue::encode(string);

    if (static_cast<unsigned>(lastIndex) < sourceLen)
        sourceRanges.append(StringRange(lastIndex, sourceLen - lastIndex));

    return JSValue::encode(jsSpliceSubstrings(exec, string, source, sourceRanges.data(), sourceRanges.size()));
}

static NEVER_INLINE EncodedJSValue replaceUsingRegExpSearch(ExecState* exec, JSString* string, JSValue searchValue)
{
    JSValue replaceValue = exec->argument(1);
    String replacementString;
    CallData callData;
    CallType callType = getCallData(replaceValue, callData);
    if (callType == CallTypeNone)
        replacementString = replaceValue.toString(exec)->value(exec);

    const String& source = string->value(exec);
    unsigned sourceLen = source.length();
    if (exec->hadException())
        return JSValue::encode(JSValue());
    RegExpObject* regExpObject = asRegExpObject(searchValue);
    RegExp* regExp = regExpObject->regExp();
    bool global = regExp->global();

    if (global) {
        // ES5.1 15.5.4.10 step 8.a.
        regExpObject->setLastIndex(exec, 0);
        if (exec->hadException())
            return JSValue::encode(JSValue());

        if (callType == CallTypeNone && !replacementString.length())
            return removeUsingRegExpSearch(exec, string, source, regExp);
    }

    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();

    size_t lastIndex = 0;
    unsigned startPosition = 0;

    Vector<StringRange, 16> sourceRanges;
    Vector<String, 16> replacements;

    // This is either a loop (if global is set) or a one-way (if not).
    if (global && callType == CallTypeJS) {
        // regExp->numSubpatterns() + 1 for pattern args, + 2 for match start and string
        int argCount = regExp->numSubpatterns() + 1 + 2;
        JSFunction* func = jsCast<JSFunction*>(replaceValue);
        CachedCall cachedCall(exec, func, argCount);
        if (exec->hadException())
            return JSValue::encode(jsNull());
        VM* vm = &exec->vm();
        if (source.is8Bit()) {
            while (true) {
                int* ovector;
                MatchResult result = regExpConstructor->performMatch(*vm, regExp, string, source, startPosition, &ovector);
                if (!result)
                    break;

                sourceRanges.append(StringRange(lastIndex, result.start - lastIndex));

                unsigned i = 0;
                for (; i < regExp->numSubpatterns() + 1; ++i) {
                    int matchStart = ovector[i * 2];
                    int matchLen = ovector[i * 2 + 1] - matchStart;

                    if (matchStart < 0)
                        cachedCall.setArgument(i, jsUndefined());
                    else
                        cachedCall.setArgument(i, jsSubstring8(vm, source, matchStart, matchLen));
                }

                cachedCall.setArgument(i++, jsNumber(result.start));
                cachedCall.setArgument(i++, string);

                cachedCall.setThis(jsUndefined());
                JSValue jsResult = cachedCall.call();
                replacements.append(jsResult.toString(exec)->value(exec));
                if (exec->hadException())
                    break;

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
            while (true) {
                int* ovector;
                MatchResult result = regExpConstructor->performMatch(*vm, regExp, string, source, startPosition, &ovector);
                if (!result)
                    break;

                sourceRanges.append(StringRange(lastIndex, result.start - lastIndex));

                unsigned i = 0;
                for (; i < regExp->numSubpatterns() + 1; ++i) {
                    int matchStart = ovector[i * 2];
                    int matchLen = ovector[i * 2 + 1] - matchStart;

                    if (matchStart < 0)
                        cachedCall.setArgument(i, jsUndefined());
                    else
                        cachedCall.setArgument(i, jsSubstring(vm, source, matchStart, matchLen));
                }

                cachedCall.setArgument(i++, jsNumber(result.start));
                cachedCall.setArgument(i++, string);

                cachedCall.setThis(jsUndefined());
                JSValue jsResult = cachedCall.call();
                replacements.append(jsResult.toString(exec)->value(exec));
                if (exec->hadException())
                    break;

                lastIndex = result.end;
                startPosition = lastIndex;

                // special case of empty match
                if (result.empty()) {
                    startPosition++;
                    if (startPosition > sourceLen)
                        break;
                }
            }
        }
    } else {
        VM* vm = &exec->vm();
        do {
            int* ovector;
            MatchResult result = regExpConstructor->performMatch(*vm, regExp, string, source, startPosition, &ovector);
            if (!result)
                break;

            if (callType != CallTypeNone) {
                sourceRanges.append(StringRange(lastIndex, result.start - lastIndex));

                MarkedArgumentBuffer args;

                for (unsigned i = 0; i < regExp->numSubpatterns() + 1; ++i) {
                    int matchStart = ovector[i * 2];
                    int matchLen = ovector[i * 2 + 1] - matchStart;

                    if (matchStart < 0)
                        args.append(jsUndefined());
                    else
                        args.append(jsSubstring(exec, source, matchStart, matchLen));
                }

                args.append(jsNumber(result.start));
                args.append(string);

                replacements.append(call(exec, replaceValue, callType, callData, jsUndefined(), args).toString(exec)->value(exec));
                if (exec->hadException())
                    break;
            } else {
                int replLen = replacementString.length();
                if (lastIndex < result.start || replLen) {
                    sourceRanges.append(StringRange(lastIndex, result.start - lastIndex));

                    if (replLen)
                        replacements.append(substituteBackreferences(replacementString, source, ovector, regExp));
                    else
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
        return JSValue::encode(string);

    if (static_cast<unsigned>(lastIndex) < sourceLen)
        sourceRanges.append(StringRange(lastIndex, sourceLen - lastIndex));

    return JSValue::encode(jsSpliceSubstringsWithSeparators(exec, string, source, sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size()));
}

static inline EncodedJSValue replaceUsingStringSearch(ExecState* exec, JSString* jsString, JSValue searchValue)
{
    const String& string = jsString->value(exec);
    String searchString = searchValue.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    size_t matchStart = string.find(searchString);

    if (matchStart == notFound)
        return JSValue::encode(jsString);

    JSValue replaceValue = exec->argument(1);
    CallData callData;
    CallType callType = getCallData(replaceValue, callData);
    if (callType != CallTypeNone) {
        MarkedArgumentBuffer args;
        args.append(jsSubstring(exec, string, matchStart, searchString.impl()->length()));
        args.append(jsNumber(matchStart));
        args.append(jsString);
        replaceValue = call(exec, replaceValue, callType, callData, jsUndefined(), args);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    String replaceString = replaceValue.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    StringImpl* stringImpl = string.impl();
    String leftPart(StringImpl::createSubstringSharingImpl(stringImpl, 0, matchStart));

    size_t matchEnd = matchStart + searchString.impl()->length();
    int ovector[2] = { static_cast<int>(matchStart),  static_cast<int>(matchEnd)};
    String middlePart = substituteBackreferences(replaceString, string, ovector, 0);

    size_t leftLength = stringImpl->length() - matchEnd;
    String rightPart(StringImpl::createSubstringSharingImpl(stringImpl, matchEnd, leftLength));
    return JSValue::encode(JSC::jsString(exec, leftPart, middlePart, rightPart));
}

static inline bool checkObjectCoercible(JSValue thisValue)
{
    if (thisValue.isString())
        return true;

    if (thisValue.isUndefinedOrNull())
        return false;

    if (thisValue.isCell() && thisValue.asCell()->structure()->typeInfo().isEnvironmentRecord())
        return false;

    return true;
}

template <typename CharacterType>
static inline JSValue repeatCharacter(ExecState* exec, CharacterType character, unsigned repeatCount)
{
    CharacterType* buffer = nullptr;
    RefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(repeatCount, buffer);
    if (!impl)
        return throwOutOfMemoryError(exec);

    std::fill_n(buffer, repeatCount, character);

    return jsString(exec, impl.release());
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncRepeat(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    JSString* string = thisValue.toString(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    double repeatCountDouble = exec->argument(0).toInteger(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    if (repeatCountDouble < 0 || std::isinf(repeatCountDouble))
        return throwVMError(exec, createRangeError(exec, ASCIILiteral("repeat() argument must be greater than or equal to 0 and not be infinity")));

    VM& vm = exec->vm();

    if (!string->length() || !repeatCountDouble)
        return JSValue::encode(jsEmptyString(&vm));

    if (repeatCountDouble == 1)
        return JSValue::encode(string);

    // JSString requires the limitation that its length is in the range of int32_t.
    if (repeatCountDouble > std::numeric_limits<int32_t>::max() / string->length())
        return JSValue::encode(throwOutOfMemoryError(exec));
    unsigned repeatCount = static_cast<unsigned>(repeatCountDouble);

    // For a string which length is small, instead of creating ropes,
    // allocating a sequential buffer and fill with the repeated string for efficiency.
    if (string->length() == 1) {
        String repeatedString = string->value(exec);
        UChar character = repeatedString.at(0);
        if (!(character & ~0xff))
            return JSValue::encode(repeatCharacter(exec, static_cast<LChar>(character), repeatCount));
        return JSValue::encode(repeatCharacter(exec, character, repeatCount));
    }

    JSRopeString::RopeBuilder ropeBuilder(vm);
    for (unsigned i = 0; i < repeatCount; ++i) {
        if (!ropeBuilder.append(string))
            return JSValue::encode(throwOutOfMemoryError(exec));
    }
    return JSValue::encode(ropeBuilder.release());
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncReplace(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    JSString* string = thisValue.toString(exec);
    JSValue searchValue = exec->argument(0);

    if (searchValue.inherits(RegExpObject::info()))
        return replaceUsingRegExpSearch(exec, string, searchValue);
    return replaceUsingStringSearch(exec, string, searchValue);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToString(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    // Also used for valueOf.

    if (thisValue.isString())
        return JSValue::encode(thisValue);

    if (thisValue.inherits(StringObject::info()))
        return JSValue::encode(asStringObject(thisValue)->internalValue());

    return throwVMTypeError(exec);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncCharAt(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    StringView string = thisValue.toString(exec)->view(exec);
    JSValue a0 = exec->argument(0);
    if (a0.isUInt32()) {
        uint32_t i = a0.asUInt32();
        if (i < string.length())
            return JSValue::encode(jsSingleCharacterString(exec, string[i]));
        return JSValue::encode(jsEmptyString(exec));
    }
    double dpos = a0.toInteger(exec);
    if (dpos >= 0 && dpos < string.length())
        return JSValue::encode(jsSingleCharacterString(exec, string[static_cast<unsigned>(dpos)]));
    return JSValue::encode(jsEmptyString(exec));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncCharCodeAt(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    StringView string = thisValue.toString(exec)->view(exec);
    JSValue a0 = exec->argument(0);
    if (a0.isUInt32()) {
        uint32_t i = a0.asUInt32();
        if (i < string.length())
            return JSValue::encode(jsNumber(string[i]));
        return JSValue::encode(jsNaN());
    }
    double dpos = a0.toInteger(exec);
    if (dpos >= 0 && dpos < string.length())
        return JSValue::encode(jsNumber(string[static_cast<int>(dpos)]));
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

EncodedJSValue JSC_HOST_CALL stringProtoFuncCodePointAt(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    String string = thisValue.toWTFString(exec);
    unsigned length = string.length();

    JSValue argument0 = exec->argument(0);
    if (argument0.isUInt32()) {
        unsigned position = argument0.asUInt32();
        if (position < length)
            return JSValue::encode(jsNumber(codePointAt(string, position, length)));
        return JSValue::encode(jsUndefined());
    }

    if (UNLIKELY(exec->hadException()))
        return JSValue::encode(jsUndefined());

    double doublePosition = argument0.toInteger(exec);
    if (doublePosition >= 0 && doublePosition < length)
        return JSValue::encode(jsNumber(codePointAt(string, static_cast<unsigned>(doublePosition), length)));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncConcat(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (thisValue.isString() && exec->argumentCount() == 1)
        return JSValue::encode(jsString(exec, asString(thisValue), exec->uncheckedArgument(0).toString(exec)));

    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    return JSValue::encode(jsStringFromArguments(exec, thisValue));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncIndexOf(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    JSValue a0 = exec->argument(0);
    JSValue a1 = exec->argument(1);

    JSString* thisJSString = thisValue.toString(exec);
    JSString* otherJSString = a0.toString(exec);

    unsigned pos = 0;
    if (!a1.isUndefined()) {
        int len = thisJSString->length();
        RELEASE_ASSERT(len >= 0);
        if (a1.isUInt32())
            pos = std::min<uint32_t>(a1.asUInt32(), len);
        else {
            double dpos = a1.toInteger(exec);
            if (dpos < 0)
                dpos = 0;
            else if (dpos > len)
                dpos = len;
            pos = static_cast<unsigned>(dpos);
        }
    }

    if (thisJSString->length() < otherJSString->length() + pos)
        return JSValue::encode(jsNumber(-1));

    size_t result = thisJSString->view(exec).get().find(otherJSString->view(exec), pos);
    if (result == notFound)
        return JSValue::encode(jsNumber(-1));
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncLastIndexOf(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    JSValue a0 = exec->argument(0);
    JSValue a1 = exec->argument(1);

    JSString* thisJSString = thisValue.toString(exec);
    unsigned len = thisJSString->length();
    JSString* otherJSString = a0.toString(exec);

    double dpos = a1.toIntegerPreserveNaN(exec);
    unsigned startPosition;
    if (dpos < 0)
        startPosition = 0;
    else if (!(dpos <= len)) // true for NaN
        startPosition = len;
    else
        startPosition = static_cast<unsigned>(dpos);

    if (len < otherJSString->length())
        return JSValue::encode(jsNumber(-1));

    String thisString = thisJSString->value(exec);
    String otherString = otherJSString->value(exec);
    size_t result;
    if (!startPosition)
        result = thisString.startsWith(otherString) ? 0 : notFound;
    else
        result = thisString.reverseFind(otherString, startPosition);
    if (result == notFound)
        return JSValue::encode(jsNumber(-1));
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncMatch(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    JSString* string = thisValue.toString(exec);
    String s = string->value(exec);
    VM* vm = &exec->vm();

    JSValue a0 = exec->argument(0);

    RegExp* regExp;
    bool global = false;
    if (a0.inherits(RegExpObject::info())) {
        RegExpObject* regExpObject = asRegExpObject(a0);
        regExp = regExpObject->regExp();
        if ((global = regExp->global())) {
            // ES5.1 15.5.4.10 step 8.a.
            regExpObject->setLastIndex(exec, 0);
            if (exec->hadException())
                return JSValue::encode(JSValue());
        }
    } else {
        /*
         *  ECMA 15.5.4.12 String.prototype.search (regexp)
         *  If regexp is not an object whose [[Class]] property is "RegExp", it is
         *  replaced with the result of the expression new RegExp(regexp).
         *  Per ECMA 15.10.4.1, if a0 is undefined substitute the empty string.
         */
        regExp = RegExp::create(exec->vm(), a0.isUndefined() ? emptyString() : a0.toString(exec)->value(exec), NoFlags);
        if (!regExp->isValid())
            return throwVMError(exec, createSyntaxError(exec, regExp->errorMessage()));
    }
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    MatchResult result = regExpConstructor->performMatch(*vm, regExp, string, s, 0);
    // case without 'g' flag is handled like RegExp.prototype.exec
    if (!global)
        return JSValue::encode(result ? createRegExpMatchesArray(exec, string, regExp, result) : jsNull());

    // return array of matches
    MarkedArgumentBuffer list;
    while (result) {
        size_t end = result.end;
        size_t length = end - result.start;
        list.append(jsSubstring(exec, s, result.start, length));
        if (!length)
            ++end;
        result = regExpConstructor->performMatch(*vm, regExp, string, s, end);
    }
    if (list.isEmpty()) {
        // if there are no matches at all, it's important to return
        // Null instead of an empty array, because this matches
        // other browsers and because Null is a false value.
        return JSValue::encode(jsNull());
    }

    return JSValue::encode(constructArray(exec, static_cast<ArrayAllocationProfile*>(0), list));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSearch(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    JSString* string = thisValue.toString(exec);
    String s = string->value(exec);
    VM* vm = &exec->vm();

    JSValue a0 = exec->argument(0);

    RegExp* reg;
    if (a0.inherits(RegExpObject::info()))
        reg = asRegExpObject(a0)->regExp();
    else { 
        /*
         *  ECMA 15.5.4.12 String.prototype.search (regexp)
         *  If regexp is not an object whose [[Class]] property is "RegExp", it is
         *  replaced with the result of the expression new RegExp(regexp).
         *  Per ECMA 15.10.4.1, if a0 is undefined substitute the empty string.
         */
        reg = RegExp::create(exec->vm(), a0.isUndefined() ? emptyString() : a0.toString(exec)->value(exec), NoFlags);
        if (!reg->isValid())
            return throwVMError(exec, createSyntaxError(exec, reg->errorMessage()));
    }
    RegExpConstructor* regExpConstructor = exec->lexicalGlobalObject()->regExpConstructor();
    MatchResult result = regExpConstructor->performMatch(*vm, reg, string, s, 0);
    return JSValue::encode(result ? jsNumber(result.start) : jsNumber(-1));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSlice(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    int len = s.length();
    RELEASE_ASSERT(len >= 0);

    JSValue a0 = exec->argument(0);
    JSValue a1 = exec->argument(1);

    // The arg processing is very much like ArrayProtoFunc::Slice
    double start = a0.toInteger(exec);
    double end = a1.isUndefined() ? len : a1.toInteger(exec);
    double from = start < 0 ? len + start : start;
    double to = end < 0 ? len + end : end;
    if (to > from && to > 0 && from < len) {
        if (from < 0)
            from = 0;
        if (to > len)
            to = len;
        return JSValue::encode(jsSubstring(exec, s, static_cast<unsigned>(from), static_cast<unsigned>(to) - static_cast<unsigned>(from)));
    }

    return JSValue::encode(jsEmptyString(exec));
}

// Return true in case of early return (resultLength got to limitLength).
template<typename CharacterType>
static ALWAYS_INLINE bool splitStringByOneCharacterImpl(ExecState* exec, JSArray* result, JSValue originalValue, const String& input, StringImpl* string, UChar separatorCharacter, size_t& position, unsigned& resultLength, unsigned limitLength)
{
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
        result->putDirectIndex(exec, resultLength, jsSubstring(exec, originalValue, input, position, matchPosition - position));
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

// ES 5.1 - 15.5.4.14 String.prototype.split (separator, limit)
EncodedJSValue JSC_HOST_CALL stringProtoFuncSplit(ExecState* exec)
{
    // 1. Call CheckObjectCoercible passing the this value as its argument.
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    // 2. Let S be the result of calling ToString, giving it the this value as its argument.
    // 6. Let s be the number of characters in S.
    String input = thisValue.toString(exec)->value(exec);

    // 3. Let A be a new array created as if by the expression new Array()
    //    where Array is the standard built-in constructor with that name.
    JSArray* result = constructEmptyArray(exec, 0);

    // 4. Let lengthA be 0.
    unsigned resultLength = 0;

    // 5. If limit is undefined, let lim = 2^32-1; else let lim = ToUint32(limit).
    JSValue limitValue = exec->argument(1);
    unsigned limit = limitValue.isUndefined() ? 0xFFFFFFFFu : limitValue.toUInt32(exec);

    // 7. Let p = 0.
    size_t position = 0;

    // 8. If separator is a RegExp object (its [[Class]] is "RegExp"), let R = separator;
    //    otherwise let R = ToString(separator).
    JSValue separatorValue = exec->argument(0);
    if (separatorValue.inherits(RegExpObject::info())) {
        VM* vm = &exec->vm();
        RegExp* reg = asRegExpObject(separatorValue)->regExp();

        // 9. If lim == 0, return A.
        if (!limit)
            return JSValue::encode(result);

        // 10. If separator is undefined, then
        if (separatorValue.isUndefined()) {
            // a. Call the [[DefineOwnProperty]] internal method of A with arguments "0",
            //    Property Descriptor {[[Value]]: S, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            result->putDirectIndex(exec, 0, jsStringWithReuse(exec, thisValue, input));
            // b. Return A.
            return JSValue::encode(result);
        }

        // 11. If s == 0, then
        if (input.isEmpty()) {
            // a. Call SplitMatch(S, 0, R) and let z be its MatchResult result.
            // b. If z is not failure, return A.
            // c. Call the [[DefineOwnProperty]] internal method of A with arguments "0",
            //    Property Descriptor {[[Value]]: S, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            // d. Return A.
            if (!reg->match(*vm, input, 0))
                result->putDirectIndex(exec, 0, jsStringWithReuse(exec, thisValue, input));
            return JSValue::encode(result);
        }

        // 12. Let q = p.
        size_t matchPosition = 0;
        // 13. Repeat, while q != s
        while (matchPosition < input.length()) {
            // a. Call SplitMatch(S, q, R) and let z be its MatchResult result.
            Vector<int, 32> ovector;
            int mpos = reg->match(*vm, input, matchPosition, ovector);

            // b. If z is a failure then we can break because there are no matches
            if (mpos < 0)
                break;
            matchPosition = mpos;

            // if the match is the empty match at the end, break.
            if (matchPosition >= input.length())
                break;

            // c. Else, z is not failure
            // i. z must be a State. Let e be z's endIndex and let cap be z's captures array.
            size_t matchEnd = ovector[1];

            // ii. If e == p, then let q = q + 1.
            if (matchEnd == position) {
                ++matchPosition;
                continue;
            }
            // iii. if matchEnd == 0 then position should also be zero and thus matchEnd should equal position.
            ASSERT(matchEnd);

            // iii. Else, e != p

            // 1. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
            //    through q (exclusive).
            // 2. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(lengthA),
            //    Property Descriptor {[[Value]]: T, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            result->putDirectIndex(exec, resultLength, jsSubstring(exec, thisValue, input, position, matchPosition - position));

            // 3. Increment lengthA by 1.
            // 4. If lengthA == lim, return A.
            if (++resultLength == limit)
                return JSValue::encode(result);

            // 5. Let p = e.
            // 8. Let q = p.
            position = matchEnd;
            matchPosition = matchEnd;

            // 6. Let i = 0.
            // 7. Repeat, while i is not equal to the number of elements in cap.
            //  a Let i = i + 1.
            for (unsigned i = 1; i <= reg->numSubpatterns(); ++i) {
                // b Call the [[DefineOwnProperty]] internal method of A with arguments
                //   ToString(lengthA), Property Descriptor {[[Value]]: cap[i], [[Writable]]:
                //   true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
                int sub = ovector[i * 2];
                result->putDirectIndex(exec, resultLength, sub < 0 ? jsUndefined() : jsSubstring(exec, thisValue, input, sub, ovector[i * 2 + 1] - sub));
                // c Increment lengthA by 1.
                // d If lengthA == lim, return A.
                if (++resultLength == limit)
                    return JSValue::encode(result);
            }
        }
    } else {
        String separator = separatorValue.toString(exec)->value(exec);

        // 9. If lim == 0, return A.
        if (!limit)
            return JSValue::encode(result);

        // 10. If separator is undefined, then
        JSValue separatorValue = exec->argument(0);
        if (separatorValue.isUndefined()) {
            // a.  Call the [[DefineOwnProperty]] internal method of A with arguments "0",
            //     Property Descriptor {[[Value]]: S, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            result->putDirectIndex(exec, 0, jsStringWithReuse(exec, thisValue, input));
            // b.  Return A.
            return JSValue::encode(result);
        }

        // 11. If s == 0, then
        if (input.isEmpty()) {
            // a. Call SplitMatch(S, 0, R) and let z be its MatchResult result.
            // b. If z is not failure, return A.
            // c. Call the [[DefineOwnProperty]] internal method of A with arguments "0",
            //    Property Descriptor {[[Value]]: S, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
            // d. Return A.
            if (!separator.isEmpty())
                result->putDirectIndex(exec, 0, jsStringWithReuse(exec, thisValue, input));
            return JSValue::encode(result);
        }

        // Optimized case for splitting on the empty string.
        if (separator.isEmpty()) {
            limit = std::min(limit, input.length());
            // Zero limt/input length handled in steps 9/11 respectively, above.
            ASSERT(limit);

            do {
                result->putDirectIndex(exec, position, jsSingleCharacterString(exec, input[position]));
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
                if (splitStringByOneCharacterImpl<LChar>(exec, result, thisValue, input, stringImpl, separatorCharacter, position, resultLength, limit))
                    return JSValue::encode(result);
            } else {
                if (splitStringByOneCharacterImpl<UChar>(exec, result, thisValue, input, stringImpl, separatorCharacter, position, resultLength, limit))
                    return JSValue::encode(result);
            }
        } else {
            // 12. Let q = p.
            size_t matchPosition;
            // 13. Repeat, while q != s
            //   a. Call SplitMatch(S, q, R) and let z be its MatchResult result.
            //   b. If z is failure, then let q = q+1.
            //   c. Else, z is not failure
            while ((matchPosition = stringImpl->find(separatorImpl, position)) != notFound) {
                // 1. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
                //    through q (exclusive).
                // 2. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(lengthA),
                //    Property Descriptor {[[Value]]: T, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
                result->putDirectIndex(exec, resultLength, jsSubstring(exec, thisValue, input, position, matchPosition - position));
                // 3. Increment lengthA by 1.
                // 4. If lengthA == lim, return A.
                if (++resultLength == limit)
                    return JSValue::encode(result);

                // 5. Let p = e.
                // 8. Let q = p.
                position = matchPosition + separator.length();
            }
        }
    }

    // 14. Let T be a String value equal to the substring of S consisting of the characters at positions p (inclusive)
    //     through s (exclusive).
    // 15. Call the [[DefineOwnProperty]] internal method of A with arguments ToString(lengthA), Property Descriptor
    //     {[[Value]]: T, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false.
    result->putDirectIndex(exec, resultLength++, jsSubstring(exec, thisValue, input, position, input.length() - position));

    // 16. Return A.
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstr(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    unsigned len;
    JSString* jsString = 0;
    String uString;
    if (thisValue.isString()) {
        jsString = jsCast<JSString*>(thisValue.asCell());
        len = jsString->length();
    } else {
        uString = thisValue.toString(exec)->value(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        len = uString.length();
    }

    JSValue a0 = exec->argument(0);
    JSValue a1 = exec->argument(1);

    double start = a0.toInteger(exec);
    double length = a1.isUndefined() ? len : a1.toInteger(exec);
    if (start >= len || length <= 0)
        return JSValue::encode(jsEmptyString(exec));
    if (start < 0) {
        start += len;
        if (start < 0)
            start = 0;
    }
    if (start + length > len)
        length = len - start;
    unsigned substringStart = static_cast<unsigned>(start);
    unsigned substringLength = static_cast<unsigned>(length);
    if (jsString)
        return JSValue::encode(jsSubstring(exec, jsString, substringStart, substringLength));
    return JSValue::encode(jsSubstring(exec, uString, substringStart, substringLength));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSubstring(ExecState* exec)
{
    SamplingRegion samplingRegion("Doing substringing");
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    JSString* jsString = thisValue.toString(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue a0 = exec->argument(0);
    JSValue a1 = exec->argument(1);
    int len = jsString->length();
    RELEASE_ASSERT(len >= 0);

    double start = a0.toNumber(exec);
    double end;
    if (!(start >= 0)) // check for negative values or NaN
        start = 0;
    else if (start > len)
        start = len;
    if (a1.isUndefined())
        end = len;
    else { 
        end = a1.toNumber(exec);
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
    return JSValue::encode(jsSubstring(exec, jsString, substringStart, substringLength));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToLowerCase(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    JSString* sVal = thisValue.toString(exec);
    const String& s = sVal->value(exec);

    int sSize = s.length();
    if (!sSize)
        return JSValue::encode(sVal);
    RELEASE_ASSERT(sSize >= 0);

    StringImpl* ourImpl = s.impl();
    RefPtr<StringImpl> lower = ourImpl->lower();
    if (ourImpl == lower)
        return JSValue::encode(sVal);
    return JSValue::encode(jsString(exec, String(lower.release())));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToUpperCase(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    JSString* sVal = thisValue.toString(exec);
    const String& s = sVal->value(exec);

    int sSize = s.length();
    if (!sSize)
        return JSValue::encode(sVal);

    StringImpl* sImpl = s.impl();
    RefPtr<StringImpl> upper = sImpl->upper();
    if (sImpl == upper)
        return JSValue::encode(sVal);
    return JSValue::encode(jsString(exec, String(upper.release())));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncLocaleCompare(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);

    JSValue a0 = exec->argument(0);
    return JSValue::encode(jsNumber(Collator().collate(s, a0.toString(exec)->value(exec))));
}

#if ENABLE(INTL)
static EncodedJSValue toLocaleCase(ExecState* state, int32_t (*convertCase)(UChar*, int32_t, const UChar*, int32_t, const char*, UErrorCode*))
{
    // 1. Let O be RequireObjectCoercible(this value).
    JSValue thisValue = state->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(state);

    // 2. Let S be ToString(O).
    JSString* sVal = thisValue.toString(state);
    const String& s = sVal->value(state);

    // 3. ReturnIfAbrupt(S).
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    // Optimization for empty strings.
    if (s.isEmpty())
        return JSValue::encode(sVal);

    // 4. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(*state, state->argument(0));

    // 5. ReturnIfAbrupt(requestedLocales).
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    // 6. Let len be the number of elements in requestedLocales.
    size_t len = requestedLocales.size();

    // 7. If len > 0, then
    // a. Let requestedLocale be the first element of requestedLocales.
    // 8. Else
    // a. Let requestedLocale be DefaultLocale().
    String requestedLocale = len > 0 ? requestedLocales.first() : defaultLocale();

    // 9. Let noExtensionsLocale be the String value that is requestedLocale with all Unicode locale extension sequences (6.2.1) removed.
    String noExtensionsLocale = removeUnicodeLocaleExtension(requestedLocale);

    // 10. Let availableLocales be a List with the language tags of the languages for which the Unicode character database contains language sensitive case mappings.
    // Note 1: As of Unicode 5.1, the availableLocales list contains the elements "az", "lt", and "tr".
    const HashSet<String> availableLocales({ ASCIILiteral("az"), ASCIILiteral("lt"), ASCIILiteral("tr") });

    // 11. Let locale be BestAvailableLocale(availableLocales, noExtensionsLocale).
    String locale = bestAvailableLocale(availableLocales, noExtensionsLocale);

    // 12. If locale is undefined, let locale be "und".
    if (locale.isNull())
        locale = ASCIILiteral("und");

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
            return throwVMTypeError(state, u_errorName(error));
        lower = String(buffer.data(), resultLength);
    } else
        return throwVMTypeError(state, u_errorName(error));

    // 18. Return L.
    return JSValue::encode(jsString(state, lower));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleLowerCase(ExecState* state)
{
    // 13.1.2 String.prototype.toLocaleLowerCase ([locales])
    // http://ecma-international.org/publications/standards/Ecma-402.htm
    return toLocaleCase(state, u_strToLower);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncToLocaleUpperCase(ExecState* state)
{
    // 13.1.3 String.prototype.toLocaleUpperCase ([locales])
    // http://ecma-international.org/publications/standards/Ecma-402.htm
    // This function interprets a string value as a sequence of code points, as described in ES2015, 6.1.4. This function behaves in exactly the same way as String.prototype.toLocaleLowerCase, except that characters are mapped to their uppercase equivalents as specified in the Unicode character database.
    return toLocaleCase(state, u_strToUpper);
}
#endif // ENABLE(INTL)

EncodedJSValue JSC_HOST_CALL stringProtoFuncBig(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<big>", s, "</big>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSmall(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<small>", s, "</small>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncBlink(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<blink>", s, "</blink>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncBold(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<b>", s, "</b>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncFixed(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<tt>", s, "</tt>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncItalics(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<i>", s, "</i>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncStrike(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<strike>", s, "</strike>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSub(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<sub>", s, "</sub>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncSup(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    return JSValue::encode(jsMakeNontrivialString(exec, "<sup>", s, "</sup>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncFontcolor(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    JSValue a0 = exec->argument(0);
    String color = a0.toWTFString(exec);
    color.replaceWithLiteral('"', "&quot;");

    return JSValue::encode(jsMakeNontrivialString(exec, "<font color=\"", color, "\">", s, "</font>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncFontsize(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    JSValue a0 = exec->argument(0);

    uint32_t smallInteger;
    if (a0.getUInt32(smallInteger) && smallInteger <= 9) {
        unsigned stringSize = s.length();
        unsigned bufferSize = 22 + stringSize;
        // FIXME: Should we have an 8-bit version of this code path too? Or maybe only an 8-bit version?
        UChar* buffer;
        PassRefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(bufferSize, buffer);
        if (!impl)
            return JSValue::encode(jsUndefined());
        buffer[0] = '<';
        buffer[1] = 'f';
        buffer[2] = 'o';
        buffer[3] = 'n';
        buffer[4] = 't';
        buffer[5] = ' ';
        buffer[6] = 's';
        buffer[7] = 'i';
        buffer[8] = 'z';
        buffer[9] = 'e';
        buffer[10] = '=';
        buffer[11] = '"';
        buffer[12] = '0' + smallInteger;
        buffer[13] = '"';
        buffer[14] = '>';
        StringView(s).getCharactersWithUpconvert(&buffer[15]);
        buffer[15 + stringSize] = '<';
        buffer[16 + stringSize] = '/';
        buffer[17 + stringSize] = 'f';
        buffer[18 + stringSize] = 'o';
        buffer[19 + stringSize] = 'n';
        buffer[20 + stringSize] = 't';
        buffer[21 + stringSize] = '>';
        return JSValue::encode(jsNontrivialString(exec, impl));
    }

    String fontSize = a0.toWTFString(exec);
    fontSize.replaceWithLiteral('"', "&quot;");

    return JSValue::encode(jsMakeNontrivialString(exec, "<font size=\"", fontSize, "\">", s, "</font>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncAnchor(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    JSValue a0 = exec->argument(0);
    String anchor = a0.toWTFString(exec);
    anchor.replaceWithLiteral('"', "&quot;");

    return JSValue::encode(jsMakeNontrivialString(exec, "<a name=\"", anchor, "\">", s, "</a>"));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncLink(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    String s = thisValue.toString(exec)->value(exec);
    JSValue a0 = exec->argument(0);
    String linkText = a0.toWTFString(exec);
    linkText.replaceWithLiteral('"', "&quot;");

    unsigned linkTextSize = linkText.length();
    unsigned stringSize = s.length();
    unsigned bufferSize = 15 + linkTextSize + stringSize;
    // FIXME: Should we have an 8-bit version of this code path too? Or maybe only an 8-bit version?
    UChar* buffer;
    PassRefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(bufferSize, buffer);
    if (!impl)
        return JSValue::encode(jsUndefined());
    buffer[0] = '<';
    buffer[1] = 'a';
    buffer[2] = ' ';
    buffer[3] = 'h';
    buffer[4] = 'r';
    buffer[5] = 'e';
    buffer[6] = 'f';
    buffer[7] = '=';
    buffer[8] = '"';
    StringView(linkText).getCharactersWithUpconvert(&buffer[9]);
    buffer[9 + linkTextSize] = '"';
    buffer[10 + linkTextSize] = '>';
    StringView(s).getCharactersWithUpconvert(&buffer[11 + linkTextSize]);
    buffer[11 + linkTextSize + stringSize] = '<';
    buffer[12 + linkTextSize + stringSize] = '/';
    buffer[13 + linkTextSize + stringSize] = 'a';
    buffer[14 + linkTextSize + stringSize] = '>';
    return JSValue::encode(jsNontrivialString(exec, impl));
}

enum {
    TrimLeft = 1,
    TrimRight = 2
};

static inline JSValue trimString(ExecState* exec, JSValue thisValue, int trimKind)
{
    if (!checkObjectCoercible(thisValue))
        return throwTypeError(exec);
    String str = thisValue.toString(exec)->value(exec);
    unsigned left = 0;
    if (trimKind & TrimLeft) {
        while (left < str.length() && isStrWhiteSpace(str[left]))
            left++;
    }
    unsigned right = str.length();
    if (trimKind & TrimRight) {
        while (right > left && isStrWhiteSpace(str[right - 1]))
            right--;
    }

    // Don't gc allocate a new string if we don't have to.
    if (left == 0 && right == str.length() && thisValue.isString())
        return thisValue;

    return jsString(exec, str.substringSharingImpl(left, right - left));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncTrim(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    return JSValue::encode(trimString(exec, thisValue, TrimLeft | TrimRight));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimLeft(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    return JSValue::encode(trimString(exec, thisValue, TrimLeft));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncTrimRight(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    return JSValue::encode(trimString(exec, thisValue, TrimRight));
}

static inline unsigned clampAndTruncateToUnsigned(double value, unsigned min, unsigned max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return static_cast<unsigned>(value);
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncStartsWith(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    String stringToSearchIn = thisValue.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue a0 = exec->argument(0);
    if (jsDynamicCast<RegExpObject*>(a0))
        return throwVMTypeError(exec);

    String searchString = a0.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue positionArg = exec->argument(1);
    unsigned start = 0;
    if (positionArg.isInt32())
        start = std::max(0, positionArg.asInt32());
    else {
        unsigned length = stringToSearchIn.length();
        start = clampAndTruncateToUnsigned(positionArg.toInteger(exec), 0, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.hasInfixStartingAt(searchString, start)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncEndsWith(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    String stringToSearchIn = thisValue.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue a0 = exec->argument(0);
    if (jsDynamicCast<RegExpObject*>(a0))
        return throwVMTypeError(exec);

    String searchString = a0.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    unsigned length = stringToSearchIn.length();

    JSValue endPositionArg = exec->argument(1);
    unsigned end = length;
    if (endPositionArg.isInt32())
        end = std::max(0, endPositionArg.asInt32());
    else if (!endPositionArg.isUndefined()) {
        end = clampAndTruncateToUnsigned(endPositionArg.toInteger(exec), 0, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.hasInfixEndingAt(searchString, std::min(end, length))));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncIncludes(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);

    String stringToSearchIn = thisValue.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue a0 = exec->argument(0);
    if (jsDynamicCast<RegExpObject*>(a0))
        return throwVMTypeError(exec);

    String searchString = a0.toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue positionArg = exec->argument(1);
    unsigned start = 0;
    if (positionArg.isInt32())
        start = std::max(0, positionArg.asInt32());
    else {
        unsigned length = stringToSearchIn.length();
        start = clampAndTruncateToUnsigned(positionArg.toInteger(exec), 0, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(jsBoolean(stringToSearchIn.contains(searchString, true, start)));
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncIterator(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    JSString* string = thisValue.toString(exec);
    return JSValue::encode(JSStringIterator::create(exec, exec->callee()->globalObject()->stringIteratorStructure(), string));
}

static JSValue normalize(ExecState* exec, const UChar* source, size_t sourceLength, UNormalizationMode form)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t normalizedStringLength = unorm_normalize(source, sourceLength, form, 0, nullptr, 0, &status);

    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        // The behavior is not specified when normalize fails.
        // Now we throw a type erorr since it seems that the contents of the string are invalid.
        return throwTypeError(exec);
    }

    UChar* buffer = nullptr;
    RefPtr<StringImpl> impl = StringImpl::tryCreateUninitialized(normalizedStringLength, buffer);
    if (!impl)
        return throwOutOfMemoryError(exec);

    status = U_ZERO_ERROR;
    unorm_normalize(source, sourceLength, form, 0, buffer, normalizedStringLength, &status);
    if (U_FAILURE(status))
        return throwTypeError(exec);

    return jsString(exec, impl.get());
}

EncodedJSValue JSC_HOST_CALL stringProtoFuncNormalize(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!checkObjectCoercible(thisValue))
        return throwVMTypeError(exec);
    StringView source = thisValue.toString(exec)->view(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    UNormalizationMode form = UNORM_NFC;
    // Verify that the argument is provided and is not undefined.
    if (!exec->argument(0).isUndefined()) {
        String formString = exec->uncheckedArgument(0).toString(exec)->value(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        if (formString == "NFC")
            form = UNORM_NFC;
        else if (formString == "NFD")
            form = UNORM_NFD;
        else if (formString == "NFKC")
            form = UNORM_NFKC;
        else if (formString == "NFKD")
            form = UNORM_NFKD;
        else
            return throwVMError(exec, createRangeError(exec, ASCIILiteral("argument does not match any normalization form")));
    }

    return JSValue::encode(normalize(exec, source.upconvertedCharacters(), source.length(), form));
}

} // namespace JSC
