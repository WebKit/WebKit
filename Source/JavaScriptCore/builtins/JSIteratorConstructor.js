/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
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

// https://tc39.es/proposal-iterator-helpers/#sec-getiteratorflattenable
@linkTimeConstant
function getIteratorFlattenable(obj)
{
    "use strict";

    // 1. If obj is not an Object, then
    //   a. If stringHandling is reject-strings or obj is not a String, throw a TypeError exception.
    if (!@isObject(obj) && typeof obj !== "string")
        @throwTypeError("Iterafor.from requires an object or string");
    // 2. Let method be ? GetMethod(obj, @@iterator).
    var method = obj.@@iterator;
    // 3. If method is undefined, then
    //   a. Let iterator be obj.
    var iterator;
    if (@isUndefinedOrNull(method)) {
        iterator = obj;
        // 4. Else,
        //   a. Let iterator be ? Call(method, obj).
    } else
        iterator = method.@call(obj);
    // 5. If iterator is not an Object, throw a TypeError exception.
    if (!@isObject(iterator))
        @throwTypeError("Iterator is not an object");
    // 6. Return ? GetIteratorDirect(iterator).
    return [iterator, iterator.next];
}

// https://tc39.es/proposal-iterator-helpers/#sec-iterator.from
function from(o)
{
    "use strict";

    // 1. Let iteratorRecord be ? GetIteratorFlattenable(O, iterate-strings).
    var iteratorRecord = @getIteratorFlattenable(o);
    // 2. Let hasInstance be ? OrdinaryHasInstance(%Iterator%, iteratorRecord.[[Iterator]]).
    // 3. If hasInstance is true, then
    //   a. Return iteratorRecord.[[Iterator]].
    if (@instanceOf(iteratorRecord[0], @Iterator.prototype))
        return iteratorRecord[0];

    // 4. Let wrapper be OrdinaryObjectCreate(%WrapForValidIteratorPrototype%, « [[Iterated]] »).
    // 5. Set wrapper.[[Iterated]] to iteratorRecord.
    // 6. Return wrapper.
    return @wrapForValidIteratorCreate(iteratorRecord[0], iteratorRecord[1])
}
