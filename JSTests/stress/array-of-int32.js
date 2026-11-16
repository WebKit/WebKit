function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

for (let i = 0; i < testLoopCount; i++) {
  const array = Array.of(1, 2, 3, 4, 5);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  shouldBe(array[3], 4);
  shouldBe(array[4], 5);
}

for (let i = 0; i < testLoopCount; i++) {
  const array = Array.of.call(1, 1, 2, 3, 4, 5);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  shouldBe(array[3], 4);
  shouldBe(array[4], 5);
}

for (let i = 0; i < testLoopCount; i++) {
  const array = Array.of.call({}, 1, 2, 3, 4, 5);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  shouldBe(array[3], 4);
  shouldBe(array[4], 5);
}
