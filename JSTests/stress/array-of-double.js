function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

for (let i = 0; i < testLoopCount; i++) {
  const array = Array.of(1.1, 2.1, 3.1, 4.1, 5.1);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.1);
  shouldBe(array[2], 3.1);
  shouldBe(array[3], 4.1);
  shouldBe(array[4], 5.1);
}

for (let i = 0; i < testLoopCount; i++) {
  const array = Array.of.call(1, 1.1, 2.1, 3.1, 4.1, 5.1);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.1);
  shouldBe(array[2], 3.1);
  shouldBe(array[3], 4.1);
  shouldBe(array[4], 5.1);
}

for (let i = 0; i < testLoopCount; i++) {
  const array = Array.of.call({}, 1.1, 2.1, 3.1, 4.1, 5.1);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.1);
  shouldBe(array[2], 3.1);
  shouldBe(array[3], 4.1);
  shouldBe(array[4], 5.1);
}
