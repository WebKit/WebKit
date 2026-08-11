function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

Object.defineProperty(Object.prototype, '1', {
    get() { return "bad_time_value"; },
    set(v) { },
    configurable: true
});
shouldBe($vm.isHavingABadTime(), true);

{
    function* generator() {
        yield 100;
        yield 200;
        yield 300;
    }

    const result = generator().toArray();

    shouldBe(result.length, 3);
    shouldBe(result[0], 100);
    shouldBe(result[1], 200);
    shouldBe(result[2], 300);
}

{
    function* generator() {
        yield 10;
        yield 20;
        yield 30;
    }
    
    const iter = generator();
    let mutationApplied = false;
    const originalNext = iter.next;
    
    iter.next = function() {
        if (!mutationApplied) {
            mutationApplied = true;
            return { value: 777, done: false };
        }
        return originalNext.call(this);
    };
    
    const result = iter.toArray();
    shouldBe(result.length, 4);
    shouldBe(result[0], 777);
    shouldBe(result[1], 10);
    shouldBe(result[2], 20);
    shouldBe(result[3], 30);
}
