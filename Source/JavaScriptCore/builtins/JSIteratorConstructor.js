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

// https://tc39.es/proposal-joint-iteration/#sec-getoptionsobject
@linkTimeConstant
function getOptionsObject(options)
{
    if (options === @undefined)
        return @Object.@create(null);
    if (@isObject(options))
        return options;
    @throwTypeError("options should be undefined or object");
}

// https://tc39.es/proposal-joint-iteration/#sec-closeall
@linkTimeConstant
function iteratorCloseAll(openIters, completion)
{
    "use strict";

    for (var i = openIters.length - 1; i >= 0; i--) {
        var iter = openIters[i];
        if (iter === null)
            continue;
        openIters[i] = null;

        var returnMethod;
        try {
            returnMethod = iter.return;
        } catch (e) {
            if (completion === @undefined)
                completion = e;
            continue;
        }

        if (returnMethod !== @undefined && returnMethod !== null) {
            try {
                returnMethod.@call(iter);
            } catch (e) {
                if (completion === @undefined)
                    completion = e;
            }
        }
    }

    if (completion !== @undefined)
        throw completion;

    return completion;
}

// https://tc39.es/proposal-joint-iteration/#sec-IteratorZip
@linkTimeConstant
function iteratorZip(iters, iterNextMethods, mode, padding, finishResults)
{
    "use strict";

    var iterCount = iters.length;
    var openIters = [];
    for (var i = 0; i < iterCount; i++)
        @arrayPush(openIters, iters[i]);
    var generator = (function*() {
        if (iterCount === 0)
            return;
        for (;;) {
            var results = [];
            for (var i = 0; i < iterCount; i++) {
                var iter = iters[i];
                if (iter === null) {
                    @arrayPush(results, padding[i]);
                } else {
                    var result;
                    var resultValue;
                    var isDone = false;
                    try {
                        result = iterNextMethods[i].@call(iter);
                        if (!@isObject(result))
                            @throwTypeError("Iterator result interface is not an object");
                        isDone = result.done;
                        if (!isDone)
                            resultValue = result.value;
                    } catch (e) {
                        openIters[i] = null;
                        throw @iteratorCloseAll(openIters, e);
                    }
                    if (isDone) {
                        openIters[i] = null;
                        if (mode === "shortest") {
                            @iteratorCloseAll(openIters, @undefined);
                            return;
                        } else if (mode === "strict") {
                            if (i !== 0)
                                throw @iteratorCloseAll(openIters, @makeTypeError("Iterators in strict mode have different lengths"));

                            for (var k = 1; k < iterCount; k++) {
                                var openDone = false;
                                try {
                                    var openResult = iterNextMethods[k].@call(iters[k]);
                                    if (!@isObject(openResult))
                                        @throwTypeError("Iterator result interface is not an object");
                                    openDone = openResult.done;
                                } catch (e) {
                                    openIters[k] = null;
                                    throw @iteratorCloseAll(openIters, e);
                                }
                                if (openDone)
                                    openIters[k] = null;
                                else
                                    throw @iteratorCloseAll(openIters, @makeTypeError("Iterators in strict mode have different lengths"));
                            }
                            return;
                        } else {
                            var allClosed = true;
                            for (var j = 0; j < iterCount; j++) {
                                if (openIters[j] !== null) {
                                    allClosed = false;
                                    break;
                                }
                            }
                            if (allClosed)
                                return;
                            iters[i] = null;
                            resultValue = padding[i];
                        }
                    }
                    @arrayPush(results, resultValue);
                }
            }
            results = finishResults(results);
            var yieldAbrupt = true;
            try {
                yield results;
                yieldAbrupt = false;
            } finally {
                if (yieldAbrupt)
                    @iteratorCloseAll(openIters, @undefined);
            }
        }
    })();
    return @iteratorHelperCreate(generator, openIters);
}

// https://tc39.es/proposal-joint-iteration/#sec-iterator.zip
function zip(iterables)
{
    "use strict";

    if (!@isObject(iterables))
        @throwTypeError("Iterator.zip requires iterables to be an object");

    var options = @getOptionsObject(@argument(1));
    var mode = options.mode;
    if (mode === @undefined)
        mode = "shortest";
    if (mode !== "shortest" && mode !== "longest" && mode !== "strict")
        @throwTypeError("mode should be 'shortest' or 'longest' or 'strict'");

    var paddingOption;
    if (mode === "longest") {
        paddingOption = options.padding;
        if (paddingOption !== @undefined && !@isObject(paddingOption))
            @throwTypeError("padding option should be an object");
    }

    var iters = [];
    var iterNextMethods = [];
    var padding = [];

    var iteratorMethod = iterables.@@iterator;
    if (!@isCallable(iteratorMethod))
        @throwTypeError("Iterator.zip requires that iterables[Symbol.iterator] be a function");
    var inputIter = iteratorMethod.@call(iterables);
    if (!@isObject(inputIter))
        @throwTypeError("Iterator.zip requires that iterables[Symbol.iterator]() returns an object");
    var inputIterNextMethod = inputIter.next;

    for (;;) {
        var result;
        var done;
        var value;
        try {
            result = inputIterNextMethod.@call(inputIter);
            if (!@isObject(result))
                @throwTypeError("Iterator result interface is not an object");
            done = result.done;
            if (!done)
                value = result.value;
        } catch (e) {
            throw @iteratorCloseAll(iters, e);
        }
        if (done)
            break;
        var iter;
        var iterNextMethod;
        try {
            iter = @getIteratorFlattenable(value, true);
            iterNextMethod = iter.next;
        } catch (e) {
            var allIters = [inputIter];
            for (var j = 0; j < iters.length; j++)
                @arrayPush(allIters, iters[j]);
            throw @iteratorCloseAll(allIters, e);
        }
        @arrayPush(iters, iter);
        @arrayPush(iterNextMethods, iterNextMethod);
    }

    var iterCount = iters.length;
    if (mode === "longest") {
        if (paddingOption === @undefined) {
            for (var i = 0; i < iterCount; i++)
                @arrayPush(padding, @undefined);
        } else {
            var paddingIterMethod = paddingOption.@@iterator;
            if (!@isCallable(paddingIterMethod))
                throw @iteratorCloseAll(iters, @makeTypeError("padding[Symbol.iterator] is not a function"));

            var paddingIter;
            var paddingIterNextMethod;
            try {
                paddingIter = paddingIterMethod.@call(paddingOption);
                if (!@isObject(paddingIter))
                    @throwTypeError("padding[Symbol.iterator]() did not return an object");
                paddingIterNextMethod = paddingIter.next;
            } catch (e) {
                throw @iteratorCloseAll(iters, e);
            }
            var usingIterator = true;
            try {
                for (var i = 0; i < iterCount; i++) {
                    if (usingIterator) {
                        var paddingResult = paddingIterNextMethod.@call(paddingIter);
                        if (!@isObject(paddingResult))
                            @throwTypeError("Iterator result interface is not an object");

                        if (paddingResult.done)
                            usingIterator = false;
                        else
                            @arrayPush(padding, paddingResult.value);
                    }
                    if (!usingIterator)
                        @arrayPush(padding, @undefined);
                }
            } catch (e) {
                throw @iteratorCloseAll(iters, e);
            }
            if (usingIterator) {
                try {
                    @iteratorGenericClose(paddingIter);
                } catch (e) {
                    throw @iteratorCloseAll(iters, e);
                }
            }
        }
    }
    return @iteratorZip(iters, iterNextMethods, mode, padding, function (results) { return results });
}

// https://tc39.es/proposal-joint-iteration/#sec-iterator.zipkeyed
function zipKeyed(iterables)
{
    "use strict";

    if (!@isObject(iterables))
        @throwTypeError("Iterator.zipKeyed requires iterables to be an object");

    var options = @getOptionsObject(@argument(1));
    var mode = options.mode;
    if (mode === @undefined)
        mode = "shortest";
    if (mode !== "shortest" && mode !== "longest" && mode !== "strict")
        @throwTypeError("mode should be 'shortest' or 'longest' or 'strict'");

    var paddingOption;
    if (mode === "longest") {
        paddingOption = options.padding;
        if (paddingOption !== @undefined && !@isObject(paddingOption))
            @throwTypeError("padding option should be an object");
    }

    var iters = [];
    var iterNextMethods = [];
    var padding = [];
    var keys = [];
    var allKeys = @reflectOwnKeys(iterables);

    for (var i = 0; i < allKeys.length; i++) {
        var key = allKeys[i];
        var desc;
        try {
            desc = @Object.@getOwnPropertyDescriptor(iterables, key);
        } catch (e) {
            throw @iteratorCloseAll(iters, e);
        }
        if (desc !== @undefined && desc.enumerable === true) {
            var value;
            try {
                value = iterables[key];
            } catch (e) {
                throw @iteratorCloseAll(iters, e);
            }
            if (value !== @undefined) {
                @arrayPush(keys, key);
                var iter;
                var iterNextMethod;
                try {
                    iter = @getIteratorFlattenable(value, true);
                    iterNextMethod = iter.next;
                } catch (e) {
                    throw @iteratorCloseAll(iters, e);
                }
                @arrayPush(iters, iter);
                @arrayPush(iterNextMethods, iterNextMethod);
            }
        }
    }

    var iterCount = iters.length;
    if (mode === "longest") {
        if (paddingOption === @undefined) {
            for (var i = 0; i < iterCount; i++)
                @arrayPush(padding, @undefined);
        } else {
            for (var i = 0; i < keys.length; i++) {
                var key = keys[i];
                var value;
                try {
                    value = paddingOption[key];
                } catch (e) {
                    throw @iteratorCloseAll(iters, e);
                }
                @arrayPush(padding, value);
            }
        }
    }
    var finishResults = function(results) {
        var obj = @Object.@create(null);
        for (var i = 0; i < iterCount; i++)
            obj[keys[i]] = results[i];
        return obj;
    };
    return @iteratorZip(iters, iterNextMethods, mode, padding, finishResults);
}
