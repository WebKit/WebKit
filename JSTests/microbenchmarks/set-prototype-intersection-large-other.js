function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const n1 = 512;
const array1 = [];
for (let i = 0; i < n1; i++) {
    array1[i] = i;
}

const n2 = 1024;
const array2 = [];
for (let i = 0; i < n2; i++) {
    array2[i] = i;
}

const s1 = new Set(array1);
const s2 = new Set(array2);

let r;
for (let i = 0; i < 1e5; i++) {
    r = s1.intersection(s2);
}

shouldBe(r.size, n2 / 2);
