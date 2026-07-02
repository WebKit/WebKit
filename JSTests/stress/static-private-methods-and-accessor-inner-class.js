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

(function() {
    class C {
        static #method() {
            return 'outer class';
        }

        static access() {
            return this.#method();
        }

        static InnerC = class {
            static #method2() {
                return 'inner class';
            }

            static access() {
                return this.#method2();
            }
        }
    }

    assert.sameValue(C.access(), 'outer class');
    assert.sameValue(C.InnerC.access(), 'inner class');

    assert.throws(TypeError, function() {
        C.access.call(C.InnerC);
    });

    assert.throws(TypeError, function() {
        C.InnerC.access.call(C);
    });
})();

(function() {
    class C {
        static #method() {
            return 'outer class';
        }

        static access() {
            return this.#method();
        }

        static InnerC = class {
            static #method2() {
                throw new Error("Should never be called");
            }

            static access(o) {
                return o.#method();
            }
        }
    }

    assert.sameValue(C.access(), 'outer class');
    assert.sameValue(C.InnerC.access(C), 'outer class');

    assert.throws(TypeError, function() {
        C.access.call(C.InnerC);
    });

    assert.throws(TypeError, function() {
        C.InnerC.access(C.InnerC);
    });
})();

(function() {
    class C {
        static get #m() {
            return 'outer class';
        }

        static access() {
            return this.#m;
        }

        static InnerC = class {
            static get #m2() {
                return 'inner class';
            }

            static access() {
                return this.#m2;
            }
        }
    }

    assert.sameValue(C.access(), 'outer class');
    assert.sameValue(C.InnerC.access(), 'inner class');

    assert.throws(TypeError, function() {
        C.access.call(C.InnerC);
    });

    assert.throws(TypeError, function() {
        C.InnerC.access.call(C);
    });
})();

(function() {
    class C {
        static get #m() {
            return 'outer class';
        }

        static access() {
            return this.#m;
        }

        static InnerC = class {
            static get #m2() {
                throw new Error("Should never be called");
            }

            static access(o) {
                return o.#m;
            }
        }
    }

    assert.sameValue(C.access(), 'outer class');
    assert.sameValue(C.InnerC.access(C), 'outer class');

    assert.throws(TypeError, function() {
        C.access.call(C.InnerC);
    });

    assert.throws(TypeError, function() {
        C.InnerC.access(C.InnerC);
    });
})();

(function() {
    class C {
        static set #m(v) {
            this._v = v;
        }

        static access() {
            this.#m = 'outer class';
        }

        static InnerC = class {
            static set #m2(v) {
                this._v = v;
            }

            static access() {
                this.#m2 = 'inner class';
            }
        }
    }

    C.access();
    assert.sameValue(C._v, 'outer class');

    C.InnerC.access();
    assert.sameValue(C.InnerC._v, 'inner class');

    assert.throws(TypeError, function() {
        C.access.call(C.InnerC);
    });

    assert.throws(TypeError, function() {
        C.InnerC.access.call(C);
    });
})();

(function() {
    class C {
        static set #m(v) {
            this._v = v;
        }

        static access() {
            this.#m = 'outer class';
        }

        static InnerC = class {
            static set #m2(v) {
                throw new Error("Should never be executed");
            }

            static access(o) {
                o.#m = 'inner class';
            }
        }
    }

    C.access();
    assert.sameValue(C._v, 'outer class');

    C.InnerC.access(C);
    assert.sameValue(C._v, 'inner class');

    assert.throws(TypeError, function() {
        C.access.call(C.InnerC);
    });

    assert.throws(TypeError, function() {
        C.InnerC.access(C.InnerC);
    });
})();

