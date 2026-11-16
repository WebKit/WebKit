function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

// `re` is RegExpObjectUse, `str` is StringUse
function test(re, str) {
    return re[Symbol.search](str);
}
noInline(test);

let catchCount = 0;
let expectedCatchCount = testLoopCount / 2;
const re = /foo/g;
for (let i = 0 ; i < testLoopCount; i++) {
    if (i === expectedCatchCount)
        Object.defineProperty(re, "lastIndex", { writable: false });
    try {
        test(re, " foo");
    } catch {
        catchCount++;
    }
}
shouldBe(catchCount, expectedCatchCount);
