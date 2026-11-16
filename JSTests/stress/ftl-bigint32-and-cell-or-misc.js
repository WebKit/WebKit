//@ runDefault("--jitPolicyScale=0", "--useConcurrentJIT=0")

[].__proto__.x = undefined;

function foo() {
  for (const q in [0]) {
    const m1 = ~(0n>>0n);
    for (let i=0; i<2; i++) {}
    let m2 = m1;
  }
}
for (let i=0; i<100; i++) {
  foo();
}
