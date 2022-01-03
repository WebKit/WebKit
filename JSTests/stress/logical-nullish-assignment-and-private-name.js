function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class C {
    #field;

    m(object) {
        object.#field ??= 44;
        return this.#field;
    }
}

let c = new C();
shouldBe(c.m(c), 44);
