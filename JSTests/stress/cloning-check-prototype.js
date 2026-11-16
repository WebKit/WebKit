function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class Test {field1 = 'foo';}
const t1 = { ...new Test() };
shouldBe(t1 instanceof Test, false);
