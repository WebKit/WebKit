//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=1")

function foo() {
  while(1);
}

if ($vm.useJIT()) {
    Reflect.construct(foo, {});
}
