function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const array = new Array(1024);
array.fill(99);
delete array[array.length - 1];

let result;
for (let i = 0; i < 1e4; i++) {
    result = array.concat();
}

shouldBe(result.length, 1024);
