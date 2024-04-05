function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


function test(array, i) {
    return array[i];
}
noInline(test);

Uint8Array.prototype[-1] = 42;
Uint8Array.prototype[1000000000] = 42;

var array = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(array, -1), undefined);
    shouldBe(test(array, 1000000000), undefined);
    shouldBe(test(array, 0), 1);
    shouldBe(test(array, 1), 2);
}
