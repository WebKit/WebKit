function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let count = 0;
function foo() {
    for (let i = 0; i < testLoopCount; ++i) {
        let r = /abcd/;
        regexLastIndex = {};
        regexLastIndex.toString = function () {
            count++;
            return "1";
        };
        r.lastIndex = regexLastIndex;
        "test".replace(r, "cons")
    }
}

foo();

shouldBe(count, testLoopCount);
