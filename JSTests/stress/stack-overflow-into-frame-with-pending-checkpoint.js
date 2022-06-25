//@ requireOptions("--jitPolicyScale=0")

function foo() {
  'use strict'
  try {
    foo();
  } catch(e) {
    ({__proto__:0});
    async function bar(a0) {
      bar(a0, ...[0]);
    }
    for (let i=0; i<1000; i++) {
      bar();
    }
    bar(0,  0);
  }
}

foo();
