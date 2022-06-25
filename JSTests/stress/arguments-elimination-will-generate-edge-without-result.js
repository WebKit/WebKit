//@ runDefault("--validateGraphAtEachPhase=true", "--jitPolicyScale=0", "--useConcurrentJIT=0")

'use strict';
function foo() {
  return arguments[1][0] === arguments[0];
}
for (let i = 0; i < 100000; ++i) {
  foo(0, 0);
}
