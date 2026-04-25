function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = new Array(100000);
array.fill(42);
for (let i = 0; i < 1000; i++) {
    array[i * 100] = [1, 2, 3, 4, 5];
}

let r;
for (let i = 0; i < 1e2; i++) {
    r = array.flat();
}

shouldBe(r.length, 99000 + 1000 * 5);
