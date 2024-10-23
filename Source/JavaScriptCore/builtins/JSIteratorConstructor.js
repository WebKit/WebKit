/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
function getIteratorFlattenable(obj, rejectStrings)
{
    "use strict";

    // 1. If obj is not an Object, then
    //   a. If stringHandling is reject-strings or obj is not a String, throw a TypeError exception.
    if (!@isObject(obj) && (rejectStrings || typeof obj !== "string"))
        @throwTypeError("GetIteratorFlattenable expects its first argument to be an object" + (rejectStrings ? "" : " or a string"));
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
    // Step 6 isn't performed; raw iterator is returned instead of an Iterator Record.
    return iterator;
}

// https://tc39.es/proposal-iterator-helpers/#sec-iterator.from
function from(value)
{
    "use strict";

    // 1. Let iteratorRecord be ? GetIteratorFlattenable(O, iterate-strings).
    var iterator = @getIteratorFlattenable(value, /* rejectStrings: */ false);
    var iteratorNextMethod = iterator.next;
    // 2. Let hasInstance be ? OrdinaryHasInstance(%Iterator%, iteratorRecord.[[Iterator]]).
    // 3. If hasInstance is true, then
    //   a. Return iteratorRecord.[[Iterator]].
    if (@instanceOf(iterator, @Iterator.prototype))
        return iterator;

    // 4. Let wrapper be OrdinaryObjectCreate(%WrapForValidIteratorPrototype%, « [[Iterated]] »).
    // 5. Set wrapper.[[Iterated]] to iteratorRecord.
    // 6. Return wrapper.
    return @wrapForValidIteratorCreate(iterator, iteratorNextMethod);
}

// https://tc39.es/proposal-iterator-sequencing/#sec-iterator.concat
function concat(/* ...items */)
{
    "use strict";

    var argumentCount = @argumentCount();

    var openMethods = [];
    var iterables = [];
    for (var i = 0; i < argumentCount; ++i) {
        var iterable = arguments[i];
        if (!@isObject(iterable))
            @throwTypeError("Iterator.concat expects all arguments to be objects");

        var openMethod = iterable.@@iterator;
        if (!@isCallable(openMethod))
            @throwTypeError("Iterator.concat expects all arguments to be iterable");

        @arrayPush(openMethods, openMethod);
        @arrayPush(iterables, iterable);
    }

    var generator = (function*() {
        for (var i = 0; i < argumentCount; ++i) {
            var iterator = openMethods[i].@call(iterables[i]);
            if (!@isObject(iterator))
                @throwTypeError("Iterator.concat expects all arguments to be iterable");

            var nextMethod = iterator.next;

            for (;;) {
                var result = @iteratorGenericNext(nextMethod, iterator);
                if (result.done)
                    break;

                var value = result.value;
                @ifAbruptCloseIterator(iterator, (
                    yield value
                ));
            }
        }
    })();

    return @iteratorHelperCreate(generator, null);
}
