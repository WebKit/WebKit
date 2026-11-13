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
        // FIXME: "callee" should come before "foo" and "bar"
        // shouldBeArray(Reflect.ownKeys(unmappedArguments), ["0", "1", "2", "length", "callee", "foo", "bar", Symbol.iterator]);
    }
})();

function getScopedArguments(a) {
    function capture() { return a; }
    return arguments;
}
noInline(getScopedArguments);

(function() {
    var symbol = Symbol("s");
    for (var i = 0; i < 1e4; ++i) {
        var mappedArguments = getMappedArguments();
        mappedArguments[symbol] = 1;
        mappedArguments.z = 2;
        shouldBeArray(Object.keys(mappedArguments), ["z"]);
        shouldBeArray(Object.getOwnPropertyNames(mappedArguments), ["length", "callee", "z"]);
        shouldBeArray(Reflect.ownKeys(mappedArguments), ["length", "callee", "z", Symbol.iterator, symbol]);

        var scopedArguments = getScopedArguments();
        scopedArguments[symbol] = 1;
        scopedArguments.z = 2;
        shouldBeArray(Object.keys(scopedArguments), ["z"]);
        shouldBeArray(Object.getOwnPropertyNames(scopedArguments), ["length", "callee", "z"]);
        shouldBeArray(Reflect.ownKeys(scopedArguments), ["length", "callee", "z", Symbol.iterator, symbol]);
    }
})();

// FIXME: Ordering once length, callee, or @@iterator have been materialized, deleted, or redefined.
