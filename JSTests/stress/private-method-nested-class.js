function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

(() => {
    class Base {
        C = class {
            method() {
                return this.#mBase();
            }
        }

        #mBase() {
            return 4;
        }
    }

    let base = new Base();
    let c = new base.C();
    assert(c.method.call(base), 4);

    try {
        c.method();
    } catch (e) {
        assert(e instanceof TypeError, true);
    }
})();

// Test shadow methods

(() => {
    class Base {
        method() {
            return this.#m();
        }

        C = class {
            method(o) {
                return o.#m();
            }

            #m() {
                return this.foo;
            }

            foo = 4;
        };

        #m() {
            return "foo";
        }
    }

    let base = new Base();
    let c = new base.C();
    assert(c.method(c), 4);
    assert(base.method(), "foo");

    try {
        c.method(base);
    } catch (e) {
        assert(e instanceof TypeError, true);
    }
})();

