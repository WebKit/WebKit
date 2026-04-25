function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

class Base {
    #mBase() {
        return 4;
    }

    methodBase() {
        return this.#mBase();
    }
}

class C extends Base {
    #m() {
        return this.#f;
    }

    method() {
        return this.#m();
    }

    #f = 15;
}

let c = new C();
assert(c.method(), 15);
assert(c.methodBase(), 4);

let base = new Base ();
assert(base.methodBase(), 4);
try {
    c.method.call(base);
} catch (e) {
    assert(e instanceof TypeError, true);
}

