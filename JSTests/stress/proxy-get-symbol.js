function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let target = {};
let handlers = {
    get(target, property, receiver) {
        shouldBe(target, target);
        shouldBe(receiver, p);
        shouldBe(property, Symbol.iterator);
        shouldBe(this, handlers);
        return 42;
    }
};
let p = new Proxy(target, handlers);

for (var i = 0; i < 1e4; ++i)
    shouldBe(p[Symbol.iterator], 42);
