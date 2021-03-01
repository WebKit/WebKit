//@ runDefault("--forceEagerCompilation=1", "--maximumOptimizationDelay=100")
function foo(a0, a1, a2, a3) {
  $vm.haveABadTime();
  for (let i=0; i<3; i++) {
    for (let _ of Array(1325)) {}
  }
  if (Object) {}
  for (let _ in a0) {}
}

for (let i=0; i<100; i++) {
  foo('', 0, '');
}

[0].forEach(foo);
