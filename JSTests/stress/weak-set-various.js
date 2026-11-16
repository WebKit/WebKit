
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var s1 = Symbol("A");
var s2 = Symbol("B");
var s3 = Symbol("C");
var o1 = {};
var o2 = {};
var o3 = {};

function test() {
    var set = new WeakSet();
    shouldBe(set.has(s1), false);
    shouldBe(set.has(s2), false);
    shouldBe(set.has(s3), false);
    shouldBe(set.has(o1), false);
    shouldBe(set.has(o2), false);
    shouldBe(set.has(o3), false);

    shouldBe(set.add(s1), set);
    shouldBe(set.add(s2), set);
    shouldBe(set.add(o1), set);
    shouldBe(set.add(o2), set);

    shouldBe(set.has(s1), true);
    shouldBe(set.has(s2), true);
    shouldBe(set.has(s3), false);
    shouldBe(set.has(o1), true);
    shouldBe(set.has(o2), true);
    shouldBe(set.has(o3), false);

    shouldBe(set.delete(s1), true);
    shouldBe(set.has(s1), false);

    shouldBe(set.delete(o1), true);
    shouldBe(set.has(o1), false);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
