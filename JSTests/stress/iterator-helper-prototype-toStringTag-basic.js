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

const iterator = new class extends Iterator {}
const iteratorHelper = iterator.map(() => {});

assert(Object.prototype.toString.call(iteratorHelper), "[object Iterator Helper]");
