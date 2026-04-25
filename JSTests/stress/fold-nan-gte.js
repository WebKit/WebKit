function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(a) {
   return a >= NaN;
}
noInline(test);

for (let i = 0; i < testLoopCount; i ++) {
    shouldBe(test(i), false);
}
