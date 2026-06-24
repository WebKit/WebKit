function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function get(object, key) {
    return object[key];
}
noInline(get);

function put(object, key, value) {
    object[key] = value;
}
noInline(put);

const object = {};
object["4294967295"] = "max-uint32";
object["4294967296"] = "two-to-the-32";
object["-1"] = "minus-one";

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(get(object, 4294967296), "two-to-the-32");
    shouldBe(get(object, 4294967295), "max-uint32");
    shouldBe(get(object, -1), "minus-one");
    shouldBe(get(object, 4294967296.5), undefined);
    shouldBe(get(object, -0.5), undefined);
}

const array = [];
for (let i = 0; i < testLoopCount; ++i)
    put(array, 4294967296, i);
shouldBe(array.length, 0);
shouldBe(array["4294967296"], testLoopCount - 1);
