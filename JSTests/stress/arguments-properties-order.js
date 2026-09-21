function getMappedArguments(a, b) { return arguments; }
function getUnmappedArguments(a, b) { "use strict"; return arguments; }

function shouldBeArray(actual, expected) {
    var isEqual =
        actual.length === expected.length &&
        actual.every((item, index) => item === expected[index]);
    if (!isEqual)
        throw new Error(`Expected [${actual.map(String)}] to equal [${expected.map(String)}]`);
}

function forIn(object) {
    var keys = [];
    for (var key in object)
        keys.push(key);
    return keys;
}

noInline(getMappedArguments);
noInline(getUnmappedArguments);
noInline(forIn);

(function() {
    for (var i = 0; i < 1e4; ++i) {
        var mappedArguments = getMappedArguments(0, 1, 2);
        shouldBeArray(forIn(mappedArguments), ["0", "1", "2"]);
        shouldBeArray(Object.keys(mappedArguments), ["0", "1", "2"]);
        shouldBeArray(Reflect.ownKeys(mappedArguments), ["0", "1", "2", "length", "callee", Symbol.iterator]);

        var unmappedArguments = getUnmappedArguments(0);
        shouldBeArray(forIn(unmappedArguments), ["0"]);
        shouldBeArray(Object.keys(unmappedArguments), ["0"]);
        shouldBeArray(Reflect.ownKeys(unmappedArguments), ["0", "length", "callee", Symbol.iterator]);
    }
})();

(function() {
    for (var i = 0; i < 1e4; ++i) {
        var mappedArguments = getMappedArguments(0, 1);
        mappedArguments[8] = 8;
        mappedArguments[2] = 2;
        shouldBeArray(forIn(mappedArguments), ["0", "1", "2", "8"]);
        shouldBeArray(Object.keys(mappedArguments), ["0", "1", "2", "8"]);
        shouldBeArray(Reflect.ownKeys(mappedArguments), ["0", "1", "2", "8", "length", "callee", Symbol.iterator]);

        var unmappedArguments = getUnmappedArguments();
        unmappedArguments[12] = 12;
        unmappedArguments[3] = 3;
        shouldBeArray(forIn(unmappedArguments), ["3", "12"]);
        shouldBeArray(Object.keys(unmappedArguments), ["3", "12"]);
        shouldBeArray(Reflect.ownKeys(unmappedArguments), ["3", "12", "length", "callee", Symbol.iterator]);
    }
})();

(function() {
    for (var i = 0; i < 1e4; ++i) {
        var mappedArguments = getMappedArguments(0);
        mappedArguments.foo = 1;
        mappedArguments.bar = 2;
        shouldBeArray(forIn(mappedArguments), ["0", "foo", "bar"]);
        shouldBeArray(Object.keys(mappedArguments), ["0", "foo", "bar"]);
        shouldBeArray(Object.getOwnPropertyNames(mappedArguments), ["0", "length", "callee", "foo", "bar"]);
        shouldBeArray(Reflect.ownKeys(mappedArguments), ["0", "length", "callee", "foo", "bar", Symbol.iterator]);

        var unmappedArguments = getUnmappedArguments(0, 1, 2);
        unmappedArguments.foo = 1;
        unmappedArguments.bar = 2;
        shouldBeArray(forIn(unmappedArguments), ["0", "1", "2", "foo", "bar"]);
        shouldBeArray(Object.keys(unmappedArguments), ["0", "1", "2", "foo", "bar"]);
        shouldBeArray(Object.getOwnPropertyNames(unmappedArguments), ["0", "1", "2", "length", "callee", "foo", "bar"]);
        shouldBeArray(Reflect.ownKeys(unmappedArguments), ["0", "1", "2", "length", "callee", "foo", "bar", Symbol.iterator]);
    }
})();

function getEmptyMappedArguments() { return arguments; }
noInline(getEmptyMappedArguments);

(function() {
    var symbol = Symbol("s");
    function sloppyCallee(a) {
        var args = sloppyCallee.arguments;
        args.foo = 1;
        delete args.callee;
        args.callee = function() { };
        return Reflect.ownKeys(args);
    }
    function sloppyKeys(a) {
        var args = sloppyKeys.arguments;
        args.foo = 1;
        Reflect.ownKeys(args);
        return Object.keys(args);
    }
    function strictArgs(a) { "use strict"; return arguments; }
    function nonStrictArgs(a) { return nonStrictArgs.arguments; }
    function deleteCallee(args) { return delete args.callee; }
    noInline(sloppyCallee);
    noInline(sloppyKeys);
    noInline(strictArgs);
    noInline(nonStrictArgs);
    noInline(deleteCallee);

    for (var i = 0; i < 1e4; ++i) {
        var mappedArguments = getEmptyMappedArguments();
        mappedArguments[symbol] = 1;
        mappedArguments.z = 2;
        mappedArguments[4] = 4;
        shouldBeArray(Object.keys(mappedArguments), ["4", "z"]);
        shouldBeArray(Reflect.ownKeys(mappedArguments), ["4", "length", "callee", "z", Symbol.iterator, symbol]);

        var unmappedArguments = getUnmappedArguments();
        unmappedArguments[symbol] = 1;
        unmappedArguments.z = 2;
        unmappedArguments[4] = 4;
        shouldBeArray(Object.keys(unmappedArguments), ["4", "z"]);
        shouldBeArray(Reflect.ownKeys(unmappedArguments), ["4", "length", "callee", "z", Symbol.iterator, symbol]);

        var mappedAfterLength = getEmptyMappedArguments();
        mappedAfterLength.foo = 1;
        mappedAfterLength.length = 0;
        shouldBeArray(Reflect.ownKeys(mappedAfterLength), ["length", "callee", "foo", Symbol.iterator]);

        var mappedReaddedLength = getEmptyMappedArguments();
        mappedReaddedLength.foo = 1;
        delete mappedReaddedLength.length;
        mappedReaddedLength.length = 2;
        shouldBeArray(Reflect.ownKeys(mappedReaddedLength), ["callee", "foo", "length", Symbol.iterator]);

        var unmappedAfterDeleteIterator = getUnmappedArguments();
        unmappedAfterDeleteIterator.foo = 1;
        delete unmappedAfterDeleteIterator[Symbol.iterator];
        shouldBeArray(Reflect.ownKeys(unmappedAfterDeleteIterator), ["length", "callee", "foo"]);

        var unmappedReaddedLength = getUnmappedArguments();
        unmappedReaddedLength.foo = 1;
        delete unmappedReaddedLength.length;
        unmappedReaddedLength.length = 2;
        shouldBeArray(Reflect.ownKeys(unmappedReaddedLength), ["callee", "foo", "length", Symbol.iterator]);

        var mappedReaddedIterator = getEmptyMappedArguments();
        mappedReaddedIterator.foo = 1;
        mappedReaddedIterator[symbol] = 1;
        delete mappedReaddedIterator[Symbol.iterator];
        mappedReaddedIterator[Symbol.iterator] = Array.prototype.values;
        shouldBeArray(Reflect.ownKeys(mappedReaddedIterator), ["length", "callee", "foo", symbol, Symbol.iterator]);

        var mappedReaddedLengthAndIterator = getEmptyMappedArguments();
        mappedReaddedLengthAndIterator.foo = 1;
        mappedReaddedLengthAndIterator[symbol] = 1;
        delete mappedReaddedLengthAndIterator.length;
        mappedReaddedLengthAndIterator.length = 2;
        delete mappedReaddedLengthAndIterator[Symbol.iterator];
        mappedReaddedLengthAndIterator[Symbol.iterator] = Array.prototype.values;
        shouldBeArray(Reflect.ownKeys(mappedReaddedLengthAndIterator), ["callee", "foo", "length", symbol, Symbol.iterator]);

        var mappedReaddedCallee = getEmptyMappedArguments();
        mappedReaddedCallee.foo = 1;
        delete mappedReaddedCallee[Symbol.iterator];
        delete mappedReaddedCallee.callee;
        mappedReaddedCallee.callee = function() { };
        shouldBeArray(Reflect.ownKeys(mappedReaddedCallee), ["length", "foo", "callee"]);

        var unmappedReaddedIterator = getUnmappedArguments(0);
        unmappedReaddedIterator.foo = 1;
        unmappedReaddedIterator[symbol] = 1;
        delete unmappedReaddedIterator[Symbol.iterator];
        unmappedReaddedIterator[Symbol.iterator] = Array.prototype.values;
        shouldBeArray(Reflect.ownKeys(unmappedReaddedIterator), ["0", "length", "callee", "foo", symbol, Symbol.iterator]);

        shouldBeArray(sloppyCallee(1), ["0", "length", "foo", "callee", Symbol.iterator]);
        shouldBeArray(sloppyKeys(1), ["0", "callee", "foo"]);

        var mappedEnumerable = getEmptyMappedArguments();
        mappedEnumerable.foo = 1;
        Object.defineProperty(mappedEnumerable, "length", { enumerable: true });
        Object.defineProperty(mappedEnumerable, "callee", { enumerable: true });
        shouldBeArray(Object.keys(mappedEnumerable), ["length", "callee", "foo"]);
        shouldBeArray(forIn(mappedEnumerable), ["length", "callee", "foo"]);
        shouldBeArray(Reflect.ownKeys(mappedEnumerable), ["length", "callee", "foo", Symbol.iterator]);

        var mappedReaddedLengthKeys = getEmptyMappedArguments();
        mappedReaddedLengthKeys.foo = 1;
        delete mappedReaddedLengthKeys.length;
        mappedReaddedLengthKeys.length = 2;
        shouldBeArray(Object.keys(mappedReaddedLengthKeys), ["foo", "length"]);

        if (deleteCallee(strictArgs(1)))
            throw new Error("strict callee delete should fail");
        if (!deleteCallee(nonStrictArgs(1)))
            throw new Error("non-strict callee delete should succeed");

        var strictWithFoo = strictArgs(1);
        strictWithFoo.foo = 1;
        var nonStrictWithFoo = nonStrictArgs(1);
        nonStrictWithFoo.foo = 1;
        if (deleteCallee(strictWithFoo))
            throw new Error("strict callee delete should fail");
        if (!deleteCallee(nonStrictWithFoo))
            throw new Error("non-strict callee delete should succeed");
    }

    var mappedIndices = getMappedArguments(0, 1, 2);
    Object.defineProperty(mappedIndices, "1", { enumerable: false });
    shouldBeArray(Object.keys(mappedIndices), ["0", "2"]);
    shouldBeArray(forIn(mappedIndices), ["0", "2"]);
    shouldBeArray(Object.getOwnPropertyNames(mappedIndices), ["0", "1", "2", "length", "callee"]);
    shouldBeArray(Reflect.ownKeys(mappedIndices), ["0", "1", "2", "length", "callee", Symbol.iterator]);
    delete mappedIndices[0];
    mappedIndices[0] = 9;
    mappedIndices[8] = 8;
    mappedIndices.foo = 1;
    shouldBeArray(Object.keys(mappedIndices), ["0", "2", "8", "foo"]);
    shouldBeArray(Reflect.ownKeys(mappedIndices), ["0", "1", "2", "8", "length", "callee", "foo", Symbol.iterator]);
    shouldBeArray(Object.getOwnPropertySymbols(mappedIndices), [Symbol.iterator]);

    var unmappedIndices = getUnmappedArguments(0, 1, 2);
    Object.defineProperty(unmappedIndices, "1", { enumerable: false });
    shouldBeArray(Object.keys(unmappedIndices), ["0", "2"]);
    shouldBeArray(Object.getOwnPropertyNames(unmappedIndices), ["0", "1", "2", "length", "callee"]);
    shouldBeArray(Reflect.ownKeys(unmappedIndices), ["0", "1", "2", "length", "callee", Symbol.iterator]);
    delete unmappedIndices[0];
    unmappedIndices[0] = 9;
    unmappedIndices[5] = 5;
    shouldBeArray(Reflect.ownKeys(unmappedIndices), ["0", "1", "2", "5", "length", "callee", Symbol.iterator]);
    shouldBeArray(Object.keys(unmappedIndices), ["0", "2", "5"]);

    function scoped(a) {
        eval("");
        var args = arguments;
        args.foo = 1;
        args[symbol] = 1;
        delete args[Symbol.iterator];
        args[Symbol.iterator] = Array.prototype.values;
        return Reflect.ownKeys(args);
    }
    noInline(scoped);
    shouldBeArray(scoped(1), ["0", "length", "callee", "foo", symbol, Symbol.iterator]);
})();
