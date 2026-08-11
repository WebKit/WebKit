function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

function concat(a, b, c) {
  return String.prototype.concat.call(a, b, c);
}
noInline(concat);

const str = "a".repeat(10);
const arg1 = "b".repeat(10);
const arg2 = "c".repeat(10);

for (let i = 0; i < testLoopCount; i++) {
  const result = concat(str, arg1, arg2);
  shouldBe(result, `${str}${arg1}${arg2}`);
}
