let assert = {
    sameValue: function (actual, expected) {
        if (actual !== expected)
            throw new Error("Expected: " + expected + " but got: " + actual);
    },

    throws: function (expectedError, op) {
        try {
          op();
        } catch(e) {
            if (!(e instanceof expectedError))
                throw new Error("Expected to throw: " + expectedError + " but threw: " + e);
        }
    }
};

class C {
    #m() { return 'test'; }

    access() {
        return this.#m();
    }
}

let c1 = new C();
c1.foo = 'test';
assert.sameValue(c1.access(), 'test');
assert.sameValue(c1.foo, 'test');
assert.throws(TypeError, function () {
    c1.access.call({});
});

Object.defineProperty(c1, 'foo', {
    writable: false,
    enumerable: false
});

assert.sameValue(c1.access(), 'test');
assert.throws(TypeError, function () {
    c1.access.call({});
});

let descriptor = Object.getOwnPropertyDescriptor(c1, 'foo');

assert.sameValue(descriptor.value, 'test');
assert.sameValue(descriptor.enumerable, false);
assert.sameValue(descriptor.writable, false);

