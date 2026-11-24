function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const SIZE = 1e3;
const array = new Array(SIZE).fill(0);

for (let i = 0; i < SIZE; i++) {
  array[i] = i * 1.1;
}

function test() {
  const newArray = array.flat(1);
  return newArray;
}

for (let i = 0; i < 1e4; i++) {
  shouldBe(test().length, SIZE);
}

