//@ runDefault("--slowPathAllocsBetweenGCs=10", "--jitPolicyScale=0")
let a = new BigUint64Array(1000);

function foo(a0) {
  ~a0;
}

for (let i = 0; i < 1000; i++) {
  foo.apply(null, a);
}
