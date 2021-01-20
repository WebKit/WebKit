//@ runBytecodeCache("--usePrivateClassFields=true")

function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

class C {
    #f = 3;

    method() {
        return this.#f;
    }
}

let c = new C();
assert(c.method(), 3);

