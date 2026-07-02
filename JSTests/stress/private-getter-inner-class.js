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
        get #m() { return 'test'; }

        B = class {
            method(o) {
                return o.#m;
            }
        }
    }

    let c = new C();
    let innerB = new c.B();
    assert.sameValue(innerB.method(c), 'test');
})();

(function () {
    class C {
        get #m() { return 'outer class'; }

        method() { return this.#m; }

        B = class {
            method(o) {
                return o.#m;
            }

            #m = 'test';
        }
    }

    let c = new C();
    let innerB = new c.B();
    assert.sameValue(innerB.method(innerB), 'test');
    assert.sameValue(c.method(), 'outer class');
    assert.throws(TypeError, function() {
        innerB.method(c);
    });
})();

(function () {
    class C {
        get #m() { return 'outer class'; }

        method() { return this.#m; }

        B = class {
            method(o) {
                return o.#m;
            }

            get #m() { return 'test'; }
        }
    }

    let c = new C();
    let innerB = new c.B();
    assert.sameValue(innerB.method(innerB), 'test');
    assert.sameValue(c.method(), 'outer class');
    assert.throws(TypeError, function() {
        innerB.method(c);
    });
})();

(function () {
    class C {
        get #m() { throw new Error('Should never execute'); }

        B = class {
            method(o) {
                return o.#m();
            }

            #m() { return 'test'; }
        }
    }

    let c = new C();
    let innerB = new c.B();
    assert.sameValue(innerB.method(innerB), 'test');
    assert.throws(TypeError, function() {
        innerB.method(c);
    });
})();

(function () {
    class C {
        get #m() { return 'outer class'; }

        method() { return this.#m; }

        B = class {
            method(o) {
                return o.#m;
            }

            set #m(v) { this._v = v; }
        }
    }

    let c = new C();
    let innerB = new c.B();

    assert.throws(TypeError, function() {
        innerB.method(innerB);
    });

    assert.sameValue(c.method(), 'outer class');

    assert.throws(TypeError, function() {
        C.prototype.method.call(innerB);
    });
})();

(function () {
    class C {
        #m() { return 'outer class'; }

        method() { return this.#m(); }

        B = class {
            method(o) {
                return o.#m;
            }

            get #m() { return 'test262'; }
        }
    }

    let c = new C();
    let innerB = new c.B();
    assert.sameValue(innerB.method(innerB), 'test262');
    assert.sameValue(c.method(), 'outer class');
    assert.throws(TypeError, function() {
        innerB.method(c);
    }, 'accessed inner class getter from an object of outer class');
    assert.throws(TypeError, function() {
        C.prototype.method.call(innerB);
    });
})();

