function assert(b) {
    if (!b)
        throw new Error;
}

class C {
    #y;
    #method() { return 44; }
    constructor() {
        this.#y = 42;
    }
    a() { return eval('this.#y;'); }
    b() { return eval('this.#method();'); }
}

for (let i = 0; i < 1000; ++i) {
    let c = new C;
    assert(c.a() === 42);
    assert(c.b() === 44);
}
