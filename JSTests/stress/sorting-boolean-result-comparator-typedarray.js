function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

const makeArray = () => new Int8Array([10, 3, 8, 5, 30, 100, 6, 7, 100, 3]);
const expected = "100,100,30,10,8,7,6,5,3,3";

const comparator1 = (x, y) => x < y;
const comparator2 = comparator1.bind();
const comparator3 = new Proxy(comparator2, {});

shouldBe(makeArray().sort(comparator1).join(), expected);
shouldBe(makeArray().toSorted(comparator1).join(), expected);
shouldBe(makeArray().sort(comparator2).join(), expected);
shouldBe(makeArray().toSorted(comparator3).join(), expected);
