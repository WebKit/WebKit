function test() {
    try {
        class Test {
            static #method() { };
            ['foo'.#method];
        };
    } catch (e) {
        return;
    }
    throw 'Should have thrown an exception.';
}

for (let i = 0; i < 100; i ++) {
    test();
}
