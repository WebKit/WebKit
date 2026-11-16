function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function hasTest(set, key)
{
    return set.has(key);
}
noInline(hasTest);

function addTest(set, key)
{
    return set.add(key);
}
noInline(addTest);

function deleteTest(set, key)
{
    return set.delete(key);
}
noInline(deleteTest);

var s1 = Symbol("A");
var s2 = Symbol("B");
var s3 = Symbol("C");
var o1 = {};
var o2 = {};
var o3 = {};

function test() {
    var set = new WeakSet();
    shouldBe(hasTest(set, s1), false);
    shouldBe(hasTest(set, s2), false);
    shouldBe(hasTest(set, s3), false);
    shouldBe(hasTest(set, o1), false);
    shouldBe(hasTest(set, o2), false);
    shouldBe(hasTest(set, o3), false);

    shouldBe(addTest(set, s1), set);
    shouldBe(addTest(set, s2), set);
    shouldBe(addTest(set, o1), set);
    shouldBe(addTest(set, o2), set);

    shouldBe(hasTest(set, s1), true);
    shouldBe(hasTest(set, s2), true);
    shouldBe(hasTest(set, s3), false);
    shouldBe(hasTest(set, o1), true);
    shouldBe(hasTest(set, o2), true);
    shouldBe(hasTest(set, o3), false);

    shouldBe(deleteTest(set, s1), true);
    shouldBe(hasTest(set, s1), false);

    shouldBe(deleteTest(set, o1), true);
    shouldBe(hasTest(set, o1), false);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();

{
    var set = new WeakSet();
    shouldBe(hasTest(set, "hey"), false);
    shouldBe(deleteTest(set, "hey"), false);
    shouldThrow(() => {
        addTest(set, "hey");
    }, `TypeError: WeakSet values must be objects or non-registered symbols`);

    shouldBe(hasTest(set, 42), false);
    shouldBe(deleteTest(set, 42), false);
    shouldThrow(() => {
        addTest(set, 42);
    }, `TypeError: WeakSet values must be objects or non-registered symbols`);
}
