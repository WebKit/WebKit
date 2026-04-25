function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var f = new Function(`obj`, `return 0 ${"+ obj.i".repeat(1000)}`);
shouldBe(f({ i: 42 }), 42000);
