//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(str, count) {
    return str.repeat(count);
}

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test("a", i).length, i);
}
