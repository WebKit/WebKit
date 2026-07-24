//@ requireOptions("--useJointIteration=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

const size = 128;
const a = new Array(size);
const b = new Array(size);
const c = new Array(size);
for (let i = 0; i < size; i++) {
    a[i] = i;
    b[i] = i * 2;
    c[i] = i * 3;
}

function test() {
    let sum = 0;
    for (const [x, y, z] of Iterator.zip([a, b, c]))
        sum += x + y + z;
    return sum;
}
noInline(test);

let result;
for (let i = 0; i < testLoopCount; i++)
    result = test();

shouldBe(result, 6 * (size * (size - 1) / 2));
