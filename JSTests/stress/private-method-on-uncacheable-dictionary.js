let assert = {
    sameValue: function (actual, expected) {
        if (actual !== expected)
            throw new Error("Expected: " + actual + " bug got: " + expected);
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
assert.sameValue(c1.access(), 'test');
assert.throws(TypeError, function () {
    c1.access.call({});
});

Object.freeze(c1);

assert.sameValue(c1.access(), 'test');
assert.throws(TypeError, function () {
    c1.access.call({});
});

// Perform a set brand transition on frozen object
let frozenObject = {};
Object.freeze(frozenObject);

class Base {
    constructor() {
        return frozenObject;
    }
}

class D extends Base {
    #m() { return 'test D'; }

    static access(o) {
        return o.#m();
    }
}

let d = new D();

assert.sameValue(D.access(d), 'test D');
assert.throws(TypeError, function () {
    D.access({});
});

