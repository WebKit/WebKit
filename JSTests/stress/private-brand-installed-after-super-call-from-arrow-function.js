function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

class A {}

class C extends A {
    #method() {
        return "base";
    }

    constructor() {
        let init = () => super();
        init();
    }

    baseAccess() {
        return this.#method();
    }
}

let c = new C();
assert(c.baseAccess(), "base");

