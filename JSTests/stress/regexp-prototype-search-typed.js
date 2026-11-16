function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(re, str) {
    return re[Symbol.search](str);
}
noInline(test);

// `re` is RegExpObjectUse, `str` is StringUse
const re = /foo/;
for (let i = 0 ; i < testLoopCount; i++) {
    shouldBe(test(re, " foo"), 1);
}
