//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")
const a = new Int8Array(4);
a[3];
function opt() {
  const arr = [...[1, 2, 3]];
  const func = arr?.constructor;
  try {
    new func(arr); // JSC use GetLocal to get Constructor, GetLocal is converted into Undefined in constant folding
    describe(1234)
  } catch (e) {
    throw e;
  }
  for (let i = 0; i < 100; i++) {}
}
for (let i = 0; i < 5; i++) {
  opt();
}
