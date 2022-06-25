function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

Object.defineProperty(globalThis, 'x', {
    get() {
        delete globalThis.x;
        return 2;
    },
    configurable: true,
});

x += 42;
shouldBe(x, 44);
