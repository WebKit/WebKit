function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = [1, [2, 3], 4, [5], 6, [7, 8, 9], 10];

let r;
for (let i = 0; i < 1e5; i++) {
    r = array.flat();
}

shouldBe(r.length, 10);
