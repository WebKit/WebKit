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

(function () {
    let createAndInstantiateClass = function () {
        class C {
            get #m() { return 'test'; }

            access(o) {
                return o.#m;
            }
        }

        return new C();
    }

    let c1 = createAndInstantiateClass();
    let c2 = createAndInstantiateClass();

    assert.sameValue(c1.access(c1), 'test');
    assert.sameValue(c2.access(c2), 'test');

    assert.throws(TypeError, function() {
        c1.access(c2);
    });

    assert.throws(TypeError, function() {
        c2.access(c1);
    });
})();

(function () {
    class S {
        get #m() { return 'super class'; }

        superAccess() { return this.#m; }
    }

    class C extends S {
        get #m() { return 'subclass'; }

        access() {
            return this.#m;
        }
    }

    let c = new C();

    assert.sameValue(c.access(), 'subclass');
    assert.sameValue(c.superAccess(), 'super class');

    let s = new S();
    assert.sameValue(s.superAccess(), 'super class');
    assert.throws(TypeError, function() {
        c.access.call(s);
    });
})();

(function () {
    class C {
        get #m() { return 'test'; }

        access(o) {
            return o.#m;
        }
    }

    let c = new C();
    assert.sameValue(c.access(c), 'test');

    let o = {};
    assert.throws(TypeError, function() {
        c.access(o);
    });
})();

