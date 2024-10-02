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

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.some
function some(predicate)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.some requires that |this| be an Object.");

    if (!@isCallable(predicate))
        @throwTypeError("Iterator.prototype.some callback must be a function.");

    var count = 0;
    var iterator = this;
    var wrapper = { @@iterator: function () { return iterator; }};
    for (var item of wrapper) {
        if (predicate(item, count++))
            return true;
    }

    return false;
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.every
function every(predicate)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.every requires that |this| be an Object.");

    if (!@isCallable(predicate))
        @throwTypeError("Iterator.prototype.every callback must be a function.");

    var count = 0;
    var iterator = this;
    var wrapper = { @@iterator: function () { return iterator; }};
    for (var item of wrapper) {
        if (!predicate(item, count++))
            return false;
    }

    return true;
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.find
function find(predicate)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.find requires that |this| be an Object.");

    if (!@isCallable(predicate))
        @throwTypeError("Iterator.prototype.find callback must be a function.");

    var count = 0;
    var iterator = this;
    var wrapper = { @@iterator: function () { return iterator; }};
    for (var item of wrapper) {
        if (predicate(item, count++))
            return item;
    }

    return @undefined;
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.reduce
function reduce(reducer /*, initialValue */)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.reduce requires that |this| be an Object.");

    if (!@isCallable(reducer))
        @throwTypeError("Iterator.prototype.reduce reducer argument must be a function.");

    var initialValue = @argument(1);

    var iteratedIterator = this;
    var iteratedNextMethod = this.next;

    var accumulator;
    var counter = 0;
    if (initialValue === @undefined) {
        var result = iteratedNextMethod.@call(iteratedIterator);
        if (!@isObject(result))
            @throwTypeError("Iterator result interface is not an object.");
        if (result.done)
            @throwTypeError("Iterator.prototype.reduce requires an initial value or an iterator that is not done.");
        accumulator = result.value;
        counter = 1;
    } else
        accumulator = initialValue;

    var wrapper = {
        @@iterator: function()
            {
                return {
                    next: function() { return iteratedNextMethod.@call(iteratedIterator); },
                    get return() { return iteratedIterator.return; },
                };
            },
    };
    for (var item of wrapper) {
        accumulator = reducer(accumulator, item, counter++);
    }

    return accumulator;
}
