function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(a) {
   return a <= NaN;
}
noInline(test);

let called = 0;
const value = (i) => ({
    valueOf() {
        called++;
        return i;
    }
});
for (let i = 0; i < testLoopCount; i ++) {
    shouldBe(test(value(i)), false);
}

shouldBe(called, testLoopCount);
