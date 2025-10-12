function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(regexp, string) {
    const iterator = String.prototype.matchAll.call(string, regexp);
    let count = 0;
    for (const match of iterator) {
        count++;
    }
    return count;
}
noInline(test);

const globalRegexp = /t(e)(s)t/g;
const testString = "test test test";

for (let i = 0; i < testLoopCount; i++) {
    const result = test(globalRegexp, testString);
    shouldBe(result, 3);
}

const unicodeRegexp = /[\u{1F600}-\u{1F64F}]/gu;
const emojiString = "Hello 😀 World 😃!";

for (let i = 0; i < testLoopCount; i++) {
    const result = test(unicodeRegexp, emojiString);
    shouldBe(result, 2);
}

const emptyRegexp = /(?:)/g;
const emptyTestString = "abc";

for (let i = 0; i < testLoopCount; i++) {
    const result = test(emptyRegexp, emptyTestString);
    shouldBe(result, 4);
}
