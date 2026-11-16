function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function makeObject(count) {
    var object = {};
    for (var i = 0; i < count; ++i)
        object["p" + i] = i;
    return object;
}

function reference(object) {
    var result = [];
    for (var key of Object.keys(object))
        result.push(object[key]);
    return result;
}

function check(object) {
    shouldBe(JSON.stringify(Object.values(object)), JSON.stringify(reference(object)));
}

for (var count of [0, 1, 8, 9, 10, 16, 17, 32, 33, 100, 1000])
    check(makeObject(count));

{
    var object = makeObject(12);
    Object.defineProperty(object, "hidden", { value: 42, enumerable: false });
    object[Symbol("symbol")] = 43;
    check(object);
}

{
    var object = makeObject(12);
    object[3] = "three";
    object[1] = "one";
    object[100] = "hundred";
    check(object);
}

{
    var object = makeObject(12);
    delete object.p5;
    check(object);
    object.p5 = "again";
    check(object);
}

{
    var object = makeObject(12);
    Object.defineProperty(object, "getter", { get() { return "getter"; }, enumerable: true });
    check(object);
}

{
    var object = makeObject(12);
    Object.freeze(object);
    check(object);
}

{
    var object = makeObject(20);
    for (var i = 0; i < 20; ++i)
        object["p" + i] = { index: i };
    var results = [];
    for (var i = 0; i < testLoopCount; ++i)
        results.push(Object.values(object));
    gc();
    for (var result of results) {
        shouldBe(result.length, 20);
        for (var i = 0; i < 20; ++i)
            shouldBe(result[i].index, i);
    }
}

{
    var object = makeObject(9);
    var first = Object.values(object);
    var second = Object.values(object);
    first[0] = "changed";
    shouldBe(second[0], 0);
    first.push("pushed");
    shouldBe(first.length, 10);
}
