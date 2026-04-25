/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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


function every(callback /*, thisArg */)
{
    "use strict";

    if (this === null)
        throw new @TypeError("Array.prototype.every requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.every requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = @toLength(array.length);

    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.every callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    
    for (var i = 0; i < length; i++) {
        if (!(i in array))
            continue;
        if (!callback.@call(thisArg, array[i], i, array))
            return false;
    }
    
    return true;
}

function forEach(callback /*, thisArg */)
{
    "use strict";

    if (this === null)
        throw new @TypeError("Array.prototype.forEach requires that |this| not be null");
    
    if (this === undefined)
        throw new @TypeError("Array.prototype.forEach requires that |this| not be undefined");
    
    var array = @Object(this);
    var length = @toLength(array.length);

    if (typeof callback !== "function")
        throw new @TypeError("Array.prototype.forEach callback must be a function");
    
    var thisArg = arguments.length > 1 ? arguments[1] : undefined;
    
    for (var i = 0; i < length; i++) {
        if (i in array)
            callback.@call(thisArg, array[i], i, array);
    }
}

@overriddenName="[Symbol.match]"
function match(strArg)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("RegExp.prototype.@@match requires that |this| be an Object");

    let regexp = this;

    // Check for observable side effects and call the fast path if there aren't any.
    if (!@hasObservableSideEffectsForRegExpMatch(regexp))
        return @regExpMatchFast.@call(regexp, strArg);

    let str = @toString(strArg);

    if (!regexp.global)
        return @regExpExec(regexp, str);
    
    let unicode = regexp.unicode;
    regexp.lastIndex = 0;
    let resultList = [];

    // FIXME: It would be great to implement a solution similar to what we do in
    // RegExpObject::matchGlobal(). It's not clear if this is possible, since this loop has
    // effects. https://bugs.webkit.org/show_bug.cgi?id=158145
    const maximumReasonableMatchSize = 100000000;

    while (true) {
        let result = @regExpExec(regexp, str);
        
        if (result === null) {
            if (resultList.length === 0)
                return null;
            return resultList;
        }

        if (resultList.length > maximumReasonableMatchSize)
            @throwOutOfMemoryError();

        if (!@isObject(result))
            @throwTypeError("RegExp.prototype.@@match call to RegExp.exec didn't return null or an object");

        let resultString = @toString(result[0]);

        if (!resultString.length)
            regexp.lastIndex = @advanceStringIndex(str, regexp.lastIndex, unicode);

        resultList.@push(resultString);
    }
}

@intrinsic=RegExpTestIntrinsic
function test(strArg)
{
    "use strict";

    let regexp = this;

    // Check for observable side effects and call the fast path if there aren't any.
    if (@isRegExpObject(regexp) && @tryGetById(regexp, "exec") === @regExpBuiltinExec)
        return @regExpTestFast.@call(regexp, strArg);

    // 1. Let R be the this value.
    // 2. If Type(R) is not Object, throw a TypeError exception.
    if (!@isObject(regexp))
        @throwTypeError("RegExp.prototype.test requires that |this| be an Object");

    // 3. Let string be ? ToString(S).
    let str = @toString(strArg);

    // 4. Let match be ? RegExpExec(R, string).
    let match = @regExpExec(regexp, str);

    // 5. If match is not null, return true; else return false.
    if (match !== null)
        return true;
    return false;
}
