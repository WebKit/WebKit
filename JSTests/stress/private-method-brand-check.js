function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

class C {
    #m() {
        return this.#f;
    }

    method() {
        return this.#m();
    }

    #f = 4;
}

let c = new C();
assert(c.method(), 4);

try {
    class C {
        #m() { return 3; }
    }

    let o = new C();
    c.method.call(o);
} catch (e) {
    assert(e instanceof TypeError, true);
}

