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

function getTest(map, key)
{
    return map.get(key);
}
noInline(getTest);

function hasTest(map, key)
{
    return map.has(key);
}
noInline(hasTest);

function setTest(map, key, value)
{
    return map.set(key, value);
}
noInline(setTest);

function deleteTest(map, key)
{
    return map.delete(key);
}
noInline(deleteTest);

var s1 = Symbol("A");
var s2 = Symbol("B");
var s3 = Symbol("C");
var o1 = {};
var o2 = {};
var o3 = {};

function test() {
    var map = new WeakMap();
    shouldBe(getTest(map, s1), undefined);
    shouldBe(getTest(map, s2), undefined);
    shouldBe(getTest(map, s3), undefined);
    shouldBe(getTest(map, o1), undefined);
    shouldBe(getTest(map, o2), undefined);
    shouldBe(getTest(map, o3), undefined);

    shouldBe(hasTest(map, s1), false);
    shouldBe(hasTest(map, s2), false);
    shouldBe(hasTest(map, s3), false);
    shouldBe(hasTest(map, o1), false);
    shouldBe(hasTest(map, o2), false);
    shouldBe(hasTest(map, o3), false);

    shouldBe(setTest(map, s1, 1), map);
    shouldBe(setTest(map, s2, 2), map);
    shouldBe(setTest(map, o1, 3), map);
    shouldBe(setTest(map, o2, 4), map);

    shouldBe(getTest(map, s1), 1);
    shouldBe(getTest(map, s2), 2);
    shouldBe(getTest(map, s3), undefined);
    shouldBe(getTest(map, o1), 3);
    shouldBe(getTest(map, o2), 4);
    shouldBe(getTest(map, o3), undefined);

    shouldBe(hasTest(map, s1), true);
    shouldBe(hasTest(map, s2), true);
    shouldBe(hasTest(map, s3), false);
    shouldBe(hasTest(map, o1), true);
    shouldBe(hasTest(map, o2), true);
    shouldBe(hasTest(map, o3), false);

    shouldBe(deleteTest(map, s1), true);
    shouldBe(hasTest(map, s1), false);

    shouldBe(deleteTest(map, s3), false);
    shouldBe(hasTest(map, s3), false);

    shouldBe(deleteTest(map, o1), true);
    shouldBe(hasTest(map, o1), false);

    shouldBe(deleteTest(map, o3), false);
    shouldBe(hasTest(map, o3), false);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();

{
    var map = new WeakMap();
    shouldBe(getTest(map, "hey"), undefined);
    shouldBe(hasTest(map, "hey"), false);
    shouldBe(deleteTest(map, "hey"), false);
    shouldThrow(() => {
        setTest(map, "hey", "Hello");
    }, `TypeError: WeakMap keys must be objects or non-registered symbols`);

    shouldBe(getTest(map, 42), undefined);
    shouldBe(hasTest(map, 42), false);
    shouldBe(deleteTest(map, 42), false);
    shouldThrow(() => {
        setTest(map, 42, "Hello");
    }, `TypeError: WeakMap keys must be objects or non-registered symbols`);

}
