function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = new Array(1024);
array.fill(99);
array[256] = [[1, 2], [3, 4], [5, 6]];
array[512] = [[7, 8, 9], [10, 11, 12]];
array[768] = [[13], [14, 15], [16, 17, 18]];

let r;
for (let i = 0; i < 1e5; i++) {
    r = array.flat(2);
}

shouldBe(r.length, 1021 + 6 + 6 + 6);
