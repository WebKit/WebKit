function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

function NonArray() {}

for (let i = 0; i < testLoopCount; i ++) {
  const nonArray = Array.of.call(NonArray, i, i + 1, i + 2, i + 3);
  shouldBe(nonArray instanceof Array, false);
  shouldBe(nonArray instanceof NonArray, true);
  shouldBe(nonArray.length, 4);
  shouldBe(nonArray[0], i);
  shouldBe(nonArray[1], i + 1);
  shouldBe(nonArray[2], i + 2);
  shouldBe(nonArray[3], i + 3);
}
