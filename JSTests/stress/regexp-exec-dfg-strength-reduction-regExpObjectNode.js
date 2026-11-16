//@ runDefault("--forceEagerCompilation=1", "--useConcurrentJIT=0", "--jitAllowlist=<global>")
function foo(s, a0, a1, a2, a3) {
  'use strict';
  return /foo/.exec(s);
}
for (var i = 0; i < 100; ++i) {
  foo('bar');
}
