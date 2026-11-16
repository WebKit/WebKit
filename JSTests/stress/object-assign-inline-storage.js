function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class C extends Object {
    constructor() {
        super();
    }
}

const a = new C();
const b = new C();
a.f = 42;
const c = Object["assign"](a, a, b);

shouldBe(a.f, 42);
shouldBe(b.f, undefined);
shouldBe(c.f, 42);
