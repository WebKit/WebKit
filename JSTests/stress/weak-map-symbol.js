function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var s1 = Symbol("A");
var s2 = Symbol("B");
var s3 = Symbol("C");
var s4 = Symbol("A");
var s5 = Symbol("B");
var s6 = Symbol("C");

function test() {
    var map = new WeakMap();
    shouldBe(map.get(s1), undefined);
    shouldBe(map.get(s2), undefined);
    shouldBe(map.get(s3), undefined);
    shouldBe(map.get(s4), undefined);
    shouldBe(map.get(s5), undefined);
    shouldBe(map.get(s6), undefined);

    shouldBe(map.has(s1), false);
    shouldBe(map.has(s2), false);
    shouldBe(map.has(s3), false);
    shouldBe(map.has(s4), false);
    shouldBe(map.has(s5), false);
    shouldBe(map.has(s6), false);

    map.set(s1, 1);
    map.set(s2, 2);
    map.set(s3, 3);

    shouldBe(map.get(s1), 1);
    shouldBe(map.get(s2), 2);
    shouldBe(map.get(s3), 3);
    shouldBe(map.get(s4), undefined);
    shouldBe(map.get(s5), undefined);
    shouldBe(map.get(s6), undefined);

    shouldBe(map.has(s1), true);
    shouldBe(map.has(s2), true);
    shouldBe(map.has(s3), true);
    shouldBe(map.has(s4), false);
    shouldBe(map.has(s5), false);
    shouldBe(map.has(s6), false);

    shouldBe(map.delete(s1), true);
    shouldBe(map.has(s1), false);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
