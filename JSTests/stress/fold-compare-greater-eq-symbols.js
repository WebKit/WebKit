function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}
let count = 0;
const x = Symbol();
for (let i = 0; i < testLoopCount; i++) {
  try {
      x >= x;
  } catch {
      count++;
  }
}

shouldBe(count, testLoopCount);
