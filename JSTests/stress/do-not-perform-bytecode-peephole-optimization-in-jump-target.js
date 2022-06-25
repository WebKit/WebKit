function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let a = (1 || typeof 1) === 'string';
shouldBe(a, false);
