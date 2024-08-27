//@ requireOptions("--useIteratorHelpers=1")

function assert(a, b) {
    if (a !== b)
        throw new Error("Expected: " + b + " but got: " + a);
}

function assertThrows(expectedError, f) {
    try {
        f();
    } catch (e) {
        assert(e instanceof expectedError, true);
    }
}

// see https://tc39.es/proposal-iterator-helpers/#sec-set-iteratorprototype-constructor
assertThrows(TypeError, function () {
    Iterator.prototype.constructor = {};
});