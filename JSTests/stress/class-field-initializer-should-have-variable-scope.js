function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function test1() {
    const a = (x) => x

    class B {
        c = a('OK');
    }

    shouldBe(new B().c, "OK");
})();

(function test2() {
    const a = (x) => x

    class B {
        c = a(('OK'));
    }

    shouldBe(new B().c, "OK");
})();

(function test3() {
    const a = (x) => x;
    const b = 'ok';

    class B {
        [b]() { return 42; }
        c = a('OK');
    }

    shouldBe(new B().c, "OK");
    shouldBe(new B().ok(), 42);
})();
