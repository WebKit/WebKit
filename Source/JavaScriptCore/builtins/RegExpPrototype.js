/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

@globalPrivate
@constructor
function RegExpStringIterator(regExp, string, global, fullUnicode)
{
    "use strict";

    @putByIdDirectPrivate(this, "regExpStringIteratorRegExp", regExp);
    @putByIdDirectPrivate(this, "regExpStringIteratorString", string);
    @putByIdDirectPrivate(this, "regExpStringIteratorGlobal", global);
    @putByIdDirectPrivate(this, "regExpStringIteratorUnicode", fullUnicode);
    @putByIdDirectPrivate(this, "regExpStringIteratorDone", false);
}

@globalPrivate
function advanceStringIndex(string, index, unicode)
{
    // This function implements AdvanceStringIndex described in ES6 21.2.5.2.3.
    "use strict";

    if (!unicode)
        return index + 1;

    if (index + 1 >= string.length)
        return index + 1;

    var first = string.@charCodeAt(index);
    if (first < 0xD800 || first > 0xDBFF)
        return index + 1;

    var second = string.@charCodeAt(index + 1);
    if (second < 0xDC00 || second > 0xDFFF)
        return index + 1;

    return index + 2;
}

@globalPrivate
function regExpExec(regexp, str)
{
    "use strict";

    var exec = regexp.exec;
    var builtinExec = @regExpBuiltinExec;
    if (exec !== builtinExec && typeof exec === "function") {
        var result = exec.@call(regexp, str);
        if (result !== null && !@isObject(result))
            @throwTypeError("The result of a RegExp exec must be null or an object");
        return result;
    }
    return builtinExec.@call(regexp, str);
}

@globalPrivate
function hasObservableSideEffectsForRegExpMatch(regexp)
{
    "use strict";

    if (!@isRegExpObject(regexp))
        return true;

    // This is accessed by the RegExpExec internal function.
    var regexpExec = @tryGetById(regexp, "exec");
    if (regexpExec !== @regExpBuiltinExec)
        return true;

    var regexpGlobal = @tryGetById(regexp, "global");
    if (regexpGlobal !== @regExpProtoGlobalGetter)
        return true;
    var regexpUnicode = @tryGetById(regexp, "unicode");
    if (regexpUnicode !== @regExpProtoUnicodeGetter)
        return true;

    return typeof regexp.lastIndex !== "number";
}

@globalPrivate
function matchSlow(regexp, str)
{
    "use strict";

    if (!regexp.global)
        return @regExpExec(regexp, str);
    
    var unicode = regexp.unicode;
    regexp.lastIndex = 0;
    var resultList = [];

    // FIXME: It would be great to implement a solution similar to what we do in
    // RegExpObject::matchGlobal(). It's not clear if this is possible, since this loop has
    // effects. https://bugs.webkit.org/show_bug.cgi?id=158145
    var maximumReasonableMatchSize = 100000000;

    while (true) {
        var result = @regExpExec(regexp, str);
        
        if (result === null) {
            if (resultList.length === 0)
                return null;
            return resultList;
        }

        if (resultList.length > maximumReasonableMatchSize)
            @throwOutOfMemoryError();

        var resultString = @toString(result[0]);

        if (!resultString.length)
            regexp.lastIndex = @advanceStringIndex(str, regexp.lastIndex, unicode);

        resultList.@push(resultString);
    }
}

@overriddenName="[Symbol.match]"
function match(strArg)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("RegExp.prototype.@@match requires that |this| be an Object");

    var str = @toString(strArg);

    // Check for observable side effects and call the fast path if there aren't any.
    if (!@hasObservableSideEffectsForRegExpMatch(this))
        return @regExpMatchFast.@call(this, str);
    return @matchSlow(this, str);
}

@overriddenName="[Symbol.matchAll]"
function matchAll(strArg)
{
    "use strict";

    var regExp = this;
    if (!@isObject(regExp))
        @throwTypeError("RegExp.prototype.@@matchAll requires |this| to be an Object");

    var string = @toString(strArg);
    var Matcher = @speciesConstructor(regExp, @RegExp);

    var flags = @toString(regExp.flags);
    var matcher = new Matcher(regExp, flags);
    matcher.lastIndex = @toLength(regExp.lastIndex);

    var global = @stringIncludesInternal.@call(flags, "g");
    var fullUnicode = @stringIncludesInternal.@call(flags, "u");

    return new @RegExpStringIterator(matcher, string, global, fullUnicode);
}

@overriddenName="[Symbol.replace]"
function replace(strArg, replace)
{
    "use strict";

    function getSubstitution(matched, str, position, captures, namedCaptures, replacement)
    {
        "use strict";

        var matchLength = matched.length;
        var stringLength = str.length;
        var tailPos = position + matchLength;
        var m = captures.length;
        var replacementLength = replacement.length;
        var result = "";
        var lastStart = 0;

        for (var start = 0; start = replacement.indexOf("$", lastStart), start !== -1; lastStart = start) {
            if (start - lastStart > 0)
                result = result + replacement.substring(lastStart, start);
            start++;
            var ch = replacement.charAt(start);
            if (ch === "")
                result = result + "$";
            else {
                switch (ch)
                {
                case "$":
                    result = result + "$";
                    start++;
                    break;
                case "&":
                    result = result + matched;
                    start++;
                    break;
                case "`":
                    if (position > 0)
                        result = result + str.substring(0, position);
                    start++;
                    break;
                case "'":
                    if (tailPos < stringLength)
                        result = result + str.substring(tailPos);
                    start++;
                    break;
                case "<":
                    if (namedCaptures !== @undefined) {
                        var groupNameStartIndex = start + 1;
                        var groupNameEndIndex = replacement.indexOf(">", groupNameStartIndex);
                        if (groupNameEndIndex !== -1) {
                            var groupName = replacement.substring(groupNameStartIndex, groupNameEndIndex);
                            var capture = namedCaptures[groupName];
                            if (capture !== @undefined)
                                result = result + @toString(capture);

                            start = groupNameEndIndex + 1;
                            break;
                        }
                    }

                    result = result + "$<";
                    start++;
                    break;
                default:
                    var chCode = ch.charCodeAt(0);
                    if (chCode >= 0x30 && chCode <= 0x39) {
                        var originalStart = start - 1;
                        start++;

                        var n = chCode - 0x30;
                        if (n > m) {
                            result = result + replacement.substring(originalStart, start);
                            break;
                        }

                        if (start < replacementLength) {
                            var nextChCode = replacement.charCodeAt(start);
                            if (nextChCode >= 0x30 && nextChCode <= 0x39) {
                                var nn = 10 * n + nextChCode - 0x30;
                                if (nn <= m) {
                                    n = nn;
                                    start++;
                                }
                            }
                        }

                        if (n == 0) {
                            result = result + replacement.substring(originalStart, start);
                            break;
                        }

                        var capture = captures[n - 1];
                        if (capture !== @undefined)
                            result = result + capture;
                    } else
                        result = result + "$";
                    break;
                }
            }
        }

        return result + replacement.substring(lastStart);
    }

    if (!@isObject(this))
        @throwTypeError("RegExp.prototype.@@replace requires that |this| be an Object");

    var regexp = this;

    var str = @toString(strArg);
    var stringLength = str.length;
    var functionalReplace = typeof replace === 'function';

    if (!functionalReplace)
        replace = @toString(replace);

    var global = regexp.global;
    var unicode = false;

    if (global) {
        unicode = regexp.unicode;
        regexp.lastIndex = 0;
    }

    var resultList = [];
    var result;
    var done = false;
    while (!done) {
        result = @regExpExec(regexp, str);

        if (result === null)
            done = true;
        else {
            resultList.@push(result);
            if (!global)
                done = true;
            else {
                var matchStr = @toString(result[0]);

                if (!matchStr.length)
                    regexp.lastIndex = @advanceStringIndex(str, regexp.lastIndex, unicode);
            }
        }
    }

    var accumulatedResult = "";
    var nextSourcePosition = 0;
    var lastPosition = 0;

    for (var i = 0, resultListLength = resultList.length; i < resultListLength; ++i) {
        var result = resultList[i];
        var nCaptures = result.length - 1;
        if (nCaptures < 0)
            nCaptures = 0;
        var matched = @toString(result[0]);
        var matchLength = matched.length;
        var position = result.index;
        position = (position > stringLength) ? stringLength : position;
        position = (position < 0) ? 0 : position;

        var captures = [];
        for (var n = 1; n <= nCaptures; n++) {
            var capN = result[n];
            if (capN !== @undefined)
                capN = @toString(capN);
            captures.@push(capN);
        }

        var replacement;
        var namedCaptures = result.groups;

        if (functionalReplace) {
            var replacerArgs = [ matched ].concat(captures);
            replacerArgs.@push(position);
            replacerArgs.@push(str);

            if (namedCaptures !== @undefined)
                replacerArgs.@push(namedCaptures);

            var replValue = replace.@apply(@undefined, replacerArgs);
            replacement = @toString(replValue);
        } else {
            if (namedCaptures !== @undefined)
                namedCaptures = @toObject(namedCaptures, "RegExp.prototype[Symbol.replace] requires 'groups' property of a match not be null");

            replacement = getSubstitution(matched, str, position, captures, namedCaptures, replace);
        }

        if (position >= nextSourcePosition && position >= lastPosition) {
            accumulatedResult = accumulatedResult + str.substring(nextSourcePosition, position) + replacement;
            nextSourcePosition = position + matchLength;
            lastPosition = position;
        }
    }

    if (nextSourcePosition >= stringLength)
        return  accumulatedResult;

    return accumulatedResult + str.substring(nextSourcePosition);
}

// 21.2.5.9 RegExp.prototype[@@search] (string)
@overriddenName="[Symbol.search]"
function search(strArg)
{
    "use strict";

    var regexp = this;

    // Check for observable side effects and call the fast path if there aren't any.
    if (@isRegExpObject(regexp)
        && @tryGetById(regexp, "exec") === @regExpBuiltinExec
        && typeof regexp.lastIndex === "number")
        return @regExpSearchFast.@call(regexp, strArg);

    // 1. Let rx be the this value.
    // 2. If Type(rx) is not Object, throw a TypeError exception.
    if (!@isObject(this))
        @throwTypeError("RegExp.prototype.@@search requires that |this| be an Object");

    // 3. Let S be ? ToString(string).
    var str = @toString(strArg)

    // 4. Let previousLastIndex be ? Get(rx, "lastIndex").
    var previousLastIndex = regexp.lastIndex;

    // 5.If SameValue(previousLastIndex, 0) is false, then
    // 5.a. Perform ? Set(rx, "lastIndex", 0, true).
    // FIXME: Add SameValue support. https://bugs.webkit.org/show_bug.cgi?id=173226
    if (previousLastIndex !== 0)
        regexp.lastIndex = 0;

    // 6. Let result be ? RegExpExec(rx, S).
    var result = @regExpExec(regexp, str);

    // 7. Let currentLastIndex be ? Get(rx, "lastIndex").
    // 8. If SameValue(currentLastIndex, previousLastIndex) is false, then
    // 8.a. Perform ? Set(rx, "lastIndex", previousLastIndex, true).
    // FIXME: Add SameValue support. https://bugs.webkit.org/show_bug.cgi?id=173226
    if (regexp.lastIndex !== previousLastIndex)
        regexp.lastIndex = previousLastIndex;

    // 9. If result is null, return -1.
    if (result === null)
        return -1;

    // 10. Return ? Get(result, "index").
    return result.index;
}

@globalPrivate
function hasObservableSideEffectsForRegExpSplit(regexp)
{
    "use strict";

    if (!@isRegExpObject(regexp))
        return true;

    // This is accessed by the RegExpExec internal function.
    var regexpExec = @tryGetById(regexp, "exec");
    if (regexpExec !== @regExpBuiltinExec)
        return true;
    
    // This is accessed by step 5 below.
    var regexpFlags = @tryGetById(regexp, "flags");
    if (regexpFlags !== @regExpProtoFlagsGetter)
        return true;
    
    // These are accessed by the builtin flags getter.
    var regexpGlobal = @tryGetById(regexp, "global");
    if (regexpGlobal !== @regExpProtoGlobalGetter)
        return true;
    var regexpIgnoreCase = @tryGetById(regexp, "ignoreCase");
    if (regexpIgnoreCase !== @regExpProtoIgnoreCaseGetter)
        return true;
    var regexpMultiline = @tryGetById(regexp, "multiline");
    if (regexpMultiline !== @regExpProtoMultilineGetter)
        return true;
    var regexpSticky = @tryGetById(regexp, "sticky");
    if (regexpSticky !== @regExpProtoStickyGetter)
        return true;
    var regexpUnicode = @tryGetById(regexp, "unicode");
    if (regexpUnicode !== @regExpProtoUnicodeGetter)
        return true;
    
    // This is accessed by the RegExp species constructor.
    var regexpSource = @tryGetById(regexp, "source");
    if (regexpSource !== @regExpProtoSourceGetter)
        return true;

    return typeof regexp.lastIndex !== "number";
}

// ES 21.2.5.11 RegExp.prototype[@@split](string, limit)
@overriddenName="[Symbol.split]"
function split(string, limit)
{
    "use strict";

    // 1. Let rx be the this value.
    // 2. If Type(rx) is not Object, throw a TypeError exception.
    if (!@isObject(this))
        @throwTypeError("RegExp.prototype.@@split requires that |this| be an Object");
    var regexp = this;

    // 3. Let S be ? ToString(string).
    var str = @toString(string);

    // 4. Let C be ? SpeciesConstructor(rx, %RegExp%).
    var speciesConstructor = @speciesConstructor(regexp, @RegExp);

    if (speciesConstructor === @RegExp && !@hasObservableSideEffectsForRegExpSplit(regexp))
        return @regExpSplitFast.@call(regexp, str, limit);

    // 5. Let flags be ? ToString(? Get(rx, "flags")).
    var flags = @toString(regexp.flags);

    // 6. If flags contains "u", var unicodeMatching be true.
    // 7. Else, let unicodeMatching be false.
    var unicodeMatching = @stringIncludesInternal.@call(flags, "u");
    // 8. If flags contains "y", var newFlags be flags.
    // 9. Else, let newFlags be the string that is the concatenation of flags and "y".
    var newFlags = @stringIncludesInternal.@call(flags, "y") ? flags : flags + "y";

    // 10. Let splitter be ? Construct(C, « rx, newFlags »).
    var splitter = new speciesConstructor(regexp, newFlags);

    // We need to check again for RegExp subclasses that will fail the speciesConstructor test
    // but can still use the fast path after we invoke the constructor above.
    if (!@hasObservableSideEffectsForRegExpSplit(splitter))
        return @regExpSplitFast.@call(splitter, str, limit);

    // 11. Let A be ArrayCreate(0).
    // 12. Let lengthA be 0.
    var result = [];

    // 13. If limit is undefined, let lim be 2^32-1; else var lim be ? ToUint32(limit).
    limit = (limit === @undefined) ? 0xffffffff : limit >>> 0;

    // 16. If lim = 0, return A.
    if (!limit)
        return result;

    // 14. [Defered from above] Let size be the number of elements in S.
    var size = str.length;

    // 17. If size = 0, then
    if (!size) {
        // a. Let z be ? RegExpExec(splitter, S).
        var z = @regExpExec(splitter, str);
        // b. If z is not null, return A.
        if (z != null)
            return result;
        // c. Perform ! CreateDataProperty(A, "0", S).
        @putByValDirect(result, 0, str);
        // d. Return A.
        return result;
    }

    // 15. [Defered from above] Let p be 0.
    var position = 0;
    // 18. Let q be p.
    var matchPosition = 0;

    // 19. Repeat, while q < size
    while (matchPosition < size) {
        // a. Perform ? Set(splitter, "lastIndex", q, true).
        splitter.lastIndex = matchPosition;
        // b. Let z be ? RegExpExec(splitter, S).
        var matches = @regExpExec(splitter, str);
        // c. If z is null, let q be AdvanceStringIndex(S, q, unicodeMatching).
        if (matches === null)
            matchPosition = @advanceStringIndex(str, matchPosition, unicodeMatching);
        // d. Else z is not null,
        else {
            // i. Let e be ? ToLength(? Get(splitter, "lastIndex")).
            var endPosition = @toLength(splitter.lastIndex);
            // ii. Let e be min(e, size).
            endPosition = (endPosition <= size) ? endPosition : size;
            // iii. If e = p, let q be AdvanceStringIndex(S, q, unicodeMatching).
            if (endPosition === position)
                matchPosition = @advanceStringIndex(str, matchPosition, unicodeMatching);
            // iv. Else e != p,
            else {
                // 1. Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through q (exclusive).
                var subStr = @stringSubstrInternal.@call(str, position, matchPosition - position);
                // 2. Perform ! CreateDataProperty(A, ! ToString(lengthA), T).
                // 3. Let lengthA be lengthA + 1.
                @putByValDirect(result, result.length, subStr);
                // 4. If lengthA = lim, return A.
                if (result.length == limit)
                    return result;

                // 5. Let p be e.
                position = endPosition;
                // 6. Let numberOfCaptures be ? ToLength(? Get(z, "length")).
                // 7. Let numberOfCaptures be max(numberOfCaptures-1, 0).
                var numberOfCaptures = matches.length > 1 ? matches.length - 1 : 0;

                // 8. Let i be 1.
                var i = 1;
                // 9. Repeat, while i <= numberOfCaptures,
                while (i <= numberOfCaptures) {
                    // a. Let nextCapture be ? Get(z, ! ToString(i)).
                    var nextCapture = matches[i];
                    // b. Perform ! CreateDataProperty(A, ! ToString(lengthA), nextCapture).
                    // d. Let lengthA be lengthA + 1.
                    @putByValDirect(result, result.length, nextCapture);
                    // e. If lengthA = lim, return A.
                    if (result.length == limit)
                        return result;
                    // c. Let i be i + 1.
                    i++;
                }
                // 10. Let q be p.
                matchPosition = position;
            }
        }
    }
    // 20. Let T be a String value equal to the substring of S consisting of the elements at indices p (inclusive) through size (exclusive).
    var remainingStr = @stringSubstrInternal.@call(str, position, size);
    // 21. Perform ! CreateDataProperty(A, ! ToString(lengthA), T).
    @putByValDirect(result, result.length, remainingStr);
    // 22. Return A.
    return result;
}

// ES 21.2.5.13 RegExp.prototype.test(string)
@intrinsic=RegExpTestIntrinsic
function test(strArg)
{
    "use strict";

    var regexp = this;

    // Check for observable side effects and call the fast path if there aren't any.
    if (@isRegExpObject(regexp)
        && @tryGetById(regexp, "exec") === @regExpBuiltinExec
        && typeof regexp.lastIndex === "number")
        return @regExpTestFast.@call(regexp, strArg);

    // 1. Let R be the this value.
    // 2. If Type(R) is not Object, throw a TypeError exception.
    if (!@isObject(regexp))
        @throwTypeError("RegExp.prototype.test requires that |this| be an Object");

    // 3. Let string be ? ToString(S).
    var str = @toString(strArg);

    // 4. Let match be ? RegExpExec(R, string).
    var match = @regExpExec(regexp, str);

    // 5. If match is not null, return true; else return false.
    if (match !== null)
        return true;
    return false;
}
