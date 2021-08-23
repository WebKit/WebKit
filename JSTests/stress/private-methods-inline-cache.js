function assert(actual, expected) {
    if (actual !== expected)
        throw Error("Expected: " + expected + " Actual: " + actual);
}

class C {
    #method() {
        return 'foo';
    }

    access() {
       return this.#method();
    }
}

for (let i = 0; i < 10000; i++) {
    let c = new C();
    assert(c.access(), 'foo');
}

