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
    class C {
        set #m(v) { this._v = v; }

        B = class {
            method(o, v) {
                o.#m = v;
            }
        }
    }

    let c = new C();
    let innerB = new c.B();
    innerB.method(c, 'test');
    assert.sameValue(c._v, 'test');
})();

(function () {
    class C {
        set #m(v) { this._v = v; }

        method(v) { this.#m = v; }

        B = class {
            method(o, v) {
                o.#m = v;
            }

            get m() { return this.#m; }

            #m;
        }
    }

    let c = new C();
    let innerB = new c.B();

    innerB.method(innerB, 'test');
    assert.sameValue(innerB.m, 'test');

    c.method('outer class');
    assert.sameValue(c._v, 'outer class');

    assert.throws(TypeError, function() {
        innerB.method(c, 'foo');
    });
})();

(function () {
    class C {
        set #m(v) { this._v = v; }

        method(v) { this.#m = v; }

        B = class {
            method(o, v) {
                o.#m = v;
            }

            get #m() { return 'test'; }
        }
    }

    let c = new C();
    let innerB = new c.B();

    assert.throws(TypeError, function() {
        innerB.method(innerB);
    });

    c.method('outer class');
    assert.sameValue(c._v, 'outer class');

    assert.throws(TypeError, function() {
        innerB.method(c);
    });
})();

(function () {
    class C {
        set #m(v) { this._v = v; }

        method(v) { this.#m = v; }

        B = class {
            method(o, v) {
                o.#m = v;
            }

            #m() { return 'test'; }
        }
    }

    let c = new C();
    let innerB = new c.B();

    assert.throws(TypeError, function() {
        innerB.method(innerB, 'foo');
    });

    c.method('outer class');
    assert.sameValue(c._v, 'outer class');

    assert.throws(TypeError, function() {
        innerB.method(c);
    });
})();

(function () {
    class C {
        set #m(v) { this._v = v; }

        method(v) { this.#m = v; }

        B = class {
            method(o, v) {
                o.#m = v;
            }

            set #m(v) { this._v = v; }
        }
    }

    let c = new C();
    let innerB = new c.B();

    innerB.method(innerB, 'test262');
    assert.sameValue(innerB._v, 'test262');

    c.method('outer class');
    assert.sameValue(c._v, 'outer class');

    assert.throws(TypeError, function() {
        innerB.method(c, 'foo');
    });
})();

