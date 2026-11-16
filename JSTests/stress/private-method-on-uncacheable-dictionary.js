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

Object.freeze(c1);

// Freezing does not apply to private names, so a brand installed beforehand keeps working.
assert.sameValue(c1.access(), 'test');
assert.throws(TypeError, function () {
    c1.access.call({});
});

// A brand transition on a dictionary structure has to preserve the existing out-of-line storage.
function testDictionary(makeDictionary, tag) {
    let dictionary = {};
    for (let i = 0; i < 100; ++i)
        dictionary['p' + i] = i;
    makeDictionary(dictionary);

    class Base {
        constructor() {
            return dictionary;
        }
    }

    class D extends Base {
        #m() { return tag; }

        static access(o) {
            return o.#m();
        }
    }

    new D();

    assert.sameValue(D.access(dictionary), tag);
    assert.sameValue(dictionary.p99, 99);
    assert.throws(TypeError, function () {
        D.access({});
    });
}

testDictionary($vm.toUncacheableDictionary, 'uncacheable dictionary');
testDictionary($vm.toCacheableDictionary, 'cacheable dictionary');

// A private brand cannot be installed on a non-extensible object:
// https://github.com/tc39/proposal-nonextensible-applies-to-private
let frozenObject = {};
for (let i = 0; i < 100; ++i)
    frozenObject['p' + i] = i;
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

    static has(o) {
        return #m in o;
    }
}

assert.throws(TypeError, function () {
    new D();
});
assert.sameValue(D.has(frozenObject), false);
assert.throws(TypeError, function () {
    D.access(frozenObject);
});
