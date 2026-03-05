function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

function concat(a) {
  return String.prototype.concat.call(a);
}
noInline(concat);

const str = "a".repeat(10);

for (let i = 0; i < testLoopCount; i++) {
  const result = concat(str);
  shouldBe(result, str);
}
