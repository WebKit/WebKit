function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = [1, 2, 3, 4, 5, 6];
let get2Count = 0;
Object.defineProperty(array, 2, {
    set(value) {
        get2Count++;
    }
});
array.fill(12, 1, 4);
shouldBe(get2Count, 1);
shouldBe(array[0], 1);
shouldBe(array[1], 12);
shouldBe(array[2], undefined);
shouldBe(array[3], 12);
shouldBe(array[4], 5);
shouldBe(array[5], 6);
