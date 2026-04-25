//@ runDefault("--jitPolicyScale=0")
let a = new Float32Array(new ArrayBuffer());

function foo(f) {
  try {
    f();
  } catch {}
}

for (let x of [null, null, null, null, null, null, null, a, a, a, a,]) {
  foo(() => Atomics.sub(x));
  foo(() => Atomics.sub(x));
  foo(() => {
    'use strict';
    return Atomics.sub(x, 0, 0);
  });
  foo(() => Atomics.sub());
  foo(() => Atomics.sub());
}

for (let z of '') {}
