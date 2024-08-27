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

// https://tc39.es/proposal-iterator-helpers/#sec-iterator
assertThrows(TypeError, function () {
    new Iterator();
});

// https://tc39.es/proposal-iterator-helpers/#sec-iterator
class MyIterator extends Iterator {};
const myIterator = new MyIterator();
assert(myIterator instanceof Iterator, true);