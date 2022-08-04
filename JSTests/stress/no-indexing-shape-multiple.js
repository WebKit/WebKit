function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object, index) {
    return object[index];
}
noInline(test);

let v1 = { length: 42 };
let v2 = { length: 42, test: 42 };
for (let i = 0; i < 1e6; ++i) {
    shouldBe(test(v1, i), undefined);
    shouldBe(test(v2, i), undefined);
}
