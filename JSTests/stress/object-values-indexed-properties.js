function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
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

check({ 0: "a", 1: "b", 2: "c" });
check({ 0: "a", 1: "b", 2: "c", x: 1, y: 2 });
check({ 0: 10, 1: 20, z: 3 });
check({ 0: 1.5, 1: 2.5, w: "q" });

{
    var object = {};
    object[0] = "a";
    object[2] = "c";
    object.n = "named";
    check(object);
}

{
    var object = {};
    object[100000] = "big";
    object[5] = "small";
    object[50000] = "mid";
    object.k = "named";
    check(object);
    shouldBe(JSON.stringify(Object.values(object)), '["small","mid","big","named"]');
}

{
    var object = {};
    object[10] = "sparse";
    Object.defineProperty(object, 20, { value: "hidden", enumerable: false });
    object.k = "named";
    shouldBe(JSON.stringify(Object.values(object)), '["sparse","named"]');
}

{
    var object = {};
    for (var i = 0; i < 10000; ++i)
        object[i] = i;
    object.tail = "end";
    var values = Object.values(object);
    shouldBe(values.length, 10001);
    shouldBe(values[0], 0);
    shouldBe(values[9999], 9999);
    shouldBe(values[10000], "end");
}

for (var i = 0; i < testLoopCount; ++i) {
    var object = { 0: i, 1: i + 1, a: i, b: i + 2 };
    var values = Object.values(object);
    shouldBe(values.length, 4);
    shouldBe(values[0], i);
    shouldBe(values[1], i + 1);
    shouldBe(values[2], i);
    shouldBe(values[3], i + 2);
}
