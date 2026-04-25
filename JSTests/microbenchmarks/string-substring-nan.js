function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error("bad value: " + actual);
    }
}

function test() {
    return "/assets/sakana".substring(0, NaN) === "";
}
noInline(test);

var count = 0;
for (var i = 0; i < testLoopCount; ++i) {
    if (test()) {
        ++count;
    }
}
shouldBe(count, testLoopCount);
