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

assertThrows(TypeError, function () {
    const iterator = new Iterator();
});

class MyIterator extends Iterator {};
const myIterator = new MyIterator();
assert(myIterator instanceof Iterator, true);
assert(typeof myIterator.map, "function");
