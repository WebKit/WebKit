function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

function test(x) {
  (function () { return x; })();
  return Array.from(arguments);
}
noInline(test);


// contiguous
{
  const value1 = { value: 1 };
  const value2 = { value: 2 };
  const value3 = { value: 3 };
  const value4 = { value: 4 };
  const value5 = { value: 5 };
  const array = test(value1, value2, value3, value4, value5);
  shouldBe(array.length, 5);
  shouldBe(array[0], value1);
  shouldBe(array[1], value2);
  shouldBe(array[2], value3);
  shouldBe(array[3], value4);
  shouldBe(array[4], value5);
}

// double
{
  const array = test(1.1, 2.1, 3.1, 4.1, 5.1);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1.1);
  shouldBe(array[1], 2.1);
  shouldBe(array[2], 3.1);
  shouldBe(array[3], 4.1);
  shouldBe(array[4], 5.1);
}

// int32
{
  const array = test(1, 2, 3, 4, 5);
  shouldBe(array.length, 5);
  shouldBe(array[0], 1);
  shouldBe(array[1], 2);
  shouldBe(array[2], 3);
  shouldBe(array[3], 4);
  shouldBe(array[4], 5);
}
