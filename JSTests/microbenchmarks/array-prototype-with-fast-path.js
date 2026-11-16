function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array1, index, value) {
    return array1.with(index, value);
}

noInline(test);

const array = new Array(1024);
array.fill(99);

const index = 512;
const newValue = 300;

for (let i = 0; i < 1e2; i++) {
    const result = test(array, index, newValue);
    shouldBe(result[index], newValue);
    shouldBe(result.length, array.length);
}
