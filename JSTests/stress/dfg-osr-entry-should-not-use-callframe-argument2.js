//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe)

const v11 = ["abc","abcd", "abcde"];
const v21 = [1, 2, 3, 4, 5, 6];
function f22(a26) {
  [a26] = v11;
  for (let v37 = 0; v37 < 5; v37++) {}
  shouldBe(a26 !== undefined, true);
}
noInline(f22)
for (const v44 of v21) {
  f22();
  shouldBe(typeof v44, "number");
}
