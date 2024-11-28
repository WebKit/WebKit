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

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.map
function map(mapper)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.map requires that |this| be an Object.");

    if (!@isCallable(mapper))
        @throwTypeError("Iterator.prototype.map callback must be a function.");

    var iterated = this;
    var iteratedNextMethod = iterated.next;

    var generator = (function*() {
        var counter = 0;
        for (;;) {
            var result = @iteratorGenericNext(iteratedNextMethod, iterated);
            if (result.done)
                return;

            var value = result.value;
            @ifAbruptCloseIterator(iterated, (
                yield mapper(value, counter++)
            ));
        }
    })();

    return @iteratorHelperCreate(generator, iterated);
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.filter
function filter(predicate)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.filter requires that |this| be an Object.");

    if (!@isCallable(predicate))
        @throwTypeError("Iterator.prototype.filter callback must be a function.");

    var iterated = this;
    var iteratedNextMethod = iterated.next;

    var generator = (function*() {
        var counter = 0;
        for (;;) {
            var result = @iteratorGenericNext(iteratedNextMethod, iterated);
            if (result.done)
                return;

            var value = result.value;
            @ifAbruptCloseIterator(iterated, (
                predicate(value, counter++) && (yield value)
            ));
        }
    })();

    return @iteratorHelperCreate(generator, iterated);
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.take
function take(limit)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.take requires that |this| be an Object.");

    var numLimit = @toNumber(limit);
    if (numLimit !== numLimit)
        @throwRangeError("Iterator.prototype.take argument must not be NaN.");

    var intLimit = @toIntegerOrInfinity(numLimit);
    if (intLimit < 0)
        @throwRangeError("Iterator.prototype.take argument must be non-negative.");

    var iterated = this;
    var iteratedNextMethod = iterated.next;

    var generator = (function*() {
        var remaining = intLimit;
        for (;;) {
            if (remaining === 0) {
                @iteratorGenericClose(iterated);
                return;
            }

            if (remaining !== @Infinity)
                remaining--;

            var result = @iteratorGenericNext(iteratedNextMethod, iterated);
            if (result.done)
                return;

            var value = result.value;
            @ifAbruptCloseIterator(iterated, (
                yield value
            ));
        }
    })();

    return @iteratorHelperCreate(generator, iterated);
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.drop
function drop(limit)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.drop requires that |this| be an Object.");

    var numLimit = @toNumber(limit);
    if (numLimit !== numLimit)
        @throwRangeError("Iterator.prototype.drop argument must not be NaN.");

    var intLimit = @toIntegerOrInfinity(numLimit);
    if (intLimit < 0)
        @throwRangeError("Iterator.prototype.drop argument must be non-negative.");

    var iterated = this;
    var iteratedNextMethod = iterated.next;

    var generator = (function*() {
        var remaining = intLimit;
        for (;;) {
            var result = @iteratorGenericNext(iteratedNextMethod, iterated);
            if (result.done)
                return;

            if (remaining > 0) {
                if (remaining !== @Infinity)
                    remaining--;
            } else {
                var value = result.value;
                @ifAbruptCloseIterator(iterated, (
                    yield value
                ));
            }
        }
    })();

    return @iteratorHelperCreate(generator, iterated);
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.flatmap
function flatMap(mapper)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.flatMap requires that |this| be an Object.");

    if (!@isCallable(mapper))
        @throwTypeError("Iterator.prototype.flatMap callback must be a function.");

    var iterated = this;
    var iteratedNextMethod = iterated.next;
    var iteratedWrapper = {
        @@iterator: function() { return this; },
        next: function() { return iteratedNextMethod.@call(iterated); },
        return: function() { return iterated.return(@undefined); },
    };

    var generator = (function*() {
        var counter = 0;
        for (var item of iteratedWrapper) {
            var mapped = mapper(item, counter++);
            var innerIterator = @getIteratorFlattenable(mapped, /* rejectStrings: */ true);

            for (var innerItem of { @@iterator: function() { return innerIterator; } })
                yield innerItem;
        }
    })();

    return @iteratorHelperCreate(generator, iterated);
}

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

    var iterated = this;
    var iteratedNextMethod = this.next;

    var accumulator;
    var counter = 0;
    if (initialValue === @undefined) {
        var result = @iteratorGenericNext(iteratedNextMethod, iterated);
        if (result.done)
            @throwTypeError("Iterator.prototype.reduce requires an initial value or an iterator that is not done.");
        accumulator = result.value;
        counter = 1;
    } else
        accumulator = initialValue;

    for (;;) {
        var result = @iteratorGenericNext(iteratedNextMethod, iterated);
        if (result.done)
            break;

        var value = result.value;
        @ifAbruptCloseIterator(iterated, (
            accumulator = reducer(accumulator, value, counter++)
        ));
    }

    return accumulator;
}

// https://tc39.es/proposal-iterator-chunking/#sec-iterator.prototype.chunks
function chunks(chunkSize)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.chunks requires that |this| be an Object.");

    var numChunkSize = @toNumber(chunkSize);
    if (numChunkSize !== numChunkSize)
        @throwRangeError("Iterator.prototype.chunks requires that argument not be NaN.");

    var intChunkSize = @toIntegerOrInfinity(numChunkSize);
    if (intChunkSize < 1 || intChunkSize > @MAX_ARRAY_INDEX)
        @throwRangeError("Iterator.prototype.chunks requires that argument be between 1 and 2**32 - 1.");

    var iterated = this;
    var iteratedNextMethod = this.next;

    var generator = (function*() {
        var buffer = [];

        for (;;) {
            var result = @iteratorGenericNext(iteratedNextMethod, iterated);
            if (result.done) {
                if (buffer.length)
                    yield buffer;
                return;
            }

            @arrayPush(buffer, result.value);

            if (buffer.length === intChunkSize) {
                @ifAbruptCloseIterator(iterated, (
                    yield buffer
                ));

                buffer = [];
            }
        }
    })();

    return @iteratorHelperCreate(generator, iterated);
}

// https://tc39.es/proposal-iterator-chunking/#sec-iterator.prototype.windows
function windows(windowSize)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.windows requires that |this| be an Object.");

    var numWindowSize = @toNumber(windowSize);
    if (numWindowSize !== numWindowSize)
        @throwRangeError("Iterator.prototype.windows requires that argument not be NaN.");

    var intWindowSize = @toIntegerOrInfinity(numWindowSize);
    if (intWindowSize < 1 || intWindowSize > @MAX_ARRAY_INDEX)
        @throwRangeError("Iterator.prototype.windows requires that argument be between 1 and 2**32 - 1.");

    var iterated = this;
    var iteratedNextMethod = this.next;

    var generator = (function*() {
        var buffer = [];

        for (;;) {
            var result = @iteratorGenericNext(iteratedNextMethod, iterated);
            if (result.done)
                return;

            if (buffer.length === intWindowSize) {
                for (var i = 0; i < buffer.length - 1; ++i)
                    buffer[i] = buffer[i + 1];
                buffer[buffer.length - 1] = result.value;
            } else
                @arrayPush(buffer, result.value);

            if (buffer.length === intWindowSize) {
                var copy = @newArrayWithSize(buffer.length);
                for (var i = 0; i < buffer.length; ++i)
                    copy[i] = buffer[i];

                @ifAbruptCloseIterator(iterated, (
                    yield copy
                ));
            }
        }
    })();

    return @iteratorHelperCreate(generator, iterated);
}
