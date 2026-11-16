function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = new Array(1024);
array.fill(99);
array[512] = [1, 2, 3, 4, 5];

let r;
for (let i = 0; i < 1e5; i++) {
    r = array.flat();
}

shouldBe(r.length, 1023 + 5);
