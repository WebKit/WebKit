function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

function test(result, length) {
  shouldBe(result.length, length);
  let i = 0;
  while (i < result.length) {
    shouldBe(Object.hasOwn(result, 0), true);
    shouldBe(result[i], void 0);
    i++;
  }
  shouldBe(i, result.length);
}
noInline(test);

{
  const length = 100;
  const result = Array.from({ length });

  shouldBe(result.length, length);
  let i = 0;
  while (i < result.length) {
    shouldBe(Object.hasOwn(result, i), true);
    shouldBe(result[i], void 0);
    i++;
  }
  shouldBe(i, result.length);
}

{
  const length = 100;
  const result = Array.from({ length, 0: "zero" });

  shouldBe(result.length, length);
  let i = 0;
  while (i < result.length) {
    shouldBe(Object.hasOwn(result, i), true);
    shouldBe(result[i], i === 0 ? "zero" : void 0);
    i++;
  }
  shouldBe(i, result.length);
}

{
  const length = 100;
  const result = Array.from({ length, prop: "prop" });

  shouldBe(result.length, length);
  let i = 0;
  while (i < result.length) {
    shouldBe(Object.hasOwn(result, i), true);
    shouldBe(result[i], void 0);
    i++;
  }
  shouldBe(i, result.length);
}

{
  const length = 100;
  class O {
    constructor(length) {
      this.length = length;
    }
  }
  const result = Array.from(new O(length));

  shouldBe(result.length, length);
  let i = 0;
  while (i < result.length) {
    shouldBe(Object.hasOwn(result, i), true);
    shouldBe(result[i], void 0);
    i++;
  }
  shouldBe(i, result.length);
}

{
  const result = Array.from({ length: "int32??" });
  shouldBe(result.length, 0);
}

{
  const length = 100.5;
  const result = Array.from({ length });

  shouldBe(result.length, Math.trunc(length));
  let i = 0;
  while (i < result.length) {
    shouldBe(Object.hasOwn(result, i), true);
    shouldBe(result[i], void 0);
    i++;
  }
  shouldBe(i, result.length);
}
