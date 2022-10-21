//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function slice(string) {
    return string.slice(3, 3);
}
noInline(slice);

for (var i = 0; i < 1e6; ++i)
    shouldBe(slice("Cocoa"), "");
