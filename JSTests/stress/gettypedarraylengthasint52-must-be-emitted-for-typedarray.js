//@ runDefault("--jitPolicyScale=0")
function foo(arg0) {
  arg0[0.1] = '';
  for (let j = 0; j < 100; j++);
}

foo([0]);
for (let i = 0; i < 1000; i++) {
  foo(0);
}
