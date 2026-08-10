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
            return;
        }
        throw new Error("Expected to throw: " + expectedError + " but did not throw");
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

Object.seal(c1);

// Sealing does not apply to private names, so a brand installed beforehand keeps working.
assert.sameValue(c1.access(), 'test');
assert.throws(TypeError, function () {
    c1.access.call({});
});

// A private brand cannot be installed on a non-extensible object:
// https://github.com/tc39/proposal-nonextensible-applies-to-private
let sealedObject = { x: 1 };
Object.seal(sealedObject);

class Base {
    constructor() {
        return sealedObject;
    }
}

class D extends Base {
    #m() { return 'test D'; }

    static access(o) {
        return o.#m();
    }

    static has(o) {
        return #m in o;
    }
}

assert.throws(TypeError, function () {
    new D();
});
assert.sameValue(D.has(sealedObject), false);
assert.throws(TypeError, function () {
    D.access(sealedObject);
});
