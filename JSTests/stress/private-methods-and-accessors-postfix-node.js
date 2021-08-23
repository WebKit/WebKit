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
        #method() {
            throw new Error("Should never be called");
        }

        access() {
            return this.#method++;
        }
    }

    let c = new C();
    assert.throws(TypeError, function() {
        c.access();
    });
})();

(function() {
    let executedGetter = false;
    class C {
        get #m() {
            executedGetter = true;
        }

        access() {
            return this.#m++;
        }
    }

    let c = new C();
    assert.throws(TypeError, function() {
        c.access();
    });

    assert.sameValue(true, executedGetter);
})();

(function() {
    class C {
        set #m(v) {
            throw new Error("Should never be executed");
        }

        access() {
            return this.#m++;
        }
    }

    let c = new C();
    assert.throws(TypeError, function() {
        c.access();
    });
})();

(function() {
    class C {
        set #m(v) {
            assert.sameValue(5, v);
        }

        get #m() {
            return 4;
        }

        access() {
            return this.#m++;
        }
    }

    let c = new C();
    assert.sameValue(4, c.access());
})();

// Ignored result

(function() {
    class C {
        #method() {
            throw new Error("Should never be called");
        }

        access() {
            this.#method--;
        }
    }

    let c = new C();
    assert.throws(TypeError, function() {
        c.access();
    });
})();

(function() {
    let executedGetter = false;
    class C {
        get #m() {
            executedGetter = true;
        }

        access() {
            this.#m--;
        }
    }

    let c = new C();
    assert.throws(TypeError, function() {
        c.access();
    });

    assert.sameValue(true, executedGetter);
})();

(function() {
    class C {
        set #m(v) {
            throw new Error("Should never be executed");
        }

        access() {
            this.#m--;
        }
    }

    let c = new C();
    assert.throws(TypeError, function() {
        c.access();
    });
})();

(function() {
    class C {
        set #m(v) {
            assert.sameValue(3, v);
        }

        get #m() {
            return 4;
        }

        access() {
            this.#m--;
        }
    }

    let c = new C();
    c.access();
})();

