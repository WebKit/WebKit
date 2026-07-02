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
            set #m(v) { this._v = v; }

            access(o, v) {
                o.#m = v;
            }
        }

        let c = new C();
        return c;
    };

    let c1 = createAndInstantiateClass();
    let c2 = createAndInstantiateClass();

    c1.access(c1, 'test');
    assert.sameValue(c1._v, 'test');
    c2.access(c2, 'test');
    assert.sameValue(c2._v, 'test');

    assert.throws(TypeError, function() {
        c1.access(c2, 'foo');
    });

    assert.throws(TypeError, function() {
        c2.access(c1, 'foo');
    });
})();

(function () {
    class S {
        set #m(v) { this._v = v }

        superAccess(v) { this.#m = v; }
    }

    class C extends S {
        set #m(v) { this._u = v; }

        access(v) {
            return this.#m = v;
        }
    }

    let c = new C();

    c.access('test');
    assert.sameValue(c._u, 'test');

    c.superAccess('super class');
    assert.sameValue(c._v, 'super class');

    let s = new S();
    s.superAccess('super class')
    assert.sameValue(s._v, 'super class');

    assert.throws(TypeError, function() {
        c.access.call(s, 'foo');
    });
})();

(function () {
    class C {
        set #m(v) { this._v = v; }

        access(o, v) {
            return o.#m = v;
        }
    }

    let c = new C();
    c.access(c, 'test');
    assert.sameValue(c._v, 'test');

    let o = {};
    assert.throws(TypeError, function() {
        c.access(o, 'foo');
    });
})();

