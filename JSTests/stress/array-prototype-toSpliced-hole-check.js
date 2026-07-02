function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`length: expected ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; ++i) {
        if (a[i] !== b[i])
            throw new Error(`${i}: expected ${b[i]} but got ${a[i]}`);
    }
}

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = [, , , 1, 2, 3, 4];
const result = array.toSpliced(0, 0, 9, 8, 7);

sameArray(result, [9, 8, 7, undefined, undefined, undefined, 1, 2, 3, 4]);

shouldBe(Object.hasOwn(result, 3), true);
shouldBe(Object.hasOwn(result, 4), true);
shouldBe(Object.hasOwn(result, 5), true);
