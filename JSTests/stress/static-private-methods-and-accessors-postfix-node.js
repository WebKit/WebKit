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
            throw new Error("Should never be called");
        }

        static access() {
            return this.#method++;
        }
    }

    assert.throws(TypeError, function() {
        C.access();
    });
})();

(function() {
    let executedGetter = false;
    class C {
        static get #m() {
            executedGetter = true;
        }

        static access() {
            return this.#m++;
        }
    }

    assert.throws(TypeError, function() {
        C.access();
    });

    assert.sameValue(true, executedGetter);
})();

(function() {
    class C {
        static set #m(v) {
            throw new Error("Should never be executed");
        }

        static access() {
            return this.#m++;
        }
    }

    assert.throws(TypeError, function() {
        C.access();
    });
})();

(function() {
    class C {
        static set #m(v) {
            assert.sameValue(5, v);
        }

        static get #m() {
            return 4;
        }

        static access() {
            return this.#m++;
        }
    }

    assert.sameValue(4, C.access());
})();

// Ignored result

(function() {
    class C {
        static #method() {
            throw new Error("Should never be called");
        }

        static access() {
            this.#method--;
        }
    }

    assert.throws(TypeError, function() {
        C.access();
    });
})();

(function() {
    let executedGetter = false;
    class C {
        static get #m() {
            executedGetter = true;
        }

        static access() {
            this.#m--;
        }
    }

    assert.throws(TypeError, function() {
        C.access();
    });

    assert.sameValue(true, executedGetter);
})();

(function() {
    class C {
        static set #m(v) {
            throw new Error("Should never be executed");
        }

        static access() {
            this.#m--;
        }
    }

    assert.throws(TypeError, function() {
        C.access();
    });
})();

(function() {
    class C {
        static set #m(v) {
            assert.sameValue(3, v);
        }

        static get #m() {
            return 4;
        }

        static access() {
            this.#m--;
        }
    }

    C.access();
})();

