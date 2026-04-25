function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = new Array(2048);
array[100] = 1;
array[500] = [2, 3, 4];
array[1000] = 5;
array[1500] = [6, 7, 8, 9];
array[2000] = [[10, 11], [12, 13]];

let r;
for (let i = 0; i < 1e5; i++) {
    r = array.flat();
}

let count = 0;
for (let i = 0; i < r.length; i++) {
    if (r[i] !== undefined) count++;
}
shouldBe(count, 11);
