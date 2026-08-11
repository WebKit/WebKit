function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function createLargeNestedArray(depth, width) {
    if (depth === 0) {
        return Array(width).fill(1);
    }
    const arr = [];
    for (let i = 0; i < width; i++) {
        arr.push(createLargeNestedArray(depth - 1, width));
    }
    return arr;
}

const array = new Array(512);
array.fill(99);
array[100] = createLargeNestedArray(3, 10);
array[200] = createLargeNestedArray(2, 20);
array[300] = createLargeNestedArray(4, 5);

let r;
for (let i = 0; i < 1e3; i++) {
    r = array.flat(Infinity);
}

shouldBe(r.length, 21634);
