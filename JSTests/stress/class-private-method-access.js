function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

function assertThrows(expectedError, f) {
    try {
        f();
    } catch (e) {
        assert(e instanceof expectedError, true);
    }
}

(() => {
    class C {
        static get #m() {
            return 'static';
        }

        static staticAccess() {
            return this.#m;
        }

        get #f() {
            return 'instance';
        }

        instanceAccess() {
            return this.#f;
        }
    }

    let c = new C();
    assert(C.staticAccess(), 'static');
    assert(c.instanceAccess(), 'instance');

    assertThrows(TypeError, () => {
        C.staticAccess.call(c);
    });

    assertThrows(TypeError, () => {
        c.instanceAccess.call(C);
    });
})();

(() => {
    class C {
        static #m() {
            return 'static';
        }

        static staticAccess() {
            return this.#m();
        }

        #f() {
            return 'instance';
        }

        instanceAccess() {
            return this.#f();
        }
    }

    let c = new C();
    assert(C.staticAccess(), 'static');
    assert(c.instanceAccess(), 'instance');

    assertThrows(TypeError, () => {
        C.staticAccess.call(c);
    });

    assertThrows(TypeError, () => {
        c.instanceAccess.call(C);
    });
})();

(() => {
    class C {
        static set #m(v) {
            this._m = v;
        }

        static staticAccess() {
            this.#m = 'static';
        }

        set #f(v) {
            this._f = v;
        }

        instanceAccess() {
            this.#f = 'instance';
        }
    }

    let c = new C();
    C.staticAccess();
    assert(C._m, 'static');
    c.instanceAccess()
    assert(c._f, 'instance');

    assertThrows(TypeError, () => {
        C.staticAccess.call(c);
    });

    assertThrows(TypeError, () => {
        c.instanceAccess.call(C);
    });
})();

