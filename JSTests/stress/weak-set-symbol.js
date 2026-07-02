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
    var set = new WeakSet();
    shouldBe(set.has(s1), false);
    shouldBe(set.has(s2), false);
    shouldBe(set.has(s3), false);
    shouldBe(set.has(s4), false);
    shouldBe(set.has(s5), false);
    shouldBe(set.has(s6), false);

    set.add(s1);
    set.add(s2);
    set.add(s3);

    shouldBe(set.has(s1), true);
    shouldBe(set.has(s2), true);
    shouldBe(set.has(s3), true);
    shouldBe(set.has(s4), false);
    shouldBe(set.has(s5), false);
    shouldBe(set.has(s6), false);

    shouldBe(set.delete(s1), true);
    shouldBe(set.has(s1), false);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
