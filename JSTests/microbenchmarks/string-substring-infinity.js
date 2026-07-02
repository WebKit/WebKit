function shouldBe(actual, expected) {
    if (actual !== expected) {
        throw new Error("bad value: " + actual);
    }
}

function test(start, end) {
    return "/assets/nanona".substring(0, Infinity) === "/assets/nanona";
}
noInline(test);

var count = 0;
for (var i = 0; i < testLoopCount; ++i) {
    if (test()) {
        ++count;
    }
}
shouldBe(count, testLoopCount);
