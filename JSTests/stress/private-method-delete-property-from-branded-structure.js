let assert = {
    sameValue: function (lhs, rhs) {
        if (lhs !== rhs)
            throw new Error("Expected: " + lhs + " bug got: " + rhs);
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

delete c1.foo;

assert.sameValue(c1.access(), 'test');
assert.sameValue(c1.foo, undefined);
assert.throws(TypeError, function () {
    c1.access.call({});
});

