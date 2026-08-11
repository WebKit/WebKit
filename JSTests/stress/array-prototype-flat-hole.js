function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

Array.prototype[2] = 222;
const arr = [0, 1, , 3];
const f = arr.flat(0);

shouldBe(f.length, 4);
shouldBe(f[0], 0);
shouldBe(f[1], 1);
shouldBe(f[2], 222);
shouldBe(f[3], 3);
