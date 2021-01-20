//@ runDefault("--collectContinuously=1", "--forceMiniVMMode=1", "--verifyHeap=1", "--forceEagerCompilation=1", "--useConcurrentJIT=0")
function foo() {
  const a0 = [1,2,3];
  let zero = 0;
  for (let i=0; i<100000; i++) {
    const zero2 = zero;
    const a1 = [1];
    const a2 = [a1];
    const o = {
      x: a0,
      y: Object
    };
    a2.z = a2;
    delete o.x;
    zero.q
  }
}

foo();
