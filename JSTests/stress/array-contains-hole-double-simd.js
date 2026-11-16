function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function test(a) {
    return a.includes(undefined);
}
noInline(test);

const dense1 = [1.5];
const dense3 = [1.5, 2.5, 3.5];
const dense4 = [1.5, 2.5, 3.5, 4.5];
const dense8 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5];
const dense16 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5];

const hole0 = [, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5];
const hole3 = [1.5, 2.5, 3.5, , 5.5, 6.5, 7.5, 8.5, 9.5, 10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5];
const hole4 = [1.5, 2.5, 3.5, 4.5, , 6.5, 7.5, 8.5, 9.5, 10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5];
const hole7 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, , 9.5, 10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5];
const hole8 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, , 10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5];
const hole15 = [1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5, 10.5, 11.5, 12.5, 13.5, 14.5, 15.5, ,];

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(test(dense1), false);
    shouldBe(test(dense3), false);
    shouldBe(test(dense4), false);
    shouldBe(test(dense8), false);
    shouldBe(test(dense16), false);

    shouldBe(test(hole0), true);
    shouldBe(test(hole3), true);
    shouldBe(test(hole4), true);
    shouldBe(test(hole7), true);
    shouldBe(test(hole8), true);
    shouldBe(test(hole15), true);
}
