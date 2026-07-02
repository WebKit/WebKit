function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

for (let i = 0; i < testLoopCount; i++) {
  const value1 = { value: 1 };
  const value2 = { value: 2 };
  const value3 = { value: 3 };
  const value4 = { value: 4 };
  const value5 = { value: 5 };
  const array = Array.of(value1, value2, value3, value4, value5);
  shouldBe(array.length, 5);
  shouldBe(array[0], value1);
  shouldBe(array[1], value2);
  shouldBe(array[2], value3);
  shouldBe(array[3], value4);
  shouldBe(array[4], value5);
}

for (let i = 0; i < testLoopCount; i++) {
  const value1 = { value: 1 };
  const value2 = { value: 2 };
  const value3 = { value: 3 };
  const value4 = { value: 4 };
  const value5 = { value: 5 };
  const array = Array.of.call({}, value1, value2, value3, value4, value5);
  shouldBe(array.length, 5);
  shouldBe(array[0], value1);
  shouldBe(array[1], value2);
  shouldBe(array[2], value3);
  shouldBe(array[3], value4);
  shouldBe(array[4], value5);
}

for (let i = 0; i < testLoopCount; i++) {
  const value1 = { value: 1 };
  const value2 = { value: 2 };
  const value3 = { value: 3 };
  const value4 = { value: 4 };
  const value5 = { value: 5 };
  const array = Array.of.call(1, value1, value2, value3, value4, value5);
  shouldBe(array.length, 5);
  shouldBe(array[0], value1);
  shouldBe(array[1], value2);
  shouldBe(array[2], value3);
  shouldBe(array[3], value4);
  shouldBe(array[4], value5);
}
