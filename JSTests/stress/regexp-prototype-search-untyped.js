function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(re, str) {
    return RegExp.prototype[Symbol.search].call(re, str);
}
noInline(test);

// `re` is Untyped
for (let i = 0 ; i < testLoopCount; i++) {
    try {
        test({ toString() { throw new Error("error"); }}, " foo");
    } catch {}
    shouldBe(test(/foo/, " foo"), 1);
}
