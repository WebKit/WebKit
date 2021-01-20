//@ requireOptions("--usePrivateClassFields=1")

function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

class C {
    #m = () => 25;

    method() {
        return this.#m();
    }
}

let c = new C();
assert(c.method(), 25);

