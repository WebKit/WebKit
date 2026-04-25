function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array0 = new Uint8Array(128);
var array1 = new Uint8Array(1024);

function test(array) {
    return array.length;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i) {
    test(array0);
    test(array1);
}

array0.buffer.transfer();
array1.buffer.transfer();
shouldBe(array0.buffer.detached, true);
shouldBe(array1.buffer.detached, true);
shouldBe(test(array0), 0);
shouldBe(test(array1), 0);
