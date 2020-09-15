//@ skip if $architecture != "arm64" and $architecture != "x86-64"
//@ runDefault("--returnEarlyFromInfiniteLoopsForFuzzing=1")

function foo() {
  while(1);
}

if ($vm.useJIT()) {
    Reflect.construct(foo, {});
}
